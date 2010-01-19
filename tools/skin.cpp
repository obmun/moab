#include <iostream>
#include <set>
#include <limits>
#include <time.h>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <assert.h>
#if !defined(_MSC_VER) && !defined(__MINGW32__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include "MBInterface.hpp"
#include "MBTagConventions.hpp"
#include "MBCore.hpp"
#include "MBRange.hpp"
#include "MBSkinner.hpp"
#include "MBAdaptiveKDTree.hpp"
#include "MBCN.hpp"

void get_time_mem(double &tot_time, double &tot_mem);

// Different platforms follow different conventions for usage
#if !defined(_MSC_VER) && !defined(__MINGW32__)
#include <sys/resource.h>
#endif
#ifdef SOLARIS
extern "C" int getrusage(int, struct rusage *);
#ifndef RUSAGE_SELF
#include </usr/ucbinclude/sys/rusage.h>
#endif
#endif

const char DEFAULT_FIXED_TAG[] = "fixed"; 
const int MIN_EDGE_LEN_DENOM = 4;

#define CHKERROR( A ) do { if (MB_SUCCESS != (A)) { \
 std::cerr << "Internal error at line " << __LINE__ << std::endl; \
 return 3; } } while(false)

MBErrorCode merge_duplicate_vertices( MBInterface&, double epsilon );
MBErrorCode min_edge_length( MBInterface&, double& result );

void usage( const char* argv0, bool help = false ) 
{
  std::ostream& str = help ? std::cout : std::cerr;

  str << "Usage: " << argv0 
      << " [-b <block_num> [-b ...] ] [-p] [-s <sideset_num>] [-t|-T <name>] [-w] [-v|-V <n>]"
      << " <input_file> [<output_file>]" << std::endl;
  str << "Help : " << argv0 << " -h" << std::endl;
  if (!help)
    exit(1);
    
  str << "Options: " << std::endl;
  str << "-a : Compute skin using vert-elem adjacencies (more memory, less time)." << std::endl;
  str << "-b <block_num> : Compute skin only for material set/block <block_num>." << std::endl;
  str << "-p : Print cpu & memory performance." << std::endl;
  str << "-s <sideset_num> : Put skin in neumann set/sideset <sideset_num>." << std::endl;
  str << "-t : Set '" << DEFAULT_FIXED_TAG << "' tag on skin vertices." << std::endl;
  str << "-T <name> : Create tag with specified name and set to 1 on skin vertices." << std::endl;
  str << "-w : Write out whole mesh (otherwise just writes skin)." << std::endl;
  str << "-m : consolidate duplicate vertices" << std::endl;
  str << "-M <n> : consolidate duplicate vertices with specified tolerance. "
          "(Default: min_edge_length/" << MIN_EDGE_LEN_DENOM << ")" << std::endl;

  exit(0);
}


int main( int argc, char* argv[] )
{
  int i = 1;
  std::vector<int> matsets;
  int neuset_num = -1;
  bool write_tag = false, write_whole_mesh = false;
  bool print_perf = false;
  bool use_vert_elem_adjs = false;
  bool merge_vertices = false;
  double merge_epsilon = -1;
  const char* fixed_tag = DEFAULT_FIXED_TAG;
  const char *input_file = 0, *output_file = 0;
  
  bool no_more_flags = false;
  char* endptr = 0;
  long block;
  while (i < argc) {
    if (!no_more_flags && argv[i][0] == '-') {
      const int f = i++;
      for (int j = 1; argv[f][j]; ++j) {
        switch (argv[f][j]) {
          case 'a': use_vert_elem_adjs = true; break;
          case 'p': print_perf = true;         break;
          case 't': write_tag = true;          break;
          case 'w': write_whole_mesh = true;   break;
          case 'm': merge_vertices = true;     break;
          case '-': no_more_flags = true;      break;
          case 'h': usage( argv[0], true );    break;
          case 'b': 
            if (i == argc || 0 >= (block = strtol(argv[i],&endptr,0)) || *endptr) {
              std::cerr << "Expected positive integer following '-b' flag" << std::endl;
              usage(argv[0]);
            }
            matsets.push_back((int)block);
            ++i;
            break;
          case 'T':
            if (i == argc || argv[i][0] == '-') {
              std::cerr << "Expected tag name following '-T' flag" << std::endl;
              usage(argv[0]);
            }
            fixed_tag = argv[i++];
            break;
          case 'M':  
            if (i == argc || 0.0 > (merge_epsilon = strtod(argv[i],&endptr)) || *endptr) {
              std::cerr << "Expected positive numeric value following '-M' flag" << std::endl;
              usage(argv[0]);
            }
            merge_vertices = true;
            ++i;
            break;
          default:
            std::cerr << "Unrecognized flag: '" << argv[i][j] << "'" << std::endl;
            usage(argv[0]);
            break;
        }
      }
    }
    else if (input_file && output_file) {
      std::cerr << "Extra argument: " << argv[i] << std::endl;
      usage(argv[0]);
    }
    else if (input_file) {
      output_file = argv[i++];
    }
    else {
      input_file = argv[i++];
    }
  }

  if (!input_file) {
    std::cerr << "No input file specified" << std::endl;
    usage(argv[0]);
  }

  
  MBErrorCode result;
  MBCore mbimpl;
  MBInterface* iface = &mbimpl;
  
  if (print_perf) {
    double tmp_time1, tmp_mem1;
    get_time_mem(tmp_time1, tmp_mem1);
    std::cout << "Before reading: cpu time = " << tmp_time1 << ", memory = " 
              << tmp_mem1/1.0e6 << "MB." << std::endl;
  }

    // read input file
  result = iface->load_mesh( input_file );
  if (MB_SUCCESS != result)
  { 
    std::cerr << "Failed to load \"" << input_file << "\"." << std::endl; 
    return 2;
  }
  std::cerr << "Read \"" << input_file << "\"" << std::endl;
  if (print_perf) {
    double tmp_time2, tmp_mem2;
    get_time_mem(tmp_time2, tmp_mem2);
    std::cout << "After reading: cpu time = " << tmp_time2 << ", memory = " 
              << tmp_mem2/1.0e6 << "MB." << std::endl;
  }
  
  if (merge_vertices) {
    if (merge_epsilon < 0.0) {
      if (MB_SUCCESS != min_edge_length( *iface, merge_epsilon )) {
        std::cerr << "Error determining minimum edge length" << std::endl;
        return 1;
      }
      merge_epsilon /= MIN_EDGE_LEN_DENOM;
    }
    if (MB_SUCCESS != merge_duplicate_vertices( *iface, merge_epsilon )) {
      std::cerr << "Error merging duplicate vertices" << std::endl;
      return 1;
    }
  }
  
    // get entities of largest dimension
  int dim = 4;
  MBRange entities;
  while (entities.empty() && dim > 1)
  {
    dim--;
    result = iface->get_entities_by_dimension( 0, dim, entities );
    CHKERROR(result);
  }

  MBRange skin_ents;
  MBTag matset_tag = 0, neuset_tag = 0;
  result = iface->tag_get_handle(MATERIAL_SET_TAG_NAME, matset_tag);
  result = iface->tag_get_handle(NEUMANN_SET_TAG_NAME, neuset_tag);

  if (matsets.empty()) skin_ents = entities;
  else {
      // get all entities in the specified blocks
    if (0 == matset_tag) {
      std::cerr << "Couldn't find any material sets in this mesh." << std::endl;
      return 1;
    }
    
    for (std::vector<int>::iterator vit = matsets.begin(); vit != matsets.end(); vit++) {
      int this_matset = *vit;
      const void *this_matset_ptr = &this_matset;
      MBRange this_range, ent_range;
      result = iface->get_entities_by_type_and_tag(0, MBENTITYSET, &matset_tag,
                                                    &this_matset_ptr, 1, this_range);
      if (MB_SUCCESS != result) {
        std::cerr << "Trouble getting material set #" << *vit << std::endl;
        return 1;
      }
      else if (this_range.empty()) {
        std::cerr << "Warning: couldn't find material set " << *vit << std::endl;
        continue;
      }
      
      result = iface->get_entities_by_dimension(*this_range.begin(), dim, ent_range, true);
      if (MB_SUCCESS != result) continue;
      skin_ents.merge(ent_range);
    }
  }
  
  if (skin_ents.empty()) {
    std::cerr << "No entities for which to compute skin; exiting." << std::endl;
    return 1;
  }

  if (use_vert_elem_adjs) {
      // make a call which we know will generate vert-elem adjs
    MBRange dum_range;
    result = iface->get_adjacencies(&(*skin_ents.begin()), 1, 1, false,
                                    dum_range);
  }
  
  double tmp_time = 0.0, tmp_mem = 0.0;
  if (print_perf) {
    get_time_mem(tmp_time, tmp_mem);
    std::cout << "Before skinning: cpu time = " << tmp_time << ", memory = " 
              << tmp_mem/1.0e6 << "MB." << std::endl;
  }

    // skin the mesh
  MBRange forward_lower, reverse_lower;
  MBSkinner tool( iface );
  result = tool.find_skin( skin_ents, false, forward_lower, &reverse_lower );
  MBRange boundary;
  boundary.merge( forward_lower );
  boundary.merge( reverse_lower );
  if (MB_SUCCESS != result || boundary.empty())
  {
    std::cerr << "Mesh skinning failed." << std::endl;
    return 3;
  }

  if (write_tag) {
      // get tag handle
    MBTag tag;
    result = iface->tag_get_handle( fixed_tag, tag );
    if (result == MB_SUCCESS)
    {
      int size;
      MBDataType type;
      iface->tag_get_size(tag, size);
      iface->tag_get_data_type(tag, type);
    
      if (size != sizeof(int) || type != MB_TYPE_INTEGER)
      {
        std::cerr << '"' << fixed_tag << "\" tag defined with incorrect size or type" << std::endl;
        return 3;
      }
    }
    else if (result == MB_TAG_NOT_FOUND)
    {
      int zero = 0;
      result = iface->tag_create( fixed_tag, sizeof(int), MB_TAG_DENSE, MB_TYPE_INTEGER, tag, &zero );
      CHKERROR(result);
    }
    else
    {
      CHKERROR(result);
    }
  
      // Set tags
    std::vector<int> ones;
    MBRange bverts;
    result = iface->get_adjacencies(boundary, 0, false, bverts, MBInterface::UNION);
    if (MB_SUCCESS != result) {
      std::cerr << "Trouble getting vertices on boundary." << std::endl;
      return 1;
    }
    ones.resize( bverts.size(), 1 );
    result = iface->tag_set_data( tag, bverts, &ones[0] );
    CHKERROR(result);
  }
  
  if (-1 != neuset_num) {
      // create a neumann set with these entities
    if (0 == neuset_tag) {
      result = iface->tag_create("NEUMANN_SET_TAG_NAME", sizeof(int), MB_TAG_SPARSE,
                                  MB_TYPE_INTEGER, neuset_tag, NULL);
      if (MB_SUCCESS != result || 0 == neuset_tag) return 1;
    }
    

      // always create a forward neumann set, assuming we have something in the set
    MBEntityHandle forward_neuset = 0;
    result = iface->create_meshset(MESHSET_SET, forward_neuset);
    if (MB_SUCCESS != result || 0 == forward_neuset) return 1;
    result = iface->tag_set_data(neuset_tag, &forward_neuset, 1, &neuset_num);
    if (MB_SUCCESS != result) return 1;

    if (!forward_lower.empty()) {
      result = iface->add_entities(forward_neuset, forward_lower);
      if (MB_SUCCESS != result) return 1;
    }
    if (!reverse_lower.empty()) {
      MBEntityHandle reverse_neuset = 1;
      result = iface->create_meshset(MESHSET_SET, reverse_neuset);
      if (MB_SUCCESS != result || 0 == forward_neuset) return 1;

      result = iface->add_entities(reverse_neuset, reverse_lower);
      if (MB_SUCCESS != result) return 1;
      MBTag sense_tag;
      result = iface->tag_get_handle("SENSE", sense_tag);
      if (result == MB_TAG_NOT_FOUND) {
        int dum_sense = 0;
        result = iface->tag_create("SENSE", sizeof(int), MB_TAG_SPARSE, sense_tag, &dum_sense);
      }
      if (result != MB_SUCCESS) return 1;
      int sense_val = -1;
      result = iface->tag_set_data(neuset_tag, &reverse_neuset, 1, &sense_val);
      if (MB_SUCCESS != result) return 0;
      result = iface->add_entities(forward_neuset, &reverse_neuset, 1);
      if (MB_SUCCESS != result) return 0;
    }
  }

  if (NULL != output_file && write_whole_mesh) {
    
      // write output file
    result = iface->write_mesh( output_file);
    if (MB_SUCCESS != result)
    { 
      std::cerr << "Failed to write \"" << output_file << "\"." << std::endl; 
      return 2;
    }
    std::cerr << "Wrote \"" << output_file << "\"" << std::endl;
  }
  else if (NULL != output_file) {
      // write only skin; write them as one set
    MBEntityHandle skin_set;
    result = iface->create_meshset(MESHSET_SET, skin_set);
    if (MB_SUCCESS != result) return 1;
    result = iface->add_entities(skin_set, forward_lower);
    if (MB_SUCCESS != result) return 1;
    result = iface->add_entities(skin_set, reverse_lower);
    if (MB_SUCCESS != result) return 1;

    MBRange this_range, ent_range;
    result = iface->get_entities_by_type_and_tag(0, MBENTITYSET, &matset_tag,
                                                  NULL, 0, this_range);
    if (!this_range.empty()) iface->delete_entities(this_range);

    int dum = 10000;
    result = iface->tag_set_data(matset_tag, &skin_set, 1, &dum);
    

    result = iface->write_mesh( output_file, &skin_set, 1);
    if (MB_SUCCESS != result)
    { 
      std::cerr << "Failed to write \"" << output_file << "\"." << std::endl; 
      return 2;
    }
    std::cerr << "Wrote \"" << output_file << "\"" << std::endl;
  }

  if (print_perf) {
    double tot_time, tot_mem;
    get_time_mem(tot_time, tot_mem);
    std::cout << "Total cpu time = " << tot_time << " seconds." << std::endl;
    std::cout << "Total skin cpu time = " << tot_time-tmp_time << " seconds." << std::endl;
    std::cout << "Total memory = " << tot_mem/1.0e6 << " MB." << std::endl;
    std::cout << "Total skin memory = " << (tot_mem-tmp_mem)/1.0e6 << " MB." << std::endl;
    std::cout << "Entities: " << std::endl;
    iface->list_entities(0, 0);
  }
  
  return 0;
}

#if defined(_MSC_VER) || defined(__MINGW32__)
void get_time_mem(double &tot_time, double &tot_mem) 
{
  tot_time = (double)clock() / CLOCKS_PER_SEC;
  tot_mem = 0;
}
#else
void get_time_mem(double &tot_time, double &tot_mem) 
{
  struct rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);
  double utime = (double)r_usage.ru_utime.tv_sec +
    ((double)r_usage.ru_utime.tv_usec/1.e6);
  double stime = (double)r_usage.ru_stime.tv_sec +
    ((double)r_usage.ru_stime.tv_usec/1.e6);
  tot_time = utime + stime;
  tot_mem = 0;
  if (0 != r_usage.ru_maxrss) {
    tot_mem = r_usage.ru_idrss; 
  }
  else {
      // this machine doesn't return rss - try going to /proc
      // print the file name to open
    char file_str[4096], dum_str[4096];
    int file_ptr = -1, file_len;
    file_ptr = open("/proc/self/stat", O_RDONLY);
    file_len = read(file_ptr, file_str, sizeof(file_str)-1);
    if (file_len == 0) return;
    
    close(file_ptr);
    file_str[file_len] = '\0';
      // read the preceeding fields and the ones we really want...
    int dum_int;
    unsigned int dum_uint, vm_size, rss;
    int num_fields = sscanf(file_str, 
                            "%d " // pid
                            "%s " // comm
                            "%c " // state
                            "%d %d %d %d %d " // ppid, pgrp, session, tty, tpgid
                            "%u %u %u %u %u " // flags, minflt, cminflt, majflt, cmajflt
                            "%d %d %d %d %d %d " // utime, stime, cutime, cstime, counter, priority
                            "%u %u " // timeout, itrealvalue
                            "%d " // starttime
                            "%u %u", // vsize, rss
                            &dum_int, 
                            dum_str, 
                            dum_str, 
                            &dum_int, &dum_int, &dum_int, &dum_int, &dum_int, 
                            &dum_uint, &dum_uint, &dum_uint, &dum_uint, &dum_uint,
                            &dum_int, &dum_int, &dum_int, &dum_int, &dum_int, &dum_int, 
                            &dum_uint, &dum_uint, 
                            &dum_int,
                            &vm_size, &rss);
    if (num_fields == 24)
      tot_mem = ((double)vm_size);
  }
}
#endif
  
  
  
MBErrorCode min_edge_length( MBInterface& moab, double& result )
{
  double sqr_result = std::numeric_limits<double>::max();
  
  MBErrorCode rval;
  MBRange entities;
  rval = moab.get_entities_by_handle( 0, entities );
  if (MB_SUCCESS != rval) return rval;
  MBRange::iterator i = entities.upper_bound( MBVERTEX );
  entities.erase( entities.begin(), i );
  i = entities.lower_bound( MBENTITYSET );
  entities.erase( i, entities.end() );
  
  std::vector<MBEntityHandle> storage;
  for (i = entities.begin(); i != entities.end(); ++i) {
    MBEntityType t = moab.type_from_handle( *i );
    const MBEntityHandle* conn;
    int conn_len, indices[2];
    rval = moab.get_connectivity( *i, conn, conn_len, true, &storage );
    if (MB_SUCCESS != rval) return rval;
    
    int num_edges = MBCN::NumSubEntities( t, 1 );
    for (int j = 0; j < num_edges; ++j) {
      MBCN::SubEntityVertexIndices( t, 1, j, indices );
      MBEntityHandle v[2] = { conn[indices[0]], conn[indices[1]] };
      if (v[0] == v[1])
        continue;
      
      double c[6];
      rval = moab.get_coords( v, 2, c );
      if (MB_SUCCESS != rval) return rval;
      
      c[0] -= c[3];
      c[1] -= c[4];
      c[2] -= c[5];
      double len_sqr = c[0]*c[0] + c[1]*c[1] + c[2]*c[2];
      if (len_sqr < sqr_result)
        sqr_result = len_sqr;
    }
  }
  
  result = sqrt(sqr_result);
  return MB_SUCCESS;
}
  
  
  
MBErrorCode merge_duplicate_vertices( MBInterface& moab, const double epsilon )
{
  MBErrorCode rval;
  MBRange verts;
  rval = moab.get_entities_by_type( 0, MBVERTEX, verts );
  if (MB_SUCCESS != rval)
    return rval;
  
  MBAdaptiveKDTree tree( &moab );
  MBEntityHandle root;
  rval = tree.build_tree( verts, root );
  if (MB_SUCCESS != rval) {
    fprintf(stderr,"Failed to build kD-tree.\n");
    return rval;
  }
  
  std::set<MBEntityHandle> dead_verts;
  std::vector<MBEntityHandle> leaves;
  for (MBRange::iterator i = verts.begin(); i != verts.end(); ++i) {
    double coords[3];
    rval = moab.get_coords( &*i, 1, coords );
    if (MB_SUCCESS != rval) return rval;
    
    leaves.clear();;
    rval = tree.leaves_within_distance( root, coords, epsilon, leaves );
    if (MB_SUCCESS != rval) return rval;
    
    MBRange near;
    for (std::vector<MBEntityHandle>::iterator j = leaves.begin(); j != leaves.end(); ++j) {
      MBRange tmp;
      rval = moab.get_entities_by_type( *j, MBVERTEX, tmp );
      if (MB_SUCCESS != rval)
        return rval;
      near.merge( tmp.begin(), tmp.end() );
    }
    
    MBRange::iterator v = near.find( *i );
    assert( v != near.end() );
    near.erase( v );
    
    MBEntityHandle merge = 0;
    for (MBRange::iterator j = near.begin(); j != near.end(); ++j) {
      if (*j < *i && dead_verts.find( *j ) != dead_verts.end())
        continue;
      
      double coords2[3];
      rval = moab.get_coords( &*j, 1, coords2 );
      if (MB_SUCCESS != rval) return rval;
      
      coords2[0] -= coords[0];
      coords2[1] -= coords[1];
      coords2[2] -= coords[2];
      double dsqr = coords2[0]*coords2[0] +
                    coords2[1]*coords2[1] +
                    coords2[2]*coords2[2];
      if (dsqr <= epsilon*epsilon) {
        merge = *j;
        break;
      }
    }
    
    if (merge) {
      dead_verts.insert(*i);
      rval = moab.merge_entities( merge, *i, false, true );
      if (MB_SUCCESS != rval) return rval;
    }
  }
  
  if (dead_verts.empty()) 
    std::cout << "No duplicate/coincident vertices." << std::endl;
  else
    std::cout << "Merged and deleted " << dead_verts.size() << " vertices "
              << "coincident within " << epsilon << std::endl;
  
  return MB_SUCCESS;
}

    

