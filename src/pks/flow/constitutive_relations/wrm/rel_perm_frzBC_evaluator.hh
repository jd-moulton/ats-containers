/*
  Copyright 2010-202x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon (ecoon@lanl.gov)
           Bo Gao (gaob@ornl.gov)
*/

//! Evaluates relative permeability using an empirical model for frozen conditions.
/*!

This is an empirical relative permeability model according to Niu and Yang (2006). 
This model is based on Brooks-Corey relative permeability model and an additional
coefficient term is added to account for the effect of soil ice. This model is 
used for freezing conditions to make snowmelt water infiltrate deeper. See paper
Agnihotri et al. (2023) for discussions about the influence of relative permeability 
model on discharge under freezing conditions.

.. math::
   k_{rel} = ( 1 - F_{frz} ) \times ( \frac{1 - s_{g} - s_r}{1 - s_r} )^{2*b + 3} \\
   F_{frz} = \mathrm{exp}( -\omega \times ( s_{l} + s_{g} ) ) - \mathrm{exp}( -\omega )

Under freezing conditions, it is recommended to call Brooks-Corey based relative 
permeability corrected by ice content. This model needs Brooks-Corey parameters:
Brooks-Corey lambda, Brooks-Corey saturated matric suction (Pa), and residual 
saturation. The reciprocal of Brooks-Corey lambda is Clapp-Hornberger b. Use tool
`"convert_paramters_vg2bc.py`" to convert van Genuchten parameters to Brooks-Corey 
paramters. The conversion method is referred to Lenhard et al. (1989) or Ma et al. (1999)
method 2. 

.. _rel-perm-evaluator-spec
.. admonition:: rel-perm-evaluator-spec

   * `"use density on viscosity in rel perm`" ``[bool]`` **true**

   * `"boundary rel perm strategy`" ``[string]`` **boundary pressure** Controls
     how the rel perm is calculated on boundary faces.  Note, this may be
     overwritten by upwinding later!  One of:

      - `"boundary pressure`" Evaluates kr of pressure on the boundary face, upwinds normally.
      - `"interior pressure`" Evaluates kr of the pressure on the interior cell (bad idea).
      - `"harmonic mean`" Takes the harmonic mean of kr on the boundary face and kr on the interior cell.
      - `"arithmetic mean`" Takes the arithmetic mean of kr on the boundary face and kr on the interior cell.
      - `"one`" Sets the boundary kr to 1.
      - `"surface rel perm`" Looks for a field on the surface mesh and uses that.

   * `"minimum rel perm cutoff`" ``[double]`` **0.** Provides a lower bound on rel perm.

   * `"permeability rescaling`" ``[double]`` Typically rho * kr / mu is very big
     and K_sat is very small.  To avoid roundoff propagation issues, rescaling
     this quantity by offsetting and equal values is encourage.  Typically 10^7 or so is good.

   * `"omega [-]`" ``[double]`` **2.0** A scale dependent parameter in the relative permeability model. 
     See paper Niu & Yang (2006) for details about the model. Typical values range from 2-3.

   * `"WRM parameters`" ``[wrm-typedinline-spec-list]`` List (by region) of WRM specs.

   KEYS:

   - `"saturation_liquid`"
   - `"saturation_gas`"
   - `"density`" (if `"use density on viscosity in rel perm`" == true)
   - `"viscosity`" (if `"use density on viscosity in rel perm`" == true)
   - `"surface relative permeability`" (if `"boundary rel perm strategy`" == `"surface rel perm`")

*/

#pragma once

#include "wrm.hh"
#include "wrm_partition.hh"
#include "rel_perm_evaluator.hh"
#include "EvaluatorSecondaryMonotype.hh"
#include "Factory.hh"

namespace Amanzi {
namespace Flow {

class RelPermFrzBCEvaluator : public EvaluatorSecondaryMonotypeCV {
 public:
  // constructor format for all derived classes
  explicit RelPermFrzBCEvaluator(Teuchos::ParameterList& plist);

  RelPermFrzBCEvaluator(Teuchos::ParameterList& plist, const Teuchos::RCP<WRMPartition>& wrms);

  RelPermFrzBCEvaluator(const RelPermFrzBCEvaluator& other) = default;
  virtual Teuchos::RCP<Evaluator> Clone() const override;

  Teuchos::RCP<WRMPartition> get_WRMs() { return wrms_; }

 protected:
  virtual void EnsureCompatibility_ToDeps_(State& S) override;

  // Required methods from EvaluatorSecondaryMonotypeCV
  virtual void Evaluate_(const State& S, const std::vector<CompositeVector*>& result) override;
  virtual void EvaluatePartialDerivative_(const State& S,
                                          const Key& wrt_key,
                                          const Tag& wrt_tag,
                                          const std::vector<CompositeVector*>& result) override;

 protected:
  void InitializeFromPlist_();

  Teuchos::RCP<WRMPartition> wrms_;
  Key sat_key_;
  Key sat_gas_key_;
  Key dens_key_;
  Key visc_key_;
  Key surf_rel_perm_key_;

  bool is_dens_visc_;
  Key surf_domain_;
  BoundaryRelPerm boundary_krel_;

  double perm_scale_;
  double min_val_;
  double omega_;

 private:
  static Utils::RegisteredFactory<Evaluator, RelPermFrzBCEvaluator> factory_;
};

} // namespace Flow
} // namespace Amanzi
