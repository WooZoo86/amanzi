// Emacs Mode Line: -*- Mode:c++;-*-
// -------------------------------------------------------------
// file: MeshDefs.hh
// -------------------------------------------------------------
/**
 * @file   MeshDefs.hh
 * @author William A. Perkins
 * @date Mon May  2 13:03:23 2011
 * 
 * @brief  Various definitions needed by Mesh
 * 
 * 
 */
// -------------------------------------------------------------
// Created May  2, 2011 by William A. Perkins
// Last Change: Mon May  2 13:03:23 2011 by William A. Perkins <d3g096@PE10900.pnl.gov>
// -------------------------------------------------------------

// SCCS ID: $Id$ Battelle PNL

#ifndef _MeshDefs_hh_
#define _MeshDefs_hh_

#include <vector>
#include <string>
#include "boost/algorithm/string.hpp"

#include "errors.hh"
#include "GeometryDefs.hh"

namespace Amanzi {
namespace AmanziMesh {

// Necessary typedefs and enumerations
  using AmanziGeometry::Entity_ID;
  using AmanziGeometry::Entity_ID_List;
  using AmanziGeometry::Set_ID;
  using AmanziGeometry::Set_ID_List;
  

  
// Mesh Type

enum Mesh_type
{
  RECTANGULAR,   // Equivalent of structured but can't use i,j,k notation
  GENERAL        // general unstructured
};

// Cells (aka zones/elements) are the highest dimension entities in a mesh 
// Nodes (aka vertices) are lowest dimension entities in a mesh 
// Faces in a 3D mesh are 2D entities, in a 2D mesh are 1D entities
// BOUNDARY_FACE is a special type of entity that is need so that process 
// kernels can define composite vectors (see src/data_structures) on 
// exterior boundary faces of the mesh only
enum Entity_kind 
{
  NODE = 0,
  EDGE,
  FACE,
  CELL,
  BOUNDARY_FACE
};

// Check if Entity_kind is valid
inline 
bool entity_valid_kind (const Entity_kind kind) {
  return (kind >= NODE && kind <= CELL);
}

// entity kind from string
inline
Entity_kind entity_kind(const std::string& instring)
{
  std::string estring = instring; // note not done in signature to throw a better error
  boost::algorithm::to_lower(estring);
  if (estring == "cell") return CELL;
  else if (estring == "face") return FACE;
  else if (estring == "boundary_face") return BOUNDARY_FACE;
  else if (estring == "edge") return EDGE;
  else if (estring == "node") return NODE;
  else {
    Errors::Message msg;
    msg << "Unknown entity kind string: \"" << instring << "\", valid are \"cell\", \"face\", \"boundary_face\", \"edge\", and \"node\".";
    Exceptions::amanzi_throw(msg);
    return NODE;
  }
}


// Parallel status of entity 
    
enum Parallel_type 
{
  PTYPE_UNKNOWN = 0, // Initializer
  OWNED = 1,         // Owned by this processor
  GHOST = 2,         // Owned by another processor
  USED  = 3          // OWNED + GHOST
};

// Check if Parallel_type is valid

inline 
bool entity_valid_ptype (const Parallel_type ptype) {
  return (ptype >= OWNED && ptype <= USED);
}
    
// Standard element types and catchall (POLYGON/POLYHED)

enum Cell_type {
  CELLTYPE_UNKNOWN = 0,
  TRI = 1,
  QUAD,
  POLYGON,
  TET,
  PRISM,
  PYRAMID,
  HEX,
  POLYHED  // Polyhedron 
};
    
// Check if Cell_type is valid
inline 
bool cell_valid_type (const Cell_type type) {
  return (type >= TRI && type <= POLYHED); 
}

// Types of partitioners (partitioning scheme bundled into the name)
enum class Partitioner_type : std::uint8_t {
  METIS,
  ZOLTAN_GRAPH,
  ZOLTAN_RCB
};

constexpr int NUM_PARTITIONER_TYPES = 3;
constexpr Partitioner_type PARTITIONER_DEFAULT = Partitioner_type::METIS;

// Return an string description for each partitioner type
inline
std::string Partitioner_type_string(const Partitioner_type partitioner_type) {
  static std::string partitioner_type_str[NUM_PARTITIONER_TYPES] =
      {"Partitioner_type::METIS",
       "Partitioner_type::ZOLTAN_GRAPH", "Partitioner_type::ZOLTAN_RCB"};

  int iptype = static_cast<int>(partitioner_type);
  return (iptype >= 0 && iptype < NUM_PARTITIONER_TYPES) ?
      partitioner_type_str[iptype] : "";
}

// Output operator for Partitioner_type
inline
std::ostream& operator<<(std::ostream& os,
                         const Partitioner_type& partitioner_type) {
  os << " " << Partitioner_type_string(partitioner_type) << " ";
  return os;
}

// Types of partitioning algorithms - Add as needed in the format METIS_RCB etc.
enum class Partitioning_scheme {DEFAULT};
  
}  // namespace Amanzi 
}  // namespace AmanziMesh



#endif
