/**
 * MOAB, a Mesh-Oriented datABase, is a software component for creating,
 * storing and accessing finite element mesh data.
 * 
 * Copyright 2004 Sandia Corporation.  Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government
 * retains certain rights in this software.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 */


#include "moab/HalfFacetRep.hpp"
#include <iostream>
#include <assert.h>
#include <vector>
#include "moab/Core.hpp"
#include "moab/Range.hpp"
#include "moab/CN.hpp"

namespace moab {

  HalfFacetRep::HalfFacetRep(Core *impl)
  {
    assert(NULL != impl);
    mb = impl;
    mInitAHFmaps = false;
    chk_mixed = false;
    is_mixed = false;
  }

  HalfFacetRep::~HalfFacetRep() {}


  MESHTYPE HalfFacetRep::get_mesh_type(int nverts, int nedges, int nfaces, int ncells)
  {
    MESHTYPE mesh_type = CURVE;

    if (nverts && nedges && (!nfaces) && (!ncells))
      mesh_type = CURVE;
    else if (nverts && !nedges && nfaces && !ncells)
      mesh_type = SURFACE;
    else if (nverts && nedges && nfaces && !ncells)
      mesh_type = SURFACE_MIXED;
    else if (nverts && !nedges && !nfaces && ncells)
      mesh_type = VOLUME;
    else if (nverts && nedges && !nfaces && ncells)
      mesh_type = VOLUME_MIXED_1;
    else if (nverts && !nedges && nfaces && ncells)
      mesh_type = VOLUME_MIXED_2;
    else if (nverts && nedges && nfaces && ncells)
      mesh_type = VOLUME_MIXED;

    return mesh_type;
  }

  const HalfFacetRep::adj_matrix HalfFacetRep::adjMatrix[7] =
  {
      // Stores the adjacency matrix for each mesh type.
      //CURVE
      {{{0,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}}},

      //SURFACE
      {{{0,0,1,0},{0,0,0,0},{1,0,1,0},{0,0,0,0}}},

      //SURFACE_MIXED
      {{{0,1,1,0},{1,1,1,0},{1,1,1,0},{0,0,0,0}}},

      //VOLUME
      {{{0,0,0,1},{0,0,0,0},{0,0,0,0},{1,0,0,1}}},

      //VOLUME_MIXED_1
      {{{0,1,0,1},{1,1,0,1},{0,0,0,0},{1,1,0,1}}},

      //VOLUME_MIXED_2
      {{{0,0,1,1},{0,0,0,0},{1,0,1,1},{1,0,1,1}}},

      //VOLUME_MIXED
      {{{0,1,1,1},{1,1,1,1},{1,1,1,1},{1,1,1,1}}}
  };

  int HalfFacetRep::get_index_for_meshtype(MESHTYPE mesh_type)
  {
      int index = 0;
      if (mesh_type == CURVE) index = 0;
      else if (mesh_type == SURFACE) index = 1;
      else if (mesh_type == SURFACE_MIXED) index = 2;
      else if (mesh_type == VOLUME)  index = 3;
      else if (mesh_type == VOLUME_MIXED_1) index = 4;
      else if (mesh_type == VOLUME_MIXED_2) index = 5;
      else if (mesh_type == VOLUME_MIXED) index = 6;
      return index;
  }

  bool HalfFacetRep::check_mixed_entity_type()
  {
      if (!chk_mixed)
      {
          chk_mixed = true;

          ErrorCode error;
          Range felems, celems;

          error = mb->get_entities_by_dimension( 0, 2, felems ); MB_CHK_ERR(error);

          if (felems.size()){
              Range tris, quad, poly;
              tris = felems.subset_by_type(MBTRI);
              quad = felems.subset_by_type(MBQUAD);
              poly = felems.subset_by_type(MBPOLYGON);
              if ((tris.size()&&quad.size())||(tris.size()&&poly.size())||(quad.size()&&poly.size()))
                  is_mixed = true;
              if (poly.size())
                  is_mixed = true;

              return is_mixed;
          }

          error = mb->get_entities_by_dimension( 0, 3, celems);   MB_CHK_ERR(error);
          if (celems.size()){
              Range tet, pyr, prism, hex, polyhed;
              tet = celems.subset_by_type(MBTET);
              pyr = celems.subset_by_type(MBPYRAMID);
              prism = celems.subset_by_type(MBPRISM);
              hex = celems.subset_by_type(MBHEX);
              polyhed = celems.subset_by_type(MBPOLYHEDRON);
              if ((tet.size() && pyr.size())||(tet.size() && prism.size())||(tet.size() && hex.size())||(tet.size()&&polyhed.size())||(pyr.size() && prism.size())||(pyr.size() && hex.size()) ||(pyr.size()&&polyhed.size())|| (prism.size() && hex.size())||(prism.size()&&polyhed.size())||(hex.size()&&polyhed.size()))
                  is_mixed = true;

              if (polyhed.size())
                  is_mixed = true;
              return is_mixed;
          }
      }
      return is_mixed;
  }

  /*******************************************************
   * initialize                                          *
   ******************************************************/

  ErrorCode HalfFacetRep::initialize()
  {
    mInitAHFmaps = true;

    ErrorCode error;

    error = mb->get_entities_by_dimension( 0, 0, _verts); MB_CHK_ERR(error);

    error = mb->get_entities_by_dimension( 0, 1, _edges); MB_CHK_ERR(error);

    error = mb->get_entities_by_dimension( 0, 2, _faces); MB_CHK_ERR(error);
    
    error = mb->get_entities_by_dimension( 0, 3, _cells); MB_CHK_ERR(error);
    
    int nverts = _verts.size();
    int nedges = _edges.size();
    int nfaces = _faces.size();
    int ncells = _cells.size();

    MESHTYPE mesh_type = get_mesh_type(nverts, nedges, nfaces, ncells);
    thismeshtype = mesh_type;
  
    //Initialize mesh type specific maps
    if (thismeshtype == CURVE){
        error = init_curve(); MB_CHK_ERR(error);
      }   
    else if (thismeshtype == SURFACE){
      error = init_surface();MB_CHK_ERR(error);
      }
    else if (thismeshtype == SURFACE_MIXED){
        error = init_curve();MB_CHK_ERR(error);
        error = init_surface();MB_CHK_ERR(error);
      }
    else if (thismeshtype == VOLUME){
        error = init_volume();MB_CHK_ERR(error);
      }
    else if (thismeshtype == VOLUME_MIXED_1){
        error = init_curve();MB_CHK_ERR(error);
        error = init_volume();MB_CHK_ERR(error);
    }
    else if (thismeshtype == VOLUME_MIXED_2){
        error = init_surface();MB_CHK_ERR(error);
        error = init_volume();MB_CHK_ERR(error);
      }
    else if (thismeshtype == VOLUME_MIXED){
        error = init_curve();MB_CHK_ERR(error);
        error = init_surface();MB_CHK_ERR(error);
        error = init_volume();MB_CHK_ERR(error);
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::init_curve()
  {
    ErrorCode error;

    int nv = _verts.size();
    int ne = _edges.size();
    sibhvs_eid.reserve(ne*2);
    sibhvs_lvid.reserve(ne*2);
    v2hv_eid.reserve(nv);
    v2hv_lvid.reserve(nv);

    for (int i=0; i<2*ne; i++)
      {
        sibhvs_eid.push_back(0);
        sibhvs_lvid.push_back(0);
      }

    for (int i=0; i<nv; i++)
      {
        v2hv_eid.push_back(0);
        v2hv_lvid.push_back(0);
      }

    error = determine_sibling_halfverts(_edges);MB_CHK_ERR(error);
    error = determine_incident_halfverts(_edges);MB_CHK_ERR(error);

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::init_surface()
  {
    ErrorCode error;
    EntityType ftype = mb->type_from_handle(*_faces.begin());
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;
    int nv = _verts.size();
    int nf = _faces.size();
    sibhes_fid.reserve(nf*nepf);
    sibhes_leid.reserve(nf*nepf);
    v2he_fid.reserve(nv);
    v2he_leid.reserve(nv);

    for (int i=0; i<nf*nepf; i++)
      {
        sibhes_fid.push_back(0);
        sibhes_leid.push_back(0);
      }

    for (int i=0; i<nv; i++)
      {
        v2he_fid.push_back(0);
        v2he_leid.push_back(0);
      }

    // Construct ahf maps
    error = determine_sibling_halfedges(_faces);MB_CHK_ERR(error);
    error = determine_incident_halfedges(_faces);MB_CHK_ERR(error);

    //Initialize queues for storing face and local id's during local search
    for (int i = 0; i< MAXSIZE; i++)
      {
        queue_fid[i] = 0;
        queue_lid[i] = 0;
        trackfaces[i] = 0;
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::init_volume()
  {
    ErrorCode error;

    //Initialize std::map between cell-types and their index in lConnMap3D
    cell_index[MBTET] = 0;
    cell_index[MBPYRAMID] = 1;
    cell_index[MBPRISM] = 2;
    cell_index[MBHEX] = 3;

    int index = get_index_in_lmap(*_cells.begin());
    int nfpc = lConnMap3D[index].num_faces_in_cell;
    int nv = _verts.size();
    int nc = _cells.size();
    sibhfs_cid.reserve(nc*nfpc);
    sibhfs_lfid.reserve(nc*nfpc);
    v2hf_cid.reserve(nv);
    v2hf_lfid.reserve(nv);

    for (int i=0; i<nc*nfpc; i++)
      {
        sibhfs_cid.push_back(0);
        sibhfs_lfid.push_back(0);
      }

    for (int i=0; i<nv; i++)
      {
        v2hf_cid.push_back(0);
        v2hf_lfid.push_back(0);
      }

    //Construct the maps
    error = determine_sibling_halffaces(_cells);MB_CHK_ERR(error);
    error = determine_incident_halffaces(_cells);MB_CHK_ERR(error);

    //Initialize queues for storing face and local id's during local search
    for (int i = 0; i< MAXSIZE; i++)
      {
        Stkcells[i] = 0;
        cellq[i] = 0;
        trackcells[i] = 0;
      }

    return MB_SUCCESS;
  }

   //////////////////////////////////////////////////
   ErrorCode HalfFacetRep::print_tags()
   {
     EntityType ftype = mb->type_from_handle(*_faces.begin());
     int nepf = lConnMap2D[ftype-2].num_verts_in_face;
     int index = get_index_in_lmap(*_cells.begin());
     int nfpc = lConnMap3D[index].num_faces_in_cell;

     //////////////////////////
     // Print out the tags
     EntityHandle start_edge = *_edges.begin();
     EntityHandle start_face = *_faces.begin();
     EntityHandle start_cell = *_cells.begin();
     std::cout<<"start_edge = "<<start_edge<<std::endl;
     std::cout<<"<SIBHVS_EID,SIBHVS_LVID>"<<std::endl;

     for (Range::iterator i = _edges.begin(); i != _edges.end(); ++i){
       EntityHandle eid[2];  int lvid[2];
       int eidx = _edges.index(*i);

       eid[0] = sibhvs_eid[2*eidx]; eid[1] = sibhvs_eid[2*eidx+1];
       lvid[0] = sibhvs_lvid[2*eidx]; lvid[1] = sibhvs_lvid[2*eidx+1];
       std::cout<<"Entity = "<<*i<<" :: <"<<eid[0]<<","<<lvid[0]<<">"<<"      "<<"<"<<eid[1]<<","<<lvid[1]<<">"<<std::endl;
     }

    std::cout<<"<V2HV_EID, V2HV_LVID>"<<std::endl;

     for (Range::iterator i = _verts.begin(); i != _verts.end(); ++i){
         int vidx = _verts.index(*i);
         EntityHandle eid = v2hv_eid[vidx];
         int lvid = v2hv_lvid[vidx];
         std::cout<<"Vertex = "<<*i<<" :: <"<<eid<<","<<lvid<<">"<<std::endl;
       }

     std::cout<<"start_face = "<<start_face<<std::endl;
     std::cout<<"<SIBHES_FID,SIBHES_LEID>"<<std::endl;

     for (Range::iterator i = _faces.begin(); i != _faces.end(); ++i){
         int fidx = _faces.index(*i);
         std::cout<<"Entity = "<<*i;
         for (int j=0; j<nepf; j++){
             EntityHandle sib = sibhes_fid[nepf*fidx+j];
             int lid = sibhes_leid[nepf*fidx+j];
             std::cout<<" :: <"<<sib<<","<<lid<<">"<<"       ";
           }
         std::cout<<std::endl;
       }

     std::cout<<"<V2HE_FID, V2HE_LEID>"<<std::endl;

     for (Range::iterator i = _verts.begin(); i != _verts.end(); ++i){
         int vidx = _verts.index(*i);
         EntityHandle fid = v2he_fid[vidx];
         int lid = v2he_leid[vidx];
         std::cout<<"Vertex = "<<*i<<" :: <"<<fid<<","<<lid<<">"<<std::endl;
       }

     std::cout<<"start_cell = "<<start_cell<<std::endl;
     std::cout<<"<SIBHES_CID,SIBHES_LFID>"<<std::endl;

     for (Range::iterator i = _cells.begin(); i != _cells.end(); ++i){
         int cidx = _cells.index(*i);
         std::cout<<"Entity = "<<*i;
         for (int j=0; j<nfpc; j++){
             EntityHandle sib = sibhfs_cid[nfpc*cidx+j];
             int lid = sibhfs_lfid[nfpc*cidx+j];
             std::cout<<" :: <"<<sib<<","<<lid<<">"<<"       ";
           }
         std::cout<<std::endl;
     }

     std::cout<<"<V2HF_CID, V2HF_LFID>"<<std::endl;

     for (Range::iterator i = _verts.begin(); i != _verts.end(); ++i){
         int vidx = _verts.index(*i);
         EntityHandle cid = v2hf_cid[vidx];
         int lid = v2hf_lfid[vidx];
         std::cout<<"Vertex = "<<*i<<" :: <"<<cid<<","<<lid<<">"<<std::endl;
     }

     return MB_SUCCESS;
   }

   /**********************************************************
   *      User interface for adjacency functions            *
   ********************************************************/

  ErrorCode HalfFacetRep::get_adjacencies(const EntityHandle source_entity,
                                          const unsigned int target_dimension,
                                          std::vector<EntityHandle> &target_entities)
  {

      ErrorCode error;

      unsigned int source_dimension = mb->dimension_from_handle(source_entity);
      assert((source_dimension <= target_dimension) || (source_dimension > target_dimension));

      if (mInitAHFmaps == false)
      {
          error = initialize(); MB_CHK_ERR(error);
      }

      int mindex = get_index_for_meshtype(thismeshtype);
      int adj_possible = adjMatrix[mindex].val[source_dimension][target_dimension];

      if (adj_possible)
      {
          if (source_dimension < target_dimension)
          {
              error = get_up_adjacencies(source_entity, target_dimension, target_entities); MB_CHK_ERR(error);
          }
          else if (source_dimension == target_dimension)
          {
              error = get_neighbor_adjacencies(source_entity, target_entities);  MB_CHK_ERR(error);
          }
          else
          {           
              error = get_down_adjacencies(source_entity, target_dimension, target_entities); MB_CHK_ERR(error);
          }
      }
      else
          return MB_SUCCESS;

      return MB_SUCCESS;
  }


   ErrorCode HalfFacetRep::get_up_adjacencies(EntityHandle ent,
                                              int out_dim,
                                              std::vector<EntityHandle> &adjents,
                                              std::vector<int> * lids)
   {
    ErrorCode error;
    int in_dim = mb->dimension_from_handle(ent);
    assert((in_dim >=0 && in_dim <= 2) && (out_dim > in_dim));

    if (in_dim == 0)
      {
        if (out_dim == 1)
        {
            error = get_up_adjacencies_1d(ent, adjents, lids);  MB_CHK_ERR(error);
        }
        else if (out_dim == 2)
        {
            error = get_up_adjacencies_vert_2d(ent, adjents);  MB_CHK_ERR(error);
        }
        else if (out_dim == 3)
        {
            error = get_up_adjacencies_vert_3d(ent, adjents); MB_CHK_ERR(error);
        }
      }

    else if ((in_dim == 1) && (out_dim == 2))
      {
        error = get_up_adjacencies_2d(ent, adjents, lids); MB_CHK_ERR(error);
      }
    else if ((in_dim == 1) && (out_dim == 3))
      {
        error = get_up_adjacencies_edg_3d(ent, adjents, lids); MB_CHK_ERR(error);
      }
    else if ((in_dim == 2) && (out_dim ==3))
      {
        error = get_up_adjacencies_face_3d(ent, adjents, lids); MB_CHK_ERR(error);
      }
    return MB_SUCCESS;
   }

   ErrorCode HalfFacetRep::get_neighbor_adjacencies(EntityHandle ent,
                                                    std::vector<EntityHandle> &adjents)
   {
     ErrorCode error;
     int in_dim = mb->dimension_from_handle(ent);
     assert(in_dim >=1 && in_dim <= 3);

     if (in_dim == 1)
       {
         error = get_neighbor_adjacencies_1d(ent, adjents); MB_CHK_ERR(error);
       }

     else if (in_dim == 2)
       {
         error = get_neighbor_adjacencies_2d(ent, adjents); MB_CHK_ERR(error);
       }
     else if (in_dim == 3)
       {
         error = get_neighbor_adjacencies_3d(ent, adjents); MB_CHK_ERR(error);
       }
     return MB_SUCCESS;
   }

   ErrorCode HalfFacetRep::get_down_adjacencies(EntityHandle ent, int out_dim, std::vector<EntityHandle> &adjents)
   {
       ErrorCode error;
       int in_dim = mb->dimension_from_handle(ent);
       assert((in_dim >=2 && in_dim <= 3) && (out_dim < in_dim));

       if ((in_dim == 2)&&(out_dim == 1))
       {
           error = get_down_adjacencies_2d(ent, adjents);  MB_CHK_ERR(error);
       }
       else if ((in_dim == 3)&&(out_dim == 1))
       {
           error = get_down_adjacencies_edg_3d(ent, adjents); MB_CHK_ERR(error);
       }
       else if ((in_dim == 3)&&(out_dim == 2))
       {
           error = get_down_adjacencies_face_3d(ent, adjents); MB_CHK_ERR(error);
       }
       return MB_SUCCESS;
   }

   ErrorCode HalfFacetRep::count_subentities(Range &edges, Range &faces, Range &cells, int *nedges, int *nfaces)
   {
     ErrorCode error;
     if (edges.size() && !faces.size() && !cells.size())
       {
         nedges[0] = edges.size();
         nfaces[0] = 0;
       }
     else if (faces.size() && !cells.size())
       {
         nedges[0] = find_total_edges_2d(faces);
         nfaces[0] = 0;
       }
     else if (cells.size())
       {
         error = find_total_edges_faces_3d(cells, nedges, nfaces); MB_CHK_ERR(error);
       }
     return MB_SUCCESS;
   }

  /******************************************************** 
  * 1D: sibhvs, v2hv, incident and neighborhood queries   *
  *********************************************************/
  ErrorCode HalfFacetRep::determine_sibling_halfverts( Range &edges)
  {
    ErrorCode error;

    //Step 1: Create an index list storing the starting position for each vertex
    int nv = _verts.size();
    int *is_index = new int[nv+1];
    for (int i =0; i<nv+1; i++)
      is_index[i] = 0;

    //std::vector<EntityHandle> conn(2);
    for (Range::iterator eid = edges.begin(); eid != edges.end(); ++eid)
      {
        const EntityHandle* conn;
        int num_conn = 0;
        error = mb->get_connectivity(*eid, conn, num_conn);MB_CHK_ERR(error);

        int index = _verts.index(conn[0]);
        is_index[index+1] += 1;
        index = _verts.index(conn[1]);
        is_index[index+1] += 1;
      }
    is_index[0] = 0;

    for (int i=0; i<nv; i++)
      is_index[i+1] = is_index[i] + is_index[i+1];

    //Step 2: Define two arrays v2hv_eid, v2hv_lvid storing every half-facet on a vertex
    EntityHandle *v2hv_map_eid = new EntityHandle[2*edges.size()];
    int *v2hv_map_lvid = new int[2*edges.size()];

    for (Range::iterator eid = edges.begin(); eid != edges.end(); ++eid)
      {
        const EntityHandle* conn;
        int num_conn = 0;
        error = mb->get_connectivity(*eid, conn, num_conn);MB_CHK_ERR(error);

        for (int j = 0; j< 2; j++)
          {
            int v = _verts.index(conn[j]);
            v2hv_map_eid[is_index[v]] = *eid;
            v2hv_map_lvid[is_index[v]] = j;
            is_index[v] += 1;
          }
      }

    for (int i=nv-2; i>=0; i--)
      is_index[i+1] = is_index[i];
    is_index[0] = 0;

    //Step 3: Fill up sibling half-verts map
    for (Range::iterator vid = _verts.begin(); vid != _verts.end(); ++vid)
      {
        int v = _verts.index(*vid);
        int last = is_index[v+1] - 1;
        if (last > is_index[v])
          {
            EntityHandle prev_eid = v2hv_map_eid[last];
            int prev_lvid = v2hv_map_lvid[last];

            for (int i=is_index[v]; i<=last; i++)
              {
                EntityHandle cur_eid = v2hv_map_eid[i];
                int cur_lvid = v2hv_map_lvid[i];

                int pidx = edges.index(prev_eid);
                sibhvs_eid[2*pidx+prev_lvid] = cur_eid;
                sibhvs_lvid[2*pidx+prev_lvid] = cur_lvid;
                prev_eid = cur_eid;
                prev_lvid = cur_lvid;

              }
          }
      }

    delete [] is_index;
    delete [] v2hv_map_eid;
    delete [] v2hv_map_lvid;

    return MB_SUCCESS;
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::determine_incident_halfverts( Range &edges){
    ErrorCode error;

    for (Range::iterator e_it = edges.begin(); e_it != edges.end(); ++e_it){      
        const EntityHandle* conn;
        int num_conn = 0;
        error = mb->get_connectivity(*e_it, conn, num_conn);MB_CHK_ERR(error);

        for(int i=0; i<2; ++i){
            EntityHandle v = conn[i], eid = 0;
            int vidx = _verts.index(v);
            eid = v2hv_eid[vidx];

            if (eid==0){
                v2hv_eid[vidx] = *e_it;
                v2hv_lvid[vidx] = i;
              }
          }
      }

    return MB_SUCCESS;
  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode  HalfFacetRep::get_up_adjacencies_1d( EntityHandle vid,
                                                  std::vector< EntityHandle > &adjents,
                                                  std::vector<int> * lvids)
  {
    adjents.reserve(20);

    bool local_id = false;
    if (lvids != NULL)
      {
        local_id = true;
        lvids->reserve(20);
      }

    EntityHandle start_eid, eid;
    int start_lid, lid;
   
    int vidx = _verts.index(vid);
    start_eid = v2hv_eid[vidx];
    start_lid = v2hv_lvid[vidx];

    eid = start_eid; lid = start_lid;

    if (eid != 0){
      adjents.push_back(eid);
      if (local_id)
        lvids->push_back(lid);

      while (eid !=0) {

	  int eidx = _edges.index(eid);
	  eid = sibhvs_eid[2*eidx+lid];
	  lid = sibhvs_lvid[2*eidx+lid];

	  if ((!eid)||(eid == start_eid))
	    break;
	  adjents.push_back(eid);
	  if (local_id)
	    lvids->push_back(lid);
      }
    }

    return MB_SUCCESS;
  }
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode  HalfFacetRep::get_neighbor_adjacencies_1d( EntityHandle eid,
                                                        std::vector<EntityHandle> &adjents)
  {
    adjents.reserve(20);

    EntityHandle sibhv_eid;
    int sibhv_lid;

    int eidx = _edges.index(eid);

    for (int lid = 0;lid < 2; ++lid){

        sibhv_eid = sibhvs_eid[2*eidx+lid];
        sibhv_lid = sibhvs_lvid[2*eidx+lid];

      if (sibhv_eid != 0){
        adjents.push_back(sibhv_eid);

        eidx = _edges.index(sibhv_eid);

	EntityHandle hv_eid = sibhvs_eid[2*eidx+sibhv_lid];
	int hv_lid = sibhvs_lvid[2*eidx+sibhv_lid];
	
	while (hv_eid != 0){	    
	  if (hv_eid != eid)
	    adjents.push_back(hv_eid);

	  eidx = _edges.index(hv_eid);
	  if (sibhvs_eid[2*eidx+hv_lid] == sibhv_eid)
	    break;

	  hv_eid = sibhvs_eid[2*eidx+hv_lid];
	  hv_lid = sibhvs_lvid[2*eidx+hv_lid];
	}
      }
    } 

    return MB_SUCCESS;   
  }
  
  /*******************************************************
  * 2D: sibhes, v2he, incident and neighborhood queries  *
  ********************************************************/
  const HalfFacetRep::LocalMaps2D HalfFacetRep::lConnMap2D[2] = {
    //Triangle
    {3, {1,2,0}, {2,0,1}},
    //Quad
    {4,{1,2,3,0},{3,0,1,2}}
  };
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
   ErrorCode HalfFacetRep::determine_sibling_halfedges( Range &faces)
  {
    ErrorCode error;
    EntityHandle start_face = *faces.begin();
    EntityType ftype = mb->type_from_handle(start_face);
    int nfaces = faces.size();
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;

    //Step 1: Create an index list storing the starting position for each vertex
    int nv = _verts.size();
    int *is_index = new int[nv+1];
    for (int i =0; i<nv+1; i++)
      is_index[i] = 0;

    int index;
   // std::vector<EntityHandle> conn(nepf);
    for (Range::iterator fid = faces.begin(); fid != faces.end(); ++fid)
       {
        const EntityHandle* conn;
        error = mb->get_connectivity(*fid, conn, nepf);MB_CHK_ERR(error);

         for (int i = 0; i<nepf; i++)
           {
             index = _verts.index(conn[i]);
             is_index[index+1] += 1;
           }
       }
     is_index[0] = 0;

     for (int i=0; i<nv; i++)
       is_index[i+1] = is_index[i] + is_index[i+1];

     //Step 2: Define two arrays v2hv_eid, v2hv_lvid storing every half-facet on a vertex
     EntityHandle * v2nv = new EntityHandle[nepf*nfaces];
     EntityHandle * v2he_map_fid = new EntityHandle[nepf*nfaces];
     int * v2he_map_leid = new int[nepf*nfaces];

     for (Range::iterator fid = faces.begin(); fid != faces.end(); ++fid)
       {
         const EntityHandle* conn;
         error = mb->get_connectivity(*fid, conn, nepf);MB_CHK_ERR(error);

         for (int j = 0; j< nepf; j++)
           {
             int v = _verts.index(conn[j]);
             int nidx = lConnMap2D[ftype-2].next[j];
             v2nv[is_index[v]] = conn[nidx];
             v2he_map_fid[is_index[v]] = *fid;
             v2he_map_leid[is_index[v]] = j;
             is_index[v] += 1;
           }
       }

     for (int i=nv-2; i>=0; i--)
       is_index[i+1] = is_index[i];
     is_index[0] = 0;


     //Step 3: Fill up sibling half-verts map
     for (Range::iterator fid = faces.begin(); fid != faces.end(); ++fid)
       {
         const EntityHandle* conn;
         error = mb->get_connectivity(*fid, conn, nepf);MB_CHK_ERR(error);

         int fidx = faces.index(*fid);

         for (int k =0; k<nepf; k++)
           {
             EntityHandle sibfid = sibhes_fid[nepf*fidx+k];

             if (sibfid != 0)
               continue;

             int nidx = lConnMap2D[ftype-2].next[k];
             int v = _verts.index(conn[k]);
             int vn = _verts.index(conn[nidx]);

             EntityHandle first_fid = *fid;
             int first_leid = k;

             EntityHandle prev_fid = *fid;
             int prev_leid = k;

             for (index = is_index[vn]; index <= is_index[vn+1]-1; index++)
               {
                 if (v2nv[index] == conn[k])
                   {
                     EntityHandle cur_fid = v2he_map_fid[index];
                     int cur_leid = v2he_map_leid[index];

                     int pidx = faces.index(prev_fid);
                     sibhes_fid[nepf*pidx+prev_leid] = cur_fid;
                     sibhes_leid[nepf*pidx+prev_leid] = cur_leid;
                     prev_fid = cur_fid;
                     prev_leid = cur_leid;
                   }
               }

             for (index = is_index[v]; index <= is_index[v+1]-1; index++)
               {
                 if ((v2nv[index] == conn[nidx])&&(v2he_map_fid[index] != *fid))
                   {

                     EntityHandle cur_fid = v2he_map_fid[index];
                     int cur_leid = v2he_map_leid[index];

                     int pidx = faces.index(prev_fid);
                     sibhes_fid[nepf*pidx+prev_leid] = cur_fid;
                     sibhes_leid[nepf*pidx+prev_leid] = cur_leid;
                     prev_fid = cur_fid;
                     prev_leid = cur_leid;

                   }
               }

             if (prev_fid != first_fid){
                 int pidx = faces.index(prev_fid);
                 sibhes_fid[nepf*pidx+prev_leid] = first_fid;
                 sibhes_leid[nepf*pidx+prev_leid] = first_leid;
             }
           }
       }

     delete [] is_index;
     delete [] v2nv;
     delete [] v2he_map_fid;
     delete [] v2he_map_leid;

     return MB_SUCCESS;

  }
  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::determine_incident_halfedges( Range &faces)
  {
    ErrorCode error;    
    EntityType ftype = mb->type_from_handle(*faces.begin());
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;

    for (Range::iterator it = faces.begin(); it != faces.end(); ++it){      
        const EntityHandle* conn;
        error = mb->get_connectivity(*it, conn, nepf);MB_CHK_ERR(error);

        int fidx = faces.index(*it);

        for(int i=0; i<nepf; ++i){
            EntityHandle v = conn[i];
            int vidx = _verts.index(v);
            EntityHandle vfid = v2he_fid[vidx];
            EntityHandle sibfid = sibhes_fid[nepf*fidx+i];

            if ((vfid==0)||((vfid!=0) && (sibfid==0))){
                v2he_fid[vidx] = *it;
                v2he_leid[vidx] = i;
              }
          }
      }

    return MB_SUCCESS;
  }
  ///////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::get_up_adjacencies_vert_2d(EntityHandle vid, std::vector<EntityHandle> &adjents)
  {
    ErrorCode error;

    int vidx = _verts.index(vid);
    EntityHandle fid = v2he_fid[vidx];
    int lid = v2he_leid[vidx];

    if (!fid)
      return MB_SUCCESS;

    adjents.reserve(20);
    adjents.push_back(fid);

    int qsize = 0, count = -1;
    int num_qvals = 0;

    error = gather_halfedges(vid, fid, lid, &qsize, &count);MB_CHK_ERR(error);

    while (num_qvals < qsize)
      {

        EntityHandle curfid = queue_fid[num_qvals];
        int curlid = queue_lid[num_qvals];
        num_qvals += 1;

        EntityHandle he2_fid = 0; int he2_lid = 0;
        error = another_halfedge(vid, curfid, curlid, &he2_fid, &he2_lid);  MB_CHK_ERR(error);

        bool val = find_match_in_array(he2_fid, trackfaces, count);

        if (val)
          continue;

        count += 1;
        trackfaces[count] = he2_fid;

        error = get_up_adjacencies_2d(he2_fid, he2_lid, &qsize, &count);MB_CHK_ERR(error);

        adjents.push_back(he2_fid);
      }

    //Change the visited faces to false, also empty the queue
    for (int i = 0; i<=qsize; i++)
      {
        queue_fid[i] = 0;
        queue_lid[i] = 0;
      }

    for (int i = 0; i<=count; i++)
      trackfaces[i] = 0;

    return MB_SUCCESS;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode  HalfFacetRep::get_up_adjacencies_2d( EntityHandle eid,
                                                  std::vector<EntityHandle> &adjents,
                                                  std::vector<int>  * leids)
{

  // Given an explicit edge eid, find the incident faces.
    ErrorCode error;
    EntityHandle he_fid=0; int he_lid=0;

    // Step 1: Given an explicit edge, find a corresponding half-edge from the surface mesh.
    bool found = find_matching_halfedge(eid, &he_fid, &he_lid);

    // Step 2: If there is a corresponding half-edge, collect all sibling half-edges and store the incident faces.
    if (found)
      { 
        error = get_up_adjacencies_2d(he_fid, he_lid, true, adjents, leids);MB_CHK_ERR(error);
      }

    return MB_SUCCESS;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::get_up_adjacencies_2d(EntityHandle fid,
                                                 int leid,
                                                 bool add_inent,
                                                 std::vector<EntityHandle> &adj_ents,
                                                 std::vector<int> *adj_leids, std::vector<int> *adj_orients)
  {
    // Given an implicit half-edge <fid, leid>, find the incident half-edges.
    ErrorCode error;

    EntityType ftype = mb->type_from_handle(fid);
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;

    if (!fid)
      return MB_FAILURE;
    adj_ents.reserve(20);

    bool local_id =false;
    bool orient = false;
    if (adj_leids != NULL)
      {
        local_id = true;
        adj_leids->reserve(20);
      }
    if (adj_orients != NULL)
      {
        orient = true;
        adj_orients->reserve(20);
      }

    if (add_inent)
      {
        adj_ents.push_back(fid);
        if (local_id)
          adj_leids->push_back(leid);
      }

    EntityHandle fedge[2] = {0,0};

    if (orient)
      {
        //get connectivity and match their directions
        const EntityHandle* fid_conn;
        error = mb->get_connectivity(fid, fid_conn, nepf);MB_CHK_ERR(error);

        int nidx = lConnMap2D[ftype-2].next[leid];
        fedge[0] = fid_conn[leid];
        fedge[1] = fid_conn[nidx];
      }


    int fidx = _faces.index(fid);
    EntityHandle curfid = sibhes_fid[nepf*fidx+leid];
    int curlid = sibhes_leid[nepf*fidx+leid];
    
    while ((curfid != fid)&&(curfid != 0)){//Should not go into the loop when no sibling exists
        adj_ents.push_back(curfid);

        if (local_id)
          adj_leids->push_back(curlid);

        if (orient)
          {
            //get connectivity and match their directions
            const EntityHandle* conn;
            error = mb->get_connectivity(curfid, conn, nepf);MB_CHK_ERR(error);

            int nidx = lConnMap2D[ftype-2].next[curlid];

            if ((fedge[0] == conn[curlid])&&(fedge[1] == conn[nidx]))
              adj_orients->push_back(1);
            else if ((fedge[1] == conn[curlid])&&(fedge[0] == conn[nidx]))
              adj_orients->push_back(0);
          }

        int cidx = _faces.index(curfid);
        curfid = sibhes_fid[nepf*cidx+curlid];
        curlid = sibhes_leid[nepf*cidx+curlid];
    }

    return MB_SUCCESS;
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   ErrorCode HalfFacetRep::get_up_adjacencies_2d(EntityHandle fid,
                                                 int lid,
                                                 int *qsize,
                                                 int *count
                                                )
  {
    EntityType ftype = mb->type_from_handle(fid);
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;

    int fidx = _faces.index(fid);
    EntityHandle curfid = sibhes_fid[nepf*fidx+lid];
    int curlid = sibhes_leid[nepf*fidx+lid];

    if (curfid == 0){
        int index = 0;
        bool found_ent = find_match_in_array(fid, queue_fid, qsize[0]-1, true, &index);

        if ((!found_ent)||((found_ent) && (queue_lid[index] != lid)))
          {
            queue_fid[qsize[0]] = fid;
            queue_lid[qsize[0]] = lid;
            qsize[0] += 1;
          }
      }

    while ((curfid != fid)&&(curfid != 0)) {
        bool val = find_match_in_array(curfid, trackfaces, count[0]);

        if (!val){
            queue_fid[qsize[0]] = curfid;
            queue_lid[qsize[0]] = curlid;
            qsize[0] += 1;
          }

        int cidx = _faces.index(curfid);
        curfid = sibhes_fid[nepf*cidx+curlid];
        curlid = sibhes_leid[nepf*cidx+curlid];
      }

    return MB_SUCCESS;
   }

  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  bool HalfFacetRep::find_matching_halfedge( EntityHandle eid,
                                             EntityHandle *hefid,
                                             int *helid){
    ErrorCode error;
    const EntityHandle* conn;
    int num_conn = 0;
    error = mb->get_connectivity(eid, conn, num_conn);MB_CHK_ERR(error);

    int vidx = _verts.index(conn[0]);
    EntityHandle fid = v2he_fid[vidx];
    int lid = v2he_leid[vidx];

    bool found = false;

    if (fid!=0){

        int qsize = 0, count = -1;

        EntityHandle vid = conn[0];

        error = gather_halfedges(vid, fid, lid, &qsize, &count);  MB_CHK_ERR(error);

        found =  collect_and_compare(conn, &qsize, &count, hefid, helid);

        //Change the visited faces to false
        for (int i = 0; i<qsize; i++)
        {
            queue_fid[i] = 0;
            queue_lid[i] = 0;
        }

        for (int i = 0; i<= count; i++)
            trackfaces[i] = 0;
      }

    return found;
  }
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   ErrorCode HalfFacetRep::gather_halfedges( EntityHandle vid,
                                             EntityHandle he_fid,
                                             int he_lid,
                                             int *qsize,
                                             int *count
                                            )
  {  
    ErrorCode error;
    EntityHandle he2_fid = 0; int he2_lid = 0;

    error = another_halfedge(vid, he_fid, he_lid, &he2_fid, &he2_lid);MB_CHK_ERR(error);

    queue_fid[*qsize] = he_fid;
    queue_lid[*qsize] = he_lid;
    *qsize += 1;

    queue_fid[*qsize] = he2_fid;
    queue_lid[*qsize] = he2_lid;
    *qsize += 1;

    *count += 1;
    trackfaces[*count] = he_fid;

    error = get_up_adjacencies_2d(he_fid, he_lid, qsize, count);MB_CHK_ERR(error);
    error = get_up_adjacencies_2d(he2_fid, he2_lid, qsize, count);MB_CHK_ERR(error);

    return MB_SUCCESS;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::another_halfedge( EntityHandle vid,
                                            EntityHandle he_fid,
                                            int he_lid,
                                            EntityHandle *he2_fid,
                                            int *he2_lid)
  {    
    ErrorCode error;
    EntityType ftype = mb->type_from_handle(he_fid);
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;

    const EntityHandle* conn;
    error = mb->get_connectivity(he_fid, conn, nepf);MB_CHK_ERR(error);

    *he2_fid = he_fid;
    if (conn[he_lid] == vid)
      *he2_lid = lConnMap2D[ftype-2].prev[he_lid];
    else
      *he2_lid = lConnMap2D[ftype-2].next[he_lid];

    return MB_SUCCESS;
  }
  
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  bool HalfFacetRep::collect_and_compare(const EntityHandle *edg_vert,
                                         int *qsize,
                                         int *count,
                                         EntityHandle *he_fid,
                                         int *he_lid)
  {
    ErrorCode error;
    EntityType ftype = mb->type_from_handle(*_faces.begin());
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;

    bool found = false;
    int num_qvals = 0, counter = 0;

    while (num_qvals < *qsize && counter <MAXSIZE )
      {
        EntityHandle curfid = queue_fid[num_qvals];
        int curlid = queue_lid[num_qvals];
        num_qvals += 1;       

        const EntityHandle* conn;
        error = mb->get_connectivity(curfid, conn, nepf);MB_CHK_ERR(error);

        int id = lConnMap2D[ftype-2].next[curlid];
        if (((conn[curlid]==edg_vert[0])&&(conn[id]==edg_vert[1]))||((conn[curlid]==edg_vert[1])&&(conn[id]==edg_vert[0]))){
            *he_fid = curfid;
            *he_lid = curlid;
            found = true;
            break;
        }

        bool val = find_match_in_array(curfid, trackfaces, count[0]);

        if (val)
            continue;

        count[0] += 1;
        trackfaces[*count] = curfid;

        EntityHandle he2_fid; int he2_lid;
        error = another_halfedge(edg_vert[0], curfid, curlid, &he2_fid, &he2_lid);   MB_CHK_ERR(error);
        error = get_up_adjacencies_2d(he2_fid, he2_lid, qsize, count);   MB_CHK_ERR(error);

        counter += 1;
    }
    return found;
  }
  
  ///////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode  HalfFacetRep::get_neighbor_adjacencies_2d( EntityHandle fid,
                                                        std::vector<EntityHandle> &adjents)
  {
    ErrorCode error; 

    if (fid != 0){
      EntityType ftype = mb->type_from_handle(fid);
      int nepf = lConnMap2D[ftype-2].num_verts_in_face;

      for (int lid = 0; lid < nepf; ++lid){
        error = get_up_adjacencies_2d(fid, lid, false, adjents);MB_CHK_ERR(error);
      }
    }
    
    return MB_SUCCESS;
  }


  /////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::get_down_adjacencies_2d(EntityHandle fid, std::vector<EntityHandle> &adjents)
  {
      //Returns explicit edges, if any, of the face
      ErrorCode error;
      adjents.reserve(10);
      EntityType ftype = mb->type_from_handle(fid);
      int nepf = lConnMap2D[ftype-2].num_verts_in_face;

      const EntityHandle* conn;
      error = mb->get_connectivity(fid, conn, nepf);MB_CHK_ERR(error);

      std::vector<EntityHandle> temp;

      //Loop over 2 vertices
      for (int i=0; i<2; i++)
        {
          //Choose the two adjacent vertices for a triangle, and two opposite vertices for a quad
          int l;
          if (ftype == MBTRI)
            l=i;
          else
            l = 2*i;

          //Get the current, next and prev vertices
          int nidx = lConnMap2D[ftype-2].next[l];
          int pidx = lConnMap2D[ftype-2].prev[l];
          EntityHandle v = conn[l];
          EntityHandle vnext = conn[nidx];
          EntityHandle vprev = conn[pidx];

          //Get incident edges on v
          error = get_up_adjacencies_1d(v, temp);MB_CHK_ERR(error);

          //Loop over the incident edges and check if its end vertices match those in the face
          for (int k=0; k<(int)temp.size(); k++)
            {
              const EntityHandle* econn;
              int num_conn = 0;
              error = mb->get_connectivity(temp[k],econn, num_conn);MB_CHK_ERR(error);

              if ((econn[0] == v && econn[1] == vnext)||(econn[0] == v && econn[1] == vprev)||(econn[0] == vnext && econn[1] == v)||(econn[0] == vprev && econn[1] == v))
                {
                  bool found = false;
                  for (int j=0; j<(int)adjents.size(); j++)
                    {
                      if (adjents[j] == temp[k])
                        {
                          found = true;
                          break;
                        }
                    }
                  if (!found)
                    adjents.push_back(temp[k]);
                }

            }
        }

      return MB_SUCCESS;
  }

  ////////////////////////////////////////////////////////////////////////////////////////////////
  int HalfFacetRep::find_total_edges_2d(Range &faces)
  {
    ErrorCode error;
    EntityType ftype = mb->type_from_handle(*faces.begin());
    int nepf = lConnMap2D[ftype-2].num_verts_in_face;
    int nfaces = faces.size();

    int total_edges = nepf*nfaces;
    
    std::vector<int> trackF(total_edges,0);
    std::vector<EntityHandle> adj_fids;
    std::vector<int> adj_lids;

    for (Range::iterator f = faces.begin(); f != faces.end(); f++){
      for (int l = 0; l < nepf; l++){

	adj_fids.clear();
	adj_lids.clear();

	int id = nepf*(faces.index(*f))+l;
	if (!trackF[id])
	  {
	    error = get_up_adjacencies_2d(*f, l, false, adj_fids, &adj_lids);MB_CHK_ERR(error);

	    total_edges -= adj_fids.size();

	    for (int i = 0; i < (int)adj_fids.size(); i++)
	      trackF[nepf*(faces.index(adj_fids[i]))+adj_lids[i]] = 1;
	  };
      };
   };

    return total_edges;
  }

  /*******************************************************
  * 3D: sibhfs, v2hf, incident and neighborhood queries  *
  ********************************************************/

  int HalfFacetRep::get_index_in_lmap(EntityHandle cid)
  {
    EntityType type = mb->type_from_handle(cid);
    int index = cell_index.find(type)->second;
    return index;   
  }

   const HalfFacetRep::LocalMaps3D HalfFacetRep::lConnMap3D[4] =
    {
      // Tet
      {4, 6, 4, {3,3,3,3}, {{0,1,3},{1,2,3},{2,0,3},{0,2,1}},   {3,3,3,3},   {{0,2,3},{0,1,3},{1,2,3},{0,1,2}},   {{0,1},{1,2},{2,0},{0,3},{1,3},{2,3}},   {{3,0},{3,1},{3,2},{0,2},{0,1},{1,2}},   {{0,4,3},{1,5,4},{2,3,5},{2,1,0}},     {{-1,0,2,3},{0,-1,1,4},{2,1,-1,5},{3,4,5,-1}}},

      // Pyramid: Note: In MOAB pyramid follows the CGNS convention. Look up src/MBCNArrays.hpp
      {5, 8, 5, {4,3,3,3,3}, {{0,3,2,1},{0,1,4},{1,2,4},{2,3,4},{3,0,4}},  {3,3,3,3,4},   {{0,1,4},{0,1,2},{0,2,3},{0,3,4},{1,2,3,4}},   {{0,1},{1,2},{2,3},{3,0},{0,4},{1,4},{2,4},{3,4}},   {{0,1},{0,2},{0,3},{0,4},{1,4},{1,2},{2,3},{3,4}},    {{3,2,1,0},{0,5,4},{1,6,5},{2,7,6},{3,4,7}},    {{-1,0,-1,3,4},{0,-1,1,-1,5},{-1,1,-1,2,6},{3,-1,2,-1,7},{4,5,6,7,-1}}},

      // Prism
      {6, 9, 5, {4,4,4,3,3}, {{0,1,4,3},{1,2,5,4},{0,3,5,2},{0,2,1},{3,4,5}},  {3,3,3,3,3,3}, {{0,2,3},{0,1,3},{1,2,3},{0,2,4},{0,1,4},{1,4,2}},    {{0,1},{1,2},{2,0},{0,3},{1,4},{2,5},{3,4},{4,5},{5,3}},    {{0,3},{1,3},{2,3},{0,2},{0,1},{1,2},{0,4},{1,4},{2,4}},     {{0,4,6,3},{1,5,7,4},{2,3,8,5},{2,1,0},{6,7,8}},    {{-1,0,2,3,-1,-1},{0,-1,1,-1,4,-1},{2,1,-1,-1,-1,5},{3,-1,-1,-1,6,8},{-1,4,-1,6,-1,7},{-1,-1,5,8,7,-1}}},

      // Hex
      {8, 12, 6, {4,4,4,4,4,4}, {{0,1,5,4},{1,2,6,5},{2,3,7,6},{3,0,4,7},{0,3,2,1},{4,5,6,7}},   {3,3,3,3,3,3,3,3},   {{0,3,4},{0,1,4},{1,2,4},{2,3,4},{0,3,5},{0,1,5},{1,2,5},{2,3,5}},    {{0,1},{1,2},{2,3},{3,0},{0,4},{1,5},{2,6},{3,7},{4,5},{5,6},{6,7},{7,4}},     {{0,4},{1,4},{2,4},{3,4},{0,3},{0,1},{1,2},{2,3},{0,5},{1,5},{2,5},{3,5}},     {{0,5,8,4},{1,6,9,5},{2,7,10,6},{3,4,11,7},{3,2,1,0},{8,9,10,11}},     {{-1,0,-1,3,4,-1,-1,-1},{0,-1,1,-1,-1,5,-1,-1},{-1,1,-1,2,-1,-1,6,-1},{3,-1,2,-1,-1,-1,-1,7},{4,-1,-1,-1,-1,8,-1,11},{-1,5,-1,-1,8,-1,9,-1},{-1,-1,6,-1,-1,9,-1,10},{-1,-1,-1,7,11,-1,10,-1}}}
      
    };

  
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
   ErrorCode HalfFacetRep::determine_sibling_halffaces( Range &cells)
  {
    ErrorCode error;
    EntityHandle start_cell = *cells.begin();
    int index = get_index_in_lmap(start_cell);
    int nvpc = lConnMap3D[index].num_verts_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;

    //Step 1: Create an index list storing the starting position for each vertex
    int nv = _verts.size();
    int *is_index = new int[nv+1];
    for (int i =0; i<nv+1; i++)
      is_index[i] = 0;

    int vindex;

    for (Range::iterator cid = cells.begin(); cid != cells.end(); ++cid)
       {
        const EntityHandle* conn;
        error = mb->get_connectivity(*cid, conn, nvpc);MB_CHK_ERR(error);

        for (int i = 0; i<nfpc; ++i)
          {
            int nvF = lConnMap3D[index].hf2v_num[i];
            EntityHandle v = 0;
            for (int k = 0; k<nvF; k++)
              {
                int id = lConnMap3D[index].hf2v[i][k];
                if (v <= conn[id])
                  v = conn[id];
              }
            vindex = _verts.index(v);
            is_index[vindex+1] += 1;
          }
      }
     is_index[0] = 0;

     for (int i=0; i<nv; i++)
       is_index[i+1] = is_index[i] + is_index[i+1];

     //Step 2: Define four arrays v2hv_eid, v2hv_lvid storing every half-facet on a vertex
     EntityHandle * v2oe_v1 = new EntityHandle[is_index[nv]];
     EntityHandle * v2oe_v2 = new EntityHandle[is_index[nv]];
     EntityHandle * v2hf_map_cid = new EntityHandle[is_index[nv]];
     int * v2hf_map_lfid = new int[is_index[nv]];

     for (Range::iterator cid = cells.begin(); cid != cells.end(); ++cid)
       {
         const EntityHandle* conn;
         error = mb->get_connectivity(*cid, conn, nvpc);MB_CHK_ERR(error);

         for (int i = 0; i< nfpc; i++)
           {
             int nvF = lConnMap3D[index].hf2v_num[i];
             std::vector<EntityHandle> vs(nvF);
             EntityHandle vmax = 0;
             int lv = -1;
             for (int k = 0; k<nvF; k++)
               {
                 int id = lConnMap3D[index].hf2v[i][k];
                 vs[k] = conn[id];
                 if (vmax <= conn[id])
                   {
                     vmax = conn[id];
                     lv = k;
                   }
               }

             int nidx = lConnMap2D[nvF-3].next[lv];
             int pidx = lConnMap2D[nvF-3].prev[lv];

             int v = _verts.index(vmax);
             v2oe_v1[is_index[v]] = vs[nidx];
             v2oe_v2[is_index[v]] = vs[pidx];
             v2hf_map_cid[is_index[v]] = *cid;
             v2hf_map_lfid[is_index[v]] = i;
             is_index[v] += 1;

           }
       }

     for (int i=nv-2; i>=0; i--)
       is_index[i+1] = is_index[i];
     is_index[0] = 0;

     //Step 3: Fill up sibling half-verts map
     for (Range::iterator cid = cells.begin(); cid != cells.end(); ++cid)
       {
         const EntityHandle* conn;
         error = mb->get_connectivity(*cid, conn, nvpc);MB_CHK_ERR(error);

         int cidx = _cells.index(*cid);

         for (int i =0; i<nfpc; i++)
           {
             EntityHandle sibcid = sibhfs_cid[nfpc*cidx+i];

             if (sibcid != 0)
               continue;


             int nvF = lConnMap3D[index].hf2v_num[i];
             std::vector<EntityHandle> vs(nvF);
             EntityHandle vmax = 0;
             int lv = -1;
             for (int k = 0; k<nvF; k++)
               {
                 int id = lConnMap3D[index].hf2v[i][k];
                 vs[k] = conn[id];
                 if (vmax <= conn[id])
                   {
                     vmax = conn[id];
                     lv = k;
                   }
               }

             int nidx = lConnMap2D[nvF-3].next[lv];
             int pidx = lConnMap2D[nvF-3].prev[lv];

             int v = _verts.index(vmax);
             EntityHandle v1 = vs[pidx];
             EntityHandle v2 = vs[nidx];

             for (int ind = is_index[v]; ind <= is_index[v+1]-1; ind++)
               {
                 if ((v2oe_v1[ind] == v1)&&(v2oe_v2[ind] == v2))
                   {
                     // Map to opposite hf
                     EntityHandle cur_cid = v2hf_map_cid[ind];
                     int cur_lfid = v2hf_map_lfid[ind];

                     sibhfs_cid[nfpc*cidx+i] = cur_cid;
                     sibhfs_lfid[nfpc*cidx+i] = cur_lfid;

                     int scidx = _cells.index(cur_cid);
                     sibhfs_cid[nfpc*scidx+cur_lfid] = *cid;
                     sibhfs_lfid[nfpc*scidx+cur_lfid] = i;
                   }
               }
           }
       }

     delete [] is_index;
     delete [] v2oe_v1;
     delete [] v2oe_v2;
     delete [] v2hf_map_cid;
     delete [] v2hf_map_lfid;

     return MB_SUCCESS;

  }


  ErrorCode HalfFacetRep::determine_incident_halffaces( Range &cells)
  {
    ErrorCode error;

    int index = get_index_in_lmap(*cells.begin());
    int nvpc = lConnMap3D[index].num_verts_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;
  

    for (Range::iterator cid = cells.begin(); cid != cells.end(); ++cid){
      EntityHandle cell = *cid;
      const EntityHandle* conn;
      error = mb->get_connectivity(*cid, conn, nvpc);MB_CHK_ERR(error);

      int cidx = _cells.index(cell);
      
      for(int i=0; i<nvpc; ++i){
          EntityHandle v = conn[i];
          int vidx = _verts.index(v);
          EntityHandle vcid = v2hf_cid[vidx];

          int nhf_pv = lConnMap3D[index].v2hf_num[i];

	  for (int j=0; j < nhf_pv; ++j){
	      int ind = lConnMap3D[index].v2hf[i][j];
	      EntityHandle sib_cid = sibhfs_cid[nfpc*cidx+ind];

	      if (vcid==0){
		  v2hf_cid[vidx] = cell;
		  v2hf_lfid[vidx] = ind;
		  break;
		}
	      else if ((vcid!=0) && (sib_cid ==0)){
		  v2hf_cid[vidx] = cell;
		  v2hf_lfid[vidx] = ind;
		}
	    }
	}
      }
    
    return MB_SUCCESS;
  }

  ErrorCode  HalfFacetRep::determine_border_vertices( Range &cells, Tag isborder)
  {
    ErrorCode error;
    EntityHandle start_cell = *cells.begin();
    
    int index = get_index_in_lmap(start_cell);
    int nvpc = lConnMap3D[index].num_verts_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;

    int val= 1;

    for(Range::iterator t= cells.begin(); t !=cells.end(); ++t){

        const EntityHandle* conn;
        error = mb->get_connectivity(*t, conn, nvpc);MB_CHK_ERR(error);

        int cidx = _cells.index(*t);

        for (int i = 0; i < nfpc; ++i){
            EntityHandle sib_cid = sibhfs_cid[nfpc*cidx+i];

            if (sib_cid ==0){
                int nvF = lConnMap3D[index].hf2v_num[i];

                for (int j = 0; j < nvF; ++j){
                    int ind = lConnMap3D[index].hf2v[i][j];
                    error = mb->tag_set_data(isborder, &conn[ind], 1, &val); MB_CHK_ERR(error);
                  }
              }
          }
      }

    return MB_SUCCESS;
  }
  ////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::get_up_adjacencies_vert_3d(EntityHandle vid, std::vector<EntityHandle> &adjents)
  {
      ErrorCode error;
      adjents.reserve(20);

      // Obtain a half-face incident on v
      int vidx = _verts.index(vid);
      EntityHandle cur_cid = v2hf_cid[vidx];

      int index = get_index_in_lmap(cur_cid);
      int nvpc = lConnMap3D[index].num_verts_in_cell;
      int nfpc = lConnMap3D[index].num_faces_in_cell;

      // Collect all incident cells
      if (cur_cid != 0){
          int Stksize = 0, count = -1;
          Stkcells[0] = cur_cid;

          while (Stksize >= 0 ){
              cur_cid = Stkcells[Stksize];
              Stksize -= 1 ;

              bool found = find_match_in_array(cur_cid, trackcells, count);
              if (!found){
                  count += 1;
                  trackcells[count] = cur_cid;

                  // Add the current cell
                  adjents.push_back(cur_cid);
              }

              // Connectivity of the cell
              const EntityHandle* conn;
              error = mb->get_connectivity(cur_cid, conn, nvpc);MB_CHK_ERR(error);

              // Local id of vid in the cell and the half-faces incident on it
              int lv = -1;
              for (int i = 0; i< nvpc; ++i){
                  if (conn[i] == vid)
                    {
                      lv = i;
                      break;
                    }
              };

              int nhf_thisv = lConnMap3D[index].v2hf_num[lv];

              int cidx = _cells.index(cur_cid);

              // Add new cells into the stack
              EntityHandle ngb;
              for (int i = 0; i < nhf_thisv; ++i){
                  int ind = lConnMap3D[index].v2hf[lv][i];
                  ngb = sibhfs_cid[nfpc*cidx+ind];

                  if (ngb) {
                      bool found_ent = find_match_in_array(ngb, trackcells, count);

                      if (!found_ent){
                          Stksize += 1;
                          Stkcells[Stksize] = ngb;
                      }
                  }
              }
          }


          //Change the visited faces to false
          for (int i = 0; i<Stksize; i++)          
              Stkcells[i] = 0;         

          for (int i = 0; i <= count; i++)
              trackcells[i] = 0;
      }

      return MB_SUCCESS;
  }


  ErrorCode HalfFacetRep::get_up_adjacencies_edg_3d( EntityHandle eid,
                                                     std::vector<EntityHandle> &adjents,
                                                     std::vector<int> * leids)
  {
    ErrorCode error; 
    
    EntityHandle cid=0;
    int leid=0;
    //Find one incident cell
    bool found = find_matching_implicit_edge_in_cell(eid, &cid, &leid);

    //Find all incident cells     
    if (found)
      {
        error =get_up_adjacencies_edg_3d(cid, leid, adjents, leids);  MB_CHK_ERR(error);
      }

    return MB_SUCCESS;
  }


  ErrorCode HalfFacetRep::get_up_adjacencies_edg_3d( EntityHandle cid,
                                                     int leid,
                                                     std::vector<EntityHandle> &adjents,
                                                     std::vector<int> * leids,
                                                     std::vector<int> *adj_orients)
  {
    ErrorCode error;

    int index = get_index_in_lmap(cid);
    int nvpc = lConnMap3D[index].num_verts_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;

    adjents.reserve(20);
    adjents.push_back(cid);

    if (leids != NULL)
      {
        leids->reserve(20);
        leids->push_back(leid);
      }
    if (adj_orients != NULL)
      {
        adj_orients->reserve(20);
        adj_orients->push_back(1);
      }

    const EntityHandle* conn;
    error = mb->get_connectivity(cid, conn, nvpc);MB_CHK_ERR(error);

    // Get the end vertices of the edge <cid,leid>
    int id = lConnMap3D[index].e2v[leid][0];
    EntityHandle vert0 = conn[id];
    id = lConnMap3D[index].e2v[leid][1];
    EntityHandle vert1 = conn[id];

    //Loop over the two incident half-faces on this edge
    for(int i=0; i<2; i++)
      {
        EntityHandle cur_cell = cid;
        int cur_leid = leid;

        int lface = i;

	while(true){
	  int lfid = lConnMap3D[index].e2hf[cur_leid][lface];
	  int cidx = _cells.index(cur_cell);
	  cur_cell = sibhfs_cid[nfpc*cidx+lfid];
	  lfid = sibhfs_lfid[nfpc*cidx+lfid];
	  
	  //Check if loop reached starting cell or a boundary
	  if ((cur_cell == cid) || ( cur_cell==0))
	    break;

	  const EntityHandle* sib_conn;
	  error = mb->get_connectivity(cur_cell, sib_conn, nvpc);MB_CHK_ERR(error);

	  //Find the local edge id wrt to sibhf
	  int nv_curF = lConnMap3D[index].hf2v_num[lfid];
	  int lv0 = -1, lv1 = -1, idx = -1 ;
	  for (int j = 0; j<nv_curF; j++)
	    {
	      idx = lConnMap3D[index].hf2v[lfid][j];
	      if (vert0 == sib_conn[idx])
		lv0 = idx;
	      if (vert1 == sib_conn[idx])
		lv1 = idx;
	    }

          assert((lv0 >= 0) && (lv1 >= 0));
          cur_leid = lConnMap3D[index].lookup_leids[lv0][lv1];

          int chk_lfid = lConnMap3D[index].e2hf[cur_leid][0];

	  if (lfid == chk_lfid)
	    lface = 1;
	  else
	    lface = 0;

	  //insert new incident cell and local edge ids
	  adjents.push_back(cur_cell);

	  if (leids != NULL)
	    leids->push_back(cur_leid);

	  if (adj_orients != NULL)
	    {
	      int id1 =  lConnMap3D[index].e2v[cur_leid][0];
	      int id2 =  lConnMap3D[index].e2v[cur_leid][1];
	      if ((vert0 == sib_conn[id1]) && (vert1 == sib_conn[id2]))
		adj_orients->push_back(1);
	      else if ((vert0 == sib_conn[id2]) && (vert1 == sib_conn[id1]))
		adj_orients->push_back(0);
	    }
	}

	//Loop back
	if (cur_cell != 0)
	  break;
      }
    return MB_SUCCESS;
  }
 

  ErrorCode  HalfFacetRep::get_up_adjacencies_face_3d( EntityHandle fid,
                                                       std::vector<EntityHandle> &adjents,
                                                       std::vector<int> * lfids)
  {
    ErrorCode error;

    EntityHandle cid = 0;
    int lid = 0;
    bool found = find_matching_halfface(fid, &cid, &lid);

    if (found){
      error = get_up_adjacencies_face_3d(cid, lid, adjents,  lfids); MB_CHK_ERR(error);
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::get_up_adjacencies_face_3d( EntityHandle cid,
                                                      int lfid,
                                                      std::vector<EntityHandle> &adjents,
                                                      std::vector<int> * lfids)
  {

    EntityHandle start_cell = *_cells.begin();
    int index = get_index_in_lmap(start_cell);
    int nfpc = lConnMap3D[index].num_faces_in_cell;

    adjents.reserve(4);
    adjents.push_back(cid);

    if (lfids != NULL)
      {
        lfids->reserve(4);
        lfids->push_back(lfid);
      }

    int cidx = _cells.index(cid);
    EntityHandle sibcid = sibhfs_cid[nfpc*cidx+lfid];
    int siblid = sibhfs_lfid[nfpc*cidx+lfid];

    if (sibcid !=  0)
      {
        adjents.push_back(sibcid);
        if (lfids != NULL)
          lfids->push_back(siblid);
      }

    return MB_SUCCESS;
  }

 bool HalfFacetRep::find_matching_implicit_edge_in_cell( EntityHandle eid,
                                                         EntityHandle *cid,
                                                         int *leid)
  {
    ErrorCode error;

    // Find the edge vertices
    const EntityHandle* econn;
    int num_conn = 0;
    error = mb->get_connectivity(eid, econn, num_conn);MB_CHK_ERR(error);

    EntityHandle v_start = econn[0], v_end = econn[1];
    int v1idx = _verts.index(v_start);
    int v2idx = _verts.index(v_end);

    // Find an incident cell to v_start
    EntityHandle cell2origin, cell2end;
    cell2origin = v2hf_cid[v1idx];
    cell2end = v2hf_cid[v2idx];
    
    bool found = false;
    if (cell2origin == 0|| cell2end == 0){
      return found;
    }
    
    EntityHandle start_cell = *_cells.begin();
    int index = get_index_in_lmap(start_cell);
    int nvpc = lConnMap3D[index].num_verts_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;

    cellq[0] = cell2origin;
    cellq[1] = cell2end;

    int qsize = 2, num_qvals = 0;

    while (num_qvals < qsize){
        EntityHandle cell_id = cellq[num_qvals];
        num_qvals += 1;

        const EntityHandle* conn;
        error = mb->get_connectivity(cell_id, conn, nvpc);MB_CHK_ERR(error);

        int lv0 = -1, lv1 = -1, lv = -1;

        //locate v_origin in poped out tet, check if v_end is in
        for (int i = 0; i<nvpc; i++){
            if (v_start == conn[i]){
                lv0 = i;
                lv = lv0;
              }
            else if (v_end == conn[i]){
                lv1 = i;
                lv = lv1;
              }
          }

        if ((lv0 >= 0) && (lv1 >= 0))
          {
            found = true;
            *cid = cell_id;
            *leid = lConnMap3D[index].lookup_leids[lv0][lv1];
            break;
          }

        //push back new found unchecked incident tets of v_start
        int cidx = _cells.index(cell_id);
        int nhf_thisv = lConnMap3D[index].v2hf_num[lv];

        for (int i = 0; i < nhf_thisv; i++){
            int ind = lConnMap3D[index].v2hf[lv][i];
            EntityHandle ngb = sibhfs_cid[nfpc*cidx+ind];

            if (ngb){
                bool found_ent = find_match_in_array(ngb, &cellq[0], qsize-1);

                if (!found_ent)
                  {
                    cellq[qsize] = ngb;
                    qsize += 1;
                  }
              }
          }
      }

    for (int i = 0; i<qsize; i++)
        cellq[i] = 0;

    
    return found;
 }

  bool HalfFacetRep::find_matching_halfface(EntityHandle fid, EntityHandle *cid, int *lid)
  {
    ErrorCode error;
    EntityHandle start_cell = *_cells.begin();
    int index = get_index_in_lmap(start_cell);
    int nvpc = lConnMap3D[index].num_verts_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;
    EntityType ftype = mb->type_from_handle(fid);
    int nvF = lConnMap2D[ftype-2].num_verts_in_face;

    const EntityHandle* fid_verts;
    error = mb->get_connectivity(fid, fid_verts, nvF);MB_CHK_ERR(error);

    int vidx = _verts.index(fid_verts[0]);
    EntityHandle cur_cid = v2hf_cid[vidx];

    bool found = false;

    if (cur_cid != 0){
        int Stksize = 0, count = -1;
        Stkcells[0] = cur_cid;

        while (Stksize >= 0 ){
            cur_cid = Stkcells[Stksize];
            Stksize -= 1 ;
            count += 1;
            trackcells[count] = cur_cid;

            const EntityHandle* conn;
            error = mb->get_connectivity(cur_cid, conn, nvpc);MB_CHK_ERR(error);

            // Local id of fid_verts[0] in the cell
            int lv0 = -1;
            for (int i = 0; i< nvpc; ++i){
                if (conn[i] == fid_verts[0])
                {
                    lv0 = i;
                    break;
                }
            };

            int nhf_thisv = lConnMap3D[index].v2hf_num[lv0];

            // Search each half-face to match input face
            for(int i = 0; i < nhf_thisv; ++i){
                int lfid = lConnMap3D[index].v2hf[lv0][i];
                int nv_curF = lConnMap3D[index].hf2v_num[lfid];
                if (nv_curF != nvF)
                    continue;

                // Connectivity of the current half-face

                std::vector<EntityHandle> vthisface(nvF);
                for(int l = 0; l < nvF; ++l){
                    int ind = lConnMap3D[index].hf2v[lfid][l];
                    vthisface[l] = conn[ind];
                };


                // Match this half-face with input fid
                int direct,offset;
                bool they_match = CN::ConnectivityMatch(&vthisface[0],&fid_verts[0],nvF,direct,offset);

                if (they_match){
                    found = true;
                    cid[0] = cur_cid;
                    lid[0] = lfid;

                    break;
                }
            }

            //If a matching local face is found, return from this code.
            if (found)
              {
                return found;
                break;
              }

            // Add other cells that are incident on fid_verts[0]
            int cidx = _cells.index(cur_cid);

            // Add new cells into the stack
            EntityHandle ngb;
            for (int i = 0; i < nhf_thisv; ++i){
                int ind = lConnMap3D[index].v2hf[lv0][i];
                ngb = sibhfs_cid[nfpc*cidx+ind];

                if (ngb) {

                    bool found_ent = find_match_in_array(ngb, trackcells, count);

                    if (!found_ent){
                        Stksize += 1;
                        Stkcells[Stksize] = ngb;
                    }
                }
            }
        }

        //Change the visited faces to false
        for (int i = 0; i<Stksize; i++)
            Stkcells[i] = 0;

        for (int i = 0; i <= count; i++)
            trackcells[i] = 0;


    }
    return found;
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::get_neighbor_adjacencies_3d( EntityHandle cid, std::vector<EntityHandle> &adjents)
  {
    adjents.reserve(20);
    int index = get_index_in_lmap(cid);
    int nfpc = lConnMap3D[index].num_faces_in_cell;
    int cidx = _cells.index(cid);

    if (cid != 0 ){
      for (int lfid = 0; lfid < nfpc; ++lfid){      
          EntityHandle sibcid = sibhfs_cid[nfpc*cidx+lfid];
          if (sibcid != 0)
            adjents.push_back(sibcid);
      }    
    }

    return MB_SUCCESS; 
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::get_down_adjacencies_edg_3d(EntityHandle cid, std::vector<EntityHandle> &adjents)
  {
    //TODO: Try intersection without using std templates
      //Returns explicit edges, if any, of the face
      ErrorCode error;
      adjents.reserve(20);
      int index = get_index_in_lmap(cid);
      int nvpc = lConnMap3D[index].num_verts_in_cell;
      int nepc = lConnMap3D[index].num_edges_in_cell;

      const EntityHandle* conn;
      error = mb->get_connectivity(cid, conn, nvpc);MB_CHK_ERR(error);

      //Gather all the incident edges on each vertex of the face
      std::vector< std::vector<EntityHandle> > temp(nvpc);
      for (int i=0; i<nvpc; i++)
      {
          error = get_up_adjacencies_1d(conn[i], temp[i]);MB_CHK_ERR(error);
          std::sort(temp[i].begin(), temp[i].end());
      }

      //Loop over all the local edges and find the intersection.
      for (int i = 0; i < nepc; ++i)
      {
          std::vector<EntityHandle> common(10);

          int lv0 = lConnMap3D[index].e2v[i][0];
          int lv1 = lConnMap3D[index].e2v[i][1];

          std::set_intersection(temp[lv0].begin(), temp[lv0].end(), temp[lv1].begin(), temp[lv1].end(), common.begin());
          if (*common.begin() == 0)
              continue;

          adjents.push_back(*common.begin());
      }
      return MB_SUCCESS;
  }


  ErrorCode HalfFacetRep::get_down_adjacencies_face_3d(EntityHandle cid, std::vector<EntityHandle> &adjents)
  {
      //Returns explicit face, if any of the cell
      ErrorCode error;
      adjents.reserve(10);
      int index = get_index_in_lmap(cid);
      int nvpc = lConnMap3D[index].num_verts_in_cell;
      int nfpc = lConnMap3D[index].num_faces_in_cell;

      //Get the connectivity of the input cell
      const EntityHandle* conn;
      error = mb->get_connectivity(cid, conn, nvpc);MB_CHK_ERR(error);

      //Collect all the half-faces of the cell
      EntityHandle half_faces[6][4];
      for (int i=0; i<nfpc; i++)
        {
          int nvf = lConnMap3D[index].hf2v_num[i];
          for (int j=0; j< nvf; j++)
            {
              int ind = lConnMap3D[index].hf2v[i][j];
              half_faces[i][j] = conn[ind];
            }
        }

      //Add two vertices for which the upward adjacencies are computed
      int search_verts[2] = {0,0};
      if (index==0)
        search_verts[1] = 1;
      else if (index ==1)
        search_verts[1] = 4;
      else if (index == 2)
        search_verts[1] = 5;
      else
        search_verts[1] =6;

      std::vector<EntityHandle> temp;
      temp.reserve(20);
      for (int i=0; i<2; i++)
        {
          //Get the incident faces on the local vertex
          int lv = search_verts[i];
          temp.clear();
          error = get_up_adjacencies_vert_2d(conn[lv], temp);MB_CHK_ERR(error);

          if (temp.size() == 0)
              continue;

          //Get the half-faces incident on the local vertex and match it with the obtained faces
          int nhfthisv =  lConnMap3D[index].v2hf_num[lv];
          for (int k=0; k<(int)temp.size(); k++)
            {
              const EntityHandle* fid_verts;
              int fsize = 0;
              error = mb->get_connectivity(temp[k], fid_verts, fsize);MB_CHK_ERR(error);

              for (int j=0; j<nhfthisv; j++)
                {
                  //Collect all the vertices of this half-face
                  int idx = lConnMap3D[index].v2hf[lv][j];
                  int nvF = lConnMap3D[index].hf2v_num[idx];

                  if  (fsize != nvF)
                    continue;

                  int direct,offset;
                  bool they_match = CN::ConnectivityMatch(&half_faces[idx][0],&fid_verts[0],nvF,direct,offset);

                  if (they_match)
                    {
                      bool found = false;
                      for (int p=0; p<(int)adjents.size(); p++)
                        {
                          if (adjents[p] == temp[k])
                            {
                              found = true;
                              break;
                            }
                        }
                      if (!found)
                        adjents.push_back(temp[k]);
                    }
                }
            }
        }

      return MB_SUCCESS;
  }
  ////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::find_total_edges_faces_3d(Range cells, int *nedges, int *nfaces)
  {
    ErrorCode error;
    int index = get_index_in_lmap(*cells.begin());
    int nepc = lConnMap3D[index].num_edges_in_cell;
    int nfpc = lConnMap3D[index].num_faces_in_cell;
    int ncells = cells.size();
    int total_edges = nepc*ncells;
    int total_faces = nfpc*ncells;

    std::vector<int> trackE(total_edges, 0);
    std::vector<int> trackF(total_faces,0);

    std::vector<EntityHandle> inc_cids, sib_cids;
    std::vector<int> inc_leids, sib_lfids;

    for (Range::iterator it = cells.begin(); it != cells.end(); it++)
      {
        //Count edges
        for (int i=0; i<nepc; i++)
          {
            inc_cids.clear();
            inc_leids.clear();

            int id = nepc*(cells.index(*it))+i;
            if (!trackE[id])
              {
                error = get_up_adjacencies_edg_3d(*it, i, inc_cids, &inc_leids);MB_CHK_ERR(error);

                total_edges -= inc_cids.size() -1;
                for (int j=0; j < (int)inc_cids.size(); j++)
                  trackE[nepc*(cells.index(inc_cids[j]))+inc_leids[j]] = 1;
              }
          }

        //Count faces
        for (int i=0; i<nfpc; i++)
          {
            sib_cids.clear();
            sib_lfids.clear();

            int id = nfpc*(cells.index(*it))+i;
            if (!trackF[id])
              {
                error = get_up_adjacencies_face_3d(*it, i, sib_cids, &sib_lfids);MB_CHK_ERR(error);

                if (sib_cids.size() ==1)
                  continue;

                total_faces -= sib_cids.size() -1;
                trackF[nfpc*(cells.index(sib_cids[1]))+sib_lfids[1]] = 1;
              }
          }
      }

    nedges[0] = total_edges;
    nfaces[0] = total_faces;

    return MB_SUCCESS;

  }


  bool HalfFacetRep::find_match_in_array(EntityHandle ent, EntityHandle *ent_list, int count, bool get_index, int *index)
  {
    bool found = false;
    for (int i = 0; i<= count; i++)
      {
        if (ent == ent_list[i])
          {
            found = true;
            if (get_index)
              *index = i;
            break;
          }
      }

    return found;
  }

  ///////////////////////////////////////////////////////////////////////////////////////////
  ErrorCode HalfFacetRep::resize_hf_maps(int nverts, int nedges, int nfaces, int ncells)
  {

    if (nedges)
      {
        int insz = sibhvs_eid.size();
        int nwsz = nedges*2;
        sibhvs_eid.resize(insz+nwsz, 0);
        sibhvs_lvid.resize(insz+nwsz, 0);

        insz = v2hv_eid.size();
        nwsz = nverts;
        v2hv_eid.resize(insz+nwsz, 0);
        v2hv_lvid.resize(insz+nwsz, 0);
      }
   if (nfaces)
     {
       EntityType ftype = mb->type_from_handle(*_faces.begin());
       int nepf = lConnMap2D[ftype-2].num_verts_in_face;
       int insz = sibhes_fid.size();
       int nwsz = nfaces*nepf;
       sibhes_fid.resize(insz+nwsz, 0);
       sibhes_leid.resize(insz+nwsz, 0);

       insz = v2he_fid.size();
       nwsz = nverts;
       v2he_fid.resize(insz+nwsz, 0);
       v2he_leid.resize(insz+nwsz, 0);
     }
   if (ncells)
     {
       int index = get_index_in_lmap(*_cells.begin());
       int nfpc = lConnMap3D[index].num_faces_in_cell;

       int insz = sibhfs_cid.size();
       int nwsz = ncells*nfpc;

       sibhfs_cid.resize(insz+nwsz, 0);
       sibhfs_lfid.resize(insz+nwsz, 0);

       insz = v2hf_cid.size();
       nwsz = nverts;

       v2hf_cid.resize(insz+nwsz, 0);
       v2hf_lfid.resize(insz+nwsz, 0);
     }

   return MB_SUCCESS;

  }

  ErrorCode HalfFacetRep::get_sibling_map(EntityType type, EntityHandle ent,  EntityHandle *sib_entids, int *sib_lids)
  {

    if (type == MBEDGE)
      {
        int eidx = _edges.index(ent);
        for (int i=0; i<2; i++)
          {
            sib_entids[i] = sibhvs_eid[2*eidx+i];
            sib_lids[i] = sibhvs_lvid[2*eidx+i];
          }
      }
    else if (type == MBTRI || type == MBQUAD)
      {
       int nepf = lConnMap2D[type-2].num_verts_in_face;
       int fidx = _faces.index(ent);

        for (int i=0; i<nepf; i++)
          {
            sib_entids[i] = sibhes_fid[nepf*fidx+i];
            sib_lids[i] = sibhes_leid[nepf*fidx+i];
          }
      }
    else
      {
        int idx = get_index_in_lmap(*_cells.begin());
        int nfpc = lConnMap3D[idx].num_faces_in_cell;
        int cidx = _cells.index(ent);

        for (int i=0; i<nfpc; i++)
          {
            sib_entids[i] = sibhfs_cid[nfpc*cidx+i];
            sib_lids[i] = sibhfs_lfid[nfpc*cidx+i];
          }

      }
    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::get_sibling_map(EntityType type, EntityHandle ent, int lid,  EntityHandle *sib_entid, int *sib_lid)
  {

    if (type == MBEDGE)
      {
        int eidx = _edges.index(ent);
        sib_entid[0] = sibhvs_eid[2*eidx+lid];
        sib_lid[0] = sibhvs_lvid[2*eidx+lid];
      }
    else if (type == MBTRI || type == MBQUAD)
      {
       int nepf = lConnMap2D[type-2].num_verts_in_face;
       int fidx = _faces.index(ent);

       sib_entid[0] = sibhes_fid[nepf*fidx+lid];
       sib_lid[0] = sibhes_leid[nepf*fidx+lid];
      }
    else
      {
        int idx = get_index_in_lmap(*_cells.begin());
        int nfpc = lConnMap3D[idx].num_faces_in_cell;
        int cidx = _cells.index(ent);

        sib_entid[0] = sibhfs_cid[nfpc*cidx+lid];
        sib_lid[0] = sibhfs_lfid[nfpc*cidx+lid];
      }
    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::set_sibling_map(EntityType type, EntityHandle ent, EntityHandle *set_entids, int *set_lids)
  {

    if (type == MBEDGE)
      {
        int eidx = _edges.index(ent);
        for (int i=0; i<2; i++)
          {
            sibhvs_eid[2*eidx+i] =  set_entids[i] ;
            sibhvs_lvid[2*eidx+i] = set_lids[i] ;
          }
      }
    else if (type == MBTRI || type == MBQUAD)
      {
        int nepf = lConnMap2D[type-2].num_verts_in_face;
        int fidx = _faces.index(ent);

         for (int i=0; i<nepf; i++)
           {
             sibhes_fid[nepf*fidx+i] = set_entids[i] ;
             sibhes_leid[nepf*fidx+i] = set_lids[i] ;
           }
      }
    else
      {
        int idx = get_index_in_lmap(*_cells.begin());
        int nfpc = lConnMap3D[idx].num_faces_in_cell;
        int cidx = _cells.index(ent);

        for (int i=0; i<nfpc; i++)
          {
            sibhfs_cid[nfpc*cidx+i] = set_entids[i];
            sibhfs_lfid[nfpc*cidx+i] = set_lids[i] ;
          }
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::set_sibling_map(EntityType type, EntityHandle ent, int lid, EntityHandle *set_entid, int *set_lid)
  {

    if (type == MBEDGE)
      {
        int eidx = _edges.index(ent);

        sibhvs_eid[2*eidx+lid] =  set_entid[0] ;
        sibhvs_lvid[2*eidx+lid] = set_lid[0] ;
      }
    else if (type == MBTRI || type == MBQUAD)
      {
        int nepf = lConnMap2D[type-2].num_verts_in_face;
        int fidx = _faces.index(ent);

        sibhes_fid[nepf*fidx+lid] = set_entid[0] ;
        sibhes_leid[nepf*fidx+lid] = set_lid[0] ;
      }
    else
      {
        int idx = get_index_in_lmap(*_cells.begin());
        int nfpc = lConnMap3D[idx].num_faces_in_cell;
        int cidx = _cells.index(ent);

        sibhfs_cid[nfpc*cidx+lid] = set_entid[0];
        sibhfs_lfid[nfpc*cidx+lid] = set_lid[0];
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::get_incident_map(EntityType type, EntityHandle vid, EntityHandle *inci_entid, int  *inci_lid)
  {
    int vidx = _verts.index(vid);

    if (type == MBEDGE)
      {   
        inci_entid[0] = v2hv_eid[vidx];
        inci_lid[0] = v2hv_lvid[vidx];
      }
    else if (type == MBTRI || type == MBQUAD)
      {
        inci_entid[0] = v2he_fid[vidx];
        inci_lid[0] = v2he_leid[vidx];
      }
    else
      {
        inci_entid[0] = v2hf_cid[vidx];
        inci_lid[0] = v2hf_lfid[vidx];
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::set_incident_map(EntityType type, EntityHandle vid, EntityHandle *set_entid, int *set_lid)
  {
    int vidx = _verts.index(vid);

    if (type == MBEDGE)
      {
        v2hv_eid[vidx] = set_entid[0];
        v2hv_lvid[vidx] = set_lid[0];
      }
    else if (type == MBTRI || type == MBQUAD)
      {
        v2he_fid[vidx] = set_entid[0];
        v2he_leid[vidx] = set_lid[0];
      }
    else
      {
        v2hf_cid[vidx] = set_entid[0];
        v2hf_lfid[vidx] = set_lid[0];
      }

    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::get_entity_ranges(Range &verts, Range &edges, Range &faces, Range &cells)
  {
    verts = _verts;
    edges = _edges;
    faces = _faces;
    cells = _cells;
    return MB_SUCCESS;
  }

  ErrorCode HalfFacetRep::update_entity_ranges()
  {
   ErrorCode error;
   Range verts, edges, faces, cells;

   error = mb->get_entities_by_dimension(0, 0, verts);MB_CHK_ERR(error);

   error = mb->get_entities_by_dimension(0, 1, edges);MB_CHK_ERR(error);

   error = mb->get_entities_by_dimension(0, 2, faces);MB_CHK_ERR(error);

   error = mb->get_entities_by_dimension(0, 3, cells);MB_CHK_ERR(error);

   _verts = verts;
   _edges = edges;
   _faces = faces;
   _cells = cells;

   return MB_SUCCESS;
  }


} // namespace moab

