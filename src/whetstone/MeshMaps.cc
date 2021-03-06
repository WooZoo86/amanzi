/*
  WhetStone, version 2.1
  Release name: naka-to.

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Base class for maps between mesh objects located on different 
  meshes, e.g. two states of a deformable mesh.
*/

#include "Point.hh"

#include "DenseMatrix.hh"
#include "MeshMaps.hh"
#include "Polynomial.hh"

namespace Amanzi {
namespace WhetStone {

/* ******************************************************************
* Calculate mesh velocity on face f.
* NOTE: 2D algorithm for linear velocity.
****************************************************************** */
void MeshMaps::VelocityFace(int f, VectorPolynomial& v) const
{
  AmanziMesh::Entity_ID_List nodes;
  AmanziGeometry::Point x0, x1;

  const AmanziGeometry::Point& xf0 = mesh0_->face_centroid(f);
  const AmanziGeometry::Point& xf1 = mesh1_->face_centroid(f);

  mesh0_->face_get_nodes(f, &nodes);
  mesh0_->node_get_coordinates(nodes[0], &x0);
  mesh1_->node_get_coordinates(nodes[0], &x1);

  x0 -= xf0;
  x1 -= xf1;

  v.resize(d_);
  for (int i = 0; i < d_; ++i) {
    v[i].Reshape(d_, 1);
  }

  x0 /= L22(x0);
  for (int i = 0; i < d_; ++i) {
    for (int j = 0; j < d_; ++j) {
      v[i](1, j) = x1[i] * x0[j];
    }
    v[i](0, 0) = xf1[i] - x1[i] * (x0 * xf0);
    v[i](1, i) -= 1.0;
  }
}


/* ******************************************************************
* Polynomial approximation v of map x2 = F(x1).
* We assume that vectors of vertices have a proper length.
****************************************************************** */
int MeshMaps::LeastSquareFit(int order,
                             const std::vector<AmanziGeometry::Point>& x1, 
                             const std::vector<AmanziGeometry::Point>& x2,
                             VectorPolynomial& v) const
{
  Polynomial poly(d_, order);

  int nk = poly.size();
  int nx = x1.size();

  // evaluate basis functions at given points
  DenseMatrix psi(nk, nx);

  for (auto it = poly.begin(); it.end() <= poly.end(); ++it) {
    int i = it.PolynomialPosition();
    const int* idx = it.multi_index();

    for (int n = 0; n < nx; ++n) {
      double val(1.0);
      for (int k = 0; k < d_; ++k) {
        val *= std::pow(x1[n][k], idx[k]);
      }
      psi(i, n) = val;
    }
  }
      
  // form matrix of linear system
  DenseMatrix A(nk, nk);

  for (int i = 0; i < nk; ++i) {
    for (int j = i; j < nk; ++j) {
      double tmp(0.0);
      for (int n = 0; n < nx; ++n) {
        tmp += psi(i, n) * psi(j, n);
      }
      A(i, j) = A(j, i) = tmp;
    }
  }

  A.Inverse();

  // solver linear systems
  DenseVector b(nk), u(nk);

  v.resize(d_);
  for (int k = 0; k < d_; ++k) { 
    v[k].Reshape(d_, order);

    for (int i = 0; i < nk; ++i) {
      b(i) = 0.0;
      for (int n = 0; n < nx; ++n) {
        b(i) += x2[n][k] * psi(i, n);
      }
    }

    A.Multiply(b, u, false);

    for (auto it = poly.begin(); it.end() <= poly.end(); ++it) {
      int n = it.MonomialOrder();
      int m = it.MonomialPosition();
      int i = it.PolynomialPosition();
      v[k].monomials(n).coefs()[m] = u(i);
    }
  }

  return 0;
}


/* ******************************************************************
* Error calculation requires geometric center.
****************************************************************** */
AmanziGeometry::Point MeshMaps::cell_geometric_center(int id, int c) const
{
  Entity_ID_List nodes;
  AmanziGeometry::Point v(d_), xg(d_);

  mesh0_->cell_get_nodes(c, &nodes);
  int nnodes = nodes.size();
  for (int i = 0; i < nnodes; ++i) {
    if (id == 0) {
      mesh0_->node_get_coordinates(nodes[i], &v);
    } else {
      mesh1_->node_get_coordinates(nodes[i], &v);
    }
    xg += v;
  } 
  xg /= nnodes;
  
  return xg;
}

}  // namespace WhetStone
}  // namespace Amanzi

