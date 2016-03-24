#include "Epetra_IntVector.h"

#include "RegionEnumerated.hh"
#include "MeshEmbeddedLogical.hh"

namespace Amanzi {
namespace AmanziMesh {

//
// MeshEmbeddedLogical Constructor
// 
// Combines two meshes, each representing their own domain, into a
// single mesh via set of specified connections.  The result is a
// logical mesh in the sense that it provides a limited interface.
//
//  - face_cell_list          : length nfaces array of length 2 arrays
//                              defining the topology
//  - face_cell_lengths       : length of the cell-to-face connection
//  - face_area_normals       : length nfaces array of normals of the
//                              face, points from cell 1 to 2 in
//                              face_cell_list topology, magnitude
//                              is area
MeshEmbeddedLogical::MeshEmbeddedLogical(const Epetra_MpiComm* comm,
        Teuchos::RCP<Mesh> bg_mesh,
        Teuchos::RCP<Mesh> log_mesh,
        const std::vector<Entity_ID_List>& face_cell_ids,
        const std::vector<std::vector<double> >& face_cell_lengths,
        const std::vector<AmanziGeometry::Point>& face_area_normals,
        const Teuchos::RCP<const VerboseObject>& verbosity_obj)
    : Mesh(verbosity_obj, true, false),
    bg_mesh_(bg_mesh),
    log_mesh_(log_mesh)
{
  set_comm(comm);
  set_space_dimension(3);
  set_manifold_dimension(3); // mixed?

  // ensure cached properties are available
  if (!bg_mesh->cell_geometry_precomputed_)
    bg_mesh->compute_cell_geometric_quantities_();
  if (!bg_mesh->face_geometry_precomputed_)
    bg_mesh->compute_face_geometric_quantities_();
  if (!bg_mesh->cell2face_info_cached_)
    bg_mesh->cache_cell2face_info_();
  if (!bg_mesh->face2cell_info_cached_)
    bg_mesh->cache_face2cell_info_();

  // ensure cached properties are available
  if (!log_mesh->cell_geometry_precomputed_)
    log_mesh->compute_cell_geometric_quantities_();
  if (!log_mesh->face_geometry_precomputed_)
    log_mesh->compute_face_geometric_quantities_();
  if (!log_mesh->cell2face_info_cached_)
    log_mesh->cache_cell2face_info_();
  if (!log_mesh->face2cell_info_cached_)
    log_mesh->cache_face2cell_info_();

  // merge and remap
  int ncells_bg_owned = bg_mesh->num_entities(CELL, OWNED);
  int ncells_bg_used = bg_mesh->num_entities(CELL, USED);
  int ncells_log = log_mesh->num_entities(CELL, OWNED);
  int ncells_my_owned = ncells_bg_owned + ncells_log;
  int ncells_my_used = ncells_bg_used + ncells_log;

  int nfaces_bg_owned = bg_mesh->num_entities(FACE, OWNED);
  int nfaces_bg_used = bg_mesh->num_entities(FACE, USED);
  int nfaces_log = log_mesh->num_entities(FACE, OWNED);
  int nfaces_extra = face_cell_ids_.size();
  int nfaces_my_owned = nfaces_bg_owned + nfaces_log + nfaces_extra;
  int nfaces_my_used = nfaces_bg_used + nfaces_log + nfaces_extra;

  //
  // face ordered things first
  // ----------------------------
  //
  // Note that new face lids are ordered:
  //  1. logical-logical connections
  //  2. logical-background connections (these are the new ones)
  //  3. background-background connections
  // 
  // face-to-cell map -- remap and add in extras
  // -- first faces are the logical-logical faces
  face_cell_ids_ = log_mesh->face_cell_ids_;
  // -- next are the logical-background faces, insert and remap
  face_cell_ids_.insert(face_cell_ids_.end(), face_cell_ids.begin(),
		      face_cell_ids.end());
  for (int f=nfaces_log; f!=face_cell_ids_.size(); ++f) {
    // all new faces are logical,bg.  must remap the bg cell
    face_cell_ids_[f][2] += ncells_log;
  }
  // -- finally the bg-bg faces, insert and remap
  face_cell_ids_.insert(face_cell_ids_.end(), bg_mesh->face_cell_ids_.begin(),
		      bg_mesh->face_cell_ids_.end());
  for (int f=nfaces_log+nfaces_extra; f!=face_cell_ids_.size(); ++f) {
    int n_cells = face_cell_ids_[f].size();
    for (int i=0; i!=n_cells; ++i) {
      face_cell_ids_[f][i] += ncells_log;
    }
  }

  // face normals -- same order: log-log, log-bg, bg-bg
  face_normal0_ = log_mesh->face_normal0_;
  face_normal0_.insert(face_normal0_.end(), face_area_normals.begin(),
		      face_area_normals.end());
  face_normal0_.insert(face_normal0_.end(), bg_mesh->face_normal0_.begin(),
		      bg_mesh->face_normal0_.end());

  face_normal1_ = log_mesh->face_normal1_;
  face_normal1_.insert(face_normal1_.end(), face_area_normals.begin(),
		      face_area_normals.end());
  // -- negate normal1, normal direction implied from log->bg
  for (int f=nfaces_log; f!=face_normal1_.size(); ++f) {
    face_normal1_[f] = -face_normal1_[f];
  }
  face_normal1_.insert(face_normal1_.end(), bg_mesh->face_normal1_.begin(),
		      bg_mesh->face_normal1_.end());

  // ptypes -- no remap.  all added faces are owned-owned
  face_cell_ptype_ = log_mesh->face_cell_ptype_;
  std::vector<Parallel_type> extra_ptypes(2, OWNED);
  for (int f=0; f!=nfaces_extra; ++f) {
    face_cell_ptype_.push_back(extra_ptypes);
  }
  face_cell_ptype_.insert(face_cell_ptype_.end(), bg_mesh->face_cell_ptype_.begin(),
		      bg_mesh->face_cell_ptype_.end());

  // areas -- no remap
  face_areas_ = log_mesh->face_areas_;
  for (int f=nfaces_log; f!=nfaces_log+nfaces_extra; ++f) {
    face_areas_.push_back(AmanziGeometry::norm(face_normal0_[f]));
  }
  face_areas_.insert(face_areas_.end(), bg_mesh->face_areas_.begin(),
		      bg_mesh->face_areas_.end());

  //
  // cell-ordered
  // ----------------------------
  //
  // cell-to-face map -- remap
  cell_face_ids_ = log_mesh->cell_face_ids_;
  cell_face_ids_.insert(cell_face_ids_.end(), bg_mesh->cell_face_ids_.begin(),
		      bg_mesh->cell_face_ids_.end());
  for (int c=ncells_log; c!=cell_face_ids_.size(); ++c) {
    int n_faces = cell_face_ids_[c].size();
    for (int i=0; i!=n_faces; ++i) {
      cell_face_ids_[c][i] += nfaces_log + nfaces_extra;
    }
  }

  // directions
  cell_face_dirs_ = log_mesh->cell_face_dirs_;
  cell_face_dirs_.insert(cell_face_dirs_.end(), bg_mesh->cell_face_dirs_.begin(),
		      bg_mesh->cell_face_dirs_.end());

  // bisectors -- special interface since the cache isn't used for Mesh
  Entity_ID_List faces;
  cell_face_bisectors_.resize(cell_face_ids_.size());
  for (int c=0; c!=ncells_log; ++c) {
    log_mesh_->cell_get_faces_and_bisectors(c, &faces, &cell_face_bisectors_[c]);
  } 
  for (int c=0; c!=ncells_bg_used; ++c) {
    bg_mesh_->cell_get_faces_and_bisectors(c, &faces, &cell_face_bisectors_[c+ncells_log]);
  }

  // now loop over new faces, adding the updates to the cell-ordered versions
  for (int f=nfaces_log; f!=nfaces_log+nfaces_extra; ++f) {
    Entity_ID c0 = face_cell_ids_[f][0];
    Entity_ID c1 = face_cell_ids_[f][1];
    cell_face_ids_[c0].push_back(f);
    cell_face_ids_[c1].push_back(f);
    cell_face_dirs_[c0].push_back(1);
    cell_face_dirs_[c1].push_back(-1);
    cell_face_bisectors_[c0].push_back(face_cell_lengths[f-nfaces_log][0]
				      / AmanziGeometry::norm(face_normal0_[f])
				      * face_normal0_[f]);
    cell_face_bisectors_[c1].push_back(face_cell_lengths[f-nfaces_log][1]
				      / AmanziGeometry::norm(face_normal1_[f])
				      * face_normal1_[f]);
  }
  
  // cell volumes, just a straightforward merge
  cell_volumes_ = log_mesh->cell_volumes_;
  cell_volumes_.insert(cell_volumes_.end(), bg_mesh->cell_volumes_.begin(),
		      bg_mesh->cell_volumes_.end());


  //
  // centroids
  // ---------------------
  if (bg_mesh->cell_centroids_.size() > 0) {
    // -- cell, straighforward merge
    cell_centroids_ = log_mesh->cell_centroids_;
    cell_centroids_.insert(cell_centroids_.end(), bg_mesh->cell_centroids_.begin(),
			bg_mesh->cell_centroids_.end());

    // -- face, take new face centroids as mean of cell centroids
    face_centroids_ = log_mesh->face_centroids_;
    for (int f=nfaces_log; f!=nfaces_log+nfaces_extra; ++f) {
      face_centroids_.push_back((cell_centroids_[face_cell_ids_[f][0]] +
				cell_centroids_[face_cell_ids_[f][1]])/2.0);
    }
    face_centroids_.insert(face_centroids_.end(), bg_mesh->face_centroids_.begin(),
			bg_mesh->face_centroids_.end());
  }
  
  // set up counts
  num_entities_owned_[CELL] = ncells_log + ncells_bg_owned;
  num_entities_owned_[FACE] = nfaces_log + nfaces_extra + nfaces_bg_owned;
  num_entities_owned_[BOUNDARY_FACE] = bg_mesh->num_entities(BOUNDARY_FACE,OWNED) +
    log_mesh->num_entities(BOUNDARY_FACE,OWNED);
  num_entities_owned_[NODE] = 0;

  num_entities_used_[CELL] = ncells_log + ncells_bg_used;
  num_entities_used_[FACE] = nfaces_log + nfaces_extra + nfaces_bg_used;
  num_entities_used_[BOUNDARY_FACE] = bg_mesh->num_entities(BOUNDARY_FACE,USED) +
    log_mesh->num_entities(BOUNDARY_FACE,USED);
  num_entities_used_[NODE] = 0;
  
  // toggle flags
  cell_geometry_precomputed_ = true;
  face_geometry_precomputed_ = true;
  cell2face_info_cached_ = true;
  faces_requested_ = true;
  face2cell_info_cached_ = true;

  // build epetra maps
  init_maps();
}  

  
// build maps
void
MeshEmbeddedLogical::init_maps() {
  // owned maps
  // -- cell map
  maps_owned_[CELL] =
    Teuchos::rcp(new Epetra_Map(-1, num_entities_owned_[CELL], 0, *comm_));

  // -- face map
  Teuchos::RCP<Epetra_Map> face_map =
    Teuchos::rcp(new Epetra_Map(-1, num_entities_owned_[FACE], 0, *comm_));
  maps_owned_[FACE] = face_map;

  // exterior face map and importer
  std::vector<int> extface_ids;
  int nfaces_owned = num_entities_owned_[FACE];
  for (int f=0; f != nfaces_owned; ++f) {
    if (face_cell_ids_[f].size() == 1) {
      extface_ids.push_back(face_map->GID(f));
    }
  }
  maps_owned_[BOUNDARY_FACE] =
    Teuchos::rcp(new Epetra_Map(-1, extface_ids.size(), &extface_ids[0], 0, *comm_));
  exterior_face_importer_ =
    Teuchos::rcp(new Epetra_Import(*maps_owned_[BOUNDARY_FACE], *face_map));  

  // ghosted maps: use the bg mesh to comm_unicate the new GIDs into their ghost values
  // CELL:
  int ncells_bg_owned = bg_mesh_->num_entities(CELL,OWNED);
  int ncells_bg_used = bg_mesh_->num_entities(CELL,USED);
  int ncells_log = log_mesh_->num_entities(CELL,OWNED);
  int ncells_my_used = num_entities(CELL,USED);
  
  // -- create a populate the owned GIDs
  Epetra_IntVector bg_gids_owned_c(bg_mesh_->cell_map(false));

  Epetra_Map& my_cell_map = *maps_owned_[CELL];
  for (int c=0; c!=ncells_bg_owned; ++c) {
    bg_gids_owned_c[c] = my_cell_map.GID(c+ncells_log);
  }

  // -- create the map from owned to used
  Epetra_Import cell_import(bg_mesh_->cell_map(true), bg_mesh_->cell_map(false));

  // -- create the used GIDs vector, comm_unicate
  Epetra_IntVector bg_gids_used_c(bg_mesh_->cell_map(true));
  bg_gids_used_c.Import(bg_gids_owned_c, cell_import, Insert);

  // -- create a GID list of the used components
  Entity_ID_List cells_my_used(ncells_my_used);
  for (int c=0; c!=ncells_log; ++c) {
    cells_my_used[c] = my_cell_map.GID(c);
  }
  for (int c=0; c!=ncells_bg_used; ++c) {
    cells_my_used[c+ncells_log] = bg_gids_used_c[c];
  }

  // -- create the map
  maps_used_[CELL] = Teuchos::rcp(new Epetra_Map(-1, ncells_my_used,
						 &cells_my_used[0], 0, *comm_));

  // FACE:
  int nfaces_bg_owned = bg_mesh_->num_entities(FACE,OWNED);
  int nfaces_bg_used = bg_mesh_->num_entities(FACE,USED);
  int nfaces_my_used = face_cell_ids_.size();
  int nfaces_log_extra = nfaces_my_used - nfaces_bg_used;
  
  // -- create a populate the owned GIDs
  Epetra_IntVector bg_gids_owned_f(bg_mesh_->face_map(false));

  Epetra_Map& my_face_map = *maps_owned_[FACE];
  for (int f=0; f!=nfaces_bg_owned; ++f) {
    bg_gids_owned_f[f] = my_face_map.GID(f+nfaces_log_extra);
  }

  // -- create the map from owned to used
  Epetra_Import face_import(bg_mesh_->face_map(true), bg_mesh_->face_map(false));

  // -- create the used GIDs vector, comm_unicate
  Epetra_IntVector bg_gids_used_f(bg_mesh_->face_map(true));
  bg_gids_used_f.Import(bg_gids_owned_f, face_import, Insert);

  // -- create a GID list of the used components
  Entity_ID_List faces_my_used(nfaces_my_used);
  for (int f=0; f!=nfaces_log_extra; ++f) {
    faces_my_used[f] = my_face_map.GID(f);
  }
  for (int f=0; f!=nfaces_bg_used; ++f) {
    faces_my_used[f+nfaces_log_extra] = bg_gids_used_f[f];
  }

  // -- create the map
  maps_used_[FACE] = Teuchos::rcp(new Epetra_Map(-1, nfaces_my_used,
						 &faces_my_used[0], 0, *comm_));

}

  
// Get parallel type of entity - OWNED, GHOST, USED (See MeshDefs.hh)
Parallel_type
MeshEmbeddedLogical::entity_get_ptype(const Entity_kind kind,
			      const Entity_ID entid) const {
  return entid < num_entities_owned_.at(kind) ? OWNED : USED;
}

// Parent entity in the source mesh if mesh was derived from another mesh
Entity_ID
MeshEmbeddedLogical::entity_get_parent(const Entity_kind kind, const Entity_ID entid) const {
  int nents_log = log_mesh_->num_entities(kind, OWNED);
  return entid < nents_log ? entid : entid - nents_log;
}

  
// Get cell type - UNKNOWN, TRI, QUAD, POLYGON, TET, PRISM, PYRAMID, HEX, POLYHED 
// See MeshDefs.hh
Cell_type
MeshEmbeddedLogical::cell_get_type(const Entity_ID cellid) const {
  return cellid < log_mesh_->num_entities(CELL, OWNED) ?
    log_mesh_->cell_get_type(entity_get_parent(CELL, cellid)) :
    bg_mesh_->cell_get_type(entity_get_parent(CELL, cellid));
}


//
// General mesh information
// -------------------------
//
// Number of entities of any kind (cell, face, node) and in a
// particular category (OWNED, GHOST, USED)
unsigned int
MeshEmbeddedLogical::num_entities(const Entity_kind kind,
				  const Parallel_type ptype) const {
  return ptype == OWNED ? num_entities_owned_.at(kind) : num_entities_used_.at(kind);
}

// Global ID of any entity
Entity_ID
MeshEmbeddedLogical::GID(const Entity_ID lid, const Entity_kind kind) const {
  return maps_used_.at(kind)->GID(lid);
}


//
// Mesh Entity Adjacencies
//-------------------------
// Downward Adjacencies
//---------------------
// Get nodes of a cell
void
MeshEmbeddedLogical::cell_get_nodes (const Entity_ID cellid, 
			     Entity_ID_List *nodeids) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}


// Get the bisectors, i.e. vectors from cell centroid to face centroids.
void
MeshEmbeddedLogical::cell_get_faces_and_bisectors (const Entity_ID cellid,
			   Entity_ID_List *faceids,
			   std::vector<AmanziGeometry::Point> *bisectors,
			   const bool ordered) const {
  if (faceids) *faceids = cell_face_ids_[cellid];
  if (bisectors) *bisectors = cell_face_bisectors_[cellid];
}
  

// Get nodes of face
// On a distributed mesh, all nodes (OWNED or GHOST) of the face
// are returned
// In 3D, the nodes of the face are returned in ccw order consistent
// with the face normal
// In 2D, nfnodes is 2
void
MeshEmbeddedLogical::face_get_nodes (const Entity_ID faceid,
			     Entity_ID_List *nodeids) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}
 

// Get nodes of edge
void
MeshEmbeddedLogical::edge_get_nodes (const Entity_ID edgeid, 
			     Entity_ID *nodeid0, Entity_ID *nodeid1) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}

// Upward adjacencies
//-------------------

// Cells of type 'ptype' connected to a node - The order of cells is
// not guaranteed to be the same for corresponding nodes on
// different processors
void
MeshEmbeddedLogical::node_get_cells (const Entity_ID nodeid,
			     const Parallel_type ptype,
			     Entity_ID_List *cellids) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}  

// Faces of type 'ptype' connected to a node - The order of faces is
// not guarnateed to be the same for corresponding nodes on
// different processors
void
MeshEmbeddedLogical::node_get_faces (const Entity_ID nodeid,
			     const Parallel_type ptype,
			     Entity_ID_List *faceids) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}  

// Get faces of ptype of a particular cell that are connected to the
// given node - The order of faces is not guarnateed to be the same
// for corresponding nodes on different processors
void
MeshEmbeddedLogical::node_get_cell_faces (const Entity_ID nodeid,
				  const Entity_ID cellid,
				  const Parallel_type ptype,
				  Entity_ID_List *faceids) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}  

// Same level adjacencies
//-----------------------

// Face connected neighboring cells of given cell of a particular ptype
// (e.g. a hex has 6 face neighbors)

// The order in which the cellids are returned cannot be
// guaranteed in general except when ptype = USED, in which case
// the cellids will correcpond to cells across the respective
// faces given by cell_get_faces
void
MeshEmbeddedLogical::cell_get_face_adj_cells(const Entity_ID cellid,
				     const Parallel_type ptype,
				     Entity_ID_List *fadj_cellids) const {
  Errors::Message mesg("Not implemented."); // this could be implemented, just not sure we need it yet. --etc
  Exceptions::amanzi_throw(mesg);
}  

// Node connected neighboring cells of given cell
// (a hex in a structured mesh has 26 node connected neighbors)
// The cells are returned in no particular order
void
MeshEmbeddedLogical::cell_get_node_adj_cells(const Entity_ID cellid,
				     const Parallel_type ptype,
				     Entity_ID_List *nadj_cellids) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}

//
// Mesh entity geometry
//--------------
//
// Node coordinates - 3 in 3D and 2 in 2D
void
MeshEmbeddedLogical::node_get_coordinates (const Entity_ID nodeid,
				   AmanziGeometry::Point *ncoord) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}  


// Face coordinates - conventions same as face_to_nodes call
// Number of nodes is the vector size divided by number of spatial dimensions
void
MeshEmbeddedLogical::face_get_coordinates (const Entity_ID faceid,
				   std::vector<AmanziGeometry::Point> *fcoords) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}

// Coordinates of cells in standard order (Exodus II convention)
// STANDARD CONVENTION WORKS ONLY FOR STANDARD CELL TYPES IN 3D
// For a general polyhedron this will return the node coordinates in
// arbitrary order
// Number of nodes is vector size divided by number of spatial dimensions
void
MeshEmbeddedLogical::cell_get_coordinates (const Entity_ID cellid,
				   std::vector<AmanziGeometry::Point> *ccoords) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}

//
// Mesh modification
//-------------------

// Set coordinates of node
void
MeshEmbeddedLogical::node_set_coordinates (const Entity_ID nodeid,
				   const AmanziGeometry::Point ncoord) {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}  

void
MeshEmbeddedLogical::node_set_coordinates (const Entity_ID nodeid,
				   const double *ncoord) {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}


// Deformation not supported.
int
MeshEmbeddedLogical::deform (const std::vector<double>& target_cell_volumes_in,
		     const std::vector<double>& min_cell_volumes_in,
		     const Entity_ID_List& fixed_nodes,
		     const bool move_vertical) {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
  return -1;
}  

//
// Epetra maps
//------------
const Epetra_Map&
MeshEmbeddedLogical::cell_map(bool include_ghost) const {
  return include_ghost ? *maps_used_.at(CELL) : *maps_owned_.at(CELL);
}

const Epetra_Map&
MeshEmbeddedLogical::face_map(bool include_ghost) const {
  return include_ghost ? *maps_used_.at(FACE) : *maps_owned_.at(FACE);
}  

const Epetra_Map&
MeshEmbeddedLogical::node_map(bool include_ghost) const {
  Errors::Message mesg("No nodes in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
  return include_ghost ? *maps_used_.at(NODE) : *maps_owned_.at(NODE);
}


const Epetra_Map&
MeshEmbeddedLogical::exterior_face_map(bool include_ghost) const {
  return *maps_owned_.at(BOUNDARY_FACE);
}

// Epetra importer that will allow apps to import values from a
// Epetra vector defined on all owned faces into an Epetra vector
// defined only on exterior faces
const Epetra_Import&
MeshEmbeddedLogical::exterior_face_importer (void) const {
  return *exterior_face_importer_;
}


//
// Mesh Sets for ICs, BCs, Material Properties and whatever else
//--------------------------------------------------------------
//
// Get number of entities of type 'category' in set
unsigned int
MeshEmbeddedLogical::get_set_size (const Set_ID setid,
			   const Entity_kind kind,
			   const Parallel_type ptype) const {
  Entity_ID_List ents;
  get_set_entities(setid, kind, ptype, &ents);
  return ents.size();
}

unsigned int
MeshEmbeddedLogical::get_set_size (const std::string setname,
			   const Entity_kind kind,
			   const Parallel_type ptype) const {
  return get_set_size(geometric_model_->FindRegion(setname)->id(),kind,ptype);
}

unsigned int
MeshEmbeddedLogical::get_set_size (const char *setname,
			   const Entity_kind kind,
			   const Parallel_type ptype) const {
  std::string name(setname);
  return get_set_size(name,kind,ptype);
}

// Get list of entities of type 'category' in set
// FIX ME: This is probably not generic enough. --etc
void
MeshEmbeddedLogical::get_set_entities (const Set_ID setid,
			       const Entity_kind kind,
			       const Parallel_type ptype,
			       Entity_ID_List *entids) const {
  Teuchos::RCP<const AmanziGeometry::Region> rgn =
      geometric_model_->FindRegion(setid);

  if (rgn->name() == "All" || rgn->name() == "all" || rgn->name() == "ALL") {
    int nent = num_entities(kind, ptype);
    entids->resize(num_entities(kind, ptype));
    for (int i=0; i!=nent; ++i) {
      (*entids)[i] = i;
    }
    return;
  }

  // ASSUMES that logical mesh is EnumeratedSets, bg mesh is all others.
  // This is bad.  --etc
  if (rgn->type() == AmanziGeometry::ENUMERATED) {
    log_mesh_->get_set_entities(setid, kind, ptype, entids);
  } else {
    bg_mesh_->get_set_entities(setid, kind, ptype, entids);

    // shift local indices
    Entity_ID shift = num_entities(kind, ptype)
      - bg_mesh_->num_entities(kind, ptype);
    for (Entity_ID_List::iterator i=entids->begin(); i!=entids->end(); ++i) {
      (*i) += shift;
    }
  }
  return;
}

void
MeshEmbeddedLogical::get_set_entities (const std::string setname,
			       const Entity_kind kind,
			       const Parallel_type ptype,
			       Entity_ID_List *entids) const {
  get_set_entities(geometric_model_->FindRegion(setname)->id(), kind, ptype, entids);
  return;
}
  
  
void
MeshEmbeddedLogical::get_set_entities (const char *setname,
			       const Entity_kind kind,
			       const Parallel_type ptype,
			       Entity_ID_List *entids) const {
  std::string name(setname);
  get_set_entities(name, kind, ptype, entids);
  return;
}

// Miscellaneous functions
void
MeshEmbeddedLogical::write_to_exodus_file(const std::string filename) const {
  // don't know how to do this! FIXME! --etc
}


// Geometry
int
MeshEmbeddedLogical::compute_cell_geometry_(const Entity_ID cellid, 
				   double *volume, 
				   AmanziGeometry::Point *centroid) const {
  // this is a placeholder, these cannot be recomputed
  if (volume) *volume = cell_volumes_[cellid];

  if (centroid) {
    if (cell_centroids_.size() > 0) {
      *centroid = cell_centroids_[cellid];
    } else {
      *centroid = AmanziGeometry::Point();
    }
  }
  return 1;
}


int
MeshEmbeddedLogical::compute_face_geometry_(const Entity_ID faceid, 
				   double *area, 
				   AmanziGeometry::Point *centroid, 
				   AmanziGeometry::Point *normal0,
				   AmanziGeometry::Point *normal1) const {
  // this is a placeholder, these cannot be recomputed
  if (area) *area = face_areas_[faceid];
  if (centroid) *centroid = AmanziGeometry::Point();
  if (normal0) *normal0 = face_normal0_[faceid];
  if (normal1) *normal1 = face_normal1_[faceid];
  return 1;
}
  
  
// get faces of a cell and directions in which it is used - this function
// is implemented in each mesh framework. The results are cached in 
// the base class
void
MeshEmbeddedLogical::cell_get_faces_and_dirs_internal_ (const Entity_ID cellid,
					       Entity_ID_List *faceids,
					       std::vector<int> *face_dirs,
					       const bool ordered) const {
  Errors::Message mesg("DEVELOPER ERROR: cell_get_faces_and_dirs_internal_() should not be called");
  Exceptions::amanzi_throw(mesg);
}

// Cells connected to a face - this function is implemented in each
// mesh framework. The results are cached in the base class
void
MeshEmbeddedLogical::face_get_cells_internal_ (const Entity_ID faceid,
				      const Parallel_type ptype,
				      Entity_ID_List *cellids) const {
  Errors::Message mesg("DEVELOPER ERROR: face_get_cells_internal_() should not be called");
  Exceptions::amanzi_throw(mesg);
}

// edges of a face - this function is implemented in each mesh
// framework. The results are cached in the base class
void
MeshEmbeddedLogical::face_get_edges_and_dirs_internal_ (const Entity_ID faceid,
					       Entity_ID_List *edgeids,
					       std::vector<int> *edge_dirs,
					       const bool ordered) const {
  Errors::Message mesg("No edges in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}
  

// edges of a cell - this function is implemented in each mesh
// framework. The results are cached in the base class. 
void
MeshEmbeddedLogical::cell_get_edges_internal_ (const Entity_ID cellid,
				      Entity_ID_List *edgeids) const {
  Errors::Message mesg("No edges in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}
  

// edges and directions of a 2D cell - this function is implemented
// in each mesh framework. The results are cached in the base class.
void
MeshEmbeddedLogical::cell_2D_get_edges_and_dirs_internal_ (const Entity_ID cellid,
						  Entity_ID_List *edgeids,
						  std::vector<int> *edge_dirs) const {
  Errors::Message mesg("No edges in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}
  

int
MeshEmbeddedLogical::build_columns_() const {
  Errors::Message mesg("No columns are buildable in MeshEmbeddedLogical.");
  Exceptions::amanzi_throw(mesg);
}


// Cache connectivity info.
void
MeshEmbeddedLogical::cache_cell2face_info_() const {
  Errors::Message mesg("DEVELOPER ERROR: cache should be created in finalize()");
  Exceptions::amanzi_throw(mesg);
}

void
MeshEmbeddedLogical::cache_face2cell_info_() const {
  Errors::Message mesg("DEVELOPER ERROR: cache should be created in finalize()");
  Exceptions::amanzi_throw(mesg);
}
  
  
int
MeshEmbeddedLogical::compute_cell_geometric_quantities_() const {
  Errors::Message mesg("DEVELOPER ERROR: cache should be created in finalize()");
  Exceptions::amanzi_throw(mesg);
}

int
MeshEmbeddedLogical::compute_face_geometric_quantities_() const {
  Errors::Message mesg("DEVELOPER ERROR: cache should be created in finalize()");
  Exceptions::amanzi_throw(mesg);
}
  
} // close namespace AmanziMesh
} // close namespace Amanzi