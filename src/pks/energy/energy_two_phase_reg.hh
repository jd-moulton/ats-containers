/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:
      Ethan Coon
*/

#include "energy_two_phase.hh"

namespace Amanzi {
namespace Energy {

RegisteredPKFactory<TwoPhase> TwoPhase::reg_("two-phase energy");

} // namespace Energy
} // namespace Amanzi
