#include <H5Tpublic.h>
#include <H5Dpublic.h>
#include "mhdf.h"
#include "util.h"
#include "file-handle.h"
#include "status.h"
#include "names-and-paths.h"

hid_t 
mhdf_createConnectivity( mhdf_FileHandle file_handle,
                         mhdf_ElemHandle mhdf_type,
                         int nodes_per_elem,
                         long count,
                         long* first_id_out,
                         mhdf_Status* status )
{
  FileHandle* file_ptr;
  hid_t elem_id, table_id;
  hsize_t dims[2];
  long first_id;
  
  file_ptr = (FileHandle*)(file_handle);
  if (!mhdf_check_valid_file( file_ptr, status ))
    return -1;
  
  if (nodes_per_elem <= 0 || count < 0 || !first_id_out)
  {
    mhdf_setFail( status, "Invalid argument." );
    return -1;
  }
  
  elem_id = mhdf_handle_from_type_index( file_ptr, mhdf_type, status );
  if (elem_id < 0) return -1;
  
  
  dims[0] = (hsize_t)count;
  dims[1] = (hsize_t)nodes_per_elem;
  table_id = mhdf_create_table( elem_id, 
                                CONNECTIVITY_NAME,
                                H5T_NATIVE_LONG,
                                2, dims, 
                                status );
  if (table_id < 0)
    return -1;
  
  first_id = file_ptr->max_id + 1;
  if (!mhdf_write_scalar_attrib( table_id, 
                                 START_ID_ATTRIB, 
                                 H5T_NATIVE_LONG,
                                 &first_id,
                                 status ))
  {
    H5Dclose( table_id );
    return -1;
  }
  
  *first_id_out = first_id;
  file_ptr->max_id += count;
  file_ptr->open_handle_count++;
  mhdf_setOkay( status );
 
  return table_id;
}

hid_t
mhdf_openConnectivity( mhdf_FileHandle file_handle,
                       mhdf_ElemHandle element_handle,
                       int* num_nodes_per_elem_out,
                       long* num_elements_out,
                       long* first_elem_id_out,
                       mhdf_Status* status )
{
  FileHandle* file_ptr;
  hid_t elem_id, table_id;
  hsize_t dims[2];
  
  file_ptr = (FileHandle*)(file_handle);
  if (!mhdf_check_valid_file( file_ptr, status ))
    return -1;
  
  if (!num_nodes_per_elem_out || !num_elements_out || !first_elem_id_out)
  {
    mhdf_setFail( status, "Invalid argument." );
    return -1;
  }
  
  elem_id = mhdf_handle_from_type_index( file_ptr, element_handle, status );
  if (elem_id < 0) return -1;
  
  table_id = mhdf_open_table2( elem_id, CONNECTIVITY_NAME, 
                               2, dims,
                               first_elem_id_out, status );
  
  if (table_id < 0)
    return -1;
  
  *num_elements_out = dims[0];
  *num_nodes_per_elem_out = dims[1];
  
  file_ptr->open_handle_count++;
  mhdf_setOkay( status );
  return table_id;
}

void
mhdf_writeConnectivity( hid_t table_id,
                        long offset,
                        long count,
                        hid_t hdf_integer_type,
                        const void* nodes,
                        mhdf_Status* status )
{
  mhdf_write_data( table_id, offset, count, hdf_integer_type, nodes, status );
}

void 
mhdf_readConnectivity( hid_t table_id,
                       long offset,
                       long count,
                       hid_t hdf_integer_type,
                       void* nodes,
                       mhdf_Status* status )
{
  mhdf_read_data( table_id, offset, count, hdf_integer_type, nodes, status );
}


void 
mhdf_createPolyConnectivity( mhdf_FileHandle file_handle,
                             mhdf_ElemHandle elem_type,
                             long num_poly,
                             long data_list_length,
                             long* first_id_out,
                             hid_t handles_out[2],
                             mhdf_Status* status )
{
  FileHandle* file_ptr;
  hid_t elem_id, index_id, conn_id;
  hsize_t dim;
  long first_id;
  
  file_ptr = (FileHandle*)(file_handle);
  if (!mhdf_check_valid_file( file_ptr, status ))
    return;
  
  if (num_poly <= 0 || data_list_length <= 0 || !first_id_out)
  {
    mhdf_setFail( status, "Invalid argument." );
    return;
  }
  
  if (data_list_length < 3*num_poly)
  {
    /* Could check agains 4*num_poly, but allow degenerate polys
       (1 for count plus 2 dim-1 defining entities, where > 2
        defining entities is a normal poly, 2 defining entities 
        is a degenerate poly and 1 defning entity is not valid.)
    */
    
    mhdf_setFail( status, "Invalid polygon data:  data length of %ld is "
                          "insufficient for %ld poly(gons/hedra).\n",
                          data_list_length, num_poly );
    return;
  }
  
  elem_id = mhdf_handle_from_type_index( file_ptr, elem_type, status );
  if (elem_id < 0) return;
  
  dim = (hsize_t)num_poly;
  index_id = mhdf_create_table( elem_id,
                                POLY_INDEX_NAME,
                                H5T_NATIVE_LONG,
                                1, &dim,
                                status );
  if (index_id < 0)
    return;
  
  
  dim = (hsize_t)data_list_length;
  conn_id = mhdf_create_table( elem_id, 
                                CONNECTIVITY_NAME,
                                H5T_NATIVE_LONG,
                                1, &dim, 
                                status );
  if (conn_id < 0)
  {
    H5Dclose(index_id);
    return;
  }
  
  first_id = file_ptr->max_id + 1;
  if (!mhdf_write_scalar_attrib( conn_id, 
                                 START_ID_ATTRIB, 
                                 H5T_NATIVE_LONG,
                                 &first_id,
                                 status ))
  {
    H5Dclose(index_id);
    H5Dclose( conn_id );
    return;
  }
  
  *first_id_out = first_id;
  file_ptr->max_id += num_poly;
  file_ptr->open_handle_count++;
  mhdf_setOkay( status );
  handles_out[0] = index_id;
  handles_out[1] = conn_id;
}

void
mhdf_openPolyConnectivity( mhdf_FileHandle file_handle,
                           mhdf_ElemHandle element_handle,
                           long* num_poly_out,
                           long* data_list_length_out,
                           long* first_poly_id_out,
                           hid_t handles_out[2],
                           mhdf_Status* status )
{
  FileHandle* file_ptr;
  hid_t elem_id, table_id, index_id;
  hsize_t row_count;
  
  file_ptr = (FileHandle*)(file_handle);
  if (!mhdf_check_valid_file( file_ptr, status ))
    return ;
  
  if (!num_poly_out || !data_list_length_out || !first_poly_id_out)
  {
    mhdf_setFail( status, "Invalid argument." );
    return ;
  }
  
  elem_id = mhdf_handle_from_type_index( file_ptr, element_handle, status );
  if (elem_id < 0) return ;
  
  index_id = mhdf_open_table( elem_id, POLY_INDEX_NAME,
                              1, &row_count, status );
  if (index_id < 0)
    return ;
  *num_poly_out = (int)row_count;
  
  table_id = mhdf_open_table( elem_id, CONNECTIVITY_NAME, 
                              1, &row_count, status );
  
  if (table_id < 0)
  {
    H5Dclose( index_id );
    return ;
  }
  *data_list_length_out = (long)row_count;
    
  if (!mhdf_read_scalar_attrib( table_id, START_ID_ATTRIB, H5T_NATIVE_INT, 
                                first_poly_id_out, status ))
  {
    H5Dclose( table_id );
    H5Dclose( index_id );
    return ;
  }
  
  file_ptr->open_handle_count++;
  handles_out[0] = index_id;
  handles_out[1] = table_id;
  mhdf_setOkay( status );
}

void
mhdf_writePolyConnIndices( hid_t table_id,
                           long offset,
                           long count,
                           hid_t hdf_integer_type,
                           const void* index_list,
                           mhdf_Status* status )
{
  mhdf_write_data( table_id, offset, count, hdf_integer_type, index_list, status );
}

void 
mhdf_readPolyConnIndices( hid_t table_id,
                          long offset,
                          long count,
                          hid_t hdf_integer_type,
                          void* index_list,
                          mhdf_Status* status )
{
  mhdf_read_data( table_id, offset, count, hdf_integer_type, index_list, status );
}

void
mhdf_writePolyConnIDs( hid_t table_id,
                       long offset,
                       long count,
                       hid_t hdf_integer_type,
                       const void* id_list,
                       mhdf_Status* status )
{
  mhdf_write_data( table_id, offset, count, hdf_integer_type, id_list, status );
}

void 
mhdf_readPolyConnIDs( hid_t table_id,
                      long offset,
                      long count,
                      hid_t hdf_integer_type,
                      void* id_list,
                      mhdf_Status* status )
{
  mhdf_read_data( table_id, offset, count, hdf_integer_type, id_list, status );
}

