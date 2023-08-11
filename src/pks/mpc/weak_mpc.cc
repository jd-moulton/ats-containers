/*
  Copyright 2010-202x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon
*/

/* -------------------------------------------------------------------------
ATS

Implementation for the derived WeakMPC class.  Provides only the advance()
method missing from MPC.hh.  In weak coupling, we simply loop over the
sub-PKs, calling their advance() methods and returning failure if any fail.

See additional documentation in the base class src/pks/mpc/MPC.hh
------------------------------------------------------------------------- */

#include <limits>
#include "weak_mpc.hh"

namespace Amanzi {

const std::string WeakMPC::type = "weak MPC";

WeakMPC::WeakMPC(const Comm_ptr_type& comm,
                 Teuchos::ParameterList& pk_tree,
                 const Teuchos::RCP<Teuchos::ParameterList>& global_list,
                 const Teuchos::RCP<State>& S)
  : PK(comm, pk_tree, global_list, S),
    MPC<PK>(comm, pk_tree, global_list, S)
{
  MPC<PK>::createSubPKs_(comm_);
};


// -----------------------------------------------------------------------------
// Calculate the min of sub PKs timestep sizes.
// -----------------------------------------------------------------------------
double
WeakMPC::getDt()
{
  double dt = std::numeric_limits<double>::max();
  for (auto& pk : sub_pks_) dt = std::min(dt, pk->getDt());
  return dt;
};


// -----------------------------------------------------------------------------
// Set timestep for sub PKs
// -----------------------------------------------------------------------------
void
WeakMPC::setDt(double dt)
{
  for (auto& pk : sub_pks_) pk->setDt(dt);
};

// -----------------------------------------------------------------------------
// Advance each sub-PK individually.
// -----------------------------------------------------------------------------
bool
WeakMPC::AdvanceStep(double t_old, double t_new, bool reinit)
{
  bool fail = false;
  for (auto& pk : sub_pks_) {
    fail = pk->AdvanceStep(t_old, t_new, reinit);
    if (fail) return fail;
  }
  return fail;
};


} // namespace Amanzi
