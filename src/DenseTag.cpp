/** \file   DenseTag.cpp
 *  \author Jason Kraftcheck 
 *  \date   2010-12-14
 */

#include "DenseTag.hpp"
#include "moab/Range.hpp"
#include "TagCompare.hpp"
#include "SysUtil.hpp"
#include "SequenceManager.hpp"
#include "SequenceData.hpp"
#include <utility>

namespace moab {

DenseTag::DenseTag( int index,
                    const char * name, 
                    int size, 
                    DataType type, 
                    const void * default_value )
  : TagInfo( name, size, type, default_value, size ), 
    mySequenceArray(index),
    meshValue(0)
  {}

TagType DenseTag::get_storage_type() const 
  { return MB_TAG_DENSE; }

DenseTag* DenseTag::create_tag( SequenceManager* seqman,
                                const char* name,
                                int bytes,
                                DataType type,
                                const void* default_value )
{
  if (bytes < 1)
    return 0;

  int index;
  if (MB_SUCCESS != seqman->reserve_tag_array( bytes, index ))
    return 0;
  
  return new DenseTag( index, name, bytes, type, default_value );
}

  
DenseTag::~DenseTag()
{
  assert( mySequenceArray < 0 );
  delete [] meshValue;
}

ErrorCode DenseTag::release_all_data( SequenceManager* seqman, bool delete_pending )
{
  ErrorCode result = seqman->release_tag_array( mySequenceArray, delete_pending );
  if (MB_SUCCESS == result && delete_pending)
    mySequenceArray = -1;
  return result;
}

ErrorCode DenseTag::get_array( const SequenceManager* seqman, 
                               EntityHandle h, 
                               const unsigned char*& ptr,
                               size_t& count ) const
{
  const EntitySequence* seq = 0;
  ErrorCode rval = seqman->find( h, seq );
  if (MB_SUCCESS != rval) {
    if (!h) { // root set
      ptr = meshValue;
      count = 1;
      return MB_SUCCESS;
    }
    else { // not root set
      ptr = 0;
      count = 0;
      return rval;
    }
  }
  
  const void* mem = seq->data()->get_tag_data( mySequenceArray );
  ptr = reinterpret_cast<const unsigned char*>(mem);
  count = seq->data()->end_handle() - h + 1;
  if (ptr)
    ptr += get_size() * (h - seq->data()->start_handle());

  return MB_SUCCESS;
}

ErrorCode DenseTag::get_array( SequenceManager* seqman, 
                               EntityHandle h, 
                               unsigned char*& ptr,
                               size_t& count,
                               bool allocate ) 
{
  EntitySequence* seq = 0;
  ErrorCode rval = seqman->find( h, seq );
  if (MB_SUCCESS != rval) {
    if (!h) { // root set
      if (!meshValue && allocate) 
        meshValue = new unsigned char[get_size()];
      ptr = meshValue;
      count = 1;
      return MB_SUCCESS;
    }
    else { // not root set
      ptr = 0;
      count = 0;
      return rval;
    }
  }
  
  void* mem = seq->data()->get_tag_data( mySequenceArray );
  if (!mem && allocate) {
    mem = seq->data()->allocate_tag_array( mySequenceArray, get_size() );
    if (!mem)
      return MB_FAILURE;
    
    if (get_default_value()) 
      SysUtil::setmem( mem, get_default_value(), get_size(), seq->data()->size() );
    else
      memset( mem, 0, get_size() * seq->data()->size() );
  }
  
  ptr = reinterpret_cast<unsigned char*>(mem);
  count = seq->data()->end_handle() - h + 1;
  if (ptr)
    ptr += get_size() * (h - seq->data()->start_handle());
  return MB_SUCCESS;

}

ErrorCode DenseTag::get_data( const SequenceManager* seqman,
                              const EntityHandle* entities,
                              size_t num_entities,
                              void* data ) const
{
  size_t junk;
  unsigned char* ptr = reinterpret_cast<unsigned char*>(data);
  const EntityHandle *const end = entities + num_entities;
  for (const EntityHandle* i = entities; i != end; ++i, ptr += get_size()) {
    const unsigned char* data = 0;
    ErrorCode rval = get_array( seqman, *i, data, junk );
    if (MB_SUCCESS != rval)
      return rval;
       
    if (data)
      memcpy( ptr, data, get_size() );
    else if (get_default_value())
      memcpy( ptr, get_default_value(), get_size() );
    else
      return MB_TAG_NOT_FOUND;
  }

  return MB_SUCCESS;
}


ErrorCode DenseTag::get_data( const SequenceManager* seqman,
                              const Range& entities,
                              void* values ) const
{
  ErrorCode rval;
  size_t avail;
  const unsigned char* array;
  unsigned char* data = reinterpret_cast<unsigned char*>(values);

  for (Range::const_pair_iterator p = entities.const_pair_begin(); 
       p != entities.const_pair_end(); ++p) {
       
    EntityHandle start = p->first;
    while (start <= p->second) {
      rval = get_array( seqman, start, array, avail );
      if (MB_SUCCESS != rval)
        return rval;
      
      const size_t count = std::min<size_t>(p->second - start + 1, avail);
      if (array) 
        memcpy( data, array, get_size() * count );
      else if (get_default_value())
        SysUtil::setmem( data, get_default_value(), get_size(), count );
      else
        return MB_TAG_NOT_FOUND;
      
      data += get_size() * count;
      start += count;
    }
  }
  
  return MB_SUCCESS;
}
                     
ErrorCode DenseTag::get_data( const SequenceManager* seqman,
                              const EntityHandle* entities,
                              size_t num_entities,
                              const void** pointers,
                              int* data_lengths ) const
{
  ErrorCode result;
  const EntityHandle *const end = entities + num_entities;
  size_t junk;
  const unsigned char* ptr;

  if (data_lengths) {
    const int len = get_size();
    SysUtil::setmem( data_lengths, &len, sizeof(int), num_entities );
  }

  for (const EntityHandle* i = entities; i != end; ++i, ++pointers) {
    result = get_array( seqman, *i, ptr, junk );
    if (MB_SUCCESS != result)
      return result;
  
    if (ptr)
      *pointers = ptr;
    else if (get_default_value())
      *pointers = get_default_value();
    else
      return MB_TAG_NOT_FOUND;
  }
  
  return MB_SUCCESS;
}

                      
ErrorCode DenseTag::get_data( const SequenceManager* seqman,
                              const Range& entities,
                              const void** pointers,
                              int* data_lengths ) const
{
  ErrorCode rval;
  size_t avail;
  const unsigned char* array = 0;

  if (data_lengths) {
    int len = get_size();
    SysUtil::setmem( data_lengths, &len, sizeof(int), entities.size() );
  }
  
  for (Range::const_pair_iterator p = entities.const_pair_begin(); 
       p != entities.const_pair_end(); ++p) {
       
    EntityHandle start = p->first;
    while (start <= p->second) {
      rval = get_array( seqman, start, array, avail );
      if (MB_SUCCESS != rval)
        return rval;
      
      const size_t count = std::min<size_t>(p->second - start + 1, avail);
      if (array) {
        for (EntityHandle end = start + count; start != end; ++start) {
          *pointers = array;
          array += get_size();
          ++pointers;
        }
      }
      else if (const void* val = get_default_value()) {
        SysUtil::setmem( pointers, &val, sizeof(void*), count );
        pointers += count;
        start += count;
      }
      else {
        return MB_TAG_NOT_FOUND;
      }
    }
  }
  
  return MB_SUCCESS;
}
  
ErrorCode DenseTag::set_data( SequenceManager* seqman,
                              const EntityHandle* entities,
                              size_t num_entities,
                              const void* data )
{
  ErrorCode rval;
  const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);
  const EntityHandle* const end = entities + num_entities;
  unsigned char* array;
  size_t junk;
  
  for (const EntityHandle* i = entities; i != end; ++i, ptr += get_size() ) {
    rval = get_array( seqman, *i, array, junk, true );
    if (MB_SUCCESS != rval)
      return rval;

    memcpy( array, ptr, get_size() );
  }

  return MB_SUCCESS;
}

ErrorCode DenseTag::set_data( SequenceManager* seqman,
                              const Range& entities,
                              const void* values )
{
  ErrorCode rval;
  const char* data = reinterpret_cast<const char*>(values);
  unsigned char* array;
  size_t avail;

  for (Range::const_pair_iterator p = entities.const_pair_begin(); 
       p != entities.const_pair_end(); ++p) {
       
    EntityHandle start = p->first;
    while (start <= p->second) {
      rval = get_array( seqman, start, array, avail, true );
      if (MB_SUCCESS != rval)
        return rval;
      
      const size_t count = std::min<size_t>(p->second - start + 1, avail);
      memcpy( array, data, get_size() * count );
      data += get_size() * count;
      start += count;
    }
  }
  
  return MB_SUCCESS;
}
                      
ErrorCode DenseTag::set_data( SequenceManager* seqman,
                              const EntityHandle* entities,
                              size_t num_entities,
                              void const* const* pointers,
                              const int* data_lengths )
{
  ErrorCode rval = validate_lengths( data_lengths, num_entities );
  if (MB_SUCCESS != rval)
    return rval;
  
  const EntityHandle* const end = entities + num_entities;
  unsigned char* array;
  size_t junk;
  
  for (const EntityHandle* i = entities; i != end; ++i, ++pointers ) {
    rval = get_array( seqman, *i, array, junk, true );
    if (MB_SUCCESS != rval)
      return rval;

    memcpy( array, *pointers, get_size() );
  }

  return MB_SUCCESS;
}
  
                      
ErrorCode DenseTag::set_data( SequenceManager* seqman,
                              const Range& entities,
                              void const* const* pointers,
                              const int* data_lengths )
{
  ErrorCode rval;
  unsigned char* array;
  size_t avail;

  for (Range::const_pair_iterator p = entities.const_pair_begin(); 
       p != entities.const_pair_end(); ++p) {
       
    EntityHandle start = p->first;
    while (start <= p->second) {
      rval = get_array( seqman, start, array, avail, true );
      if (MB_SUCCESS != rval)
        return rval;
      
      const EntityHandle end = std::min<EntityHandle>(p->second + 1, start + avail );
      while (start != end) {
        memcpy( array, *pointers, get_size() );
        ++start;
        ++pointers;
        array += get_size();
      }
    }
  }
  
  return MB_SUCCESS;
}


ErrorCode DenseTag::clear_data( bool allocate,
                                SequenceManager* seqman,
                                const EntityHandle* entities,
                                size_t num_entities,
                                const void* value_ptr )
{
  ErrorCode rval;
  const EntityHandle* const end = entities + num_entities;
  unsigned char* array;
  size_t junk;
  
  for (const EntityHandle* i = entities; i != end; ++i ) {
    rval = get_array( seqman, *i, array, junk, allocate );
    if (MB_SUCCESS != rval)
      return rval;
    
    if (array) // array should never be null if allocate == true
      memcpy( array, value_ptr, get_size() );
  }

  return MB_SUCCESS;
}

ErrorCode DenseTag::clear_data( bool allocate,
                                SequenceManager* seqman,
                                const Range& entities,
                                const void* value_ptr )
{
  ErrorCode rval;
  unsigned char* array;
  size_t avail;

  for (Range::const_pair_iterator p = entities.const_pair_begin(); 
       p != entities.const_pair_end(); ++p) {
       
    EntityHandle start = p->first;
    while (start <= p->second) {
      rval = get_array( seqman, start, array, avail, allocate );
      if (MB_SUCCESS != rval)
        return rval;
      
      const size_t count = std::min<size_t>(p->second - start + 1, avail);
      if (array) // array should never be null if allocate == true
        SysUtil::setmem( array, value_ptr, get_size(), count );
      start += count;
    }
  }
  
  return MB_SUCCESS;
}

ErrorCode DenseTag::clear_data( SequenceManager* seqman,
                                const EntityHandle* entities,
                                size_t num_entities,
                                const void* value_ptr,
                                int value_len )
{
  if (value_len && value_len != get_size())
    return MB_INVALID_SIZE;

  return clear_data( true, seqman, entities, num_entities, value_ptr );
}

ErrorCode DenseTag::clear_data( SequenceManager* seqman,
                                const Range& entities,
                                const void* value_ptr,
                                int value_len )
{
  if (value_len && value_len != get_size())
    return MB_INVALID_SIZE;

  return clear_data( true, seqman, entities, value_ptr );
}

ErrorCode DenseTag::remove_data( SequenceManager* seqman,
                                 const EntityHandle* entities,
                                 size_t num_entities )
{
  std::vector<unsigned char> zeros;
  const void* value = get_default_value();
  if (!value) {
    zeros.resize( get_size(), 0 );
    value = &zeros[0];
  }
  return clear_data( false, seqman, entities, num_entities, value );
}

ErrorCode DenseTag::remove_data( SequenceManager* seqman,
                                 const Range& entities )
{
  std::vector<unsigned char> zeros;
  const void* value = get_default_value();
  if (!value) {
    zeros.resize( get_size(), 0 );
    value = &zeros[0];
  }
  return clear_data( false, seqman, entities, value );
}


ErrorCode DenseTag::tag_iterate( SequenceManager* seqman,
                                 Range::iterator& iter,
                                 const Range::iterator& end,
                                 void*& data_ptr )
{
    // If asked for nothing, successfully return nothing.
  if (iter == end)
    return MB_SUCCESS;
  
  unsigned char* array;
  size_t avail;
  ErrorCode rval = get_array( seqman, *iter, array, avail, true );
  if (MB_SUCCESS != rval)
    return rval;
  data_ptr = array;
  
  size_t count = std::min<size_t>(avail, *(iter.end_of_block()) - *iter + 1);
  if (0 != *end && *end <= *(iter.end_of_block()))
    iter = end;
  else
    iter += count;

  return MB_SUCCESS;
}

ErrorCode DenseTag::get_tagged_entities( const SequenceManager* seqman,
                                         Range& entities_in,
                                         EntityType type,
                                         const Range* intersect_list ) const
{
  Range tmp;
  Range* entities = intersect_list ? &tmp : &entities_in;
  Range::iterator hint = entities->begin();
  std::pair<EntityType,EntityType> range = type_range(type);
  TypeSequenceManager::const_iterator i;
  for (EntityType t = range.first; t != range.second; ++t) {
    const TypeSequenceManager& map = seqman->entity_map(t);
    for (i = map.begin(); i != map.end(); ++i) 
      if ((*i)->data()->get_tag_data( mySequenceArray ))
        hint = entities->insert( hint, (*i)->start_handle(), (*i)->end_handle() );
  }
  
  if (intersect_list) 
    entities_in = intersect( *entities, *intersect_list );
  
  return MB_SUCCESS;
}

ErrorCode DenseTag::num_tagged_entities( const SequenceManager* seqman,
                                         size_t& output_count,
                                         EntityType type,
                                         const Range* intersect ) const
{
  Range tmp;
  ErrorCode rval = get_tagged_entities( seqman, tmp, type, intersect );
  output_count += tmp.size();
  return rval;
}
  
ErrorCode DenseTag::find_entities_with_value( const SequenceManager* seqman,
                                              Range& output_entities,
                                              const void* value,
                                              int value_bytes,
                                              EntityType type,
                                              const Range* intersect_entities ) const
{
  if (value_bytes && value_bytes != get_size())
    return MB_INVALID_SIZE;

  if (!intersect_entities) {
    std::pair<EntityType,EntityType> range = type_range(type);
    TypeSequenceManager::const_iterator i;
    for (EntityType t = range.first; t != range.second; ++i) {
      const TypeSequenceManager& map = seqman->entity_map(t);
      for (i = map.begin(); i != map.end(); ++i) {
        const void* data = (*i)->data()->get_tag_data( mySequenceArray );
        if (data) {
          ByteArrayIterator start( (*i)->data()->start_handle(), data, *this );
          ByteArrayIterator end( (*i)->end_handle() + 1, 0, 0 );
          start += (*i)->start_handle() - (*i)->data()->start_handle();
          find_tag_values_equal( *this, value, get_size(), start, end, output_entities );
        }
      }
    }
  }
  else {
    const unsigned char* array;
    size_t count;
    ErrorCode rval;
     
    Range::const_pair_iterator p = intersect_entities->begin();
    if (type != MBMAXTYPE) {
      p = intersect_entities->lower_bound(type);
      assert(TYPE_FROM_HANDLE(p->first) == type);
    }
    for (; 
         p != intersect_entities->const_pair_end() && 
         (MBMAXTYPE == type || TYPE_FROM_HANDLE(p->first) == type); 
         ++p) {

      EntityHandle start = p->first;
      while (start <= p->second) {
        rval = get_array( seqman, start, array, count );
        if (MB_SUCCESS != rval)
          return rval; 
        
        if (p->second - start < count-1)
          count = p->second - start + 1;
        
        if (array) {
          ByteArrayIterator istart( start, array, *this );
          ByteArrayIterator iend( start+count, 0, 0 );
          find_tag_values_equal( *this, value, get_size(), istart, iend, output_entities );
        }
        start += count;
      }
    }
  }    
  
  return MB_SUCCESS;
}

bool DenseTag::is_tagged( const SequenceManager* seqman, EntityHandle h) const
{
  const unsigned char* ptr;
  size_t count;
  return MB_SUCCESS == get_array( seqman, h, ptr, count ) && 0 != ptr;
} 
  
ErrorCode DenseTag::get_memory_use( const SequenceManager* seqman,
                                    unsigned long& total,
                                    unsigned long& per_entity ) const

{
  per_entity = get_size();
  total = TagInfo::get_memory_use() + sizeof(*this);
  for (EntityType t = MBVERTEX; t <= MBENTITYSET; ++t) {
    const TypeSequenceManager& map = seqman->entity_map(t);
    const SequenceData* prev_data = 0;
    for (TypeSequenceManager::const_iterator i = map.begin(); i != map.end(); ++i) {
      if ((*i)->data() != prev_data && (*i)->data()->get_tag_data(mySequenceArray)) {
        prev_data = (*i)->data();
        total += get_size() * (*i)->data()->size();
      }
    }
  }
      
  return MB_SUCCESS;
}

} // namespace moab