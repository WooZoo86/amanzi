/*
  Flow PK 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Ethan Coon

  Self-registering factory for WRM implementations.
*/

#ifndef PK_FLOW_WRM_FACTORY_HH_
#define PK_FLOW_WRM_FACTORY_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "WRM.hh"
#include "factory.hh"

namespace Amanzi {
namespace Flow {

class WRMFactory : public Utils::Factory<WRM> {
 public:
  Teuchos::RCP<WRM> Create(Teuchos::ParameterList& plist);
};

}  // namespace Flow
}  // namespace Amanzi

#endif
