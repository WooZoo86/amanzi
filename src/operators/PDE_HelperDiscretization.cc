/*
  Operators

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Author: Konstantin Lipnikov (lipnikov@lanl.gov)

  Helper class for discrete PDE operators.
*/

#include "errors.hh"

#include "PDE_HelperDiscretization.hh"

namespace Amanzi {
namespace Operators {

/* ******************************************************************
* Simple constructors.
****************************************************************** */
PDE_HelperDiscretization::PDE_HelperDiscretization(const Teuchos::RCP<Operator>& global_op) :
    global_op_(global_op)
{
  if (global_op == Teuchos::null) {
    Errors::Message msg("PDE_HelperDiscretization: Constructor received null global operator");
    Exceptions::amanzi_throw(msg);
  }

  mesh_ = global_op_->Mesh();
  PopulateDimensions_();
}


PDE_HelperDiscretization::PDE_HelperDiscretization(const Teuchos::RCP<const AmanziMesh::Mesh>& mesh) :
     mesh_(mesh)
{
  PopulateDimensions_();
}


PDE_HelperDiscretization::PDE_HelperDiscretization(const Teuchos::RCP<AmanziMesh::Mesh>& mesh) :
     mesh_(mesh)
{
  PopulateDimensions_();
}


/* ******************************************************************
* Supporting private routines.
****************************************************************** */
void PDE_HelperDiscretization::PopulateDimensions_()
{
  ncells_owned = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::OWNED);
  nfaces_owned = mesh_->num_entities(AmanziMesh::FACE, AmanziMesh::OWNED);
  nnodes_owned = mesh_->num_entities(AmanziMesh::NODE, AmanziMesh::OWNED);

  ncells_wghost = mesh_->num_entities(AmanziMesh::CELL, AmanziMesh::USED);
  nfaces_wghost = mesh_->num_entities(AmanziMesh::FACE, AmanziMesh::USED);
  nnodes_wghost = mesh_->num_entities(AmanziMesh::NODE, AmanziMesh::USED);

  if (mesh_->valid_edges()) {
    nedges_owned = mesh_->num_entities(AmanziMesh::EDGE, AmanziMesh::OWNED);
    nedges_wghost = mesh_->num_entities(AmanziMesh::EDGE, AmanziMesh::USED);
  }
}


/* ******************************************************************
* Apply boundary conditions to the local matrices. We always zero-out
* matrix rows for essential test BCs. As to trial BCs, there are
* options: (a) eliminate or not, (b) if eliminate, then put 1 on
* the diagonal or not.
****************************************************************** */
void PDE_HelperDiscretization::ApplyBCs(bool primary, bool eliminate)
{
  for (auto bc = bcs_trial_.begin(); bc != bcs_trial_.end(); ++bc) {
    if ((*bc)->type() == SCHEMA_DOFS_SCALAR ||
        (*bc)->type() == SCHEMA_DOFS_NORMAL_COMPONENT) {
      ApplyBCs_Cell_Scalar_(bc->ptr(), local_op_, primary, eliminate);
    } 
    else if ((*bc)->type() == SCHEMA_DOFS_POINT) {
      ApplyBCs_Cell_Point_(bc->ptr(), local_op_, primary, eliminate);
    }
    else if ((*bc)->type() == SCHEMA_DOFS_VECTOR) {
      ApplyBCs_Cell_Vector_(bc->ptr(), local_op_, primary, eliminate);
    }
    else {
      Errors::Message msg("PDE_HelperDiscretization: Unsupported boundary condition.\n");
      Exceptions::amanzi_throw(msg);
    }
  }
}


/* ******************************************************************
* Apply BCs of scalar type. The code is based on face (f) DOFs.
****************************************************************** */
void PDE_HelperDiscretization::ApplyBCs_Cell_Scalar_(
    const Teuchos::Ptr<BCs>& bc, Teuchos::RCP<Op> op,
    bool primary, bool eliminate)
{
  const std::vector<int>& bc_model = bc->bc_model();
  const std::vector<double>& bc_value = bc->bc_value();

  AmanziMesh::Entity_ID_List entities;
  std::vector<int> dirs, offset;

  CompositeVector& rhs = *global_op_->rhs();
  rhs.PutScalarGhosted(0.0);

  const Schema& schema_row = global_op_->schema_row();
  const Schema& schema_col = global_op_->schema_col();

  AmanziMesh::Entity_kind kind = bc->kind();
  ASSERT(kind == AmanziMesh::FACE || kind == AmanziMesh::EDGE);
  Teuchos::RCP<Epetra_MultiVector> rhs_kind;
  if (primary) rhs_kind = rhs.ViewComponent(schema_row.KindToString(kind), true);

  for (int c = 0; c != ncells_owned; ++c) {
    WhetStone::DenseMatrix& Acell = op->matrices[c];
    int ncols = Acell.NumCols();
    int nrows = Acell.NumRows();

    if (kind == AmanziMesh::FACE) {
      mesh_->cell_get_faces_and_dirs(c, &entities, &dirs);
    } else if (kind == AmanziMesh::EDGE) {
      mesh_->cell_get_edges(c, &entities);
    }
    int nents = entities.size();

    // check for a boundary face
    bool found(false);
    for (int n = 0; n != nents; ++n) {
      int f = entities[n];
      if (bc_model[f] == OPERATOR_BC_DIRICHLET) found = true;
    }
    if (!found) continue;

    // essential conditions for test functions
    schema_row.ComputeOffset(c, mesh_, offset);

    bool flag(true);
    int item(0);
    for (auto it = op->schema_row_.begin(); it != op->schema_row_.end(); ++it, ++item) {
      if (it->kind == kind) {
        for (int n = 0; n != nents; ++n) {
          int f = entities[n];
          if (bc_model[f] == OPERATOR_BC_DIRICHLET) {
            if (flag) {  // make a copy of elemental matrix
              op->matrices_shadow[c] = Acell;
              flag = false;
            }
            int noff(n + offset[item]);
            for (int m = 0; m < ncols; m++) Acell(noff, m) = 0.0;
          }
        }
      }
    }

    // essential zero conditions for trial functions
    schema_col.ComputeOffset(c, mesh_, offset);

    item = 0;
    for (auto it = op->schema_col_.begin(); it != op->schema_col_.end(); ++it, ++item) {
      if (it->kind == kind) {
        for (int n = 0; n != nents; ++n) {
          int f = entities[n];
          double value = bc_value[f];

          if (bc_model[f] == OPERATOR_BC_DIRICHLET) {
            if (flag) {  // make a copy of elemental matrix
              op->matrices_shadow[c] = Acell;
              flag = false;
            }

            int noff(n + offset[item]);
            WhetStone::DenseVector rhs_loc(nrows);

            if (eliminate) {
              for (int m = 0; m < nrows; m++) {
                rhs_loc(m) = -Acell(m, noff) * value;
                Acell(m, noff) = 0.0;
              }
            }

            if (primary) {
              rhs_loc(noff) = 0.0;
              (*rhs_kind)[0][f] = value;
              Acell(noff, noff) = 1.0;
            }

            global_op_->AssembleVectorCellOp(c, schema_row, rhs_loc, rhs);
          }
        }
      }
    }
  } 

  rhs.GatherGhostedToMaster(Add);
}


/* ******************************************************************
* Apply BCs of scalar type. The code is limited to node DOFs.
****************************************************************** */
void PDE_HelperDiscretization::ApplyBCs_Cell_Point_(
    const Teuchos::Ptr<BCs>& bc, Teuchos::RCP<Op> op,
    bool primary, bool eliminate)
{
  const std::vector<int>& bc_model = bc->bc_model();
  const std::vector<AmanziGeometry::Point>& bc_value = bc->bc_value_point();

  AmanziMesh::Entity_ID_List nodes, cells;
  std::vector<int> offset;

  CompositeVector& rhs = *global_op_->rhs();
  rhs.PutScalarGhosted(0.0);

  // AmanziMesh::Entity_kind kind = bc->kind();
  Teuchos::RCP<Epetra_MultiVector> rhs_node;
  if (primary) rhs_node = rhs.ViewComponent("node", true);

  int d = mesh_->space_dimension(); 
  const Schema& schema_row = global_op_->schema_row();
  const Schema& schema_col = global_op_->schema_col();

  for (int c = 0; c != ncells_owned; ++c) {
    WhetStone::DenseMatrix& Acell = op->matrices[c];
    int ncols = Acell.NumCols();
    int nrows = Acell.NumRows();

    mesh_->cell_get_nodes(c, &nodes);
    int nnodes = nodes.size();

    // check for a boundary face
    bool found(false);
    for (int n = 0; n != nnodes; ++n) {
      int v = nodes[n];
      if (bc_model[v] == OPERATOR_BC_DIRICHLET) found = true;
    }
    if (!found) continue;

    // essential conditions for test functions
    op->schema_row_.ComputeOffset(c, mesh_, offset);

    bool flag(true);
    int item(0);
    for (auto it = op->schema_row_.begin(); it != op->schema_row_.end(); ++it, ++item) {
      if (it->kind == AmanziMesh::NODE) {
        for (int n = 0; n != nnodes; ++n) {
          int v = nodes[n];
          if (bc_model[v] == OPERATOR_BC_DIRICHLET) {
            if (flag) {  // make a copy of elemental matrix
              op->matrices_shadow[c] = Acell;
              flag = false;
            }
            for (int k = 0; k < d; ++k) {
              int noff(d*n + k + offset[item]);
              for (int m = 0; m < ncols; m++) Acell(noff, m) = 0.0;
            }
          }
        }
      }
    }

    // essential zero conditions for trial functions
    schema_col.ComputeOffset(c, mesh_, offset);

    item = 0;
    for (auto it = op->schema_col_.begin(); it != op->schema_col_.end(); ++it, ++item) {
      if (it->kind == AmanziMesh::NODE) {
        for (int n = 0; n != nnodes; ++n) {
          int v = nodes[n];
          AmanziGeometry::Point value = bc_value[v];

          if (bc_model[v] == OPERATOR_BC_DIRICHLET) {
            if (flag) {  // make a copy of elemental matrix
              op->matrices_shadow[c] = Acell;
              flag = false;
            }

            for (int k = 0; k < d; ++k) {
              int noff(d*n + k + offset[item]);
              WhetStone::DenseVector rhs_loc(nrows);

              if (eliminate) {
                for (int m = 0; m < nrows; m++) {
                  rhs_loc(m) = -Acell(m, noff) * value[k];
                  Acell(m, noff) = 0.0;
                }
              }

              if (primary) {
                mesh_->node_get_cells(v, AmanziMesh::USED, &cells);
                rhs_loc(noff) = 0.0;
                (*rhs_node)[k][v] = value[k];
                Acell(noff, noff) = 1.0 / cells.size();
              }

              global_op_->AssembleVectorCellOp(c, schema_row, rhs_loc, rhs);
            }
          }
        }
      }
    }
  } 

  rhs.GatherGhostedToMaster(Add);
}


/* ******************************************************************
* Apply BCs of vector type. The code is based on face (f) DOFs.
****************************************************************** */
void PDE_HelperDiscretization::ApplyBCs_Cell_Vector_(
    const Teuchos::Ptr<BCs>& bc, Teuchos::RCP<Op> op,
    bool primary, bool eliminate)
{
  const std::vector<int>& bc_model = bc->bc_model();
  const std::vector<std::vector<double> >& bc_value = bc->bc_value_vector();
  int d = bc_value[0].size();

  AmanziMesh::Entity_ID_List entities;
  std::vector<int> dirs, offset;

  CompositeVector& rhs = *global_op_->rhs();
  rhs.PutScalarGhosted(0.0);

  const Schema& schema_row = global_op_->schema_row();
  const Schema& schema_col = global_op_->schema_col();

  AmanziMesh::Entity_kind kind = bc->kind();
  ASSERT(kind == AmanziMesh::FACE || kind == AmanziMesh::EDGE);
  Teuchos::RCP<Epetra_MultiVector> rhs_kind;
  if (primary) rhs_kind = rhs.ViewComponent(schema_row.KindToString(kind), true);

  for (int c = 0; c != ncells_owned; ++c) {
    WhetStone::DenseMatrix& Acell = op->matrices[c];
    int ncols = Acell.NumCols();
    int nrows = Acell.NumRows();

    if (kind == AmanziMesh::FACE) {
      mesh_->cell_get_faces_and_dirs(c, &entities, &dirs);
    }
    int nents = entities.size();

    // check for a boundary face
    bool found(false);
    for (int n = 0; n != nents; ++n) {
      int f = entities[n];
      if (bc_model[f] == OPERATOR_BC_DIRICHLET) found = true;
    }
    if (!found) continue;

    // essential conditions for test functions
    schema_row.ComputeOffset(c, mesh_, offset);

    bool flag(true);
    int item(0);
    for (auto it = op->schema_row_.begin(); it != op->schema_row_.end(); ++it, ++item) {
      if (it->kind == kind) {
        for (int n = 0; n != nents; ++n) {
          int f = entities[n];
          if (bc_model[f] == OPERATOR_BC_DIRICHLET) {
            if (flag) {  // make a copy of elemental matrix
              op->matrices_shadow[c] = Acell;
              flag = false;
            }

            for (int k = 0; k < d; ++k) {
              int noff(d*n + k + offset[item]);
              for (int m = 0; m < ncols; m++) Acell(noff, m) = 0.0;
            }
          }
        }
      }
    }

    // essential zero conditions for trial functions
    schema_col.ComputeOffset(c, mesh_, offset);

    item = 0;
    for (auto it = op->schema_col_.begin(); it != op->schema_col_.end(); ++it, ++item) {
      if (it->kind == kind) {
        for (int n = 0; n != nents; ++n) {
          int f = entities[n];
          const std::vector<double>& value = bc_value[f];

          if (bc_model[f] == OPERATOR_BC_DIRICHLET) {
            if (flag) {  // make a copy of elemental matrix
              op->matrices_shadow[c] = Acell;
              flag = false;
            }

            for (int k = 0; k < d; ++k) {
              int noff(d*n + k + offset[item]);
              WhetStone::DenseVector rhs_loc(nrows);

              if (eliminate) {
                for (int m = 0; m < nrows; m++) {
                  rhs_loc(m) = -Acell(m, noff) * value[k];
                  Acell(m, noff) = 0.0;
                }
              }

              if (primary) {
                rhs_loc(noff) = 0.0;
                (*rhs_kind)[k][f] = value[k];
                Acell(noff, noff) = 1.0;
              }

              global_op_->AssembleVectorCellOp(c, schema_row, rhs_loc, rhs);
            }
          }
        }
      }
    }
  } 

  rhs.GatherGhostedToMaster(Add);
}

}  // namespace Operators
}  // namespace Amanzi



