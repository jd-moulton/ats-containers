/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:

*/

/*
  ATS

  Linear density/viscosity EOS, defaults to reasonable values for water.

  http://software.lanl.gov/ats/trac

  Authors: Ethan Coon (ecoon@lanl.gov)
*/

#include "eos_linear.hh"

namespace Amanzi {
namespace Relations {

// registry of method
Utils::RegisteredFactory<EOS, EOSLinear> EOSLinear::factory_("linear");

} // namespace Relations
} // namespace Amanzi
