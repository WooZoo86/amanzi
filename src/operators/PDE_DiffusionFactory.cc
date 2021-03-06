/*
  Operators

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL.
  Amanzi is released under the three-clause BSD License.
  The terms of use and "as is" disclaimer for this license are
  provided in the top-level COPYRIGHT file.

  Authors: Konstantin Lipnikov (lipnikov@lanl.gov)
           Ethan Coon (ecoon@lanl.gov)

  Base factory for diffusion operators.
*/

#include "errors.hh"

#include "BCs.hh"
#include "OperatorDefs.hh"
#include "PDE_DiffusionFactory.hh"
#include "PDE_DiffusionMFD.hh"
#include "PDE_DiffusionFV.hh"
#include "PDE_DiffusionNLFV.hh"
#include "PDE_DiffusionMFDwithGravity.hh"
#include "PDE_DiffusionFVwithGravity.hh"
#include "PDE_DiffusionNLFVwithGravity.hh"

namespace Amanzi {
namespace Operators {

/* ******************************************************************
* Initialization of diffusion operator with optional gravity.
* This is the constructor used by Amanzi.
****************************************************************** */
Teuchos::RCP<PDE_Diffusion> PDE_DiffusionFactory::Create(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<const AmanziMesh::Mesh>& mesh, 
    const Teuchos::RCP<BCs>& bc, 
    double rho,
    const AmanziGeometry::Point& g)
{
  Teuchos::RCP<PDE_Diffusion> op = Teuchos::null;

  std::string name = oplist.get<std::string>("discretization primary");
  bool flag = oplist.get<bool>("gravity", false);

  // FV methods
  if (name == "fv: default" && !flag) {
    op = Teuchos::rcp(new PDE_DiffusionFV(oplist, mesh));
    op->SetBCs(bc, bc);

  } else if (name == "fv: default" && flag) {
    op = Teuchos::rcp(new PDE_DiffusionFVwithGravity(oplist, mesh, rho, g));
    op->SetBCs(bc, bc);

  // NLFV methods
  } else if (name == "nlfv: default" && !flag) {
    op = Teuchos::rcp(new PDE_DiffusionNLFV(oplist, mesh)); 
    op->SetBCs(bc, bc);

  } else if (name == "nlfv: default" && flag) {
    op = Teuchos::rcp(new PDE_DiffusionNLFVwithGravity(oplist, mesh, rho, g)); 
    op->SetBCs(bc, bc);

  // MFD methods
  } else if (!flag) {
    op = Teuchos::rcp(new PDE_DiffusionMFD(oplist, mesh));
    op->SetBCs(bc, bc);

  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFDwithGravity(oplist, mesh, rho, g));
    op->SetBCs(bc, bc);
  }
  return op;
}


/* ******************************************************************
* Initialization of diffusion operator with optional gravity.
* This is the factory used by Amanzi, though it makes life difficult
* for time-varying density.
****************************************************************** */
Teuchos::RCP<PDE_Diffusion> PDE_DiffusionFactory::Create(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<const AmanziMesh::Mesh>& mesh, 
    const Teuchos::RCP<BCs>& bc, 
    const Teuchos::RCP<const CompositeVector>& rho,
    const AmanziGeometry::Point& g)
{
  Teuchos::RCP<PDE_Diffusion> op = Teuchos::null;

  std::string name = oplist.get<std::string>("discretization primary");
  bool flag = oplist.get<bool>("gravity", false);

  // FV methods
  if (name == "fv: default" && !flag) {
    op = Teuchos::rcp(new PDE_DiffusionFV(oplist, mesh));
    op->SetBCs(bc, bc);

  } else if (name == "fv: default" && flag) {
    Teuchos::RCP<PDE_DiffusionFVwithGravity> op_g =
        Teuchos::rcp(new PDE_DiffusionFVwithGravity(oplist, mesh, g));
    op_g->SetBCs(bc, bc);
    op_g->SetDensity(rho);
    op = op_g;

  // MFD methods
  } else if (!flag) {
    op = Teuchos::rcp(new PDE_DiffusionMFD(oplist, mesh));
    op->SetBCs(bc, bc);

  } else {
    Teuchos::RCP<PDE_DiffusionMFDwithGravity> op_g =
        Teuchos::rcp(new PDE_DiffusionMFDwithGravity(oplist, mesh, g));
    op_g->SetBCs(bc, bc);
    op_g->SetDensity(rho);
    op = op_g;
  }
  return op;
}


/* ******************************************************************
* Initialization of straight diffusion operator: method 1.
****************************************************************** */
Teuchos::RCP<PDE_Diffusion> PDE_DiffusionFactory::Create(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<const AmanziMesh::Mesh>& mesh,
    const Teuchos::RCP<BCs>& bc)
{
  Teuchos::RCP<PDE_Diffusion> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  // FV methods
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFV(oplist, mesh));
    op->SetBCs(bc, bc);

  // NLFV methods
  } else if (name == "nlfv: default") {
    op = Teuchos::rcp(new PDE_DiffusionNLFV(oplist, mesh)); 
    op->SetBCs(bc, bc);

  // MFD methods
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFD(oplist, mesh));
    op->SetBCs(bc, bc);
  }
  return op;
}
  

/* ******************************************************************
* Initialization of straight diffusion operator: method 2.
****************************************************************** */
Teuchos::RCP<PDE_Diffusion> PDE_DiffusionFactory::Create(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<const AmanziMesh::Mesh>& mesh)
{
  Teuchos::RCP<PDE_Diffusion> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFV(oplist, mesh));
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFD(oplist, mesh));
  }
  return op;
}
  

/* ******************************************************************
* Initialization of straight diffusion operator: method 3.
****************************************************************** */
Teuchos::RCP<PDE_Diffusion> PDE_DiffusionFactory::Create(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<Operator>& global_op)
{
  Teuchos::RCP<PDE_Diffusion> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFV(oplist, global_op));
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFD(oplist, global_op));
  }
  return op;
}


/* ******************************************************************
* Initialization of diffusion operator with gravity: method 1.
*
* With gravity, assumed vector, temporally varying density.
* Used by ATS.
****************************************************************** */
Teuchos::RCP<PDE_DiffusionWithGravity> PDE_DiffusionFactory::CreateWithGravity(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<const AmanziMesh::Mesh>& mesh,
    const Teuchos::RCP<BCs>& bc)
{
  Teuchos::RCP<PDE_DiffusionWithGravity> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFVwithGravity(oplist, mesh));
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFDwithGravity(oplist, mesh));
  }
  op->SetBCs(bc, bc);
  return op;
}


/* ******************************************************************
* Initialization of diffusion operator with gravity: method 2.
* With gravity, assumed vector, temporally varying density.
* Used by ATS.
****************************************************************** */
Teuchos::RCP<PDE_DiffusionWithGravity> PDE_DiffusionFactory::CreateWithGravity(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<Operator>& global_op,
    const Teuchos::RCP<BCs>& bc)
{
  Teuchos::RCP<PDE_DiffusionWithGravity> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFVwithGravity(oplist, global_op));
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFDwithGravity(oplist, global_op));
  }
  op->SetBCs(bc, bc);
  return op;
}


/* ******************************************************************
* Initialization of diffusion operator with gravity: method 3.
* With gravity, assumed vector, temporally varying density.
* Used by ATS.
****************************************************************** */
Teuchos::RCP<PDE_DiffusionWithGravity> PDE_DiffusionFactory::CreateWithGravity(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<const AmanziMesh::Mesh>& mesh)
{
  Teuchos::RCP<PDE_DiffusionWithGravity> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFVwithGravity(oplist, mesh));
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFDwithGravity(oplist, mesh));
  }
  return op;
}


/* ******************************************************************
* Initialization of diffusion operator with gravity: method 4.
* With gravity, assumed vector, temporally varying density.
* Used by ATS.
****************************************************************** */
Teuchos::RCP<PDE_DiffusionWithGravity> PDE_DiffusionFactory::CreateWithGravity(
    Teuchos::ParameterList& oplist,
    const Teuchos::RCP<Operator>& global_op)
{
  Teuchos::RCP<PDE_DiffusionWithGravity> op = Teuchos::null;
  std::string name = oplist.get<std::string>("discretization primary");
  
  if (name == "fv: default") {
    op = Teuchos::rcp(new PDE_DiffusionFVwithGravity(oplist, global_op));
  } else {
    op = Teuchos::rcp(new PDE_DiffusionMFDwithGravity(oplist, global_op));
  }
  return op;
}

}  // namespace Operators
}  // namespace Amanzi


