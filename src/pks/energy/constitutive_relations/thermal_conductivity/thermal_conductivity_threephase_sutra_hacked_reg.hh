/*
  Copyright 2010-202x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Ethan Coon
*/

#include "thermal_conductivity_threephase_sutra_hacked.hh"

namespace Amanzi {
namespace Energy {

// registry of method
Utils::RegisteredFactory<ThermalConductivityThreePhase, ThermalConductivityThreePhaseSutraHacked>
  ThermalConductivityThreePhaseSutraHacked::factory_("three-phase sutra hacked");


} // namespace Energy
} // namespace Amanzi
