/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:
      Ethan Coon
*/

#include "strong_mpc.hh"

namespace Amanzi {

template <>
RegisteredPKFactory<StrongMPC<PK_BDF_Default>> StrongMPC<PK_BDF_Default>::reg_("strong MPC");

} // namespace Amanzi
