/**
 * MOAB, a Mesh-Oriented datABase, is a software component for creating,
 * storing and accessing finite element mesh data.
 * 
 * Copyright 2004 Sandia Corporation.  Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Coroporation, the U.S. Government
 * retains certain rights in this software.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 */

#ifdef WIN32
#pragma warning (disable : 4786)
#endif

#include "MeshTopoUtil.hpp"
#include "MBRange.hpp"
#include "MBInternals.hpp"

#include <assert.h>

    //! generate all the AEntities bounding the vertices
MBErrorCode MeshTopoUtil::construct_aentities(const MBRange &vertices) 
{
  MBRange out_range;
  MBErrorCode result;
  result = mbImpl->get_adjacencies(vertices, 1, true, out_range, MBInterface::UNION);
  if (MB_SUCCESS != result) return result;
  out_range.clear();
  result = mbImpl->get_adjacencies(vertices, 2, true, out_range, MBInterface::UNION);
  if (MB_SUCCESS != result) return result;
  out_range.clear();
  result = mbImpl->get_adjacencies(vertices, 3, true, out_range, MBInterface::UNION);

  return result;
}

    //! given an entity, get its average position (avg vertex locations)
MBErrorCode MeshTopoUtil::get_average_position(const MBEntityHandle entity,
                                               double *avg_position) 
{
  const MBEntityHandle *connect;
  int num_connect;
  if (MBVERTEX == mbImpl->type_from_handle(entity))
    return mbImpl->get_coords(&entity, 1, avg_position);
    
  MBErrorCode result = mbImpl->get_connectivity(entity, connect, num_connect);
  if (MB_SUCCESS != result) return result;
  double dum_pos[3];
  avg_position[0] = avg_position[1] = avg_position[2] = 0.0;
  for (int i = 0; i < num_connect; i++) {
    result = mbImpl->get_coords(connect+i, 1, dum_pos);
    if (MB_SUCCESS != result) return result;
    avg_position[0] += dum_pos[0]; 
    avg_position[1] += dum_pos[1]; 
    avg_position[2] += dum_pos[2]; 
  }
  avg_position[0] /= (double) num_connect;
  avg_position[1] /= (double) num_connect;
  avg_position[2] /= (double) num_connect;

  return MB_SUCCESS;
}

  // given an entity, find the entities of next higher dimension around
  // that entity, ordered by connection through next higher dimension entities; 
  // if any of the star entities is in only entity of next higher dimension, 
  // on_boundary is returned true
MBErrorCode MeshTopoUtil::star_entities(const MBEntityHandle star_center,
                                        std::vector<MBEntityHandle> &star_entities,
                                        bool &bdy_entity,
                                        const MBEntityHandle starting_star_entity,
                                        std::vector<MBEntityHandle> *star_entities_dp1,
                                        MBRange *star_candidates_dp1)
{
    // now start the traversal
  bdy_entity = false;
  MBEntityHandle last_entity = starting_star_entity, last_dp1 = 0, next_entity, next_dp1;
  std::vector<MBEntityHandle> star_dp1;

  do {
      // get the next star entity
    MBErrorCode result = star_next_entity(star_center, last_entity, last_dp1,
                                          star_candidates_dp1,
                                          next_entity, next_dp1);
    if (MB_SUCCESS != result) return result;
    
      // special case: if starting_star_entity isn't connected to any entities of next
      // higher dimension, it's the only entity in the star; put it on the list and return
    if (star_entities.empty() && next_entity == 0 && next_dp1 == 0 &&
        star_candidates_dp1 == NULL) {
      star_entities.push_back(last_entity);
      bdy_entity = true;
      return MB_SUCCESS;
    }

      // if we're at a bdy and bdy_entity hasn't been set yet, we're at the
      // first bdy; reverse the lists and start traversing in the other direction; but,
      // pop the last star entity off the list and find it again, so that we properly
      // check for next_dp1
    if (0 == next_dp1 && !bdy_entity) {
      star_entities.push_back(next_entity);
      bdy_entity = true;
      std::reverse(star_entities.begin(), star_entities.end());
      std::reverse(star_dp1.begin(), star_dp1.end());
      star_entities.pop_back();
      last_entity = star_entities.back();
      last_dp1 = star_dp1.back();
    }
      // else if we're not on the bdy and next_entity is already in star, that means
      // we've come all the way around; don't put next_entity on list again, and
      // zero out last_dp1 to terminate while loop
    else if (!bdy_entity && 
             std::find(star_entities.begin(), star_entities.end(), next_entity) != 
             star_entities.end())
    {
      last_dp1 = 0;
    }

      // else, just assign last entities seen and go on to next iteration
    else {
      star_entities.push_back(next_entity);
      if (0 != next_dp1) star_dp1.push_back(next_dp1);
      last_entity = next_entity;
      last_dp1 = next_dp1;
    }
  }
  while (0 != last_dp1);
  
    // copy over the star_dp1 list, if requested
  if (NULL != star_entities_dp1) 
    (*star_entities_dp1).swap(star_dp1);
  
  return MB_SUCCESS;
}

MBErrorCode MeshTopoUtil::star_next_entity(const MBEntityHandle star_center,
                                           const MBEntityHandle last_entity,
                                           const MBEntityHandle last_dp1,
                                           MBRange *star_candidates_dp1,
                                           MBEntityHandle &next_entity,
                                           MBEntityHandle &next_dp1) 
{
    // given a star_center, a last_entity (whose dimension should be 1 greater than center)
    // and last_dp1 (dimension 2 higher than center), returns the next star entity across
    // last_dp1, and the next dp1 entity sharing next_entity; if star_candidates is non-empty,
    // star must come from those
  MBRange from_ents, to_ents;
  from_ents.insert(star_center);
  if (0 != last_dp1) from_ents.insert(last_dp1);
  int dim = mbImpl->dimension_from_handle(star_center);
  
  MBErrorCode result = mbImpl->get_adjacencies(from_ents, dim+1, false, to_ents);
  if (MB_SUCCESS != result) return result;
  
    // remove last_entity from result, and should only have 1 left, if any
  if (0 != last_entity) to_ents.erase(last_entity);

    // if no last_dp1, contents of to_ents should share dp1-dimensional entity with last_entity
  if (0 != last_entity && 0 == last_dp1) {
    MBRange tmp_to_ents;
    for (MBRange::iterator rit = to_ents.begin(); rit != to_ents.end(); rit++) {
      if (0 != common_entity(last_entity, *rit, dim+2))
        tmp_to_ents.insert(*rit);
    }
    to_ents = tmp_to_ents;
  }
  
  if (!to_ents.empty()) next_entity = *to_ents.begin();
  else {
    next_entity = 0;
    next_dp1 = 0;
    return MB_SUCCESS;
  }
  
    // get next_dp1
  if (0 != star_candidates_dp1) to_ents = *star_candidates_dp1;
  else to_ents.clear();
  
  result = mbImpl->get_adjacencies(&next_entity, 1, dim+2, false, to_ents);
  if (MB_SUCCESS != result) return result;

    // can't be last one
  if (0 != last_dp1) to_ents.erase(last_dp1);
  
  if (!to_ents.empty()) next_dp1 = *to_ents.begin();

    // could be zero, means we're at bdy
  else next_dp1 = 0;

  return MB_SUCCESS;
}

    //! get "bridge" or "2nd order" adjacencies, going through dimension bridge_dim
MBErrorCode MeshTopoUtil::get_bridge_adjacencies(const MBEntityHandle from_entity,
                                                 const int bridge_dim,
                                                 const int to_dim,
                                                 MBRange &to_adjs) 
{
    // get pointer to connectivity for this entity
  const MBEntityHandle *connect;
  int num_connect;
  MBErrorCode result = mbImpl->get_connectivity(from_entity, connect, num_connect);
  if (MB_SUCCESS != result) return result;
  
  MBEntityType from_type = TYPE_FROM_HANDLE(from_entity);
  if (from_type >= MBENTITYSET) return MB_FAILURE;

  int from_dim = MBCN::Dimension(from_type);
  
  MBRange to_ents;

  if (MB_SUCCESS != result) return result;

  if (bridge_dim < from_dim) {
      // looping over each sub-entity of dimension bridge_dim...
    static MBEntityHandle bridge_verts[MB_MAX_SUB_ENTITIES];
    for (int i = 0; i < MBCN::NumSubEntities(from_type, bridge_dim); i++) {

        // get the vertices making up this sub-entity
      int num_bridge_verts;
      MBCN::SubEntityConn(connect, from_type, bridge_dim, i, &bridge_verts[0], num_bridge_verts);
    
        // get the to_dim entities adjacent
      to_ents.clear();
      MBErrorCode tmp_result = mbImpl->get_adjacencies(bridge_verts, num_bridge_verts,
                                                       to_dim, false, to_ents, MBInterface::INTERSECT);
      if (MB_SUCCESS != tmp_result) result = tmp_result;
    
      to_adjs.merge(to_ents);
    }

  }

    // now get the direct ones too, or only in the case where we're 
    // going to higher dimension for bridge
  MBRange bridge_ents, tmp_ents;
  tmp_ents.insert(from_entity);
  MBErrorCode tmp_result = mbImpl->get_adjacencies(tmp_ents, bridge_dim,
                                                   false, bridge_ents, 
                                                   MBInterface::UNION);
  if (MB_SUCCESS != tmp_result) return tmp_result;
    
  tmp_result = mbImpl->get_adjacencies(bridge_ents, to_dim, false, to_adjs, 
                                       MBInterface::UNION);
  if (MB_SUCCESS != tmp_result) return tmp_result;
  
    // if to_dimension is same as that of from_entity, make sure from_entity isn't
    // in list
  if (to_dim == from_dim) to_adjs.erase(from_entity);
  
  return result;
}

    //! return a common entity of the specified dimension, or 0 if there isn't one
MBEntityHandle MeshTopoUtil::common_entity(const MBEntityHandle ent1,
                                           const MBEntityHandle ent2,
                                           const int dim) 
{
  MBRange tmp_range, tmp_range2;
  tmp_range.insert(ent1); tmp_range.insert(ent2);
  MBErrorCode result = mbImpl->get_adjacencies(tmp_range, dim, false, tmp_range2);
  if (MB_SUCCESS != result || tmp_range2.empty()) return 0;
  else return *tmp_range2.begin();
}

MBErrorCode MeshTopoUtil::split_entities_manifold(MBRange &entities,
                                                  MBRange &new_entities,
                                                  MBRange *fill_entities) 
{
  MBRange tmp_range, *tmp_ptr_fill_entity;
  if (NULL != fill_entities) tmp_ptr_fill_entity = &tmp_range;
  else tmp_ptr_fill_entity = NULL;
  
  for (MBRange::iterator rit = entities.begin(); rit != entities.end(); rit++) {
    MBEntityHandle new_entity;
    if (NULL != tmp_ptr_fill_entity) tmp_ptr_fill_entity->clear();

    MBEntityHandle this_ent = *rit;
    MBErrorCode result = split_entities_manifold(&this_ent, 1, &new_entity, 
                                                 tmp_ptr_fill_entity);
    if (MB_SUCCESS != result) return result;

    new_entities.insert(new_entity);
    if (NULL != fill_entities) fill_entities->merge(*tmp_ptr_fill_entity);
  }
  
  return MB_SUCCESS;
}


MBErrorCode MeshTopoUtil::split_entities_manifold(MBEntityHandle *entities,
                                                  const int num_entities,
                                                  MBEntityHandle *new_entities,
                                                  MBRange *fill_entities)
{
    // split entities by duplicating them; splitting manifold means that there is at
    // most two higher-dimension entities bounded by a given entity; after split, the
    // new entity bounds one and the original entity bounds the other

#define ITERATE_RANGE(range, it) for (MBRange::iterator it = range.begin(); it != range.end(); it++)
#define GET_CONNECT_DECL(ent, connect, num_connect) \
  const MBEntityHandle *connect; int num_connect; \
  {MBErrorCode connect_result = mbImpl->get_connectivity(ent, connect, num_connect); \
   if (MB_SUCCESS != connect_result) return connect_result;}
#define GET_CONNECT(ent, connect, num_connect) \
  {MBErrorCode connect_result = mbImpl->get_connectivity(ent, connect, num_connect);\
   if (MB_SUCCESS != connect_result) return connect_result;}
#define TC if (MB_SUCCESS != tmp_result) {result = tmp_result; continue;}

  MBErrorCode result = MB_SUCCESS;
  for (int i = 0; i < num_entities; i++) {
    MBErrorCode tmp_result;
      
      // get original higher-dimensional bounding entities
    MBRange up_adjs[4];
      // can only do a split_manifold if there are at most 2 entities of each
      // higher dimension; otherwise it's a split non-manifold
    bool valid_up_adjs = true;
    for (unsigned int dim = MBCN::Dimension(TYPE_FROM_HANDLE(entities[i]))+1; 
         dim <= 3; dim++) {
      tmp_result = mbImpl->get_adjacencies(entities+i, 1, dim, false, up_adjs[dim]); TC;
      if (up_adjs[dim].size() > 2) {
        valid_up_adjs = false;
        break;
      }
    }
    if (!valid_up_adjs) return MB_FAILURE;
      
      // ok to split; create the new entity, with connectivity of the original
    GET_CONNECT_DECL(entities[i], connect, num_connect);
    MBEntityHandle new_entity;
    result = mbImpl->create_element(mbImpl->type_from_handle(entities[i]), connect, num_connect, 
                                    new_entity); TC;
      
      // by definition, new entity and original will be equivalent; need to add explicit
      // adjs to distinguish them; don't need to check if there's already one there,
      // 'cuz add_adjacency does that for us
    for (int dim = MBCN::Dimension(TYPE_FROM_HANDLE(entities[i]))+1; dim <= 3; dim++) {
      if (up_adjs[dim].empty()) continue;
        // first the new entity; if there's only one up_adj of this dimension, make the
        // new entity adjacent to it, so that any bdy information is preserved on the
        // original entity
      tmp_result = mbImpl->remove_adjacencies(entities[i], &(*(up_adjs[dim].begin())), 1);
        // (ok if there's an error, that just means there wasn't an explicit adj)
      MBEntityHandle tmp_ent = *(up_adjs[dim].begin());
      tmp_result = mbImpl->add_adjacencies(new_entity, &tmp_ent, 1, false); TC;
      if (up_adjs[dim].size() < 2) continue;
        // add adj to other up_adj
      tmp_ent = *(up_adjs[dim].rbegin());
      tmp_result = mbImpl->add_adjacencies(entities[i], &tmp_ent, 1, false); TC;
    }

      // if we're asked to build a next-higher-dimension object, do so
    MBEntityHandle fill_entity = 0;
    MBEntityHandle tmp_ents[2];
    if (NULL != fill_entities) {
        // how to do this depends on dimension
      switch (MBCN::Dimension(TYPE_FROM_HANDLE(entities[i]))) {
        case 0:
          tmp_ents[0] = entities[i];
          tmp_ents[1] = new_entity;
          tmp_result = mbImpl->create_element(MBEDGE, tmp_ents, 2, fill_entity); TC;
          break;
        case 1:
          tmp_result = mbImpl->create_element(MBPOLYGON, connect, 2, fill_entity); TC;
            // need to create explicit adj in this case
          tmp_result = mbImpl->add_adjacencies(entities[i], &fill_entity, 1, false); TC;
          tmp_result = mbImpl->add_adjacencies(new_entity, &fill_entity, 1, false); TC;
          break;
        case 2:
          tmp_ents[0] = entities[i];
          tmp_ents[1] = new_entity;
          tmp_result = mbImpl->create_element(MBPOLYHEDRON, connect, 2, fill_entity); TC;
          break;
      }
      if (0 == fill_entity) {
        result = MB_FAILURE;
        continue;
      }
      fill_entities->insert(fill_entity);
    }

    new_entities[i] = new_entity;
    
  } // end for over input entities

  return result;
}

MBErrorCode MeshTopoUtil::split_entity_nonmanifold(MBEntityHandle split_ent,
                                                   MBRange &new_ents) 
{

    // split an entity into multiple entities, one per (d+2)-connected region
    // in the (d+1)-star around the entity

    // get the regions
    // get the connected (d+1)-entities
  MBRange dp1_ents;
  int dim = mbImpl->dimension_from_handle(split_ent);
  MBErrorCode result = mbImpl->get_adjacencies(&split_ent, 1, dim+1, false, dp1_ents);
  if (MB_SUCCESS != result) return result;
  
    // while there are still entities, get another region
  std::vector<std::vector<MBEntityHandle> > star_regions;
  while (!dp1_ents.empty()) {
    std::vector<MBEntityHandle> this_region, this_region_dp2;
    bool is_bdy;
    result = star_entities(split_ent, this_region, is_bdy, *dp1_ents.begin(),
                           &this_region_dp2);
    if (MB_SUCCESS != result) return result;
    else if (!is_bdy) return MB_FAILURE;
      // star should include starting entity
    assert(std::find(this_region.begin(), this_region.end(), *dp1_ents.begin()) != 
           this_region.end());
    
      // fold the dp2-entities into the star then put in the list
    assert(this_region_dp2.size() == this_region.size()-1);
    std::vector<MBEntityHandle> folded_star;
    std::vector<MBEntityHandle>::iterator dp1_it = this_region.begin(),
      dp2_it = this_region_dp2.begin();
    for (; dp2_it != this_region_dp2.end(); dp1_it++, dp2_it++) {
      folded_star.push_back(*dp1_it);
      folded_star.push_back(*dp2_it);
    }
      // should be 1 more dp1 entity
    folded_star.push_back(*dp1_it);
    star_regions.push_back(folded_star);

      // remove these dp1 entities from the list
    MBRange dp1_copy;
    std::copy(this_region.begin(), this_region.end(), mb_range_inserter(dp1_copy));
    dp1_ents = dp1_ents.subtract(dp1_copy);
  }

    // should be at least 2 regions
  if (star_regions.size() < 2) return MB_FAILURE;
  
    // ok, have the regions; make new entities and add adjacencies to regions
  std::vector<std::vector<MBEntityHandle> >::iterator vvit = star_regions.begin();
  const MBEntityHandle *connect;
  int num_connect;
  result = mbImpl->get_connectivity(split_ent, connect, num_connect);
  if (MB_SUCCESS != result) return result;
  MBEntityType split_type = mbImpl->type_from_handle(split_ent);
  for (; vvit != star_regions.end(); vvit++) {
      // create the new entity
    MBEntityHandle new_entity;
    if (vvit != star_regions.begin()) {
      result = mbImpl->create_element(split_type, connect, num_connect, new_entity);
      if (MB_SUCCESS != result) return result;
      else if (0 == new_entity) return MB_FAILURE;
      new_ents.insert(new_entity);
    }
    else {
      new_entity = split_ent;
    }
    
      // remove any explicit adjacencies with split entity
    if (new_entity != split_ent) {
      result = mbImpl->remove_adjacencies(split_ent, &(*vvit)[0], vvit->size());
      if (MB_SUCCESS != result) return result;
    }
    
      //  add adjacency with new entity
    result = mbImpl->add_adjacencies(new_entity, &(*vvit)[0], vvit->size(), false);
    if (MB_SUCCESS != result) return result;
  }
  
  return result;

/*

Commented out for now, because I decided to do a different implementation
for the sake of brevity.  However, I still think this function is the right
way to do it, if I ever get the time.  Sigh.

    // split entity d, producing entity nd; generates various new entities,
    // see algorithm description in notes from 2/25/05
  const MBEntityHandle split_types = {MBEDGE, MBPOLYGON, MBPOLYHEDRON};
  MBErrorCode result = MB_SUCCESS;
  const int dim = MBCN::Dimension(TYPE_FROM_HANDLE(d));
  MeshTopoUtil mtu(this);

    // get all (d+2)-, (d+1)-cells connected to d
  MBRange dp2s, dp1s, dp1s_manif, dp2s_manif;
  result = get_adjacencies(&d, 1, dim+2, false, dp2s); RR;
  result = get_adjacencies(&d, 1, dim+1, false, dp1s); RR;

    // also get (d+1)-cells connected to d which are manifold
  get_manifold_dp1s(d, dp1s_manif);
  get_manifold_dp2s(d, dp2s_manif);
  
    // make new cell nd, then ndp1
  result = copy_entity(d, nd); RR;
  MBEntityHandle tmp_connect[] = {d, nd};
  MBEntityHandle ndp1;
  result = create_element(split_types[dim],
                          tmp_connect, 2, ndp1); RR;
  
    // modify (d+2)-cells, depending on what type they are
  ITERATE_RANGE(dp2s, dp2) {
      // first, get number of connected manifold (d+1)-entities
    MBRange tmp_range, tmp_range2(dp1s_manif);
    tmp_range.insert(*dp2);
    tmp_range.insert(d);
    tmp_result = get_adjacencies(tmp_range, 1, false, tmp_range2); TC;
    MBEntityHandle ndp2;
    
      // a. manif (d+1)-cells is zero
    if (tmp_range2.empty()) {
        // construct new (d+1)-cell
      MBEntityHandle ndp1a;
      MBEntityHandle tmp_result = create_element(split_types[dim],
                                                 tmp_connect, 2, ndp1a); TC;
        // now make new (d+2)-cell
      MBEntityHandle tmp_connect2[] = {ndp1, ndp1a};
      tmp_result = create_element(split_types[dim+1],
                                  tmp_connect2, 2, ndp2); TC;
        // need to add explicit adjacencies, since by definition ndp1, ndp1a will be equivalent
      tmp_result = add_adjacencies(ndp1a, &dp2, 1, false); TC;
      tmp_result = add_adjacencies(ndp1a, &ndp2, 1, false); TC;
      tmp_result = add_adjacencies(ndp1, &ndp2, 1, false); TC;

        // now insert nd into connectivity of dp2, right after d if dim < 1
      std::vector<MBEntityHandle> connect;
      tmp_result = get_connectivity(&dp2, 1, connect); TC;
      if (dim < 1) {
        std::vector<MBEntityHandle>::iterator vit = std::find(connect.begin(), connect.end(), d);
        if (vit == connect.end()) {
          result = MB_FAILURE;
          continue;
        }
        connect.insert(vit, nd);
      }
      else
        connect.push_back(nd);
      tmp_result = set_connectivity(dp2, connect); TC;

        // if dim < 1, need to add explicit adj from ndp2 to higher-dim ents, since it'll
        // be equiv to other dp2 entities
      if (dim < 1) {
        MBRange tmp_dp3s;
        tmp_result = get_adjacencies(&dp2, 1, dim+3, false, tmp_dp3s); TC;
        tmp_result = add_adjacencies(ndp2, tmp_dp3s, false); TC;
      }
    } // end if (tmp_range2.empty())

      // b. single manifold (d+1)-cell, which isn't adjacent to manifold (d+2)-cell
    else if (tmp_range2.size() == 1) {
        // b1. check validity, and skip if not valid

        // only change if not dp1-adjacent to manifold dp2cell; check that...
      MBRange tmp_adjs(dp2s_manif);
      tmp_result = get_adjacencies(&(*tmp_range2.begin()), 1, dim+2, false, tmp_adjs); TC;
      if (!tmp_adjs.empty()) continue;

      MBEntityHandle dp1 = *tmp_range2.begin();

        // b2. make new (d+1)- and (d+2)-cell next to dp2

        // get the (d+2)-cell on the other side of dp1
      tmp_result = get_adjacencies(&dp1, 1, dim+2, false, tmp_adjs); TC;
      MBEntityHandle odp2 = *tmp_adjs.begin();
      if (odp2 == dp2) odp2 = *tmp_adjs.rbegin();

        // get od, the d-cell on dp1_manif which isn't d
      tmp_result = get_adjacencies(&dp1_manif, 1, dim, false, tmp_adjs); TC;
      tmp_adjs.erase(d);
      if (tmp_adjs.size() != 1) {
        result = MB_FAILURE;
        continue;
      }
      MBEntityHandle od = *tmp_adjs.begin();

        // make a new (d+1)-cell from od and nd
      tmp_adjs.insert(nd);
      tmp_result = create_element(split_types[1], tmp_adjs, ndp1a); TC;
      
        // construct new (d+2)-cell from dp1, ndp1, ndp1a
      tmp_adjs.clear();
      tmp_adjs.insert(dp1); tmp_adjs.insert(ndp1); tmp_adjs.insert(ndp1a);
      tmp_result = create_element(split_types[2], tmp_adjs, ndp2); TC;

        // b3. replace d, dp1 in connect/adjs of odp2
      std::vector<MBEntityHandle> connect;
      tmp_result = get_connectivity(&odp2, 1, connect); TC;
      if (dim == 0) {
        *(std::find(connect.begin(), connect.end(), d)) = nd;
        remove_adjacency(dp1, odp2);
        

      
        // if dp1 was explicitly adj to odp2, remove it
      remove_adjacency

...

*/
}

