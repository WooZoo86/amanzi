/*
  WhetStone, version 2.1
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Maps between mesh objects located on different meshes, e.g. two states 
  of a deformable mesh: virtual element implementation.
*/

#include "Point.hh"

#include "DenseMatrix.hh"
#include "MeshMaps_VEM.hh"
#include "Polynomial.hh"

namespace Amanzi {
namespace WhetStone {

/* ******************************************************************
* Calculate mesh velocity in cell c: new algorithm
****************************************************************** */
void MeshMaps_VEM::VelocityCell(
    int c, const std::vector<VectorPolynomial>& vf, VectorPolynomial& vc) const
{
  // LeastSquareProjector_Cell_(1, c, vf, vc);
  AmanziGeometry::Point p0(mesh1_->cell_centroid(c) - mesh0_->cell_centroid(c));
  projector.HarmonicP1_Cell(c, p0, vf, vc);
}


/* ******************************************************************
* Calculate mesh velocity on face f.
****************************************************************** */
void MeshMaps_VEM::VelocityFace(int f, VectorPolynomial& vf) const
{
  if (d_ == 2) {
    MeshMaps::VelocityFace(f, vf);
  } else {
    AmanziMesh::Entity_ID_List edges;
    std::vector<int> dirs;

    mesh0_->face_get_edges_and_dirs(f, &edges, &dirs);
    int nedges = edges.size();

    VectorPolynomial v;
    std::vector<VectorPolynomial> ve;

    for (int n = 0; n < nedges; ++n) {
      int e = edges[n];
      VelocityEdge_(e, v);
      ve.push_back(v);
    }

    AmanziGeometry::Point p0(mesh1_->face_centroid(f) - mesh0_->face_centroid(f));
    projector.HarmonicP1_Face(f, p0, ve, vf);
  }
}


/* ******************************************************************
* Calculate mesh velocity on 2D or 3D edge e.
****************************************************************** */
void MeshMaps_VEM::VelocityEdge_(int e, VectorPolynomial& ve) const
{
  const AmanziGeometry::Point& xe0 = mesh0_->edge_centroid(e);
  const AmanziGeometry::Point& xe1 = mesh1_->edge_centroid(e);

  // velocity order 0
  ve.resize(d_);
  for (int i = 0; i < d_; ++i) {
    ve[i].Reshape(d_, 1);
  }

  // velocity order 1
  int n0, n1;
  AmanziGeometry::Point x0, x1;

  mesh0_->edge_get_nodes(e, &n0, &n1);
  mesh0_->node_get_coordinates(n0, &x0);
  mesh1_->node_get_coordinates(n0, &x1);

  x0 -= xe0;
  x1 -= xe1;

  // operator F(\xi) = x_c + R (\xi - \xi_c) where R = x1 * x0^T
  x0 /= L22(x0);
  for (int i = 0; i < d_; ++i) {
    for (int j = 0; j < d_; ++j) {
      ve[i](1, j) = x1[i] * x0[j];
    }
    ve[i](0, 0) = xe1[i] - x1[i] * (x0 * xe0);
    ve[i](1, i) -= 1.0;
  }
}


/* ******************************************************************
* Transformation of normal is defined completely by face data.
* NOTE: Limited to P1 elements. FIXME
****************************************************************** */
void MeshMaps_VEM::NansonFormula(
    int f, double t, const VectorPolynomial& v, VectorPolynomial& cn) const
{
  AmanziGeometry::Point p(d_);
  WhetStone::Tensor J(d_, 2);

  JacobianFaceValue(f, v, p, J);
  J *= t;
  J += 1.0 - t;

  Tensor C = J.Cofactors();
  p = C * mesh0_->face_normal(f);

  cn.resize(d_);
  for (int i = 0; i < d_; ++i) {
    cn[i].Reshape(d_, 0);
    cn[i](0, 0) = p[i];
  }
}


/* ******************************************************************
* Calculation of matrix of cofactors
****************************************************************** */
void MeshMaps_VEM::Cofactors(
    int c, double t, const VectorPolynomial& vc, MatrixPolynomial& C) const
{
  if (vc[0].order() == 1 || d_ == 3) {
    Cofactors_P1_(c, t, vc, C);
  } else {
    Cofactors_Pk_(c, t, vc, C);
  }
}


/* ******************************************************************
* Calculation of matrix of cofactors: linear velocity
****************************************************************** */
void MeshMaps_VEM::Cofactors_P1_(
    int c, double t, const VectorPolynomial& vc, MatrixPolynomial& C) const
{
  ASSERT(vc[0].order() == 1);

  // gradient of linear velocity is constant tensor
  Tensor T(d_, 2);
  for (int i = 0; i < d_; ++i) {
    for (int j = 0; j < d_; ++j) {
      T(i, j) = vc[i](1, j);
    }
  }
  T = T.Cofactors();

  // convert constants to polynomials
  C.resize(d_);
  for (int i = 0; i < d_; ++i) {
    C[i].resize(d_);
    for (int j = 0; j < d_; ++j) {
      C[i][j].Reshape(d_, 0);
      C[i][j](0, 0) = t * T(i, j);
    }
    C[i][i](0, 0) += 1.0;
  }
}


/* ******************************************************************
* Calculation of matrix of cofactors: nonlinear velocity
****************************************************************** */
void MeshMaps_VEM::Cofactors_Pk_(
    int c, double t, const VectorPolynomial& vc, MatrixPolynomial& C) const
{
  ASSERT(d_ == 2);

  // allocate memory for matrix of cofactors
  C.resize(d_);
  for (int i = 0; i < d_; ++i) {
    C[i].resize(d_);
  }

  // copy velocity gradients to C
  VectorPolynomial tmp(d_, 0);
  tmp.Gradient(vc[0]);
  C[1][1] = tmp[0];
  C[1][0] = tmp[1];
  C[1][0] *= -1.0;

  tmp.Gradient(vc[1]);
  C[0][0] = tmp[1];
  C[0][1] = tmp[0];
  C[0][1] *= -1.0;

  // add time dependence
  for (int i = 0; i < d_; ++i) {
    for (int j = 0; j < d_; ++j) {
      C[i][j](0, 0) *= t;
    }
    C[i][i](0, 0) += 1.0;
  }
}


/* ******************************************************************
* Calculation of Jacobian.
****************************************************************** */
void MeshMaps_VEM::JacobianCellValue(
    int c, double t, const AmanziGeometry::Point& xref, Tensor& J) const
{
}


/* ******************************************************************
* Calculate determinant of a Jacobian. A prototype for the future 
* projection scheme. Currently, we return a number.
****************************************************************** */
void MeshMaps_VEM::JacobianDet(
    int c, double t, const std::vector<VectorPolynomial>& vf, Polynomial& jac) const
{
  AmanziGeometry::Point x(d_), cn(d_);
  WhetStone::Tensor J(d_, 2); 

  Entity_ID_List faces;
  std::vector<int> dirs;

  mesh0_->cell_get_faces_and_dirs(c, &faces, &dirs);
  int nfaces = faces.size();

  double sum(0.0);
  for (int n = 0; n < nfaces; ++n) {
    int f = faces[n];

    // calculate j J^{-t} N dA
    JacobianFaceValue(f, vf[n], x, J);

    J *= t;
    J += 1.0 - t;

    Tensor C = J.Cofactors();
    cn = C * mesh0_->face_normal(f); 

    const AmanziGeometry::Point& xf0 = mesh0_->face_centroid(f);
    const AmanziGeometry::Point& xf1 = mesh1_->face_centroid(f);
    sum += (xf0 + t * (xf1 - xf0)) * cn * dirs[n];
  }
  sum /= d_ * mesh0_->cell_volume(c);

  jac.Reshape(d_, 0);
  jac(0, 0) = sum;
}


/* ******************************************************************
* Calculate determinant of a Jacobian. Different version
****************************************************************** */
void MeshMaps_VEM::JacobianDet(
    double t, const VectorPolynomial& vc, Polynomial& jac) const
{
  Tensor T(d_, 2);
  for (int i = 0; i < d_; ++i) {
    for (int j = 0; j < d_; ++j) {
      T(i, j) = vc[i](1, j);
    }
  }

  T *= t;
  T += 1.0;

  jac.Reshape(d_, 0);
  jac(0, 0) = T.Det();
}


/* ******************************************************************
* Calculate Jacobian at point x of face f 
****************************************************************** */
void MeshMaps_VEM::JacobianFaceValue(
    int f, const VectorPolynomial& v,
    const AmanziGeometry::Point& x, Tensor& J) const
{
  // FIXME x is not used
  for (int i = 0; i < d_; ++i) {
    for (int j = 0; j < d_; ++j) {
      J(i, j) = v[i](1, j);
    }
  }
  J += 1.0;
}


/* ******************************************************************
* Calculate mesh velocity in cell c: old algorithm
****************************************************************** */
void MeshMaps_VEM::LeastSquareProjector_Cell_(
    int order, int c, const std::vector<VectorPolynomial>& vf, VectorPolynomial& vc) const
{
  vc.resize(d_);
  for (int i = 0; i < d_; ++i) vc[i].Reshape(d_, 1);
  
  Entity_ID_List nodes;
  mesh0_->cell_get_nodes(c, &nodes);
  int nnodes = nodes.size();

  AmanziGeometry::Point px;
  std::vector<AmanziGeometry::Point> x1, x2, u;
  for (int i = 0; i < nnodes; ++i) {
    mesh0_->node_get_coordinates(nodes[i], &px);
    x1.push_back(px);
  }
  for (int i = 0; i < nnodes; ++i) {
    mesh1_->node_get_coordinates(nodes[i], &px);
    x2.push_back(px);
  }

  // calculate velocity u(X) = F(X) - X
  LeastSquareFit(order, x1, x2, vc);

  for (int i = 0; i < d_; ++i) {
    vc[i](1, i) -= 1.0;
  }
}

}  // namespace WhetStone
}  // namespace Amanzi

