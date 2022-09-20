
#include <iostream>

#include <Epetra_Comm.h>
#include <Epetra_MpiComm.h>
#include "Epetra_SerialComm.h"

#include "Teuchos_ParameterList.hpp"
#include "Teuchos_ParameterXMLFileReader.hpp"
#include "Teuchos_XMLParameterListHelpers.hpp"
#include "Teuchos_CommandLineProcessor.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_StandardParameterEntryValidators.hpp"
#include "Teuchos_VerboseObjectParameterListHelpers.hpp"

#include "VerboseObject_objs.hh"
#include "VerboseObject.hh"

#include "ats_version.hh"
#include "amanzi_version.hh"
#include "tpl_versions.h"

#include "AmanziComm.hh"
#include "AmanziTypes.hh"
#include "GeometricModel.hh"
#include "State.hh"

#include "errors.hh"
#include "exceptions.hh"
#include "dbc.hh"

#include "ats_mesh_factory.hh"
#include "elm_ats_coordinator.hh"
#include "elm_ats_driver.hh"

// registration files
#include "state_evaluators_registration.hh"

#include "ats_relations_registration.hh"
#include "ats_transport_registration.hh"
#include "ats_energy_pks_registration.hh"
#include "ats_energy_relations_registration.hh"
#include "ats_flow_pks_registration.hh"
#include "ats_flow_relations_registration.hh"
#include "ats_deformation_registration.hh"
#include "ats_bgc_registration.hh"
#include "ats_surface_balance_registration.hh"
#include "ats_mpc_registration.hh"
//#include "ats_sediment_transport_registration.hh"
#include "mdm_transport_registration.hh"
#include "multiscale_transport_registration.hh"
#ifdef ALQUIMIA_ENABLED
#include "pks_chemistry_registration.hh"
#endif

// include fenv if it exists
#include "boost/version.hpp"
#if (BOOST_VERSION / 100 % 1000 >= 46)
#include "boost/config.hpp"
#ifndef BOOST_NO_FENV_H
#ifdef _GNU_SOURCE
#define AMANZI_USE_FENV
#include "boost/detail/fenv.hpp"
#endif
#endif
#endif

#include "boost/filesystem.hpp"

#include "pk_helpers.hh"
#include "wrm_evaluator.hh"

namespace ATS {

void
ELM_ATSDriver::setup(MPI_Fint *f_comm, const char *infile)
{
  // -- create communicator & get process rank
  //auto comm = Amanzi::getDefaultComm();
  auto c_comm = MPI_Comm_f2c(*f_comm);
  auto comm = setComm(c_comm);
  auto rank = comm->MyPID();

  // convert input file to std::string for easier handling
  // infile must be null-terminated
  std::string input_filename(infile);

  // check validity of input file name
  if (input_filename.empty()) {
    if (rank == 0)
      std::cerr << "ERROR: no input file provided" << std::endl;
  } else if (!boost::filesystem::exists(input_filename)) {
    if (rank == 0)
      std::cerr << "ERROR: input file \"" << input_filename << "\" does not exist." << std::endl;
  }

  // -- parse input file
  Teuchos::RCP<Teuchos::ParameterList> plist = Teuchos::getParametersFromXmlFile(input_filename);

  // -- reset input plist for specific needs of elm, e.g. materials
  // by default, elm starts from time 0, with time-steps of 1800.0 s.
  //   (those may be changing when running each time-step)
  if (elmdata_.NCELLS_>0){
    ATS::elm_ats_plist elm_plist_(plist);
    double start_ts = 0.0;
    double dt = 1800.0;
    elm_plist_.set_plist(elmdata_, start_ts, dt);
  }

  // -- set default verbosity level to no output
  Amanzi::VerboseObject::global_default_level = Teuchos::VERB_NONE;

  // create the geometric model and regions
  Teuchos::ParameterList reg_params = plist->sublist("regions");
  Teuchos::RCP<Amanzi::AmanziGeometry::GeometricModel> gm =
    Teuchos::rcp(new Amanzi::AmanziGeometry::GeometricModel(3, reg_params, *comm) );

  // Create the state.
  Teuchos::ParameterList state_plist = plist->sublist("state");
  S_ = Teuchos::rcp(new Amanzi::State(state_plist));

  // create and register meshes
  ATS::Mesh::createMeshes(*plist, comm, gm, *S_);

  // domains
  domain_sub_ = plist->get<std::string>("domain name", "domain");
  domain_srf_ = Amanzi::Keys::readDomainHint(*plist, domain_sub_, "subsurface", "surface");

  // keys for fields exchanged with ELM
  sub_src_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "subsurface source", "source_sink");
  srf_src_key_ = Amanzi::Keys::readKey(*plist, domain_srf_, "surface source", "source_sink");
  pres_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "pressure", "pressure");
  pd_key_ = Amanzi::Keys::readKey(*plist, domain_srf_, "ponded depth", "ponded_depth");
  satl_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "saturation_liquid", "saturation_liquid");
  por_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "porosity", "porosity");
  elev_key_ = Amanzi::Keys::readKey(*plist, domain_srf_, "elevation", "elevation");

  watl_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "water_content", "water_content");

  // keys for fields used to convert ELM units to ATS units
  srf_mol_dens_key_ = Amanzi::Keys::readKey(*plist, domain_srf_, "surface molar density", "molar_density_liquid");
  srf_mass_dens_key_ = Amanzi::Keys::readKey(*plist, domain_srf_, "surface mass density", "mass_density_liquid");
  sub_mol_dens_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "molar density", "molar_density_liquid");
  sub_mass_dens_key_ = Amanzi::Keys::readKey(*plist, domain_sub_, "mass density", "mass_density_liquid");


  // PKs list, ...
  pks_plist_   = Teuchos::sublist(plist, "PKs");
  state_plist_ = Teuchos::sublist(plist, "state");
  subpk_key_ = "flow";
  srfpk_key_ = "overland flow";
  sub_pv_key_ = Teuchos::sublist(pks_plist_, subpk_key_)->get<std::string>("primary variable key");
  srf_pv_key_ = Teuchos::sublist(pks_plist_, srfpk_key_)->get<std::string>("primary variable key");

  // assume for now that mesh info has been communicated
  mesh_subsurf_ = S_->GetMesh(domain_sub_);
  mesh_surf_ = S_->GetMesh(domain_srf_);


  // visualization on/off from input plist
  // note: this will turn on/off what options passing from ELM interface
  //     and, have to exactly set up in *.xml as:
  /*
  <ParameterList name="visualization">
    <ParameterList name="domain">
      <Parameter name="cycles start period stop" type="Array(int)" value="{0, 1, -1}" />  <!-- This exact format of vis will control output together with ELM options  -->
    </ParameterList>
  </ParameterList>
 */
  plist_visout_ = false;
  Teuchos::RCP<Teuchos::ParameterList> pl = Teuchos::sublist(Teuchos::sublist(plist, "visualization"), "domain");
  auto vis_out = pl->get<Teuchos::Array<int>>("cycles start period stop");
  if (vis_out[2] == -1) {plist_visout_ = true; }

  // build columns to allow indexing by column
  mesh_subsurf_->build_columns();

  // check that number of surface cells = number of columns
  ncolumns_ = mesh_surf_->num_entities(Amanzi::AmanziMesh::CELL, Amanzi::AmanziMesh::Parallel_type::OWNED);
  AMANZI_ASSERT(ncolumns_ == mesh_subsurf_->num_columns(false));

  // get num cells per column - include consistency check later
  // need to know if coupling zone is the entire subsurface mesh (as currently coded)
  // or a portion of the total depth specified by # of cells into the subsurface
  auto& col_zero = mesh_subsurf_->cells_of_column(0);
  ncol_cells_ = col_zero.size();

  // require primary variables
  // -- subsurface water source
  S_->Require<Amanzi::CompositeVector,Amanzi::CompositeVectorSpace>(sub_src_key_, Amanzi::Tags::NEXT,  sub_src_key_)
    .SetMesh(mesh_subsurf_)->SetComponent("cell", Amanzi::AmanziMesh::CELL, 1);
  RequireEvaluatorPrimary(sub_src_key_, Amanzi::Tags::NEXT, *S_);
  // -- surface water source-sink
  S_->Require<Amanzi::CompositeVector,Amanzi::CompositeVectorSpace>(srf_src_key_, Amanzi::Tags::NEXT,  srf_src_key_)
    .SetMesh(mesh_surf_)->SetComponent("cell", Amanzi::AmanziMesh::CELL, 1);
  RequireEvaluatorPrimary(srf_src_key_, Amanzi::Tags::NEXT, *S_);
  S_->Require<Amanzi::CompositeVector,Amanzi::CompositeVectorSpace>("dz", Amanzi::Tags::NEXT,  "dz")
    .SetMesh(mesh_subsurf_)->SetComponent("cell", Amanzi::AmanziMesh::CELL, 1);
  RequireEvaluatorPrimary("dz", Amanzi::Tags::NEXT, *S_);
  S_->Require<Amanzi::CompositeVector,Amanzi::CompositeVectorSpace>("depth", Amanzi::Tags::NEXT,  "depth")
    .SetMesh(mesh_subsurf_)->SetComponent("cell", Amanzi::AmanziMesh::CELL, 1);
  RequireEvaluatorPrimary("depth", Amanzi::Tags::NEXT, *S_);

  // commented out for now while building and testing- setting as Primary (and initializing as 0.0)
  // causes problems when running existing unmodified ATS xml files
  // -- porosity
  //S_->Require<Amanzi::CompositeVector,Amanzi::CompositeVectorSpace>(por_key_, Amanzi::Tags::NEXT,  por_key_)
  //  .SetMesh(mesh_subsurf_)->SetComponent("cell", Amanzi::AmanziMesh::CELL, 1);
  //RequireEvaluatorPrimary(por_key_, Amanzi::Tags::NEXT, *S_);

  // create ELM coordinator object
  elm_coordinator_ = std::make_unique<ELM_ATSCoordinator>(*plist, S_, comm);
  // call coordinator setup
  elm_coordinator_->setup();
}

void ELM_ATSDriver::initialize()
{
  elm_coordinator_->initialize();

  S_->GetW<Amanzi::CompositeVector>(sub_src_key_, Amanzi::Tags::NEXT, sub_src_key_).PutScalar(0.);
  S_->GetRecordW(sub_src_key_, Amanzi::Tags::NEXT, sub_src_key_).set_initialized();
  S_->GetW<Amanzi::CompositeVector>(srf_src_key_, Amanzi::Tags::NEXT, srf_src_key_).PutScalar(0.);
  S_->GetRecordW(srf_src_key_, Amanzi::Tags::NEXT, srf_src_key_).set_initialized();

  S_->GetW<Amanzi::CompositeVector>("dz", Amanzi::Tags::NEXT, "dz").PutScalar(0.);
  S_->GetRecordW("dz", Amanzi::Tags::NEXT, "dz").set_initialized();
  S_->GetW<Amanzi::CompositeVector>("depth", Amanzi::Tags::NEXT, "depth").PutScalar(0.);
  S_->GetRecordW("depth", Amanzi::Tags::NEXT, "depth").set_initialized();
  //S_->GetW<Amanzi::CompositeVector>(por_key_, Amanzi::Tags::NEXT, por_key_).PutScalar(0.);
  //S_->GetRecordW(por_key_, Amanzi::Tags::NEXT, por_key_).set_initialized();

}


void ELM_ATSDriver::advance(double *dt, bool visout, bool chkout)
{
  //
  auto fail = elm_coordinator_->advance(*dt, visout, chkout);
  if (fail) {
    Errors::Message msg("ELM_ATSCoordinator: Coordinator advance failed.");
    Exceptions::amanzi_throw(msg);
  }

  // update ATS->ELM data if necessary
  S_->GetEvaluator(pres_key_, Amanzi::Tags::NEXT).Update(*S_, pres_key_);
  const Epetra_MultiVector& pres = *S_->Get<Amanzi::
    CompositeVector>(pres_key_, Amanzi::Tags::NEXT).ViewComponent("cell", false);
  S_->GetEvaluator(satl_key_, Amanzi::Tags::NEXT).Update(*S_, satl_key_);
  const Epetra_MultiVector& satl = *S_->Get<Amanzi::
    CompositeVector>(satl_key_, Amanzi::Tags::NEXT).ViewComponent("cell", false);
  //S_->GetEvaluator(por_key_, Amanzi::Tags::NEXT).Update(*S_, por_key_);
  //const Epetra_MultiVector& poro = *S_->Get<Amanzi::CompositeVector>(por_key_, Amanzi::Tags::NEXT).ViewComponent("cell", false);
}

// simulates external timeloop with dt coming from calling model
void ELM_ATSDriver::advance_test()
{
  while (S_->get_time() < elm_coordinator_->get_end_time()) {
    // use dt from ATS for testing
    double dt = elm_coordinator_->get_dt(false);
    // call main method
    advance(&dt);
  }
}


// finalize simulation - print stats, dump final checkpoint
void ELM_ATSDriver::finalize()
{
  elm_coordinator_->finalize();
}


// simulates one ELM timestep
void ELM_ATSDriver::advance_elmstep(double *dt_elm, bool visout, bool chkout)
{
  // elm one timestep: starting --> ending
  double ending_time_ = S_->get_time() + *dt_elm;
  bool visout2 = false;
  if (visout && plist_visout_) {
    if (ending_time_<86400.0) {
      std::cout<<"ATS running period of time: ending by - "<<ending_time_<<" seconds"<<std::endl;
    } else {
      int yr = std::floor(ending_time_/86400.0/365.0);
      double day = ending_time_/86400.0 - yr*365.0;
      std::cout<<"ATS running period of time: ending by - YEAR "<<yr<<" DOY "<<int(day)<<std::endl;
    }

    visout2 = true;
  }
  double dt = elm_coordinator_->get_dt(false);
  while (S_->get_time() < ending_time_) {
	//  call main method, with output instruction(s) from ELM
    advance(&dt, visout2, chkout);
    dt = elm_coordinator_->get_dt(false);
  };
}


//------------------------------------------------------------------------------------------------------------------------
void
ELM_ATSDriver::set_mesh(double *surf_gridsX, double *surf_gridsY, double *surf_gridsZ, double *col_verticesZ,
  const int len_gridsX, const int len_gridsY, const int len_verticesZ)
{
	//
	elmdata_.NX_ = len_gridsX;            // vertices along X/Y/Z_axis
	elmdata_.NY_ = len_gridsY;
	elmdata_.NZ_ = len_verticesZ;
	elmdata_.NCOLS_ = (elmdata_.NX_-1)*(elmdata_.NY_-1);
	elmdata_.NCELLS_ = elmdata_.NCOLS_*(elmdata_.NZ_-1);

	  // the following NOT YET used, but assuming rectangular grids
	  // and soil-column intrusion downwardly
	elmdata_.surf_gridsX_.reserve(elmdata_.NX_);
	  for (int i=0; i<elmdata_.NX_-1; i++){
		  elmdata_.surf_gridsX_.push_back(surf_gridsX[i]);
	  }
	  elmdata_.surf_gridsY_.reserve(elmdata_.NY_);
	  for (int j=0; j<elmdata_.NY_-1; j++){
		  elmdata_.surf_gridsY_.push_back(surf_gridsY[j]);
	  }
	  //surf_gridsZ_.reserve(NX_*NY_); // NOT YET

	  elmdata_.cols_verticesZ_.reserve(elmdata_.NZ_);
	  for (int k=0; k<elmdata_.NZ_-1; k++){
		  elmdata_.cols_verticesZ_.push_back(col_verticesZ[k]);
	  }


}

void
ELM_ATSDriver::set_materials(double *porosity, double *hksat, double *CH_bsw, double *CH_smpsat, double *CH_sr,
  double *eff_porosity, double *zwt)
{
	elmdata_.porosity_.reserve(elmdata_.NCELLS_);
	elmdata_.hksat_.reserve(elmdata_.NCELLS_);
	elmdata_.CH_bsw_.reserve(elmdata_.NCELLS_);
	elmdata_.CH_smpsat_.reserve(elmdata_.NCELLS_);
	elmdata_.CH_sr_.reserve(elmdata_.NCELLS_);
	elmdata_.eff_porosity_.reserve(elmdata_.NCELLS_);
	for (int i=0; i<elmdata_.NCELLS_; i++) {
		elmdata_.porosity_.push_back(porosity[i]);
		elmdata_.hksat_.push_back(hksat[i]);
		elmdata_.CH_bsw_.push_back(CH_bsw[i]);
		elmdata_.CH_smpsat_.push_back(CH_smpsat[i]);
		elmdata_.CH_sr_.push_back(CH_sr[i]);
		elmdata_.eff_porosity_.push_back(eff_porosity[i]);
	}
    
    elmdata_.zwt_.reserve(elmdata_.NCOLS_);
    for (int i=0; i<elmdata_.NCOLS_; i++) {
        elmdata_.zwt_.push_back(zwt[i]);
    }
}

void
ELM_ATSDriver::set_initialconditions(double *start_t, double *patm, double *soilpressure, double *wtd, bool visout)
{

  // checking if flow PK's initial condition type
  Teuchos::RCP<Teuchos::ParameterList> pk_flow_initial_plist_ = Teuchos::sublist(
                    Teuchos::sublist(pks_plist_, "flow"),  "initial condition");
  auto init_wh = pk_flow_initial_plist_->get<double>("hydrostatic head [m]",-999.9);

  // IF NOT IC type of hydrostatic head [m]
  if (init_wh==-999.9) {

	  std::vector<double> col_patm;
	  std::vector<double> col_wtd;
	  std::vector<double> col_surfp;
	  col_patm.reserve(elmdata_.NCOLS_);
	  col_wtd.reserve(elmdata_.NCOLS_);
	  col_surfp.reserve(elmdata_.NCOLS_);
	  for (int ij=0; ij<elmdata_.NCOLS_; ij++) {
	    col_patm.push_back(patm[ij]);
	    col_wtd.push_back(wtd[ij]);
	    col_surfp.push_back(soilpressure[ij*(elmdata_.NZ_-1)]);
	    // temporarilly set (TODO: better to include surface water depth from ELM)
	  }

	  std::vector<double> soilp;
	  soilp.reserve(elmdata_.NCELLS_);
	  for (int ijk=0; ijk<elmdata_.NCELLS_; ijk++) {
	    soilp.push_back(soilpressure[ijk]);
	  }


	  // real IC, i.e. primary variable in PKs
	  Epetra_MultiVector& srf_pv = *S_->GetW<Amanzi::CompositeVector>(srf_pv_key_, Amanzi::Tags::NEXT, srfpk_key_)
		  .ViewComponent("cell", false);
	  Epetra_MultiVector& sub_pv = *S_->GetW<Amanzi::CompositeVector>(sub_pv_key_, Amanzi::Tags::NEXT, subpk_key_)
		  .ViewComponent("cell", false);

	  for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col) {
		srf_pv[0][col] = col_surfp[col];

		auto& col_iter = mesh_subsurf_->cells_of_column(col);
		for (std::size_t i=0; i!=col_iter.size(); ++i) {
		  sub_pv[0][col_iter[i]] = soilp[col*ncol_cells_+i];
		}
	  }
	  // mark pvs as changed
	  ChangedEvaluatorPrimary(srf_pv_key_, Amanzi::Tags::NEXT, *S_);
	  ChangedEvaluatorPrimary(sub_pv_key_, Amanzi::Tags::NEXT, *S_);

	  // commit the initial conditions after resetting
	  elm_coordinator_->reinit(*start_t, visout);
  }


}

void
ELM_ATSDriver::set_boundaryconditions()
{
  //TODO
}


/*
assume that incoming data is in form
soil_infiltration(ncols)
soil_evaporation(ncols)
root_transpiration(ncells)
with ncells = ncells_per_column * ncols

assume surface can be indexed:
for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col)
{ ATS_surf_Epetra_MultiVector[0][col] = elm_surf_data[col];

  and assume subsurface can be indexed:
  auto& col_iter = mesh_subsurf_->cells_of_column(col);
  for (std::size_t i=0; i!=col_iter.size(); ++i) 
  {  ATS_subsurf_Epetra_MultiVector[0][col_iter[i]] = elm_subsurf_data[col*ncol_cells_+i]; }
}

assume evaporation and infiltration have same sign
*/
void
ELM_ATSDriver::set_sources(double *soil_infiltration, double *soil_evaporation,
  double *root_transpiration, int *ncols, int *ncells)
{
  // get densities to scale source fluxes
  S_->GetEvaluator(srf_mol_dens_key_, Amanzi::Tags::NEXT).Update(*S_, srf_mol_dens_key_);
  const Epetra_MultiVector& srf_mol_dens = *S_->Get<Amanzi::CompositeVector>(srf_mol_dens_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);
  S_->GetEvaluator(srf_mass_dens_key_, Amanzi::Tags::NEXT).Update(*S_, srf_mass_dens_key_);
  const Epetra_MultiVector& srf_mass_dens = *S_->Get<Amanzi::CompositeVector>(srf_mass_dens_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);
  S_->GetEvaluator(sub_mol_dens_key_, Amanzi::Tags::NEXT).Update(*S_, sub_mol_dens_key_);
  const Epetra_MultiVector& sub_mol_dens = *S_->Get<Amanzi::CompositeVector>(sub_mol_dens_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);
  S_->GetEvaluator(sub_mass_dens_key_, Amanzi::Tags::NEXT).Update(*S_, sub_mass_dens_key_);
  const Epetra_MultiVector& sub_mass_dens = *S_->Get<Amanzi::CompositeVector>(sub_mass_dens_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);

  // get sources
  Epetra_MultiVector& surf_ss = *S_->GetW<Amanzi::CompositeVector>(srf_src_key_, Amanzi::Tags::NEXT, srf_src_key_)
      .ViewComponent("cell", false);
  Epetra_MultiVector& subsurf_ss = *S_->GetW<Amanzi::CompositeVector>(sub_src_key_, Amanzi::Tags::NEXT, sub_src_key_)
      .ViewComponent("cell", false);


  // cell relative permeability [0-6.18?)
  Amanzi::Key kr_key=Amanzi::Keys::getKey(domain_sub_,"relative_permeability");
  S_->GetEvaluator(kr_key, Amanzi::Tags::NEXT).Update(*S_, kr_key);
  const Epetra_MultiVector& kr =
    *S_->Get<Amanzi::CompositeVector>(kr_key, Amanzi::Tags::NEXT).ViewComponent("cell",false);

  // cell porosity [0-1.0]
  Amanzi::Key poro_key=Amanzi::Keys::getKey(domain_sub_,"porosity");
  S_->GetEvaluator(poro_key, Amanzi::Tags::NEXT).Update(*S_, poro_key);
  const Epetra_MultiVector& poro =
    *S_->Get<Amanzi::CompositeVector>(poro_key, Amanzi::Tags::NEXT).ViewComponent("cell",false);

  // cell volume [m3]
  Amanzi::Key cv_key=Amanzi::Keys::getKey(domain_sub_,"cell_volume");
  S_->GetEvaluator(cv_key, Amanzi::Tags::NEXT).Update(*S_, cv_key);
  const Epetra_MultiVector& cv =
    *S_->Get<Amanzi::CompositeVector>(cv_key, Amanzi::Tags::NEXT).ViewComponent("cell",false);

  Amanzi::Key cv1_key=Amanzi::Keys::getKey(domain_srf_,"cell_volume");
  S_->GetEvaluator(cv1_key, Amanzi::Tags::NEXT).Update(*S_, cv1_key);    // this unit is in m2
  const Epetra_MultiVector& cv1 =
    *S_->Get<Amanzi::CompositeVector>(cv1_key, Amanzi::Tags::NEXT).ViewComponent("cell",false);

  // cell water content: mols
  S_->GetEvaluator(watl_key_, Amanzi::Tags::NEXT).Update(*S_, watl_key_);
  const Epetra_MultiVector& watl = *S_->Get<Amanzi::CompositeVector>(watl_key_, Amanzi::Tags::NEXT)
	      .ViewComponent("cell", false);

  S_->GetEvaluator(satl_key_, Amanzi::Tags::NEXT).Update(*S_, satl_key_);
  const Epetra_MultiVector& sat = *S_->Get<Amanzi::CompositeVector>(satl_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);

  const Epetra_MultiVector& pc = *S_->Get<Amanzi::CompositeVector>("capillary_pressure_gas_liq", Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);

  // -- get the WRM models
  auto& wrm = S_->GetEvaluator(satl_key_, Amanzi::Tags::NEXT);
  auto wrm_eval = dynamic_cast<Amanzi::Flow::WRMEvaluator*>(&wrm);
  Teuchos::RCP<Amanzi::Flow::WRMPartition> wrms_ = wrm_eval->get_WRMs();

  // ------------------------------------------------------------------
  AMANZI_ASSERT(*ncols == ncolumns_ == surf_ss.MyLength());
  AMANZI_ASSERT(*ncells == ncolumns_ * ncol_cells_);
  AMANZI_ASSERT(*ncells == subsurf_ss.MyLength());

  // scale evaporation and infiltration and add to surface source
  // negative out of subsurface (source) and possitive into subsurface (sink).
  // unit: mass-source/sink of kgH2O/m2/s - surface
  // unit: mass-source/sink of kgH2O/m3/s - subsurface

  // scale potential infiltration, rain+snowmelt-ground evap, and add to surface source
  for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col) {
    double srf_mol_h20_kg = srf_mol_dens[0][col] / srf_mass_dens[0][col];
    surf_ss[0][col] = std::max(0.0, (soil_evaporation[col]+soil_infiltration[col])) * srf_mol_h20_kg;      // (+) moles/m2/s

    // scale soil evap and root_transpiration, together add to subsurface source
    auto& col_iter = mesh_subsurf_->cells_of_column(col);
    double net_soilevap = std::min(0.0, (soil_evaporation[col]+soil_infiltration[col])) * srf_mol_h20_kg;  // (-) moles/m2/s
    net_soilevap *= (cv1[0][col_iter[0]]/cv[0][col_iter[0]]);    // convert to moles/s, and then to moles/m3/s for adding to top soil layer due to 'cv' unit differences

    for (std::size_t i=0; i!=col_iter.size(); ++i) {
      double sub_mol_h20_kg = sub_mol_dens[0][col_iter[i]] / sub_mass_dens[0][col_iter[i]];

      subsurf_ss[0][col_iter[i]] = root_transpiration[col*ncol_cells_+i] * sub_mol_h20_kg;
      if (i==0){
    	  subsurf_ss[0][col_iter[i]] += net_soilevap;
      }

    }
  }
  // mark sources as changed
  ChangedEvaluatorPrimary(srf_src_key_, Amanzi::Tags::NEXT, *S_);
  ChangedEvaluatorPrimary(sub_src_key_, Amanzi::Tags::NEXT, *S_);

}

//----------------------------------------------------------------------------------------------------------------------------

void
ELM_ATSDriver::get_waterstate(double *surface_pd, double *soil_pressure, double *soilpsi,
		double *saturation, double *saturation_ice, int *ncols, int *ncells)
{

  S_->GetEvaluator(pd_key_, Amanzi::Tags::NEXT).Update(*S_, pd_key_);
  const Epetra_MultiVector& pd = *S_->Get<Amanzi::CompositeVector>(pd_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);
  S_->GetEvaluator(pres_key_, Amanzi::Tags::NEXT).Update(*S_, pres_key_);
  const Epetra_MultiVector& pres = *S_->Get<Amanzi::CompositeVector>(pres_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);
  S_->GetEvaluator(satl_key_, Amanzi::Tags::NEXT).Update(*S_, satl_key_);
  const Epetra_MultiVector& sat = *S_->Get<Amanzi::CompositeVector>(satl_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);

  // cell water matric potential: -Pa
  const Epetra_MultiVector& pc = *S_->Get<Amanzi::CompositeVector>("capillary_pressure_gas_liq", Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);


  // cell water content: mols
  S_->GetEvaluator(watl_key_, Amanzi::Tags::NEXT).Update(*S_, watl_key_);
  const Epetra_MultiVector& watl = *S_->Get<Amanzi::CompositeVector>(watl_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);

  const Epetra_MultiVector& cv =
    *S_->Get<Amanzi::CompositeVector>(Amanzi::Keys::getKey(domain_sub_,"cell_volume"), Amanzi::Tags::NEXT)
	  .ViewComponent("cell",false);

  // Two kinds of water densities
  const Epetra_MultiVector& sub_mol_dens = *S_->Get<Amanzi::CompositeVector>(sub_mol_dens_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);
  const Epetra_MultiVector& sub_mass_dens = *S_->Get<Amanzi::CompositeVector>(sub_mass_dens_key_, Amanzi::Tags::NEXT)
      .ViewComponent("cell", false);

  AMANZI_ASSERT(*ncols == ncolumns_ == pd.MyLength());
  AMANZI_ASSERT(*ncells == ncolumns_ * ncol_cells_);
  AMANZI_ASSERT(*ncells == pres.MyLength());



  for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col) {
    surface_pd[col] = pd[0][col];

    auto& col_iter = mesh_subsurf_->cells_of_column(col);
    for (std::size_t i=0; i!=col_iter.size(); ++i) {
      soil_pressure[col*ncol_cells_+i] = pres[0][col_iter[i]];
      soilpsi[col*ncol_cells_+i] = std::max(0.0, pc[0][col_iter[i]]);  // negative for saturated, which should be 0.
#if 0
      saturation[col*ncol_cells_+i] = sat[0][col_iter[i]];
#else
      // output 'saturation' as water content per vol, which compariable to ELM
      double sub_mol_h20_kg = sub_mol_dens[0][col_iter[i]] / sub_mass_dens[0][col_iter[i]];
      double convertor = 1.0/sub_mol_h20_kg;
      // mols --> kgH2O/m3-cell
      saturation[col*ncol_cells_+i] = watl[0][col_iter[i]]/cv[0][col_iter[i]]*convertor;
#endif

      saturation_ice[col*ncol_cells_+i] = 0.0; // (TODO)

    }
  }

}

void
ELM_ATSDriver::get_waterflux(double *soil_infiltration, double *soil_evaporation, double *root_transpiration,
		int *ncols, int *ncells)
{

  S_->GetEvaluator(pd_key_, Amanzi::Tags::NEXT).Update(*S_, pd_key_);
    const Epetra_MultiVector& pd = *S_->Get<Amanzi::CompositeVector>(pd_key_, Amanzi::Tags::NEXT)
	      .ViewComponent("cell", false);
  S_->GetEvaluator(pres_key_, Amanzi::Tags::NEXT).Update(*S_, pres_key_);
    const Epetra_MultiVector& pres = *S_->Get<Amanzi::CompositeVector>(pres_key_, Amanzi::Tags::NEXT)
	      .ViewComponent("cell", false);

  AMANZI_ASSERT(*ncols == ncolumns_ == pd.MyLength());
  AMANZI_ASSERT(*ncells == ncolumns_ * ncol_cells_);
  AMANZI_ASSERT(*ncells == pres.MyLength());

  for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col) {

    auto& col_iter = mesh_subsurf_->cells_of_column(col);
    for (std::size_t i=0; i!=col_iter.size(); ++i) {

      //(TODO) get actual water fluxes from ATS to ELM
    }
  }

}

void ELM_ATSDriver::get_mesh_info(int *ncols_local, int *ncols_global, int *ncells_per_col,
    double *dz, double *depth, double *elev, double *surf_area_m2, double *lat, double *lon)
{
  *ncols_local = static_cast<int>(mesh_surf_->num_entities(Amanzi::AmanziMesh::Entity_kind::CELL, Amanzi::AmanziMesh::Parallel_type::OWNED));
  *ncols_global = static_cast<int>(mesh_surf_->num_entities(Amanzi::AmanziMesh::Entity_kind::CELL, Amanzi::AmanziMesh::Parallel_type::ALL));
  *ncells_per_col = static_cast<int>(ncol_cells_);

  col_depth(dz, depth);
  ChangedEvaluatorPrimary("dz", Amanzi::Tags::NEXT, *S_);
  ChangedEvaluatorPrimary("depth", Amanzi::Tags::NEXT, *S_);

  S_->GetEvaluator(elev_key_, Amanzi::Tags::NEXT).Update(*S_, elev_key_);
  const Epetra_MultiVector& elev_ats = *S_->Get<Amanzi::CompositeVector>(elev_key_, Amanzi::Tags::NEXT)
      .ViewComponent("face",false);

  for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col) {
    // -- get the surface cell's equivalent subsurface face
    Amanzi::AmanziMesh::Entity_ID f = mesh_surf_->entity_get_parent(Amanzi::AmanziMesh::CELL, col);
    surf_area_m2[col] = mesh_subsurf_->face_area(f);
    elev[col] = elev_ats[0][col];
  }

  // dummy lat lon for now
  *lat = 0.5;
  *lon = 0.5;
}


//-----------------------------------------------------------------------------------------------

// helper function for collecting column dz and depth
void ELM_ATSDriver::col_depth(double *dz, double *depth) {

  // get dz and depth
  Epetra_MultiVector& dz_ats = *S_->GetW<Amanzi::CompositeVector>("dz", Amanzi::Tags::NEXT, "dz")
      .ViewComponent("cell", false);
  Epetra_MultiVector& depth_ats = *S_->GetW<Amanzi::CompositeVector>("depth", Amanzi::Tags::NEXT, "depth")
      .ViewComponent("cell", false);
  for (Amanzi::AmanziMesh::Entity_ID col=0; col!=ncolumns_; ++col) {

    Amanzi::AmanziMesh::Entity_ID f_above = mesh_surf_->entity_get_parent(Amanzi::AmanziMesh::CELL, col);

    auto& col_iter = mesh_subsurf_->cells_of_column(col);

    // assumed constant, so not using now
    auto ncells_per_col = col_iter.size();

    Amanzi::AmanziGeometry::Point surf_centroid = mesh_subsurf_->face_centroid(f_above);
    Amanzi::AmanziGeometry::Point neg_z(3);
    neg_z.set(0.,0.,-1);

    for (std::size_t i=0; i!=col_iter.size(); ++i) {
      // depth centroid
      //(*depth)[i] = surf_centroid[2] - mesh_subsurf_->cell_centroid(col_iter[i])[2];
      depth[col*ncol_cells_+i] = surf_centroid[2] - mesh_subsurf_->cell_centroid(col_iter[i])[2];

      // dz
      // -- find face_below
      Amanzi::AmanziMesh::Entity_ID_List faces;
      std::vector<int> dirs;
      mesh_subsurf_->cell_get_faces_and_dirs(col_iter[i], &faces, &dirs);

      // -- mimics implementation of build_columns() in Mesh
      double mindp = 999.0;
      Amanzi::AmanziMesh::Entity_ID f_below = -1;
      for (std::size_t j=0; j!=faces.size(); ++j) {
        Amanzi::AmanziGeometry::Point normal = mesh_subsurf_->face_normal(faces[j]);
        if (dirs[j] == -1) normal *= -1;
        normal /= Amanzi::AmanziGeometry::norm(normal);

        double dp = -normal * neg_z;
        if (dp < mindp) {
          mindp = dp;
          f_below = faces[j];
        }
      }

      // -- fill the val
      //(*dz)[i] = mesh_subsurf_->face_centroid(f_above)[2] - mesh_subsurf_->face_centroid(f_below)[2];
      dz[col*ncol_cells_+i] = mesh_subsurf_->face_centroid(f_above)[2] - mesh_subsurf_->face_centroid(f_below)[2];
      AMANZI_ASSERT( dz[col*ncol_cells_+i] > 0. );
      f_above = f_below;
    }
  }

}


double ELM_ATSDriver::HfunctionSmooth(double x, double x_1, double x_0, bool derivative) {
  /*
    Usage: H(x) = 1, dH(x)=0 @ x_1
           H(x) = 0, dH(x)=0 @ x_0
           and, dH(x) is smoothed and either positive or negative Heaveside function, with peak at ~ 3-quater point
      So H(x) is monotonic.

    How to tail-smooth a curve (e.g. f(x)) monotonically:
         F(x) = f(x)*H(f), and dF(x) = f(x)*dH(x)+ df(x)*H(x),
        From: x_cutoff1 - F(x)=f(x), dF(x)=df(x), i.e. exactly matching with f(x) @ x_cutoff1
        To:   x_cutoff0 - F(x)=0, dF(x)=0.
  */

  double x_star;
  double H = 1.0;
  double dH= 0.0;

  // real Heaveside function (step function)
  if (std::abs(x_1-x_0)<1.e-50) {
    if (x<x_1) {
	  H = 0.0;  // if x<x_1, H=0; otherwise H=1
    }else {
	  H = 1.0;
	}
    dH = 0.0;


  } else {
  // smoothed H function

    if (((x-x_0)/(x_1-x_0))<0.0){         // beyond 'x_0'
      H  = 0.0;
      dH = 0.0;

    } else if (((x-x_0)/(x_1-x_0))>1.0){  // beyond 'x_1'
      H  = 1.0;
      dH = 0.0;

    } else {

      x_star  =  1.0 - (x-x_0)*(x-x_0)/(x_1-x_0)/(x_1-x_0);

      H  = 1.0 - x_star*x_star;
      // so it's a special quadratic polynomal function
      // so, dH = -2*x_star*d(x_star), with d(x_star)=-2*(x-x_0)/(x_1-x_0)^2
      //   and, due to 'x_star' ranging 0~1 and (x_1-x_0)^2 positive,
      //        sign(dH) is upon 'x-x_0' only guaranted monotonic H curve
      dH = 4.0 * x_star * (x-x_0)/(x_1-x_0)/(x_1-x_0);

    }
  }

  if (derivative) {
    return dH;
  }else{
    return H;
  }

}





} // namespace ATS
