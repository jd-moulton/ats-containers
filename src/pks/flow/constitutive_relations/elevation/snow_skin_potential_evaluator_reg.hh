/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:

*/

#include "snow_skin_potential_evaluator.hh"

namespace Amanzi {
namespace Flow {

// registry of method
Utils::RegisteredFactory<Evaluator, SnowSkinPotentialEvaluator>
  SnowSkinPotentialEvaluator::factory_("snow skin potential");

} // namespace Flow
} // namespace Amanzi
