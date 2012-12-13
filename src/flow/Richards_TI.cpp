/*
This is the flow component of the Amanzi code. 

Copyright 2010-2012 held jointly by LANS/LANL, LBNL, and PNNL. 
Amanzi is released under the three-clause BSD License. 
The terms of use and "as is" disclaimer for this license are 
provided in the top-level COPYRIGHT file.

Authors: Neil Carlson (nnc@lanl.gov), 
         Konstantin Lipnikov (lipnikov@lanl.gov)

The routine implements interface to BDFx time integrators.  
*/

#include <algorithm>
#include <string>
#include <vector>

#include "exceptions.hh"

#include "Richards_PK.hpp"

namespace Amanzi {
namespace AmanziFlow {

/* ******************************************************************
* Calculate f(u, du/dt) = a d(s(u))/dt + A*u - g.                                         
****************************************************************** */
void Richards_PK::fun(
    double Tp, const Epetra_Vector& u, const Epetra_Vector& udot, Epetra_Vector& f, double dTp)
{      
  ComputePreconditionerMFD(u, matrix_, Tp, 0.0, false);  // Calculate only stiffness matrix.
  matrix_->ComputeNegativeResidual(u, f);  // compute A*u - g

  int ncells = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
  const Epetra_Vector& phi = FS->ref_porosity();

  functional_max_norm = 0.0;
  functional_max_cell = 0;

  for (int mb = 0; mb < WRM.size(); mb++) {
    std::string region = WRM[mb]->region();
    int ncells = mesh_->get_set_size(region, AmanziMesh::CELL, AmanziMesh::OWNED);

    AmanziMesh::Entity_ID_List block(ncells);
    mesh_->get_set_entities(region, AmanziMesh::CELL, AmanziMesh::OWNED, &block);

    double v, s1, s2, volume;
    for (int i = 0; i < block.size(); i++) {
      int c = block[i];
      v = u[c] - udot[c] * dTp;
      s1 = WRM[mb]->saturation(atm_pressure - u[c]);
      s2 = WRM[mb]->saturation(atm_pressure - v);

      double factor = rho * phi[c] * mesh_->cell_volume(c) / dTp;
      f[c] += (s1 - s2) * factor;

      double tmp = fabs(f[c]) / factor;  // calculate errors
      if (tmp > functional_max_norm) {
        functional_max_norm = tmp;
        functional_max_cell = c;        
      }
    }
  }

  // DEBUG
  // if (MyPID == 0 && verbosity >= FLOW_VERBOSITY_EXTREME)
  //    std::printf("Flow PK: evaluating functional at T=%10.5e [sec] dT=%9.4e [sec]\n", Tp, dTp);
}


/* ******************************************************************
* Apply preconditioner inv(B) * X.                                                 
****************************************************************** */
void Richards_PK::precon(const Epetra_Vector& X, Epetra_Vector& Y)
{
  preconditioner_->ApplyInverse(X, Y);
}


/* ******************************************************************
* Update new preconditioner B(p, dT_prec).                                   
****************************************************************** */
void Richards_PK::update_precon(double Tp, const Epetra_Vector& u, double dTp, int& ierr)
{
  ComputePreconditionerMFD(u, preconditioner_, Tp, dTp, true);
  ierr = 0;
}


/* ******************************************************************
* .                                   
****************************************************************** */
int Richards_PK::ApllyPrecInverse(const Epetra_MultiVector& X, Epetra_MultiVector& Y)
{
  return preconditioner_->ApplyInverse(X, Y);
}


/* ******************************************************************
* Check difference du between the predicted and converged solutions.
* This is a wrapper for various error control methods. 
****************************************************************** */
double Richards_PK::enorm(const Epetra_Vector& u, const Epetra_Vector& du)
{
  double error;
  error = ErrorNormSTOMP(u, du);
  // error = ErrorNormRC1(u, du);

  return error;
}


/* ******************************************************************
* Error control a-la STOMP.
****************************************************************** */
double Richards_PK::ErrorNormSTOMP(const Epetra_Vector& u, const Epetra_Vector& du)
{
  double error, error_p, error_r;
  int cell_p, cell_r;

  if (error_control_ & FLOW_TI_ERROR_CONTROL_PRESSURE) {
    error_p = 0.0;
    cell_p = 0;
    for (int c = 0; c < ncells_owned; c++) {
      double tmp = fabs(du[c]) / (fabs(u[c] - atm_pressure) + atm_pressure);
      if (tmp > error_p) {
        error_p = tmp;
        cell_p = c;
      } 
    }
  } else {
    error_p = 0.0;
  }

  if (error_control_ & FLOW_TI_ERROR_CONTROL_RESIDUAL) {
    error_r = functional_max_norm;
  } else {
    error_r = 0.0;
  }

  error = std::max<double>(error_r, error_p);

#ifdef HAVE_MPI
  double buf = error;
  du.Comm().MaxAll(&buf, &error, 1);  // find the global maximum
#endif

  // maximum error is printed out only on one processor
  if (verbosity >= FLOW_VERBOSITY_EXTREME) {
    if (error == buf) {
      int c = functional_max_cell;
      const AmanziGeometry::Point& xp = mesh_->cell_centroid(c);

      printf("\nFlow PK: residual = %9.3g at point", functional_max_norm);
      for (int i = 0; i < dim; i++) printf(" %8.3g", xp[i]);
      printf("\n");
 
      c = cell_p;
      const AmanziGeometry::Point& yp = mesh_->cell_centroid(c);
      printf("       pressure error = %9.3g at point", error_p);
      for (int i = 0; i < dim; i++) printf(" %8.3g", yp[i]);

      int mb = (*map_c2mb)[c];
      double s = WRM[mb]->saturation(atm_pressure - u[c]);
      printf(",  saturation = %5.3g,  pressure = %9.3g\n", s, u[c]);
    }
  }

  // if (error_control_ & FLOW_TI_ERROR_CONTROL_SATURATION) {
  //  if (saturation_max_change > 0.125) throw 100;
  // }
  
  return error;
}


/* ******************************************************************
* Error control from RC1 (OBSOLETE)
****************************************************************** */
double Richards_PK::ErrorNormRC1(const Epetra_Vector& u, const Epetra_Vector& du)
{
  double error_norm = 0.0;
  double absolute_tol = 1.0, relative_tol = 1e-6;
  for (int n = 0; n < u.MyLength(); n++) {
    double tmp = fabs(du[n]) / (absolute_tol + relative_tol * fabs(u[n]));
    error_norm = std::max<double>(error_norm, tmp);
  }
 
#ifdef HAVE_MPI
  double buf = error_norm;
  du.Comm().MaxAll(&buf, &error_norm, 1);  // find the global maximum
#endif
  return  error_norm;
}

}  // namespace AmanziFlow
}  // namespace Amanzi



