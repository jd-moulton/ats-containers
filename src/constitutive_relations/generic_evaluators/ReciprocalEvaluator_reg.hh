/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:

*/

/*
  ReciprocalEvaluator is the generic evaluator for dividing two vectors.

  Authors: Daniil Svyatsky  (dasvyat@lanl.gov)
*/

#include "ReciprocalEvaluator.hh"

namespace Amanzi {
namespace Relations {

// registry of method
Utils::RegisteredFactory<Evaluator, ReciprocalEvaluator>
  ReciprocalEvaluator::factory_("reciprocal evaluator");

} // namespace Relations
} // namespace Amanzi
