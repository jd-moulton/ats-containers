/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:

*/

/*
  InitialTimeEvaluator is the generic evaluator for multipying two vectors.

  Authors: Ethan Coon (ecoon@lanl.gov)
*/

#include "InitialTimeEvaluator.hh"

namespace Amanzi {
namespace Relations {

// registry of method
Utils::RegisteredFactory<Evaluator, InitialTimeEvaluator>
  InitialTimeEvaluator::reg_("initial value");

} // namespace Relations
} // namespace Amanzi
