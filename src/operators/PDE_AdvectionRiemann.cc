/*
  Operators 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <vector>

#include "DG_Modal.hh"
#include "MFD3D_BernardiRaugel.hh"

#include "OperatorDefs.hh"
#include "Operator_Schema.hh"
#include "Op_Cell_Schema.hh"
#include "Op_Face_Schema.hh"
#include "PDE_AdvectionRiemann.hh"

namespace Amanzi {
namespace Operators {

/* ******************************************************************
* Initialize operator from parameter list.
****************************************************************** */
void PDE_AdvectionRiemann::InitAdvection_(Teuchos::ParameterList& plist)
{
  Teuchos::ParameterList& range = plist.sublist("schema range");
  Teuchos::ParameterList& domain = plist.sublist("schema domain");

  if (global_op_ == Teuchos::null) {
    // constructor was given a mesh
    // -- range schema and cvs
    local_schema_row_.Init(range, mesh_);
    global_schema_row_ = local_schema_row_;

    Teuchos::RCP<CompositeVectorSpace> cvs_row = Teuchos::rcp(new CompositeVectorSpace());
    cvs_row->SetMesh(mesh_)->SetGhosted(true);

    for (auto it = global_schema_row_.begin(); it != global_schema_row_.end(); ++it) {
      std::string name(local_schema_row_.KindToString(it->kind));
      cvs_row->AddComponent(name, it->kind, it->num);
    }

    // -- domain schema and cvs
    local_schema_col_.Init(domain, mesh_);
    global_schema_col_ = local_schema_col_;

    Teuchos::RCP<CompositeVectorSpace> cvs_col = Teuchos::rcp(new CompositeVectorSpace());
    cvs_col->SetMesh(mesh_)->SetGhosted(true);

    for (auto it = global_schema_col_.begin(); it != global_schema_col_.end(); ++it) {
      std::string name(local_schema_col_.KindToString(it->kind));
      cvs_col->AddComponent(name, it->kind, it->num);
    }

    global_op_ = Teuchos::rcp(new Operator_Schema(cvs_row, cvs_col, plist, global_schema_row_, global_schema_col_));
    if (local_schema_col_.base() == AmanziMesh::CELL) {
      local_op_ = Teuchos::rcp(new Op_Cell_Schema(global_schema_row_, global_schema_col_, mesh_));
    } else if (local_schema_col_.base() == AmanziMesh::FACE) {
      local_op_ = Teuchos::rcp(new Op_Face_Schema(global_schema_row_, global_schema_col_, mesh_));
    }

  } else {
    // constructor was given an Operator
    global_schema_row_ = global_op_->schema_row();
    global_schema_col_ = global_op_->schema_col();

    mesh_ = global_op_->DomainMap().Mesh();
    local_schema_row_.Init(range, mesh_);
    local_schema_col_.Init(domain, mesh_);

    local_op_ = Teuchos::rcp(new Op_Cell_Schema(global_schema_row_, global_schema_col_, mesh_));
  }

  // register the advection Op
  global_op_->OpPushBack(local_op_);

  // parameters
  // -- discretization method
  std::string name = plist.get<std::string>("method");
  method_order_ = plist.get<int>("method order", 0);
  if (name == "dg modal") {
    space_col_ = DG;
  } else {
    Errors::Message msg;
    msg << "Advection operator method \"" << name << "\" is invalid.";
    Exceptions::amanzi_throw(msg);
  }

  if (local_schema_row_ == local_schema_col_) { 
    space_row_ = space_col_;
  } else {
    space_row_ = P0;  // FIXME
  }


  // -- fluxes
  flux_ = plist.get<std::string>("flux formula", "remap");
  riemann_ = plist.get<std::string>("riemann problem", "average");
  jump_on_test_ = plist.get<bool>("jump operator on test function", true);
}


/* ******************************************************************
* A simple first-order transport method of the form div (u C), where 
* u is the given velocity field and C is the advected field.
****************************************************************** */
void PDE_AdvectionRiemann::UpdateMatrices(
    const Teuchos::Ptr<const std::vector<WhetStone::Polynomial> >& u)
{
  std::vector<WhetStone::DenseMatrix>& matrix = local_op_->matrices;
  std::vector<WhetStone::DenseMatrix>& matrix_shadow = local_op_->matrices_shadow;

  int d(mesh_->space_dimension());
  WhetStone::DG_Modal dg(mesh_);

  for (int f = 0; f < nfaces_owned; ++f) {
    WhetStone::DenseMatrix Aface;

    if (space_col_ == DG && space_row_ == DG) {
      dg.set_order(method_order_);
      dg.FluxMatrixPoly(f, (*u)[f], Aface, jump_on_test_);
    }

    matrix[f] = Aface;
  }
}


/* *******************************************************************
* Identify the advected flux of u
******************************************************************* */
void PDE_AdvectionRiemann::UpdateFlux(
    const Teuchos::Ptr<const CompositeVector>& h,
    const Teuchos::Ptr<const CompositeVector>& u,
    const Teuchos::RCP<BCs>& bc, const Teuchos::Ptr<CompositeVector>& flux)
{
  h->ScatterMasterToGhosted("cell");
  
  const Epetra_MultiVector& h_f = *h->ViewComponent("face", true);
  const Epetra_MultiVector& u_f = *u->ViewComponent("face", false);
  Epetra_MultiVector& flux_f = *flux->ViewComponent("face", false);

  flux->PutScalar(0.0);
  for (int f = 0; f < nfaces_owned; ++f) {
    flux_f[0][f] = u_f[0][f] * h_f[0][f];
  }  
}

}  // namespace Operators
}  // namespace Amanzi
