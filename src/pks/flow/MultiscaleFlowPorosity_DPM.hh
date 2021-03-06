/*
  Flow PK 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  A two-scale porosity model (fracture + matrix) aka dual porosity
  model. Current naming convention is that the fields used in the 
  single-porosity model correspond now to the fracture continuum.
  Example: pressure = pressure in the fracture continuum;
           pressure_matrix = pressure in the matrix continuum.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#ifndef MULTISCALE_FLOW_POROSITY_DPM_HH_
#define MULTISCALE_FLOW_POROSITY_DPM_HH_

#include "Teuchos_ParameterList.hpp"

#include "factory.hh"

#include "MultiscaleFlowPorosity.hh"
#include "WRM.hh"

namespace Amanzi {
namespace Flow {

class MultiscaleFlowPorosity_DPM : public MultiscaleFlowPorosity {
 public:
  MultiscaleFlowPorosity_DPM(Teuchos::ParameterList& plist);
  ~MultiscaleFlowPorosity_DPM() {};

  // Calculate field water content assuming pressure equilibrium
  double ComputeField(double phi, double n_l, double pcm);

  // local (cell-based) solver returns water content and capilalry
  // pressure in the matrix. max_itrs is input/output parameter
  double WaterContentMatrix(
      double dt, double phi, double n_l, double wcm0, double pcf0, 
      double& pcm, int& max_itrs);

 private:
  Teuchos::RCP<WRM> wrm_;
  double alpha_;
  double tol_;

  static Utils::RegisteredFactory<MultiscaleFlowPorosity, MultiscaleFlowPorosity_DPM> factory_;
};

}  // namespace Flow
}  // namespace Amanzi
  
#endif
  
