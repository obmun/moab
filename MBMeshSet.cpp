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

//
//-------------------------------------------------------------------------
// Filename      : MBMeshSet.cpp 
// Creator       : Tim Tautges
//
// Date          : 02/01/02
//
// Owner         : Tim Tautges
//
// Description   : MBMeshSet is the MB implementation of MBSet
//-------------------------------------------------------------------------

#ifdef WIN32
#ifdef _DEBUG
// turn off warnings that say they debugging identifier has been truncated
// this warning comes up when using some STL containers
#pragma warning(disable : 4786)
#endif
#endif



#include <algorithm>
#include <utility>
#include <set>
#include "assert.h"
#include "MBInternals.hpp"
#include "MBMeshSet.hpp"
#include "AEntityFactory.hpp"
#include "MBCN.hpp"

using namespace std;

#ifndef MB_MESH_SET_COMPACT_PARENT_CHILD_LISTS

MBMeshSet::MBMeshSet( bool track_ownership ) 
  : mTracking(track_ownership)
  {}

MBMeshSet::~MBMeshSet() 
{
}

static inline int insert_in_vector( MBMeshSet::LinkSet& s, MBEntityHandle h )
{
  MBMeshSet::LinkSet::iterator i = find( s.begin(), s.end(), h );
  if (i != s.end())
    return 0;
  s.push_back( h );
  return 1;
}
int MBMeshSet::add_parent( MBEntityHandle parent )
  { return insert_in_vector( parentMeshSets, parent ); }
int MBMeshSet::add_child( MBEntityHandle child )
  { return insert_in_vector( childMeshSets, child ); }

static inline int remove_from_vector( MBMeshSet::LinkSet& s, MBEntityHandle h )
{
  MBMeshSet::LinkSet::iterator i = find( s.begin(), s.end(), h );
  if (i == s.end())
    return 0;
  s.erase( i );
  return 1;
}
int MBMeshSet::remove_parent( MBEntityHandle parent )
  { return remove_from_vector( parentMeshSets, parent ); }
int MBMeshSet::remove_child( MBEntityHandle child )
  { return remove_from_vector( childMeshSets, child ); }

unsigned long MBMeshSet::parent_child_memory_use() const
{
  return (parentMeshSets.capacity() + childMeshSets.capacity())
       * sizeof(MBEntityHandle);
}

#else 

MBMeshSet::MBMeshSet( bool track_ownership ) 
  : mTracking(track_ownership),
    mParentCount(ZERO),
    mChildCount(ZERO)
  {}

MBMeshSet::~MBMeshSet() 
{
  if (mParentCount > 2)
    free( parentMeshSets.ptr[0] );
  if (mChildCount > 2)
    free( childMeshSets.ptr[0] );
}

static inline 
MBMeshSet::Count insert_in_vector( const MBMeshSet::Count count, 
                                MBMeshSet::CompactList& list,
                                const MBEntityHandle h,
                                int &result )
{
  switch (count) {
    case MBMeshSet::ZERO:
      list.hnd[0] = h;
      result = true;
      return MBMeshSet::ONE;
    case MBMeshSet::ONE:
      if (list.hnd[0] == h) {
        result = false;
        return MBMeshSet::ONE;
      }
      else {
        result = true;
        list.hnd[1] = h;
        return MBMeshSet::TWO;
      }
    case MBMeshSet::TWO:
      if (list.hnd[0] == h || list.hnd[1] == h) {
        result = false;
        return MBMeshSet::TWO;
      }
      else {
        MBEntityHandle* ptr = (MBEntityHandle*)malloc(3*sizeof(MBEntityHandle));
        ptr[0] = list.hnd[0];
        ptr[1] = list.hnd[1];
        ptr[2] = h;
        list.ptr[0] = ptr;
        list.ptr[1] = ptr + 3;
        result = true;
        return MBMeshSet::MANY;
      }
    case MBMeshSet::MANY:
      if (find( list.ptr[0], list.ptr[1], h ) != list.ptr[1]) {
        result = false;
      }
      else {
        int size = list.ptr[1] - list.ptr[0];
        list.ptr[0] = (MBEntityHandle*)realloc( list.ptr[0], (size+1)*sizeof(MBEntityHandle) );
        list.ptr[0][size] = h;
        list.ptr[1] = list.ptr[0] + size + 1;
        result = true;
      }
      return MBMeshSet::MANY;
  }
}

int MBMeshSet::add_parent( MBEntityHandle parent )
{ 
  int result;
  mParentCount = insert_in_vector( mParentCount, parentMeshSets, parent, result );
  return result;
}
int MBMeshSet::add_child( MBEntityHandle child )
{ 
  int result;
  mChildCount = insert_in_vector( mChildCount, childMeshSets, child, result );
  return result;
}

static inline
MBMeshSet::Count remove_from_vector( const MBMeshSet::Count count, 
                                  MBMeshSet::CompactList& list,
                                  const MBEntityHandle h,
                                  int &result )
{
  switch (count) {
    case MBMeshSet::ZERO:
      result = false;
      return MBMeshSet::ZERO;
    case MBMeshSet::ONE:
      if (h == list.hnd[0]) {
        result = true;
        return MBMeshSet::ZERO;
      }
      else {
        result = false;
        return MBMeshSet::ONE;
      }
    case MBMeshSet::TWO:
      if (h == list.hnd[0]) {
        list.hnd[0] = list.hnd[1];
        result = true;
        return MBMeshSet::ONE;
      } 
      else if (h == list.hnd[1]) {
        result = true;
        return MBMeshSet::ONE;
      }
      else {
        result = false;
        return MBMeshSet::TWO;
      }
    case MBMeshSet::MANY: {
      MBEntityHandle *i, *j, *p;
      i = std::find( list.ptr[0], list.ptr[1], h );
      if (i == list.ptr[1]) {
        result = false;
        return MBMeshSet::MANY;
      }
      
      result = true;
      p = list.ptr[1] - 1;
      while (i != p) {
        j = i + 1;
        *i = *j;
        i = j;
      }
      int size = p - list.ptr[0];
      if (size == 2) {
        p = list.ptr[0];
        list.hnd[0] = p[0];
        list.hnd[1] = p[1];
        free( p );
        return MBMeshSet::TWO;
      }
      else {
        list.ptr[0] = (MBEntityHandle*)realloc( list.ptr[0], size*sizeof(MBEntityHandle) );
        list.ptr[1] = list.ptr[0] + size;
        return MBMeshSet::MANY;
      }
    }
  }
}

int MBMeshSet::remove_parent( MBEntityHandle parent )
{ 
  int result;
  mParentCount = remove_from_vector( mParentCount, parentMeshSets, parent, result );
  return result;
}
int MBMeshSet::remove_child( MBEntityHandle child )
{ 
  int result;
  mChildCount = remove_from_vector( mChildCount, childMeshSets, child, result );
  return result;
}

unsigned long MBMeshSet::parent_child_memory_use() const
{
  unsigned long result = 0;
  if (num_children() > 2)
    result += sizeof(MBEntityHandle) * num_children();
  if (num_parents() > 2)
    result += sizeof(MBEntityHandle) * num_parents();
  return result;
}

#endif
  

MBMeshSet_MBRange::~MBMeshSet_MBRange()
{
}

MBErrorCode MBMeshSet_MBRange::clear( MBEntityHandle mEntityHandle,
                                      AEntityFactory* mAdjFact )
{
  if(mTracking && mAdjFact)
  {
    for(MBRange::iterator iter = mRange.begin();
        iter != mRange.end(); ++iter)
    {
      mAdjFact->remove_adjacency(*iter, mEntityHandle);
    }
  }
  mRange.clear();
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_entities(std::vector<MBEntityHandle>& entity_list) const
{
  unsigned int old_size = entity_list.size();
  entity_list.resize(old_size + mRange.size());
  for (MBRange::iterator rit = mRange.begin(); rit != mRange.end(); rit++)
    entity_list[old_size++] = *rit;
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_entities(MBRange& entity_list) const
{
  entity_list.merge( mRange );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_entities_by_type(MBEntityType type,
    std::vector<MBEntityHandle>& entity_list) const
{
  std::pair<MBRange::const_iterator,MBRange::const_iterator> its;
  its = mRange.equal_range( type );
  std::copy( its.first, its.second, std::back_inserter( entity_list ) );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_entities_by_type(MBEntityType type,
    MBRange& entity_list) const
{
  std::pair<MBRange::const_iterator,MBRange::const_iterator> its;
  its = mRange.equal_range( type );
  entity_list.merge( its.first, its.second );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_entities_by_dimension(int dim,
    std::vector<MBEntityHandle>& entity_list) const
{
  MBRange::const_iterator beg = mRange.lower_bound(MBCN::TypeDimensionMap[dim].first);
  MBRange::const_iterator end = mRange.lower_bound(MBCN::TypeDimensionMap[dim+1].first);
  std::copy( beg, end, std::back_inserter( entity_list ) );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_entities_by_dimension(int dim,
    MBRange& entity_list) const
{
  MBRange::const_iterator beg = mRange.lower_bound(MBCN::TypeDimensionMap[dim].first);
  MBRange::const_iterator end = mRange.lower_bound(MBCN::TypeDimensionMap[dim+1].first);
  entity_list.merge( beg, end );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::get_non_set_entities( MBRange& range ) const
{
  range.merge( mRange.begin(), mRange.lower_bound( MBENTITYSET ) );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::add_entities( const MBEntityHandle *entities,
                                             const int num_entities,
                                             MBEntityHandle mEntityHandle,
                                             AEntityFactory* mAdjFact )
{
  int i;
  for(i = 0; i < num_entities; i++)
    mRange.insert(entities[i]);

  if(mTracking && mAdjFact)
  {
    for(i = 0; i < num_entities; i++)
      mAdjFact->add_adjacency(entities[i], mEntityHandle);
  }
  return MB_SUCCESS;
}


MBErrorCode MBMeshSet_MBRange::add_entities( const MBRange& entities,
                                             MBEntityHandle mEntityHandle,
                                             AEntityFactory* mAdjFact )
{

  mRange.merge(entities);
  
  if(mTracking && mAdjFact)
  {
    for(MBRange::const_iterator iter = entities.begin();
        iter != entities.end(); ++iter)
      mAdjFact->add_adjacency(*iter, mEntityHandle);
  }

  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::remove_entities( const MBRange& entities,
                                                MBEntityHandle mEntityHandle,
                                                AEntityFactory* mAdjFact )
{
  MBRange::const_iterator iter = entities.begin();
  for(; iter != entities.end(); ++iter)
  {
    MBRange::iterator found = mRange.find(*iter);
    if(found != mRange.end())
    {
      mRange.erase(found);
      if(mTracking && mAdjFact)
        mAdjFact->remove_adjacency(*iter, mEntityHandle);
    }
  }

  return MB_SUCCESS;

}

MBErrorCode MBMeshSet_MBRange::remove_entities( const MBEntityHandle *entities,
                                                const int num_entities,
                                                MBEntityHandle mEntityHandle,
                                                AEntityFactory* mAdjFact )
{
  for(int i = 0; i < num_entities; i++)
  {
    MBRange::iterator found = mRange.find(entities[i]);
    if(found != mRange.end())
    {
      mRange.erase(found);
      if(mTracking && mAdjFact)
        mAdjFact->remove_adjacency(entities[i], mEntityHandle);
    }
  }

  return MB_SUCCESS;

}

unsigned int MBMeshSet_MBRange::num_entities() const
{
  return mRange.size();
}

unsigned int MBMeshSet_MBRange::num_entities_by_type(MBEntityType type) const
{
  return mRange.num_of_type( type );
}

unsigned int MBMeshSet_MBRange::num_entities_by_dimension(int dimension) const
{
  return mRange.num_of_dimension( dimension );
}


MBErrorCode MBMeshSet_MBRange::subtract( const MBMeshSet *meshset_2,
                                         MBEntityHandle handle,
                                         AEntityFactory* adj_fact )
{
  MBRange other_range;
  meshset_2->get_entities(other_range);

  return remove_entities( other_range, handle, adj_fact );
}

MBErrorCode MBMeshSet_MBRange::intersect( const MBMeshSet *meshset_2,
                                          MBEntityHandle mEntityHandle,
                                          AEntityFactory* mAdjFact )
{
  MBRange other_range;
  meshset_2->get_entities(other_range);

  std::set<MBEntityHandle> tmp;

  std::set_intersection(mRange.begin(), mRange.end(), other_range.begin(), other_range.end(), 
      std::inserter< std::set<MBEntityHandle> >(tmp, tmp.end()));

  mRange.clear();

  for(std::set<MBEntityHandle>::reverse_iterator iter = tmp.rbegin();
      iter != tmp.rend(); ++iter)
  {
    mRange.insert(*iter);
  }

  //track owner
  if(mTracking && mAdjFact)
  {
    tmp.clear();
    std::set_difference(mRange.begin(), mRange.end(), other_range.begin(), other_range.end(),
        std::inserter< std::set<MBEntityHandle> >(tmp, tmp.end()));

    for(std::set<MBEntityHandle>::iterator iter = tmp.begin();
        iter != tmp.end(); ++iter)
      mAdjFact->remove_adjacency(*iter, mEntityHandle);
  }

  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_MBRange::unite( const MBMeshSet *meshset_2,
                                      MBEntityHandle handle,
                                      AEntityFactory* adj_fact )
{
  MBRange other_range;
  meshset_2->get_entities(other_range);
  return add_entities( other_range, handle, adj_fact );
}

unsigned long MBMeshSet_MBRange::get_memory_use() const
{
  return parent_child_memory_use() + mRange.get_memory_use() + sizeof(*this);
}


MBMeshSet_Vector::~MBMeshSet_Vector()
{
}

MBErrorCode MBMeshSet_Vector::clear( MBEntityHandle mEntityHandle, 
                                     AEntityFactory* mAdjFact)
{
  if(mTracking && mAdjFact)
  {
    for(std::vector<MBEntityHandle>::iterator iter = mVector.begin();
        iter != mVector.end(); ++iter)
    {
      mAdjFact->remove_adjacency(*iter, mEntityHandle);
    }
  }
  mVector.clear();
  return MB_SUCCESS;
}

static void vector_to_range( std::vector<MBEntityHandle>& vect, MBRange& range )
{
  std::sort( vect.begin(), vect.end() );
  MBRange::iterator insert_iter = range.begin();
  std::vector<MBEntityHandle>::iterator iter = vect.begin();
  while (iter != vect.end()) {
    MBEntityHandle beg, end;
    beg = end = *iter;
    for (++iter; iter != vect.end() && *iter - end < 2; ++iter)
      end = *iter;
    insert_iter = range.insert( insert_iter, beg, end );
  }
}

class not_type_test {
  public:
    inline not_type_test( MBEntityType type ) : mType(type) {}
    inline bool operator()( MBEntityHandle handle )
      { return TYPE_FROM_HANDLE(handle) != mType; }
  private:
    MBEntityType mType;
};

class type_test {
  public:
    inline type_test( MBEntityType type ) : mType(type) {}
    inline bool operator()( MBEntityHandle handle )
      { return TYPE_FROM_HANDLE(handle) == mType; }
  private:
    MBEntityType mType;
};

class not_dim_test {
  public:
    inline not_dim_test( int dimension ) : mDim(dimension) {}
    inline bool operator()( MBEntityHandle handle ) const
      { return MBCN::Dimension(TYPE_FROM_HANDLE(handle)) != mDim; }
  private:
    int mDim;
};

class dim_test {
  public:
    inline dim_test( int dimension ) : mDim(dimension) {}
    inline bool operator()( MBEntityHandle handle ) const
      { return MBCN::Dimension(TYPE_FROM_HANDLE(handle)) == mDim; }
  private:
    int mDim;
};

MBErrorCode MBMeshSet_Vector::get_entities(std::vector<MBEntityHandle>& entity_list) const
{
  std::copy( mVector.begin(), mVector.end(), std::back_inserter(entity_list) );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::get_entities(MBRange& entity_list) const
{
  std::vector<MBEntityHandle> tmp_vect( mVector );
  vector_to_range( tmp_vect, entity_list );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::get_entities_by_type(MBEntityType type,
    std::vector<MBEntityHandle>& entity_list) const
{
  std::remove_copy_if( mVector.begin(), mVector.end(), 
                       std::back_inserter( entity_list ),
                       not_type_test(type) );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::get_entities_by_type(MBEntityType type,
    MBRange& entity_list) const
{
  std::vector<MBEntityHandle> tmp_vect;
  get_entities_by_type( type, tmp_vect );
  vector_to_range( tmp_vect, entity_list );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::get_entities_by_dimension( int dimension, 
    std::vector<MBEntityHandle>& entity_list) const
{
  std::remove_copy_if( mVector.begin(), mVector.end(), 
                       std::back_inserter( entity_list ),
                       not_dim_test(dimension) );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::get_entities_by_dimension( int dimension, 
    MBRange& entity_list) const
{
  std::vector<MBEntityHandle> tmp_vect;
  get_entities_by_dimension( dimension, tmp_vect );
  vector_to_range( tmp_vect, entity_list );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::get_non_set_entities( MBRange& entities ) const
{
  std::vector<MBEntityHandle> tmp_vect;
  std::remove_copy_if( mVector.begin(), mVector.end(), 
                       std::back_inserter( tmp_vect ),
                       type_test(MBENTITYSET) );
  vector_to_range( tmp_vect, entities );
  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::add_entities( const MBEntityHandle *entities,
                                            const int num_entities,
                                            MBEntityHandle mEntityHandle,
                                            AEntityFactory* mAdjFact )
{
  unsigned int prev_size = mVector.size();
  mVector.resize(prev_size + num_entities);
  std::copy(entities, entities+num_entities, &mVector[prev_size]);

  if(mTracking && mAdjFact)
  {
    for(int i = 0; i < num_entities; i++)
      mAdjFact->add_adjacency(entities[i], mEntityHandle);
  }

  return MB_SUCCESS;
}


MBErrorCode MBMeshSet_Vector::add_entities( const MBRange& entities,
                                            MBEntityHandle mEntityHandle,
                                            AEntityFactory* mAdjFact )
{

  unsigned int prev_size = mVector.size();
  mVector.resize(prev_size + entities.size());
  std::copy(entities.begin(), entities.end(), &mVector[prev_size]);
  
  if(mTracking && mAdjFact)
  {
    for(MBRange::const_iterator iter = entities.begin();
        iter != entities.end(); ++iter)
      mAdjFact->add_adjacency(*iter, mEntityHandle);
  }

  return MB_SUCCESS;
}


MBErrorCode MBMeshSet_Vector::remove_entities( const MBRange& entities,
                                               MBEntityHandle mEntityHandle,
                                               AEntityFactory* mAdjFact )
{
  std::vector<MBEntityHandle>::iterator iter = mVector.begin();
  for(; iter < mVector.end(); )
  {
    if(entities.find(*iter) != entities.end())
    {
      if(mTracking && mAdjFact)
        mAdjFact->remove_adjacency(*iter, mEntityHandle);
      iter = mVector.erase(iter);
    }
    else
      ++iter;
  }

  return MB_SUCCESS;

}

MBErrorCode MBMeshSet_Vector::remove_entities( const MBEntityHandle *entities, 
                                               const int num_entities,
                                               MBEntityHandle mEntityHandle,
                                               AEntityFactory* mAdjFact )
{
  std::vector<MBEntityHandle>::iterator temp_iter;
  for(int i = 0; i != num_entities; i++ )
  {
    temp_iter = std::find( mVector.begin(), mVector.end(), entities[i]); 
    if( temp_iter != mVector.end() )
    {
      if(mTracking && mAdjFact)
        mAdjFact->remove_adjacency(entities[i], mEntityHandle);
      mVector.erase(temp_iter);
    }
  }
  return MB_SUCCESS;
}


unsigned int MBMeshSet_Vector::num_entities() const
{
  return mVector.size();
}

unsigned int MBMeshSet_Vector::num_entities_by_type(MBEntityType type) const
{
  return std::count_if( mVector.begin(), mVector.end(), type_test(type) );
}

unsigned int MBMeshSet_Vector::num_entities_by_dimension( int dim ) const
{
  return std::count_if( mVector.begin(), mVector.end(), dim_test(dim) );
}


MBErrorCode MBMeshSet_Vector::subtract( const MBMeshSet *meshset_2,
                                        MBEntityHandle my_handle,
                                        AEntityFactory* adj_fact )
{
  std::vector<MBEntityHandle> other_vector;
  meshset_2->get_entities(other_vector);

  return remove_entities(&other_vector[0], other_vector.size(), my_handle, adj_fact);
}

MBErrorCode MBMeshSet_Vector::intersect( const MBMeshSet *meshset_2,
                                         MBEntityHandle mEntityHandle,
                                         AEntityFactory* mAdjFact )
{
  MBRange other_range;
  meshset_2->get_entities(other_range);

  std::sort(mVector.begin(), mVector.end());

  std::set<MBEntityHandle> tmp;

  std::set_intersection(mVector.begin(), mVector.end(), other_range.begin(), other_range.end(), 
      std::inserter< std::set<MBEntityHandle> >(tmp, tmp.end()));

  mVector.resize(tmp.size());

  std::copy(tmp.begin(), tmp.end(), mVector.begin());

  //track owner
  tmp.clear();
  std::set_difference(mVector.begin(), mVector.end(), other_range.begin(), other_range.end(),
      std::inserter< std::set<MBEntityHandle> >(tmp, tmp.end()));

  if(mTracking && mAdjFact)
  {
    for(std::set<MBEntityHandle>::iterator iter = tmp.begin();
        iter != tmp.end(); ++iter)
      mAdjFact->remove_adjacency(*iter, mEntityHandle);
  }

  return MB_SUCCESS;
}

MBErrorCode MBMeshSet_Vector::unite( const MBMeshSet *meshset_2,
                                     MBEntityHandle my_handle,
                                     AEntityFactory* adj_fact )
{
  std::vector<MBEntityHandle> other_vector;
  meshset_2->get_entities(other_vector);
  return add_entities(&other_vector[0], other_vector.size(), my_handle, adj_fact);
}

unsigned long MBMeshSet_Vector::get_memory_use() const
{
  return parent_child_memory_use() 
       + mVector.capacity()*sizeof(MBEntityHandle) 
       + sizeof(*this);
}

