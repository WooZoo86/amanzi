/*
  Operators 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)
          Ethan Coon (ecoon@lanl.gov)
*/

#include <vector>

#include "OperatorDefs.hh"
#include "Operator_Cell.hh"
#include "Op_Face_Cell.hh"
#include "Op_SurfaceFace_SurfaceCell.hh"
#include "PDE_AdvectionUpwind.hh"

namespace Amanzi {
namespace Operators {

/* ******************************************************************
* Initialize operator from parameter list.
****************************************************************** */
void PDE_AdvectionUpwind::InitAdvection_(Teuchos::ParameterList& plist)
{
  if (global_op_ == Teuchos::null) {
    // constructor was given a mesh
    global_schema_row_.SetBase(AmanziMesh::FACE);
    global_schema_row_.AddItem(AmanziMesh::CELL, SCHEMA_DOFS_SCALAR, 1);
    global_schema_col_ = global_schema_row_;

    Teuchos::RCP<CompositeVectorSpace> cvs = Teuchos::rcp(new CompositeVectorSpace());
    cvs->SetMesh(mesh_)->AddComponent("cell", AmanziMesh::CELL, 1);
    global_op_ = Teuchos::rcp(new Operator_Cell(cvs, plist, global_schema_row_.OldSchema()));

    std::string name("FACE_CELL");

    if (plist.get<bool>("surface operator", false)) {
      local_op_ = Teuchos::rcp(new Op_SurfaceFace_SurfaceCell(name, mesh_));
    } else {
      local_op_ = Teuchos::rcp(new Op_Face_Cell(name, mesh_));
    }

  } else {
    // constructor was given an Operator
    global_schema_row_ = global_op_->schema_row();
    global_schema_col_ = global_op_->schema_col();

    if (!(global_schema_row_.OldSchema() & OPERATOR_SCHEMA_DOFS_CELL)) {
      Errors::Message msg;
      msg << "Operators: global schema for adding advection operator must contain CELL dofs.\n";
      Exceptions::amanzi_throw(msg);
    } else {
      mesh_ = global_op_->DomainMap().Mesh();

      std::string name("FACE_CELL");

      if (plist.get<bool>("surface operator", false)) {
        local_op_ = Teuchos::rcp(new Op_SurfaceFace_SurfaceCell(name, mesh_));
      } else {
        local_op_ = Teuchos::rcp(new Op_Face_Cell(name, mesh_));
      }
    }
  }

  // register the advection Op
  global_op_->OpPushBack(local_op_);
}


/* ******************************************************************
* Advection requires a velocity field.
****************************************************************** */
void PDE_AdvectionUpwind::Setup(const CompositeVector& u)
{
  IdentifyUpwindCells_(u);
}

  
/* ******************************************************************
* A simple first-order transport method.
* Advection operator is of the form: div (u C), where u is the given
* velocity field and C is the advected field.
****************************************************************** */
void PDE_AdvectionUpwind::UpdateMatrices(const Teuchos::Ptr<const CompositeVector>& u)
{
  std::vector<WhetStone::DenseMatrix>& matrix = local_op_->matrices;
  std::vector<WhetStone::DenseMatrix>& matrix_shadow = local_op_->matrices_shadow;

  AmanziMesh::Entity_ID_List cells;
  const Epetra_MultiVector& uf = *u->ViewComponent("face");

  for (int f = 0; f < nfaces_owned; ++f) {
    int c1 = (*upwind_cell_)[f];
    int c2 = (*downwind_cell_)[f];

    mesh_->face_get_cells(f, AmanziMesh::USED, &cells);
    int ncells = cells.size();
    WhetStone::DenseMatrix Aface(ncells, ncells);
    Aface.PutScalar(0.0);

    double umod = fabs(uf[0][f]);
    if (c1 < 0) {
      Aface(0, 0) = umod;
    } else if (c2 < 0) {
      Aface(0, 0) = umod;
    } else {
      int i = (cells[0] == c1) ? 0 : 1;
      Aface(i, i) = umod;
      Aface(1 - i, i) = -umod;
    }

    matrix[f] = Aface;
  }
}


/* ******************************************************************
* Add a simple first-order upwind method where the advected quantity
* is not the primary variable (used in Jacobians).
* Advection operator is of the form: div (u h(T))
*     u: flux
*     h: advected quantity (i.e. enthalpy)
*     T: primary varaible (i.e. temperature)
****************************************************************** */
void PDE_AdvectionUpwind::UpdateMatrices(
    const Teuchos::Ptr<const CompositeVector>& u,
    const Teuchos::Ptr<const CompositeVector>& dhdT)
{
  std::vector<WhetStone::DenseMatrix>& matrix = local_op_->matrices;
  std::vector<WhetStone::DenseMatrix>& matrix_shadow = local_op_->matrices_shadow;

  AmanziMesh::Entity_ID_List cells;
  const Epetra_MultiVector& uf = *u->ViewComponent("face");

  dhdT->ScatterMasterToGhosted("cell");
  const Epetra_MultiVector& dh = *dhdT->ViewComponent("cell", true);

  for (int f = 0; f < nfaces_owned; ++f) {
    int c1 = (*upwind_cell_)[f];
    int c2 = (*downwind_cell_)[f];

    mesh_->face_get_cells(f, AmanziMesh::USED, &cells);
    int ncells = cells.size();
    WhetStone::DenseMatrix Aface(ncells, ncells);
    Aface.PutScalar(0.0);

    double umod = fabs(uf[0][f]);
    if (c1 < 0) {
      Aface(0, 0) = umod * dh[0][c2];
    } else if (c2 < 0) {
      Aface(0, 0) = umod * dh[0][c1];
    } else {
      int i = (cells[0] == c1) ? 0 : 1;
      Aface(i, i) = umod * dh[0][c1];
      Aface(1 - i, i) = -umod * dh[0][c2];
    }

    matrix[f] = Aface;
  }
}


/* *******************************************************************
* Apply boundary condition to the local matrices
******************************************************************* */
void PDE_AdvectionUpwind::ApplyBCs(const Teuchos::RCP<BCs>& bc, bool primary)
{
  std::vector<WhetStone::DenseMatrix>& matrix = local_op_->matrices;
  std::vector<WhetStone::DenseMatrix>& matrix_shadow = local_op_->matrices_shadow;

  Epetra_MultiVector& rhs_cell = *global_op_->rhs()->ViewComponent("cell");

  const std::vector<int>& bc_model = bc->bc_model();
  const std::vector<double>& bc_value = bc->bc_value();

  for (int f = 0; f < nfaces_owned; f++) {
    if (bc_model[f] == OPERATOR_BC_DIRICHLET) {
      int c1 = (*upwind_cell_)[f];
      int c2 = (*downwind_cell_)[f];
      if (c2 < 0) {
        // pass, the upwind cell is internal to the domain, so all is good
      } else if (c1 < 0) {
        // downwind cell is internal to the domain
        rhs_cell[0][c2] += matrix[f](0, 0) * bc_value[f];
        matrix[f](0, 0) = 0.0;
      }
    } else if (bc_model[f] == OPERATOR_BC_NEUMANN) {
      // ETC: Several cases here.
      // 1. advection only problem
      //   - must deal with inward neumann here
      //   - outward neumann is not well posed?
      // 2. advection-diffusion
      //   - FV:
      //     * outward -- let diffusion take care of it
      //     * inward -- let diffusion take care of it
      //   - MFD: MFD is special because we can't just force advective fluxes on
      //     diffusion operator, as it should break 2nd order
      //     * outward -- advective flux is independent of boundary soln, but diffusive
      //       Neumann bc must subtract off advective flux
      //     * inward -- advective flux is dependent on boundary soln, and diffusion
      //        Neumann bc must subtract off advective flux
      //
      // For now, treat 1, and for 2, zero out advective flux, forcing diffusion op to 
      // deal with both diffusive and advective flux
      int c1 = (*upwind_cell_)[f];
      int c2 = (*downwind_cell_)[f];

      if (primary) { // advection only
        if (c2 < 0) {
          // pass
          matrix[f](0, 0) = 0.0;
        } else if (c1 < 0) {
          matrix[f](0, 0) = 0.0;
          rhs_cell[0][c2] += bc_value[f] * mesh_->face_area(f);
        }
      } else { // advection-diffusion
        // push the flux onto the diffusion operator
        matrix[f](0, 0) = 0.0;
      }
    }
  }
}


/* *******************************************************************
* Identify the advected flux of u
******************************************************************* */
void PDE_AdvectionUpwind::UpdateFlux(
    const Teuchos::Ptr<const CompositeVector>& h,
    const Teuchos::Ptr<const CompositeVector>& u,
    const Teuchos::RCP<BCs>& bc, const Teuchos::Ptr<CompositeVector>& flux)
{
  // might need to think more carefully about BCs
  const std::vector<int>& bc_model = bc->bc_model();
  const std::vector<double>& bc_value = bc->bc_value();
  flux->PutScalar(0.0);
  
  // apply preconditioner inversion
  h->ScatterMasterToGhosted("cell");
  const Epetra_MultiVector& h_c = *h->ViewComponent("cell", true);
  const Epetra_MultiVector& u_f = *u->ViewComponent("face", false);
  Epetra_MultiVector& flux_f = *flux->ViewComponent("face", false);

  for (int f = 0; f < nfaces_owned; ++f) {
    int c1 = (*upwind_cell_)[f];
    if (c1 < 0) {
      // boundary enthalpy
      flux_f[0][f] = u_f[0][f] * bc_value[f];
    } else {
      // upwind cell enthalpy
      flux_f[0][f] = u_f[0][f] * h_c[0][c1];
    }
  }  
}


/* *******************************************************************
* Identify flux direction based on orientation of the face normal 
* and sign of the  Darcy velocity.                               
******************************************************************* */
void PDE_AdvectionUpwind::IdentifyUpwindCells_(const CompositeVector& u)
{
  u.ScatterMasterToGhosted("face");
  const Epetra_MultiVector& uf = *u.ViewComponent("face", true);

  const Epetra_Map& fmap_wghost = mesh_->face_map(true);
  upwind_cell_ = Teuchos::rcp(new Epetra_IntVector(fmap_wghost));
  downwind_cell_ = Teuchos::rcp(new Epetra_IntVector(fmap_wghost));

  for (int f = 0; f < nfaces_wghost; f++) {
    (*upwind_cell_)[f] = -1;  // negative value indicates boundary
    (*downwind_cell_)[f] = -1;
  }

  AmanziMesh::Entity_ID_List faces;
  std::vector<int> fdirs;

  for (int c = 0; c < ncells_wghost; c++) {
    mesh_->cell_get_faces_and_dirs(c, &faces, &fdirs);

    for (int i = 0; i < faces.size(); i++) {
      int f = faces[i];
      if (uf[0][f] * fdirs[i] >= 0) {
        (*upwind_cell_)[f] = c;
      } else {
        (*downwind_cell_)[f] = c;
      }
    }
  }
}

}  // namespace Operators
}  // namespace Amanzi
