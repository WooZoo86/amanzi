/*
  Authors: Ethan Coon (ecoon@lanl.gov)

  This decorator class wraps a nonlinear SolverFnBase as a Matrix
  class that approximates the linear problem at a point by doing a
  standard finite difference (i.e. Knoll and Keyes '04) to approximate
  the action of the Jacobian.  When used with, for instance, NKA, this
  residual evaluation reduces to GMRES on the linear system, giving as
  a solution the standard JFNK correction to the nonlinear problem.

*/


#ifndef AMANZI_JF_MATRIX_HH_
#define AMANZI_JF_MATRIX_HH_

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"

#include "SolverFnBase.hh"

namespace Amanzi {
namespace AmanziSolvers {

template<class Vector, class VectorSpace>
class MatrixJF {

 public:
  MatrixJF() {} // default constructor for LinOp usage

  MatrixJF(Teuchos::ParameterList& plist,
           const Teuchos::RCP<SolverFnBase<Vector> > fn,
           const VectorSpace& map) :
      plist_(plist),
      fn_(fn) {
    map_ = Teuchos::rcp(new VectorSpace(map));
    r0_ = Teuchos::rcp(new Vector(*map_));
    u0_ = Teuchos::rcp(new Vector(*map_));
  }

  // Space for the domain of the operator.
  const VectorSpace& DomainMap() const { return *map_; }

  // Space for the domain of the operator.
  const VectorSpace& RangeMap() const { return *map_; }

  // Apply matrix, b <-- Ax, returns ierr = 0 if success, !0 otherwise
  int Apply(const Vector& x, Vector& b) const;

  // Apply the inverse, x <-- A^-1 b, returns ierr = 0 if success, !0 otherwise
  int ApplyInverse(const Vector& b, Vector& x) const;

  void set_linearization_point(const Teuchos::RCP<const Vector>& u);

 protected:
  double CalculateEpsilon_(const Vector& u, const Vector& x) const;

 protected:
  Teuchos::RCP<const VectorSpace> map_;
  Teuchos::RCP<SolverFnBase<Vector> > fn_;
  Teuchos::ParameterList plist_;
  Teuchos::RCP<Vector> u0_;
  Teuchos::RCP<Vector> r0_;

};


template<class Vector, class VectorSpace>
int
MatrixJF<Vector,VectorSpace>::Apply(const Vector& x, Vector& b) const {
  Teuchos::RCP<Vector> r1 = Teuchos::rcp(new Vector(*map_));

  double eps = CalculateEpsilon_(*u0_, x);

  // evaluate r1 = f(u0 + eps*x)
  u0_->Update(eps, x, 1.0);
  fn_->ChangedSolution();
  fn_->Residual(u0_, r1);

  // evaluate Jx = (r1 - r0) / eps
  b = *r1;
  b.Update(-1./eps, *r0_, 1./eps);

  // revert to old u0
  u0_->Update(-eps, x, 1);
  fn_->ChangedSolution();

  return 0;
}

template<class Vector, class VectorSpace>
int
MatrixJF<Vector,VectorSpace>::ApplyInverse(const Vector& b, Vector& x) const {
  // ugliness in interfaces...
  Teuchos::RCP<const Vector> b_ptr = Teuchos::rcpFromRef(b);
  Teuchos::RCP<Vector> x_ptr = Teuchos::rcpFromRef(x);

  fn_->ApplyPreconditioner(b_ptr, x_ptr);
  return 0;
}


template<class Vector, class VectorSpace>
void
MatrixJF<Vector,VectorSpace>::set_linearization_point(const Teuchos::RCP<const Vector>& u) {
  // std::cout << "setting lin point:" << std::endl;
  // u->Print(std::cout);
  *u0_ = *u;
  fn_->Residual(u0_, r0_);
  // std::cout << "res( lin point ):" << std::endl;
  // r0_->Print(std::cout);
}


template<class Vector, class VectorSpace>
double
MatrixJF<Vector,VectorSpace>::CalculateEpsilon_(const Vector& u, const Vector& x) const {
  // simple algorithm eqn 14 from Knoll and Keyes
  double uinf = 0;
  double xinf = 0;
  u.NormInf(&uinf);
  x.NormInf(&xinf);

  double eps = 1.e-8;
  if (xinf > 0.)
    eps = std::sqrt((1 + uinf)*1.e-12) / xinf;
  return eps;
}

} // namespace
} // namespace

#endif
