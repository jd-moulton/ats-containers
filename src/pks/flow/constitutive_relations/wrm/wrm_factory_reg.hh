/*
  Copyright 2010-201x held jointly by participating institutions.
  ATS is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors:

*/

/* -------------------------------------------------------------------------

   ATS
   Author: Ethan Coon

   Self-registering factory for WRM implementations.
   ------------------------------------------------------------------------- */

#include "wrm_factory.hh"

// explicity instantitate the static data of Factory<WRM>
template <>
Amanzi::Utils::Factory<Amanzi::Flow::WRM>::map_type*
  Amanzi::Utils::Factory<Amanzi::Flow::WRM>::map_;
