#include "moab/ParallelComm.hpp"
#include "moab/Core.hpp"
#include "moab_mpi.h"
#include "TestUtil.hpp"
#include "MBTagConventions.hpp"
#include <iostream>
#include <sstream>

#ifdef MESHDIR
 std::string filename = STRINGIFY(MESHDIR) "/io/p8ex1.h5m";
#endif

#define STRINGIFY_(X) #X
#define STRINGIFY(X) STRINGIFY_(X)

using namespace moab;
void report_sets(moab::Core * mb, int rank, int nproc)
{
  // check neumann and material sets, and see if their number of quads / hexes  in them
  const char* const shared_set_tag_names[] = {MATERIAL_SET_TAG_NAME,
                                              DIRICHLET_SET_TAG_NAME,
                                              NEUMANN_SET_TAG_NAME,
                                              PARALLEL_PARTITION_TAG_NAME};

  int num_tags = sizeof(shared_set_tag_names) / sizeof(shared_set_tag_names[0]);

  for (int p=0; p<nproc; p++)
  {
    if (rank==p)
    {
      std::cout<<" Task no:" << rank <<"\n";
      for (int i = 0; i < num_tags; i++) {
        Tag tag;
        ErrorCode rval = mb->tag_get_handle(shared_set_tag_names[i], 1, MB_TYPE_INTEGER,
                                        tag, MB_TAG_ANY); CHECK_ERR(rval);
        Range sets;
        rval = mb->get_entities_by_type_and_tag(0, MBENTITYSET, &tag, 0, 1, sets, Interface::UNION);
        CHECK_ERR(rval);

        std::vector<int> vals(sets.size());
        rval = mb->tag_get_data(tag, sets, &vals[0]); CHECK_ERR(rval);
        std::cout<<"  sets: " << shared_set_tag_names[i] << "\n";
        int j=0;
        for (Range::iterator it = sets.begin(); it!=sets.end(); it++, j++)
        {
          Range ents;
          rval = mb->get_entities_by_handle(*it, ents);CHECK_ERR(rval);
          std::cout << "    set " << mb->id_from_handle(*it) << " with tagval="<<vals[j]<<  " has " << ents.size() << " entities\n";
        }
      }
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
}
void test_read_with_ghost()
{
  int nproc, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  moab::Core *mb = new moab::Core();

  ErrorCode rval = MB_SUCCESS;

  char read_opts[]="PARALLEL=READ_PART;PARALLEL_RESOLVE_SHARED_ENTS;PARALLEL_GHOSTS=3.0.1.3;PARTITION=PARALLEL_PARTITION";
  rval = mb->load_file(filename.c_str() , 0, read_opts);CHECK_ERR(rval);

  report_sets(mb, rank, nproc);
  // write in serial the database , on each rank
  std::ostringstream outfile;
  outfile <<"testReadGhost_n" <<nproc<<"."<< rank<<".h5m";
  // the mesh contains ghosts too, but they were not part of mat/neumann set
  // write in serial the file, to see what tags are missing / or not
  rval = mb->write_file(outfile.str().c_str()); // everything on root
  CHECK_ERR(rval);
  delete mb;
}

void test_read_and_ghost_after()
{
  int nproc, rank;
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  moab::Core *mb = new moab::Core();
  moab::ParallelComm *pc = new moab::ParallelComm(mb, MPI_COMM_WORLD);
  ErrorCode rval = MB_SUCCESS;

  // first read in parallel, then ghost, then augment
  char read_opts[]="PARALLEL=READ_PART;PARALLEL_RESOLVE_SHARED_ENTS;PARTITION=PARALLEL_PARTITION";
  rval = mb->load_file(filename.c_str(), 0, read_opts);CHECK_ERR(rval);

  int ghost_dim=3, bridge=0, layers=1, addl_ents=3;
  rval = pc->exchange_ghost_cells( ghost_dim, bridge, layers, addl_ents, true, true);
  CHECK_ERR(rval);

  //
  pc->set_debug_verbosity(1);
  rval = pc->augment_default_sets_with_ghosts(0);
  CHECK_ERR(rval);

  report_sets(mb, rank, nproc);

  // write in serial the database , on each rank
  std::ostringstream outfile;
  outfile <<"TaskMesh_n" <<nproc<<"."<< rank<<".h5m";
  // the mesh contains ghosts too, but they were not part of mat/neumann set
  // write in serial the file, to see what tags are missing / or not
  rval = mb->write_file(outfile.str().c_str()); // everything on root
  CHECK_ERR(rval);

  delete pc;
  delete mb;
}
int main(int argc, char* argv[])
{
  MPI_Init(&argc, &argv);

  int result = 0;
  if (argc>=2)
  	filename = argv[1];

  result += RUN_TEST(test_read_with_ghost);
  result += RUN_TEST(test_read_and_ghost_after);

  MPI_Finalize();
  return 0;
}
