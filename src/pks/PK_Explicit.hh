/*
  Process Kernels

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov, Ethan Coon

  This is a purely virtual base class for process kernels which use
  explisit time integrators.
*/

#ifndef AMANZI_PK_EXPLICIT_HH_
#define AMANZI_PK_EXPLICIT_HH_

#include "Teuchos_RCP.hpp"

#include "Explicit_TI_FnBase.hh"
#include "PK.hh"

namespace Amanzi {

template <class Vector>
class PK_Explicit : virtual public PK, public Explicit_TI::fnBase<Vector> {};

}  // namespace Amanzi

#endif
