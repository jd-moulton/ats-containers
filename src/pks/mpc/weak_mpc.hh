/*
  Copyright 2010-202x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon (ecoon@lanl.gov)
*/

//! Multi process coupler for sequential coupling.
/*!

Noniterative sequential coupling simply calls each PK's AdvanceStep() method in
order.

.. _weak-mpc-spec:
.. admonition:: weak-mpc-spec

    INCLUDES:

    - ``[mpc-spec]`` *Is a* MPC_.

*/


#pragma once

#include "PK.hh"
#include "mpc.hh"

namespace Amanzi {

class WeakMPC : public MPC<PK> {
 public:
  WeakMPC(const Comm_ptr_type& comm,
          Teuchos::ParameterList& pk_tree,
          const Teuchos::RCP<Teuchos::ParameterList>& global_list,
          const Teuchos::RCP<State>& S);

  // PK methods
  // -- dt is the minimum of the sub pks
  virtual double getDt() override;

  // -- advance each sub pk dt.
  virtual bool AdvanceStep(double t_old, double t_new, bool reinit) override;

  virtual void setDt(double dt) override;

  // type info used in PK_Factory
  static const std::string type;
  virtual const std::string& getType() const override { return type; }

 private:
  // factory registration
  static RegisteredPKFactory<WeakMPC> reg_;
};

} // namespace Amanzi
