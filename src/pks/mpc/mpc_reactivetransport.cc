/*
  This is the mpc_pk component of the Amanzi code.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Daniil Svyatskiy

  Process kernel for coupling of Transport PK and Chemistry PK.
*/

#include "mpc_reactivetransport.hh"
#include "pk_helpers.hh"

namespace Amanzi {

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
MPCReactiveTransport::MPCReactiveTransport(Teuchos::ParameterList& pk_tree,
                                          const Teuchos::RCP<Teuchos::ParameterList>& global_list,
                                          const Teuchos::RCP<State>& S,
                                          const Teuchos::RCP<TreeVector>& soln)
  : PK(pk_tree, global_list, S, soln),
    WeakMPC(pk_tree, global_list, S, soln)
{
  chem_step_succeeded_ = true;

  alquimia_timer_ = Teuchos::TimeMonitor::getNewCounter("alquimia "+name());

  domain_ = Keys::readDomain(*plist_, "domain", "domain");
  tcc_key_ = Keys::readKey(*plist_, domain_, "total component concentration", "total_component_concentration");
  mol_dens_key_ = Keys::readKey(*plist_, domain_, "molar density liquid", "molar_density_liquid");
}


void MPCReactiveTransport::Setup()
{
  WeakMPC::Setup();
  cast_sub_pks_();

  S_->Require<CompositeVector,CompositeVectorSpace>(tcc_key_, tag_next_, "state")
    .SetMesh(S_->GetMesh(domain_))->SetGhosted()
    ->AddComponent("cell", AmanziMesh::Entity_kind::CELL, 1);
  S_->RequireEvaluator(tcc_key_, tag_next_);

  S_->Require<CompositeVector,CompositeVectorSpace>(mol_dens_key_, tag_next_)
    .SetMesh(S_->GetMesh(domain_))->SetGhosted()
    ->AddComponent("cell", AmanziMesh::Entity_kind::CELL, 1);
  S_->RequireEvaluator(mol_dens_key_, tag_next_);
}

void MPCReactiveTransport::cast_sub_pks_()
{
  tranport_pk_ = Teuchos::rcp_dynamic_cast<Transport::Transport_ATS>(sub_pks_[1]);
  AMANZI_ASSERT(tranport_pk_ != Teuchos::null);

  chemistry_pk_ = Teuchos::rcp_dynamic_cast<AmanziChemistry::Chemistry_PK>(sub_pks_[0]);
  AMANZI_ASSERT(chemistry_pk_ != Teuchos::null);

  // communicate chemistry engine to transport.
#ifdef ALQUIMIA_ENABLED
  tranport_pk_->SetupAlquimia(Teuchos::rcp_static_cast<AmanziChemistry::Alquimia_PK>(chemistry_pk_),
                              chemistry_pk_->chem_engine());
#endif
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void MPCReactiveTransport::Initialize()
{
  // NOTE: this requires that Reactive-Transport is done last, or at least
  // after the density of water can be evaluated.  This could be problematic
  // for, e.g., salinity intrusion problems where water density is a function
  // of concentration itself, but should work for all other problems?
  Teuchos::RCP<Epetra_MultiVector> tcc_copy =
    S_->GetW<CompositeVector>(tcc_key_, tag_next_, "state").ViewComponent("cell", true);

  S_->GetEvaluator(mol_dens_key_, tag_next_).Update(*S_, name_);
  Teuchos::RCP<const Epetra_MultiVector> mol_dens =
    S_->Get<CompositeVector>(mol_dens_key_, tag_next_).ViewComponent("cell", true);

  int num_aqueous = chemistry_pk_->num_aqueous_components();
  ConvertConcentrationToAmanzi(*mol_dens, num_aqueous, *tcc_copy, *tcc_copy);
  chemistry_pk_->Initialize();
  ConvertConcentrationToATS(*mol_dens, num_aqueous, *tcc_copy, *tcc_copy);

  tranport_pk_->Initialize();
}


// -----------------------------------------------------------------------------
// Calculate the min of sub PKs timestep sizes.
// -----------------------------------------------------------------------------
double MPCReactiveTransport::get_dt()
{
  double dTtran = tranport_pk_->get_dt();
  double dTchem = chemistry_pk_->get_dt();

  if (!chem_step_succeeded_ && (dTchem / dTtran > 0.99)) {
     dTchem *= 0.5;
  }
  return dTchem;
}


// -----------------------------------------------------------------------------
// Advance each sub-PK individually, returning a failure as soon as possible.
// -----------------------------------------------------------------------------
bool MPCReactiveTransport::AdvanceStep(double t_old, double t_new, bool reinit)
{
  chem_step_succeeded_ = false;

  // First we do a transport step.
  bool fail = tranport_pk_->AdvanceStep(t_old, t_new, reinit);
  if (fail) return fail;

  // Second, we do a chemistry step.
  int num_aq_componets = tranport_pk_->get_num_aqueous_component();

  Teuchos::RCP<Epetra_MultiVector> tcc_copy =
    S_->GetW<CompositeVector>(tcc_key_, tag_next_, "state").ViewComponent("cell", true);

  S_->GetEvaluator(mol_dens_key_, tag_next_).Update(*S_, name_);
  Teuchos::RCP<const Epetra_MultiVector> mol_dens =
    S_->Get<CompositeVector>(mol_dens_key_, tag_next_).ViewComponent("cell", true);

  fail |= AdvanceChemistry(chemistry_pk_, t_old, t_new, reinit, *mol_dens, tcc_copy, *alquimia_timer_);
  ChangedEvaluatorPrimary(tcc_key_, tag_next_, *S_);
  if (!fail) chem_step_succeeded_ = true;
  return fail;
};


}  // namespace Amanzi


