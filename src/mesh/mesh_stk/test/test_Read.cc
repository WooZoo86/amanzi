// -------------------------------------------------------------
/**
 * @file   test_Read.cc
 * @author William A. Perkins
 * @date Wed May 18 14:15:54 2011
 * 
 * @brief Some unit tests for reading a (serial) Exodus file and
 * building a STK_mesh::Mesh_maps_stk instance.
 * 
 * 
 */

// -------------------------------------------------------------
// -------------------------------------------------------------
// Created November 22, 2010 by William A. Perkins
// Last Change: Wed May 18 14:15:54 2011 by William A. Perkins <d3g096@PE10900.pnl.gov>
// -------------------------------------------------------------

#include <sstream>
#include <UnitTest++.h>
#include <Epetra_MpiComm.h>

#include "Exodus_readers.hh"
#include "Parallel_Exodus_file.hh"
#include "../Mesh_STK_factory.hh"
#include "Auditor.hh"

// -------------------------------------------------------------
// class ReadFixture
// -------------------------------------------------------------
class ReadFixture {
 protected: 

  static const bool verbose;
  virtual void my_read(const std::string& name) = 0;

 public:

  Epetra_MpiComm comm_;
  const int nproc;

  Amanzi::AmanziMesh::STK::Mesh_STK_factory mf;
  Amanzi::AmanziMesh::Data::Fields nofields;
  Teuchos::RCP<Amanzi::AmanziMesh::Data::Data> meshdata;
  Amanzi::AmanziMesh::STK::Mesh_STK_Impl_p mesh;
  // Teuchos::RCP<Amanzi::AmanziMesh::STK::Mesh_maps_stk> maps;
  
  /// Default constructor.
  ReadFixture(void)
      : comm_(MPI_COMM_WORLD),
        nproc(comm_.NumProc()),
        mf(&comm_, 1000)
  { }

  void read(const std::string& name)
  {
    int ierr(0);
    try { 
      my_read(name);
      // maps.reset(new Amanzi::AmanziMesh::STK::Mesh_maps_stk(mesh));
    } catch (const std::exception& e) {
      std::cerr << comm_.MyPID() << ": error: " << e.what() << std::endl;
      ierr++;
    }
    int aerr(0);
    comm_.SumAll(&ierr, &aerr, 1);

    if (aerr) {
      throw std::runtime_error("Problem in ReadFixture");
    }
  }
  
};

const bool ReadFixture::verbose(false);


// -------------------------------------------------------------
//  class SerialReadFixture
// -------------------------------------------------------------
class SerialReadFixture : public ReadFixture {
 protected:
  
  void my_read(const std::string& name)         
  {
    meshdata.reset(Amanzi::Exodus::read_exodus_file(name.c_str()));
    // meshdata->to_stream(std::cerr, verbose);
    mesh.reset(mf.build_mesh(*meshdata, nofields, Teuchos::null));
  }

 public:

  /// Default constructor.
  SerialReadFixture(void) 
      : ReadFixture()
  {}

};

// -------------------------------------------------------------
//  class ParallelReadFixture
// -------------------------------------------------------------
class ParallelReadFixture : public ReadFixture {
 protected:
  
  void my_read(const std::string& name) 
  {
    Amanzi::Exodus::Parallel_Exodus_file thefile(comm_, name);
    meshdata = thefile.read_mesh();

    // Must check/verify, not just print
    // for (int p = 0; p < nproc; p++) {
    //   if (comm_.MyPID() == p) {
    //     std::cerr << std::endl;
    //     std::cerr << ">>>>>> Process " << p << " Begin <<<<<<" << std::endl;
    //     meshdata->to_stream(std::cerr, verbose);
    //     std::cerr << ">>>>>> Process " << p << " End <<<<<<" << std::endl;
    //     std::cerr << std::endl;
    //   }
    //   comm_.Barrier();
    // }

    Teuchos::RCP<Epetra_Map> cmap(thefile.cellmap());
    Teuchos::RCP<Epetra_Map> vmap(thefile.vertexmap());
    
    // Must check/verify, not just print
    //    if (verbose) {
    //      cmap->Print(std::cerr);
    //      vmap->Print(std::cerr);
    //    }
    mesh.reset(mf.build_mesh(*meshdata, *cmap, *vmap, nofields, Teuchos::null));
  }

 public:

  /// Default constructor.
  ParallelReadFixture(void) 
      : ReadFixture()
  {}

};

  

SUITE (Exodus)
{
  TEST (Processes) 
  {
    Epetra_MpiComm comm_(MPI_COMM_WORLD);
    const int nproc(comm_.NumProc());
    CHECK(nproc >= 1 && nproc <= 4);
  }

  TEST_FIXTURE (SerialReadFixture, SerialReader1)
  {
    if (nproc == 1) {
      read("../exodus_reader/test_files/hex_10x10x10_ss.exo");
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED), 11*11*11);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE), Amanzi::AmanziMesh::OWNED), 10*10*11*3);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED), 10*10*10);

      // We will check entity sets in a separate test
      // These routines are not supported any more
      // CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 3);
      // CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE)), 20);
      // CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE)), 20);

      // stk::mesh::Part *p;
      // int count;

      // p = mesh->get_set(1, mesh->kind_to_rank(Amanzi::AmanziMesh::FACE));
      // CHECK(p != NULL);
      // count = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // CHECK_EQUAL(count, 600);
            
      // p = mesh->get_set("element block 20000", mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      // CHECK(p != NULL);
      // count = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // CHECK_EQUAL(count, 9);
            
      // p = mesh->get_set("node set 103", mesh->kind_to_rank(Amanzi::AmanziMesh::NODE));
      // CHECK(p != NULL);
      // count = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // CHECK_EQUAL(count, 121);
            
      // Disable until STKmesh can give us contiguous endity IDs

      //      Auditor audit("hex_10x10x10_ss_", mesh);
      //      audit();
    }
  }

  TEST_FIXTURE (SerialReadFixture, PrismReader)
  {
    if (nproc == 1) {
      read("../exodus_reader/test_files/prism.exo");
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED), 1920);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED), 2634);

      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 1);

     //  Disable until STKmesh can give us contiguous entity IDs

     //       Auditor audit("prism_", mesh);
     //        audit();
    }

  }
        
  TEST_FIXTURE (SerialReadFixture, MixedCoarseReader)
  {
    if (nproc == 1) {
      read("../exodus_reader/test_files/mixed-coarse.exo");
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED), 361);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED), 592);
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 5);

     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("mixed-coarse_", mesh);
     //   audit();
    }

  }


  TEST_FIXTURE (SerialReadFixture, MixedReader)
  {
    if (nproc == 1) {
      read("../exodus_reader/test_files/mixed.exo");
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED), 6495);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED), 23186);
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 6);

     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("mixed_", mesh);
     //   audit();
    }

  }



  TEST_FIXTURE (SerialReadFixture, SerialReader2)
  {
    if (nproc == 1) {
      read("../exodus_reader/test_files/hex_3x3x3_ss.exo");
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED), 4*4*4);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE), Amanzi::AmanziMesh::OWNED), 3*3*4*3);
      CHECK_EQUAL(mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED), 3*3*3);

      // Check entity sets in a separate test
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 3);
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE)), 21);
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE)), 21);

      //      stk::mesh::Part *p;
      // int count;

      // p = mesh->get_set("side set 1", mesh->kind_to_rank(Amanzi::AmanziMesh::FACE));
      // CHECK(p != NULL);
      // count = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // CHECK_EQUAL(count, 54);
            
      // p = mesh->get_set("element block 20000", mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      // CHECK(p != NULL);
      // count = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // CHECK_EQUAL(count, 9);
            
      // p = mesh->get_set("node set 103", mesh->kind_to_rank(Amanzi::AmanziMesh::NODE));
      // CHECK(p != NULL);
      // count = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // CHECK_EQUAL(count, 16);
            
     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("hex_3x3x3_ss_", mesh);
     //   audit();
    }
  }

  TEST_FIXTURE (ParallelReadFixture, ParallelReader1)
  {
    if (nproc > 1 && nproc <= 4) {
      read("../exodus_reader/test_files/split1/hex_10x10x10_ss.par");
      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 3);
      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE)), 20);
      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE)), 20);
      int local, global;
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 11*11*11);
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 10*10*11*3);
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 10*10*10);


      // Check sets in a separate test

      // stk::mesh::Part *p;

      // p = mesh->get_set("side set 1", mesh->kind_to_rank(Amanzi::AmanziMesh::FACE));
      // CHECK(p != NULL);
      // local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // comm_.SumAll(&local, &global, 1);
      // CHECK_EQUAL(global, 600);
            
      // p = mesh->get_set("element block 20000", mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      // CHECK(p != NULL);
      // local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // comm_.SumAll(&local, &global, 1);
      // CHECK_EQUAL(global, 9);

      // p = mesh->get_set("node set 103", mesh->kind_to_rank(Amanzi::AmanziMesh::NODE));
      // CHECK(p != NULL);
      // local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // comm_.SumAll(&local, &global, 1);
      // CHECK_EQUAL(global, 121);

     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("hex_10x10x10_ss.par", mesh);
     //   audit();
    }
  }            

  TEST_FIXTURE (ParallelReadFixture, ParallelReader2)
  {
    if (nproc > 1 && nproc < 4) {
      read("../exodus_reader/test_files/split1/hex_3x3x3_ss.par");

      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 3);
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE)), 21);
      //      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE)), 21);

      int local, global;
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 4*4*4);
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::FACE), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 3*3*4*3);
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 3*3*3);

      //      stk::mesh::Part *p;
      //
      //      p = mesh->get_set(1, mesh->kind_to_rank(Amanzi::AmanziMesh::FACE));
      //      CHECK(p != NULL);
      // local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // comm_.SumAll(&local, &global, 1);
      // CHECK_EQUAL(global, 54);
            
      // p = mesh->get_set(20000, mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      // CHECK(p != NULL);
      // local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // comm_.SumAll(&local, &global, 1);
      // CHECK_EQUAL(global, 9);

      // p = mesh->get_set(103, mesh->kind_to_rank(Amanzi::AmanziMesh::NODE));
      // CHECK(p != NULL);
      // local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      // comm_.SumAll(&local, &global, 1);
      // CHECK_EQUAL(global, 16);

     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("hex_3x3x3_ss.par", mesh);
     //   audit();
    }
  }            


  TEST_FIXTURE (ParallelReadFixture, PrismParallelReader)
  {
    if (nproc == 2 || nproc == 4) {
      read("../exodus_reader/test_files/split1/prism.par");
      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 1);
      int local, global;
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 1920);
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 2634);

      stk::mesh::Part *p;

      p = mesh->get_set(1, mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      CHECK(p != NULL);
      local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 2634);

     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("prism.par.", mesh);
     //   audit();
    }
  }            

  TEST_FIXTURE (ParallelReadFixture, MixedCoarseParallelReader)
  {
    if (nproc == 2 || nproc == 4) {
      read("../exodus_reader/test_files/split1/mixed-coarse.par");
      CHECK_EQUAL(mesh->num_sets(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL)), 5);
      int local, global;
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::NODE), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 361);
      local = mesh->count_entities(mesh->kind_to_rank(Amanzi::AmanziMesh::CELL), Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 592);

      stk::mesh::Part *p;

      p = mesh->get_set(1, mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      CHECK(p != NULL);
      local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 120);

      p = mesh->get_set(2, mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      CHECK(p != NULL);
      local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 48);

      p = mesh->get_set(3, mesh->kind_to_rank(Amanzi::AmanziMesh::CELL));
      CHECK(p != NULL);
      local = mesh->count_entities(*p, Amanzi::AmanziMesh::OWNED);
      comm_.SumAll(&local, &global, 1);
      CHECK_EQUAL(global, 48);

     //  Disable until STKmesh can give us contiguous entity IDs

     //   Auditor audit("mixed-coarse.par.", mesh);
     //   audit();
    }
  }            

  TEST (DirectReader) 
  {
    Epetra_MpiComm comm_(MPI_COMM_WORLD);

    // FIXME: need to be able to assign path from configuration

    std::string fpath("../exodus_reader/test_files/");
    std::string fname("hex_10x10x10_ss");
    if (comm_.NumProc() == 1) {
      fname += ".exo";
    } else {
      fpath += "split1/";
      fname += ".par";
    }
    fname = fpath + fname;

    Teuchos::RCP<Amanzi::AmanziMesh::Mesh> 
      mesh(new Amanzi::AmanziMesh::Mesh_STK(&comm_, fname.c_str()));

     //  Disable until STKmesh can give us contiguous entity IDs

    // Auditor audit("stk_mesh_read_", mesh);
    // audit();
  }
      
} 

