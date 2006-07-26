#include "MBCore.hpp"
#include "MBRange.hpp"
#include "MBOrientedBoxTreeTool.hpp"
#include "MBOrientedBox.hpp"

#include <iostream>
#include <stdlib.h>
#include <limits>
#include <set>

#include "testdir.h"
const char* NAME = "obb_test";
const char* DEFAULT_FILES[] = { TEST_DIR "/3k-tri-sphere.vtk",
                                TEST_DIR "/4k-tri-plane.vtk",
                                0 };

static void usage( const char* error, const char* opt )
{
  const char* default_message = "Invalid option";
  if (opt && !error)
    error = default_message;

  std::ostream& str = error ? std::cerr : std::cout;
  if (error) {
    str << error;
    if (opt)
      str << ": " << opt;
    str << std::endl;
  }

  str << "Usage: "<<NAME<<" [output opts.] [settings] [file ...]" << std::endl;
  str << "       "<<NAME<<" -h" << std::endl;
  if (!error)
    str << " If no file is specified the defautl test files will be used" << std::endl
        << " -h  print help text. " << std::endl
        << " -v  verbose output (may be specified multiple times) " << std::endl
        << " -q  quite (minimal output) " << std::endl
        << " -c  write box geometry to Cubit journal file." << std::endl
        << " -k  write leaf contents to vtk files." << std::endl
        << " -t <real> specify tolerance" << std::endl
        << " -n <int>  specify max entities per leaf node " << std::endl
        << " -l <int>  specify max tree levels" << std::endl
        << " -r <real> specify worst cell split ratio" << std::endl
        << " -R <real> specify best cell split ratio" << std::endl
#if MB_OOB_SPLIT_BY_NON_INTERSECTING
        << " -I <real> specify weight for intersection ratio" << std::endl
#endif
        << " Verbosity (-q sets to 0, each -v increments, default is 1):" << std::endl
        << "  0 - no output" << std::endl
        << "  1 - status messages and error summary" << std::endl
        << "  2 - print tree statistics " << std::endl
        << "  3 - output errors for each node" << std::endl
        << "  4 - print tree" << std::endl
        << "  5 - print tree w/ contents of each node" << std::endl
        << " See documentation for MMOrientedBoxTreeTool::Settings for " << std::endl
        << " a description of tree generation settings." << std::endl
      ;
  exit( !!error );
}

static const char* get_option( int& i, int argc, char* argv[] ) {
  ++i;
  if (i == argc)
    usage( "Expected argument following option", argv[i-1] );
  return argv[i];
}

static int get_int_option( int& i, int argc, char* argv[] ) {
  const char* str = get_option( i, argc, argv );
  char* end_ptr;
  long val = strtol( str, &end_ptr, 0 );
  if (!*str || *end_ptr) 
    usage( "Expected integer following option", argv[i-1] );
  return val;
}

static double get_double_option( int& i, int argc, char* argv[] ) {
  const char* str = get_option( i, argc, argv );
  char* end_ptr;
  double val = strtod( str, &end_ptr );
  if (!*str || *end_ptr) 
    usage( "Expected real number following option", argv[i-1] );
  return val;
}

static bool do_file( const char* filename );
  

int verbosity = 1;
MBOrientedBoxTreeTool::Settings settings;
double tolerance = 1e-6;
bool write_cubit = false;
bool write_vtk = false;

int main( int argc, char* argv[] )
{
  std::vector<const char*> file_names;
  bool flags = true;
  for (int i = 1; i < argc; ++i) {
    if (flags && argv[i][0] =='-') {
      if (!argv[i][1] || argv[i][2])
        usage(0,argv[i]);
      switch (argv[i][1]) {
        default:  usage( 0, argv[i] );break;
        case '-': flags = false;      break;
        case 'v': ++verbosity;        break;
        case 'q': verbosity = 0;      break;
        case 'h': usage( 0, 0 );      break;
        case 'c': write_cubit = true; break;
        case 'k': write_vtk = true;   break;
        case 'n': 
          settings.max_leaf_entities = get_int_option( i, argc, argv );
          break;
        case 'l':
          settings.max_depth = get_int_option( i, argc, argv );
          break;
        case 'r':
          settings.worst_split_ratio = get_double_option( i, argc, argv );
          break;
        case 'R':
          settings.best_split_ratio = get_double_option( i, argc, argv );
          break;
#if MB_OOB_SPLIT_BY_NON_INTERSECTING
        case 'I':
          settings.intersect_ratio_factor = get_double_option( i, argc, argv );
          break;
#endif
        case 't':
          tolerance = get_double_option( i, argc, argv );
          break;
      }
    }
    else {
      file_names.push_back( argv[i] );
    }
  }
  
  if (verbosity) {
    MBCore core;
    std::string version;
    core.impl_version( & version );
    std::cout << version << std::endl;
    if (verbosity > 1) 
      std::cout << "max_leaf_entities:      " << settings.max_leaf_entities      << std::endl
                << "max_depth:              " << settings.max_depth              << std::endl
                << "worst_split_ratio:      " << settings.worst_split_ratio      << std::endl
                << "best_split_ratio:       " << settings.best_split_ratio       << std::endl
#if MB_OOB_SPLIT_BY_NON_INTERSECTING
                << "intersect ratio factor: " << settings.intersect_ratio_factor << std::endl
#endif
                << "tolerance:              " << tolerance                       << std::endl
                << std::endl;
  }
  
  if (!settings.valid() || tolerance < 0.0) {
    std::cerr << "Invalid settings specified." << std::endl;
    return 2;
  }
  
  if (file_names.empty()) {
    std::cerr << "No file(s) specified." << std::endl;
    for (int i = 0; DEFAULT_FILES[i]; ++i) {
      std::cerr << "Using default file \"" << DEFAULT_FILES[i] << '"' << std::endl;
      file_names.push_back( DEFAULT_FILES[i] );
    }
  }
  
  int exit_val = 0;
  for (unsigned j = 0; j < file_names.size(); ++j)
    if (!do_file( file_names[j] ))
      ++exit_val;
  
  return exit_val ? exit_val + 2 : 0;
}

class TreeValidator : public MBOrientedBoxTreeTool::Op
{
  private:
    MBInterface* const instance;
    MBOrientedBoxTreeTool* const tool;
    const bool printing;
    const double epsilon;
    std::ostream& stream;
    MBOrientedBoxTreeTool::Settings settings;
    
    void print( MBEntityHandle handle, const char* string ) {
      if (printing)
        stream << instance->id_from_handle(handle) << ": "
               << string << std::endl;
    }
    
    MBErrorCode error( MBEntityHandle handle, const char* string ) {
      ++error_count;
      print( handle, string );
      return MB_SUCCESS;
    }
    
  public:
    
    unsigned entity_count;
  
    unsigned loose_box_count;
    unsigned child_outside_count;
    unsigned entity_outside_count;
    unsigned num_entities_outside;
    unsigned non_ortho_count;
    unsigned error_count;
    unsigned empty_leaf_count;
    unsigned non_empty_non_leaf_count;
    unsigned entity_invalid_count;
    unsigned unsorted_axis_count;
    unsigned non_unit_count;
    unsigned duplicate_entity_count;
    std::set<MBEntityHandle> seen;

    TreeValidator( MBInterface* instance_ptr, 
                   MBOrientedBoxTreeTool* tool_ptr,
                   bool print_errors,
                   std::ostream& str,
                   double tol,
                   MBOrientedBoxTreeTool::Settings s )
      : instance(instance_ptr),
        tool(tool_ptr),
        printing(print_errors), 
        epsilon(tol), 
        stream(str),
        settings(s),
        entity_count(0),
        loose_box_count(0),
        child_outside_count(0),
        entity_outside_count(0),
        num_entities_outside(0),
        non_ortho_count(0),
        error_count(0),
        empty_leaf_count(0),
        non_empty_non_leaf_count(0),
        entity_invalid_count(0),
        unsorted_axis_count(0),
        non_unit_count(0),
        duplicate_entity_count(0)
      {}
    
    bool is_valid() const 
      { return 0 == loose_box_count+child_outside_count+entity_outside_count+
                    num_entities_outside+non_ortho_count+error_count+
                    empty_leaf_count+non_empty_non_leaf_count+entity_invalid_count
                    +unsorted_axis_count+non_unit_count+duplicate_entity_count; }
    
    virtual MBErrorCode operator()( MBEntityHandle node,
                                    int depth,
                                    bool& descend );
};


MBErrorCode TreeValidator::operator()( MBEntityHandle node,
                                       int depth,
                                       bool& descend )
{
  MBErrorCode rval;
  descend = true;
  
  MBRange contents;
  rval = instance->get_entities_by_handle( node, contents );
  if (MB_SUCCESS != rval) 
    return error(node, "Error getting contents of tree node.  Corrupt tree?");
  entity_count += contents.size();
  
  bool leaf;
  MBEntityHandle children[2];
  rval = tool->children( node, leaf, children );
  if (MB_SUCCESS != rval) 
    return error(node, "Error getting children.  Corrupt tree?");
  
  MBOrientedBox box;
  rval = tool->box( node, box );
  if (MB_SUCCESS != rval) 
    return error(node, "Error getting oriented box from tree node.  Corrupt tree?");
  
  if (leaf && contents.empty()) {
    ++empty_leaf_count;
    print( node, "Empty leaf node.\n" );
  }
  else if (!leaf && !contents.empty()) {
    ++non_empty_non_leaf_count;
    print( node, "Non-leaf node is not empty." );
  }
  
  double dot_epsilon = epsilon*(box.axis[0]+box.axis[1]+box.axis[2]).length();
  if (box.axis[0] % box.axis[1] > dot_epsilon ||
      box.axis[0] % box.axis[2] > dot_epsilon ||
      box.axis[1] % box.axis[2] > dot_epsilon ) {
    ++non_ortho_count;
    print (node, "Box axes are not orthogonal");
  }
  
  if (!leaf) {
    for (int i = 0; i < 2; ++i) {
      MBOrientedBox other_box;
      rval = tool->box( children[i], other_box );
      if (MB_SUCCESS != rval) 
        return error( children[i], " Error getting oriented box from tree node.  Corrupt tree?" );
//      else if (!box.contained( other_box, epsilon )) {
//        ++child_outside_count;
//        print( children[i], "Parent box does not contain child box." );
//        char string[64];
//        sprintf(string, "     Volume ratio is %f", other_box.volume()/box.volume() );
//        print( children [i], string );
//      }
        else {
          double vol_ratio = other_box.volume()/box.volume();
          if (vol_ratio > 2.0) {
            char string[64];
            sprintf(string, "child/parent volume ratio is %f", vol_ratio );
            print( children[i], string );
            sprintf(string, "   child/parent area ratio is %f", other_box.area()/box.area() );
            print( children[i], string );
          }
       }
    }
  }
  
  bool bad_element = false;
  bool bad_element_handle = false;
  bool bad_element_conn = false;
  bool duplicate_element = false;
  int num_outside = 0;
  bool boundary[6] = { false, false, false, false, false, false };
  for (MBRange::iterator it = contents.begin(); it != contents.end(); ++it) {
    MBEntityType type = instance->type_from_handle( *it );
    int dim = MBCN::Dimension( type );
    if (dim != 2) {
      bad_element = true;
      continue;
    }
    
    const MBEntityHandle* conn;
    int conn_len;
    rval = instance->get_connectivity( *it, conn, conn_len );
    if (MB_SUCCESS != rval) {
      bad_element_handle = true;
      continue;
    }
    
    std::vector<MBCartVect> coords(conn_len);
    rval = instance->get_coords( conn, conn_len, coords[0].array() );
    if (MB_SUCCESS != rval) {
      bad_element_conn = true;
      continue;
    }
    
    bool outside = false;
    for (std::vector<MBCartVect>::iterator j = coords.begin(); j != coords.end(); ++j) {
      if (!box.contained( *j, epsilon ))
        outside = true;
      else for (int d = 0; d < 3; ++d)
        for (int s = 0; s < 2; ++s) {
          const double sign[] = {-1, 1};
#ifdef MB_ORIENTED_BOX_UNIT_VECTORS
          MBCartVect v = sign[s] * box.length[d] * box.axis[d];
#else 
          MBCartVect v = sign[s] * box.axis[d];
#endif
          v *= ((*j - box.center - v) % v) / (v % v);
           if (v.length() <= epsilon)
            boundary[2*d+s] = true;
        }
    }
    if (outside)
      ++num_outside;
      
    if (!seen.insert(*it).second) {
      duplicate_element = true;
      ++duplicate_entity_count;
    }
  }
  
  MBCartVect alength( box.axis[0].length(), box.axis[1].length(), box.axis[2].length() );
#ifdef MB_ORIENTED_BOX_UNIT_VECTORS
  MBCartVect length = box.length;
#else
  MBCartVect length = alength;
#endif
  
  if (length[0] > length[1] || length[0] > length[2] || length[1] > length[2]) {
    ++unsorted_axis_count;
    print( node, "Box axes are not ordered from shortest to longest." );
  }
  
#ifdef MB_ORIENTED_BOX_UNIT_VECTORS
  if (fabs(alength[0] - 1.0) > epsilon ||
      fabs(alength[1] - 1.0) > epsilon ||
      fabs(alength[2] - 1.0) > epsilon) {
    ++non_unit_count;
    print( node, "Box axes are not unit vectors.");
  }
#endif

  if (depth+1 < settings.max_depth 
      && contents.size() > (unsigned)(4*settings.max_leaf_entities))
  {
    char string[64];
    sprintf(string, "leaf at depth %d with %u entities", depth, (unsigned)contents.size() );
    print( node, string );
  }
    
      
  bool all_boundaries = true;
  for (int f = 0; f < 6; ++f)
    all_boundaries = all_boundaries && boundary[f];
  
  if (bad_element) {
    ++entity_invalid_count;
    print( node, "Set contained an entity with an inappropriate dimension." );
  }
  if (bad_element_handle) {
    ++error_count;
    print( node, "Error querying face contained in set.");
  }
  if (bad_element_conn) {
    ++error_count;
    print( node, "Error querying connectivity of element.");
  }
  if (duplicate_element) {
    print( node, "Elements occur in multiple leaves of tree.");
  }
  if (num_outside > 0) {
    ++entity_outside_count;
    num_entities_outside += num_outside;
    if (printing)
      stream << instance->id_from_handle( node ) << ": "
             << num_outside << " elements outside box." << std::endl;
  }
  else if (!all_boundaries && !contents.empty()) {
    ++loose_box_count;
    print( node, "Box does not fit contained elements tightly." );
  }

  return MB_SUCCESS;
}

class CubitWriter : public MBOrientedBoxTreeTool::Op
{
  public:
    CubitWriter( FILE* file_ptr, 
                 MBOrientedBoxTreeTool* tool_ptr )
      : file(file_ptr), tool(tool_ptr) {}
    
    MBErrorCode operator() ( MBEntityHandle node,
                             int depth,
                             bool& descend );
    
  private:
    FILE* file;
    MBOrientedBoxTreeTool* tool;
};

MBErrorCode CubitWriter::operator()( MBEntityHandle node,
                                     int ,
                                     bool& descend )
{
  descend = true;
  MBOrientedBox box;
  MBErrorCode rval = tool->box( node, box );
  if (rval != MB_SUCCESS)
    return rval;

  double sign[] = {-1, 1};
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j)
      for (int k = 0; k < 2; ++k) {
#ifdef MB_ORIENTED_BOX_UNIT_VECTORS
        MBCartVect corner = box.center + box.length[0] * sign[i] * box.axis[0] +
                                         box.length[1] * sign[j] * box.axis[1] +
                                         box.length[2] * sign[k] * box.axis[2];
#else
        MBCartVect corner = box.center + sign[i] * box.axis[0] +
                                         sign[j] * box.axis[1] +
                                         sign[k] * box.axis[2];
#endif
        fprintf( file, "create vertex %f %f %f\n", corner[0],corner[1],corner[2] );
      }
  fprintf( file, "#{i=Id(\"vertex\")-7}\n" );
  fprintf( file, "create surface vertex {i  } {i+1} {i+3} {i+2}\n");
  fprintf( file, "create surface vertex {i+4} {i+5} {i+7} {i+6}\n");
  fprintf( file, "create surface vertex {i+1} {i+0} {i+4} {i+5}\n");
  fprintf( file, "create surface vertex {i+3} {i+2} {i+6} {i+7}\n");
  fprintf( file, "create surface vertex {i+2} {i+0} {i+4} {i+6}\n");
  fprintf( file, "create surface vertex {i+3} {i+1} {i+5} {i+7}\n");
  fprintf( file, "delete vertex {i}-{i+7}\n");
  fprintf( file, "#{s=Id(\"surface\")-5}\n" );
  fprintf( file, "create volume surface {s} {s+1} {s+2} {s+3} {s+4} {s+5} noheal\n" );
  int id = tool->get_moab_instance()->id_from_handle( node );
  fprintf( file, "volume {Id(\"volume\")} name \"cell%d\"\n", id );
  
  return MB_SUCCESS;
}

class VtkWriter : public MBOrientedBoxTreeTool::Op
{
   public:
    VtkWriter( std::string base_name, 
               MBInterface* interface )
      : baseName(base_name), instance(interface) {}
    
    MBErrorCode operator() ( MBEntityHandle node,
                             int depth,
                             bool& descend );
    
  private:
    std::string baseName;
    MBInterface* instance;
};

MBErrorCode VtkWriter::operator()( MBEntityHandle node,
                                   int ,
                                   bool& descend )
{
  descend = true;
  
  MBErrorCode rval;
  int count;
  rval = instance->get_number_entities_by_handle( node, count );
  if (MB_SUCCESS != rval || 0 == count)
    return rval;
  
  int id = instance->id_from_handle( node );
  char id_str[32];
  sprintf( id_str, "%d", id );
  
  std::string file_name( baseName );
  file_name += ".";
  file_name += id_str;
  file_name += ".vtk";
  
  return instance->write_mesh( file_name.c_str(), &node, 1 );
}
  
  
static bool do_file( const char* filename )
{
  MBErrorCode rval;
  MBCore instance;
  MBInterface* const iface = &instance;
  MBOrientedBoxTreeTool tool( iface );
  
  if (verbosity) 
    std::cout << filename << std::endl
              << "------" << std::endl;
  
  rval = iface->load_mesh( filename );
  if (MB_SUCCESS != rval) {
    if (verbosity)
      std::cout << "Failed to read file: \"" << filename << '"' << std::endl;
     return false;
  }
  
  MBRange entities;
  rval = iface->get_entities_by_dimension( 0, 2, entities );
  if (MB_SUCCESS != rval) {
    std::cerr << "get_entities_by_dimension( 2 ) failed." << std::endl;
    return false;
  }
  
  if (entities.empty()) {
    if (verbosity)
      std::cout << "File \"" << filename << "\" contains no 2D elements" << std::endl;
    return false;
  }
  
  if (verbosity) 
    std::cout << "Building tree from " << entities.size() << " 2D elements" << std::endl;
  
  MBEntityHandle root;
  rval = tool.build( entities, root, &settings );
  if (MB_SUCCESS != rval) {
    if (verbosity)
      std::cout << "Failed to build tree." << std::endl;
    return false;
  }

  if (write_cubit) {
    std::string name = filename;
    name += ".boxes.jou";
    FILE* ptr = fopen( name.c_str(), "w+" );
    if (!ptr) {
      perror( name.c_str() );
    }
    else {
      if (verbosity)
        std::cout << "Writing: " << name << std::endl;
      fprintf(ptr,"graphics off\n");
      CubitWriter op( ptr, &tool );
      tool.preorder_traverse( root, op );
      fprintf(ptr,"graphics on\n");
      fclose( ptr );
    }
  }
  
  if (write_vtk) {
    VtkWriter op( filename, iface );
    if (verbosity)
      std::cout << "Writing leaf contents as : " << filename 
                << ".xxx.vtk where 'xxx' is the set id" << std::endl;
    tool.preorder_traverse( root, op );
  }  

  bool print_errors = false, print_contents = false;
  switch (verbosity) {
    default:
      print_contents = true;
    case 4:
      tool.print( root, std::cout, print_contents );
    case 3:
      print_errors = true;
    case 2:
      rval = tool.stats( root, std::cout );
      if (MB_SUCCESS != rval)
        std::cout << "****** Failed to get tree statistics ******" << std::endl;
    case 1:
    case 0:
      ;
  }  
  
  TreeValidator op( iface, &tool, print_errors, std::cout, tolerance, settings ); 
  rval = tool.preorder_traverse( root, op );
  bool result = op.is_valid();
  if (MB_SUCCESS != rval) {
    result = false;
    if (verbosity)
      std::cout << "Errors traversing tree.  Corrupt tree?" << std::endl;
  }
  
  bool missing = (op.entity_count != entities.size());
  if (missing)
    result = false;
  
  if (verbosity) {
    if (result)
      std::cout << std::endl << "No errors detected." << std::endl;
    else
      std::cout << std::endl << "*********************** ERROR SUMMARY **********************" << std::endl;
    if (op.child_outside_count)
      std::cout << "* " << op.child_outside_count << " child boxes not contained in parent." << std::endl;
    if (op.entity_outside_count)
      std::cout << "* " << op.entity_outside_count << " nodes containing entities outside of box." << std::endl;
    if (op.num_entities_outside)
      std::cout << "* " << op.num_entities_outside << " entities outside boxes." << std::endl;
    if (op.empty_leaf_count)
      std::cout << "* " << op.empty_leaf_count << " empty leaf nodes." << std::endl;
    if (op.non_empty_non_leaf_count)
      std::cout << "* " << op.non_empty_non_leaf_count << " non-leaf nodes containing entities." << std::endl;
    if (op.duplicate_entity_count)
      std::cout << "* " << op.duplicate_entity_count << " duplicate entities in leaves." << std::endl;
    if (op.non_ortho_count)
      std::cout << "* " << op.non_ortho_count << " boxes with non-orthononal axes." << std::endl;
    if (op.non_unit_count)
      std::cout << "* " << op.non_unit_count << " boxes with non-unit axes." << std::endl;
    if (op.unsorted_axis_count)
      std::cout << "* " << op.unsorted_axis_count << " boxes axes in unsorted order." << std::endl;
    if (op.loose_box_count)
      std::cout << "* " << op.loose_box_count << " boxes that do not fit the contained entities tightly." << std::endl;
    if (op.error_count + op.entity_invalid_count)
      std::cout << "* " << op.error_count + op.entity_invalid_count
                << " other errors while traversing tree." << std::endl;
    if (missing)
      std::cout << "* tree built from " << entities.size() << " entities contains "
                << op.entity_count << " entities." << std::endl;
    if (!result)
      std::cout << "************************************************************" << std::endl;
  }

  rval = tool.delete_tree( root );
  if (MB_SUCCESS != rval) {
    if (verbosity)
      std::cout << "delete_tree failed." << std::endl;
    result = false;
  }
  
  return result;
}


