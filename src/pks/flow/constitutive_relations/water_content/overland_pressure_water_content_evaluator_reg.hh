/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:

*/

#include "overland_pressure_water_content_evaluator.hh"
namespace Amanzi {
namespace Flow {
Utils::RegisteredFactory<Evaluator, OverlandPressureWaterContentEvaluator>
  OverlandPressureWaterContentEvaluator::reg_("overland pressure water content");
}
} // namespace Amanzi
