#include <iostream>
#include "moab/Interface.hpp"
#include "TestUtil.hpp"
#include "Internals.hpp"
#include "moab/Core.hpp"
#include "MBTagConventions.hpp"
#include "moab/Types.hpp"
#include "moab/GeomTopoTool.hpp"


using namespace moab;

#define CHKERR(A) do { if (MB_SUCCESS != (A)) { \
  std::cerr << "Failure (error code " << (A) << ") at " __FILE__ ":" \
            << __LINE__ << std::endl; \
  return A; } } while(false)

#ifdef MESHDIR
static const char test[] = STRINGIFY(MESHDIR) "/io/test.obj";
#endif

GeomTopoTool *myGeomTool;

Tag geom_tag;
Tag name_tag;


void read_file( Interface& moab, const char* input_file);
void test_check_num_entities();
void test_check_meshsets();


int main()
{
  int result = 0;
  
  result += RUN_TEST(test_check_num_entities);
  result += RUN_TEST(test_check_meshsets);

  return result;
}


void read_file( Interface& moab, const char* input_file )
{
  ErrorCode rval = moab.load_file( input_file );
  CHECK_ERR(rval);
}


void test_check_num_entities()
{
  ErrorCode rval;
  Core core;
  Interface *mbi = &core;
  read_file(core, test);

  // check that number of verts created is 8 
  Range verts;
  int vert_dim = 0;
  rval =  mbi->get_entities_by_dimension(0, vert_dim, verts);
  CHECK_ERR(rval);
  CHECK_EQUAL(8, (int)verts.size());

  // check that number of tris created is 5
  Range tris;
  int tri_dim = 2;
  rval =  mbi->get_entities_by_dimension(0, tri_dim, tris);
  CHECK_ERR(rval);
  CHECK_EQUAL(5, (int)tris.size());

}

void test_check_meshsets()
{
  ErrorCode rval;
  Core core;
  Interface *mbi = &core;
  read_file(core, test);
 
  myGeomTool = new GeomTopoTool(mbi);
  
  Range ent_sets, mesh_sets;
  rval =  mbi->tag_get_handle(GEOM_DIMENSION_TAG_NAME, 1, MB_TYPE_INTEGER, geom_tag);
  CHECK_ERR(rval);
  rval =  mbi->get_entities_by_type_and_tag(0, MBENTITYSET, &geom_tag, NULL, 1, ent_sets);

  Range::iterator it;
  Range parents, children; 
  int dim, num_surfs = 0, num_vols = 0;
  for (it = ent_sets.begin(); it != ent_sets.end(); ++it)
    {
      rval =  mbi->tag_get_data(geom_tag, &(*it), 1, &dim);
      
      if (dim == 2)
        { 
          num_surfs++;
          
          // check that one parent is created for each surface
          parents.clear();
          rval =  mbi->get_parent_meshsets(*it, parents);
          CHECK_ERR(rval);
          CHECK_EQUAL(1, (int)parents.size());

          // check that sense of surface wrt parent is FORWARD = 1
          int sense;
          rval = myGeomTool->get_sense(*it, *parents.begin(), sense);
          CHECK_EQUAL(1, sense);
        }
      else if (dim == 3)
        { 
          num_vols++;
     
          // check that one child is created for each volume
          children.clear();
          rval =  mbi->get_child_meshsets(*it, children);
          CHECK_ERR(rval);
          CHECK_EQUAL(1, (int)children.size());
        }
    }
  
  // check that two surfaces and two volumes are created 
  CHECK_EQUAL(2, num_surfs);
  CHECK_EQUAL(2, num_vols);
}

/*
void check_relationships()
{
  ErrorCode rval;
  Core moab; 
  Interface &  mbi->= moab;
  read_file( moab, test );

  Range ent_sets;
  rval =  mbi->tag_get_handle(GEOM_SENSE_2, 2, MB_TYPE_HANDLE, sense_tag);
  CHECK_ERR(rval);
  rval =  mbi->get_entities_by_type_and_tag(0, MBENTITYSET, &sense_tag, NULL, 1, ent_sets);
  CHECK_ERR(rval);

}

void check_sense_tag()
{
  ErrorCode rval;
  Core moab; 
  Interface &  mbi->= moab;
  read_file( moab, test );

  Range ent_sets;
  rval =  mbi->tag_get_handle(GEOM_SENSE_2, 2, MB_TYPE_HANDLE, sense_tag);
  CHECK_ERR(rval);
  rval =  mbi->get_entities_by_type_and_tag(0, MBENTITYSET, &sense_tag, NULL, 1, ent_sets);
  CHECK_ERR(rval);


  CHECK_EQUAL(2, (int)ent_sets.size());
}
*/

