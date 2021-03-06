/*
  WhetStone, version 2.1
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Maps between mesh objects located on different meshes, e.g. two 
  states of a deformable mesh: virtual element implementation.
*/

#ifndef AMANZI_WHETSTONE_MESH_MAPS_VEM_HH_
#define AMANZI_WHETSTONE_MESH_MAPS_VEM_HH_

#include "Teuchos_RCP.hpp"

#include "Mesh.hh"
#include "Point.hh"

#include "DenseMatrix.hh"
#include "MeshMaps.hh"
#include "Polynomial.hh"
#include "ProjectorH1.hh"
#include "Tensor.hh"
#include "WhetStone_typedefs.hh"

namespace Amanzi {
namespace WhetStone {

class MeshMaps_VEM : public MeshMaps { 
 public:
  MeshMaps_VEM(Teuchos::RCP<const AmanziMesh::Mesh> mesh) :
      MeshMaps(mesh),
      projector(mesh) {};
  MeshMaps_VEM(Teuchos::RCP<const AmanziMesh::Mesh> mesh0,
               Teuchos::RCP<const AmanziMesh::Mesh> mesh1) :
      MeshMaps(mesh0, mesh1),
      projector(mesh0) {};
  ~MeshMaps_VEM() {};

  // Maps
  // -- pseudo-velocity in cell c
  virtual void VelocityCell(int c, const std::vector<VectorPolynomial>& vf,
                            VectorPolynomial& vc) const override;
  // -- pseudo-velocity on face f
  virtual void VelocityFace(int f, VectorPolynomial& vf) const override;
  // -- Nanson formula
  virtual void NansonFormula(int f, double t, const VectorPolynomial& v,
                             VectorPolynomial& cn) const override;

  // Jacobian
  // -- tensors
  virtual void Cofactors(int c, double t, const VectorPolynomial& vc,
                         MatrixPolynomial& C) const override;
  // -- determinant
  virtual void JacobianDet(int c, double t, const std::vector<VectorPolynomial>& vf,
                           Polynomial& vc) const override;
  void JacobianDet(double t, const VectorPolynomial& vc, Polynomial& jac) const;

  // -- value at point x
  virtual void JacobianCellValue(int c,
                                 double t, const AmanziGeometry::Point& x,
                                 Tensor& J) const override;
  virtual void JacobianFaceValue(int f, const VectorPolynomial& v,
                                 const AmanziGeometry::Point& x,
                                 Tensor& J) const override;

 private:
  // pseudo-velocity on edge e
  void VelocityEdge_(int e, VectorPolynomial& ve) const;

  // support of development 
  void Cofactors_P1_(int c, double t, const VectorPolynomial& vc, MatrixPolynomial& C) const;
  void Cofactors_Pk_(int c, double t, const VectorPolynomial& vc, MatrixPolynomial& C) const;

  // old deprecated methods
  void LeastSquareProjector_Cell_(int order, int c, const std::vector<VectorPolynomial>& vf,
                                  VectorPolynomial& vc) const;

  // elliptic projectors
  ProjectorH1 projector;
};

}  // namespace WhetStone
}  // namespace Amanzi

#endif

