/*
This is the flow component of the Amanzi code. 

Copyright 2010-2012 held jointly by LANS/LANL, LBNL, and PNNL. 
Amanzi is released under the three-clause BSD License. 
The terms of use and "as is" disclaimer for this license are 
provided in the top-level COPYRIGHT file.

Authors: Neil Carlson (nnc@lanl.gov), 
         Konstantin Lipnikov (lipnikov@lanl.gov)
*/

#include <set>
#include <vector>
#include <string>

#include "Teuchos_ParameterList.hpp"

#include "Flow_constants.hh"
#include "Richards_PK.hh"

namespace Amanzi {
namespace AmanziFlow {

/* ******************************************************************
* Routine processes parameter list. It needs to be called only once
* on each processor.                                                     
****************************************************************** */
void Richards_PK::ProcessParameterList()
{
  Errors::Message msg;

  // create verbosity list if it does not exist
  if (! rp_list_.isSublist("VerboseObject")) {
    Teuchos::ParameterList verbosity_list;
    verbosity_list.set<std::string>("Verbosity Level", "none");
    rp_list_.set("VerboseObject", verbosity_list);
  }

  // extract verbosity level
  Teuchos::ParameterList verbosity_list = rp_list_.get<Teuchos::ParameterList>("VerboseObject");
  std::string verbosity_name = verbosity_list.get<std::string>("Verbosity Level");
  ProcessStringVerbosity(verbosity_name, &verbosity);

  // Process main one-line options (not sublists)
  atm_pressure = rp_list_.get<double>("atmospheric pressure", FLOW_PRESSURE_ATMOSPHERIC);
 
  string mfd3d_method_name = rp_list_.get<string>("discretization method", "optimized mfd");
  ProcessStringMFD3D(mfd3d_method_name, &mfd3d_method_); 

  // Create the BC objects.
  Teuchos::RCP<Teuchos::ParameterList>
      bc_list = Teuchos::rcp(new Teuchos::ParameterList(rp_list_.sublist("boundary conditions", true)));
  FlowBCFactory bc_factory(mesh_, bc_list);

  bc_pressure = bc_factory.CreatePressure(bc_submodel);
  bc_head = bc_factory.CreateStaticHead(atm_pressure, rho_, gravity_, bc_submodel);
  bc_flux = bc_factory.CreateMassFlux(bc_submodel);
  bc_seepage = bc_factory.CreateSeepageFace(bc_submodel);

  ValidateBoundaryConditions(bc_pressure, bc_head, bc_flux);
  ProcessStaticBCsubmodels(bc_submodel, rainfall_factor);

  // Create the source object if any
  if (rp_list_.isSublist("source terms")) {
    string distribution_method_name = rp_list_.get<string>("source and sink distribution method", "none");
    ProcessStringSourceDistribution(distribution_method_name, &src_sink_distribution); 

    Teuchos::RCP<Teuchos::ParameterList> src_list = Teuchos::rcpFromRef(rp_list_.sublist("source terms", true));
    FlowSourceFactory src_factory(mesh_, src_list);
    src_sink = src_factory.createSource();
    src_sink_distribution = src_sink->CollectActionsList();
  }  

  // experimental solver (NKA is default)
  string experimental_solver_name = rp_list_.get<string>("experimental solver", "nka");
  ProcessStringExperimentalSolver(experimental_solver_name, &experimental_solver_);

  // Create water retention models
  if (! rp_list_.isSublist("Water retention models")) {
    msg << "Flow PK: there is no \"Water retention models\" list";
    Exceptions::amanzi_throw(msg);
  }
  Teuchos::ParameterList& wrm_list = rp_list_.sublist("Water retention models");
  rel_perm = Teuchos::rcp(new RelativePermeability(mesh_, wrm_list));
  rel_perm->Init(atm_pressure, FS_aux);
  rel_perm->ProcessParameterList();
  rel_perm->PopulateMapC2MB();
  rel_perm->set_experimental_solver(experimental_solver_);

  std::string krel_method_name = rp_list_.get<string>("relative permeability");
  rel_perm->ProcessStringRelativePermeability(krel_method_name);
 

  // Time integrator for period I, temporary called initial guess initialization
  if (rp_list_.isSublist("initial guess pseudo time integrator")) {
    Teuchos::ParameterList& igs_list = rp_list_.sublist("initial guess pseudo time integrator");

    std::string ti_method_name = igs_list.get<string>("time integration method", "none");
    ProcessStringTimeIntegration(ti_method_name, &ti_specs_igs_.ti_method);
    ProcessSublistTimeIntegration(igs_list, ti_method_name, ti_specs_igs_);
    ti_specs_igs_.ti_method_name = "initial guess pseudo time integrator";

    ti_specs_igs_.preconditioner_name = FindStringPreconditioner(igs_list);
    ProcessStringPreconditioner(ti_specs_igs_.preconditioner_name, &ti_specs_igs_.preconditioner_method);

    std::string linear_solver_name = FindStringLinearSolver(igs_list, solver_list_);
    ProcessStringLinearSolver(linear_solver_name, &ti_specs_igs_.ls_specs);

    ProcessStringPreconditioner(ti_specs_igs_.preconditioner_name, &ti_specs_igs_.preconditioner_method);
    ProcessStringErrorOptions(igs_list, &ti_specs_igs_.error_control_options);
  }

  // Time integrator for period II, temporary called steady-state time integrator
  if (rp_list_.isSublist("steady state time integrator")) {
    Teuchos::ParameterList& sss_list = rp_list_.sublist("steady state time integrator");

    std::string ti_method_name = sss_list.get<string>("time integration method", "none");
    ProcessStringTimeIntegration(ti_method_name, &ti_specs_sss_.ti_method);
    ProcessSublistTimeIntegration(sss_list, ti_method_name, ti_specs_sss_);
    ti_specs_sss_.ti_method_name = "steady state time integrator";

    ti_specs_sss_.preconditioner_name = FindStringPreconditioner(sss_list);
    ProcessStringPreconditioner(ti_specs_sss_.preconditioner_name, &ti_specs_sss_.preconditioner_method);

    std::string linear_solver_name = FindStringLinearSolver(sss_list, solver_list_);
    ProcessStringLinearSolver(linear_solver_name, &ti_specs_sss_.ls_specs);

    ProcessStringPreconditioner(ti_specs_sss_.preconditioner_name, &ti_specs_sss_.preconditioner_method);
    ProcessStringErrorOptions(sss_list, &ti_specs_sss_.error_control_options);

  } else if (verbosity >= FLOW_VERBOSITY_LOW) {
    printf("Flow PK: mandatory sublist for steady-state calculations is missing.\n");
  }

  // Time integrator for period III, called transient time integrator
  if (rp_list_.isSublist("transient time integrator")) {
    Teuchos::ParameterList& trs_list = rp_list_.sublist("transient time integrator");

    string ti_method_name = trs_list.get<string>("time integration method", "none");
    ProcessStringTimeIntegration(ti_method_name, &ti_specs_trs_.ti_method);
    ProcessSublistTimeIntegration(trs_list, ti_method_name, ti_specs_trs_);
    ti_specs_trs_.ti_method_name = "transient time integrator";

    ti_specs_trs_.preconditioner_name = FindStringPreconditioner(trs_list);
    ProcessStringPreconditioner(ti_specs_trs_.preconditioner_name, &ti_specs_trs_.preconditioner_method);

    std::string linear_solver_name = FindStringLinearSolver(trs_list, solver_list_);
    ProcessStringLinearSolver(linear_solver_name, &ti_specs_trs_.ls_specs);

    ProcessStringPreconditioner(ti_specs_trs_.preconditioner_name, &ti_specs_trs_.preconditioner_method);
    ProcessStringErrorOptions(trs_list, &ti_specs_trs_.error_control_options);

  } else if (verbosity >= FLOW_VERBOSITY_LOW) {
    printf("Flow PK: missing sublist \"transient time integrator\".\n");
  }

  // allowing developer to use non-standard simulation modes
  if (! rp_list_.isParameter("developer access granted")) AnalysisTI_Specs();

  // optional debug output
  CalculateWRMcurves(rp_list_);

  if (verbosity >= FLOW_VERBOSITY_EXTREME) {
    if (MyPID == 0) rp_list_.unused(std::cout);
  }
}


/* ****************************************************************
* Process string for error control options
**************************************************************** */
void Richards_PK::ProcessStringErrorOptions(Teuchos::ParameterList& list, int* control)
{
  *control = 0;
  if (list.isParameter("error control options")){
    std::vector<std::string> options;
    options = list.get<Teuchos::Array<std::string> >("error control options").toVector();

    for (int i=0; i < options.size(); i++) {
      if (options[i] == "pressure") {
        *control += FLOW_TI_ERROR_CONTROL_PRESSURE;
      } else if (options[i] == "saturation") {
        *control += FLOW_TI_ERROR_CONTROL_SATURATION;
      } else if (options[i] == "residual") {
        *control += FLOW_TI_ERROR_CONTROL_RESIDUAL;
      } else {
        Errors::Message msg;
        msg << "Flow PK: unknown error control option has been specified.";
        Exceptions::amanzi_throw(msg);
      }
    }
  }
}


/* ****************************************************************
* Process string for the linear solver.
**************************************************************** */
void Richards_PK::ProcessStringLinearSolver(const std::string name, LinearSolver_Specs* ls_specs)
{
  Errors::Message msg;

  if (! solver_list_.isSublist(name)) {
    msg << "Flow PK: linear solver does not exist for a time integrator.";
    Exceptions::amanzi_throw(msg);
  }

  Teuchos::ParameterList& tmp_list = solver_list_.sublist(name);
  ls_specs->max_itrs = tmp_list.get<int>("maximum number of iterations", 100);
  ls_specs->convergence_tol = tmp_list.get<double>("error tolerance", 1e-14);

  ls_specs->preconditioner_name = FindStringPreconditioner(tmp_list);
  ProcessStringPreconditioner(ls_specs->preconditioner_name, &ls_specs->preconditioner_method);

  if (MyPID == 0 && verbosity >= FLOW_VERBOSITY_LOW) {
    std::printf("Flow PK: LS preconditioner \"%s\" will be replaced by TI preconditioner.\n", 
        ls_specs->preconditioner_name.c_str());
  }
}


/* ****************************************************************
* Process string for the relative permeability
**************************************************************** */
void Richards_PK::ProcessStringExperimentalSolver(const std::string name, int* method)
{
  if (name == "newton") {
    *method = AmanziFlow::FLOW_SOLVER_NEWTON;
  } else if (name == "picard-newton") {
    *method = AmanziFlow::FLOW_SOLVER_PICARD_NEWTON;
  } else {
    *method = AmanziFlow::FLOW_SOLVER_NKA;
  }
}


/* ****************************************************************
* Find string for the preconditoner.
**************************************************************** */
std::string Richards_PK::FindStringPreconditioner(const Teuchos::ParameterList& list)
{   
  Errors::Message msg;
  std::string name; 

  if (list.isParameter("preconditioner")) {
    name = list.get<string>("preconditioner");
  } else {
    msg << "Flow PK: parameter <preconditioner> is missing either in TI or LS list.";
    Exceptions::amanzi_throw(msg);
  }

  if (! preconditioner_list_.isSublist(name)) {
    msg << "Flow PK: preconditioner \"" << name.c_str() << "\" does not exist.";
    Exceptions::amanzi_throw(msg);
  }
  return name;
}


/* ****************************************************************
* Find string for the preconditoner.
**************************************************************** */
void Richards_PK::CalculateWRMcurves(Teuchos::ParameterList& list)
{
  std::vector<Teuchos::RCP<WaterRetentionModel> >& WRM = rel_perm->WRM();
  
  if (MyPID == 0 && verbosity >= FLOW_VERBOSITY_MEDIUM) {
    if (list.isParameter("calculate krel-pc curves")) {
      std::printf("Flow PK: saving krel-pc curves in file flow_krel_pc.txt...\n");
      ofstream ofile;
      ofile.open("flow_krel_pc.txt");

      std::vector<double> spe;
      spe = list.get<Teuchos::Array<double> >("calculate krel-pc curves").toVector();

      for (double pc = spe[0]; pc < spe[2]; pc += spe[1]) {
        ofile << pc << " ";
        for (int mb = 0; mb < WRM.size(); mb++) {
          double krel = WRM[mb]->k_relative(pc);
          ofile << krel << " ";
        }
        ofile << endl;
      }
      ofile.close();
    }

    if (list.isParameter("calculate krel-sat curves")) {
      std::printf("Flow PK: saving krel-sat curves in file flow_krel_sat.txt...\n");
      ofstream ofile;
      ofile.open("flow_krel_sat.txt");

      std::vector<double> spe;
      spe = list.get<Teuchos::Array<double> >("calculate krel-sat curves").toVector();

      for (double s = spe[0]; s < spe[2]; s += spe[1]) {
        ofile << s << " ";
        for (int mb = 0; mb < WRM.size(); mb++) {
          double ss = std::max(s, WRM[mb]->residualSaturation());
          double pc = WRM[mb]->capillaryPressure(ss);
          double krel = WRM[mb]->k_relative(pc);
          ofile << krel << " ";
        }
        ofile << endl;
      }
      ofile.close();
    }
  }
}


/* ****************************************************************
* Analyzes time integration specs for logical consistency.
**************************************************************** */
void Richards_PK::AnalysisTI_Specs()
{
  Errors::Message msg;
  if (ti_specs_igs_.initialize_with_darcy) {
    if (ti_specs_sss_.initialize_with_darcy || ti_specs_trs_.initialize_with_darcy) { 
      msg << "Flow PK: cannot re-initialize pressure without developer password.";
      Exceptions::amanzi_throw(msg);
    }
  }

  if (ti_specs_igs_.dT_method == FLOW_DT_ADAPTIVE || 
      ti_specs_sss_.dT_method == FLOW_DT_ADAPTIVE) {
    msg << "Flow PK: adaptive time stepping is allowed only for transient TI phase.";
    Exceptions::amanzi_throw(msg);
  }
}


/* ****************************************************************
* Prints information about status of this PK. 
**************************************************************** */
void Richards_PK::PrintStatistics() const
{
  int method = rel_perm->method();
  if (MyPID == 0) {
    cout << "Flow PK:" << endl;
    cout << "  Verbosity level = " << verbosity << endl;
    cout << "  Upwind = " << ((method == FLOW_RELATIVE_PERM_UPWIND_GRAVITY) ? "gravity" : "other") << endl;
  }
}


/* ****************************************************************
* Prints information about CPU spend by this PK. 
**************************************************************** */
void Richards_PK::PrintStatisticsCPU()
{
  if (verbosity >= FLOW_VERBOSITY_HIGH) {
    timer.parSync(MPI_COMM_WORLD);
    if (MyPID == 0) timer.print();
  }
}

}  // namespace AmanziFlow
}  // namespace Amanzi

