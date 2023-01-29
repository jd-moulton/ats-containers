/*
  ATS is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Daniil Svyatsky (dasvyat@lanl.gov)
*/
//! A coupler which integrates surface, richards and preferantial flows implicitly.

/*!



*/

#ifndef PKS_MPC_COUPLED_DUALMEDIA_WATER_HH_
#define PKS_MPC_COUPLED_DUALMEDIA_WATER_HH_

#include "Operator.hh"
#include "Op_Cell_FaceCell.hh"
#include "PDE_CouplingFlux.hh"
#include "mpc_delegate_water.hh"
#include "pk_physical_bdf_default.hh"
#include "overland_pressure.hh"
#include "richards.hh"
#include "mpc_coupled_water.hh"
#include "TreeOperator.hh"
#include "strong_mpc.hh"

namespace Amanzi {

class MPCCoupledDualMediaWater : public StrongMPC<PK_BDF_Default> {
 public:
  MPCCoupledDualMediaWater(Teuchos::ParameterList& FElist,
                           const Teuchos::RCP<Teuchos::ParameterList>& plist,
                           const Teuchos::RCP<State>& S,
                           const Teuchos::RCP<TreeVector>& soln);

  virtual void Setup();
  virtual void Initialize();

  // -- computes the non-linear functional g = g(t,u,udot)
  //    By default this just calls each sub pk FunctionalResidual().
  virtual void FunctionalResidual(double t_old,
                                  double t_new,
                                  Teuchos::RCP<TreeVector> u_old,
                                  Teuchos::RCP<TreeVector> u_new,
                                  Teuchos::RCP<TreeVector> g);

  // -- Apply preconditioner to u and returns the result in Pu.
  virtual int ApplyPreconditioner(Teuchos::RCP<const TreeVector> u, Teuchos::RCP<TreeVector> Pu);

  // -- Update the preconditioner.
  virtual void UpdatePreconditioner(double t, Teuchos::RCP<const TreeVector> up, double h);

 protected:
  // void
  // UpdateConsistentFaceCorrectionWater_(const Teuchos::RCP<const TreeVector>& u,
  //         const Teuchos::RCP<TreeVector>& Pu);

  void GenerateOffDiagonalBlocks();
  void UpdateOffDiagonalBlocks();

 protected:
  Teuchos::RCP<Operators::TreeOperator> op_tree_matrix_, op_tree_pc_;
  Teuchos::RCP<TreeVector> op_tree_rhs_;
  Teuchos::RCP<TreeVectorSpace> tvs_;

  Teuchos::RCP<Operators::Op> matrix_local_op_;
  Teuchos::RCP<Operators::Op_Cell_FaceCell> coupling02_local_op_;
  Teuchos::RCP<Operators::PDE_CouplingFlux> coupling20_local_op_;
  //Teuchos::RCP<Operators::Op> coupling02_local_op_, coupling20_local_op;
  Teuchos::RCP<Operators::Operator> op0, op1, op2, op_coupling02, op_coupling20;
  
  // sub PKs
  Teuchos::RCP<PK_BDF_Default> surf_flow_pk_;
  Teuchos::RCP<PK_BDF_Default> macro_flow_pk_;
  Teuchos::RCP<StrongMPC<PK_PhysicalBDF_Default>> integrated_flow_pk_;
  Teuchos::RCP<PK_BDF_Default> matrix_flow_pk_;

  Key ss_flux_key_,  ss_macro_flux_key_, matrix_flux_key_, macro_flux_key_;
  Key domain_ss_, domain_surf_, domain_macro_;

  // sub meshes
  Teuchos::RCP<const AmanziMesh::Mesh> domain_mesh_;
  Teuchos::RCP<const AmanziMesh::Mesh> surf_mesh_;
  Teuchos::RCP<const AmanziMesh::Mesh> macro_mesh_;


  // debugger for dumping vectors
  Teuchos::RCP<Debugger> domain_db_;
  Teuchos::RCP<Debugger> macropore_db_;

 private:
  // factory registration
  static RegisteredPKFactory<MPCCoupledDualMediaWater> reg_;
};

} // namespace Amanzi


#endif
