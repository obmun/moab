#include "moab/ParallelMergeMesh.hpp"
#include "moab/Core.hpp"
#include "moab/CartVect.hpp"
#include "moab/Range.hpp"
#include "moab/AdaptiveKDTree.hpp"
#include "moab/Skinner.hpp"
#include "moab/MergeMesh.hpp"
#include "moab/ParallelComm.hpp"
#include "moab/CN.hpp"

extern "C" 
{
  //#include "errmem.h"
  //#include "types.h"
  //#include "sort.h"
  //#include "tuple_list.h"
#include "crystal.h"
#include "transfer.h"
}

#include <algorithm>
#ifdef USE_MPI
#  include "moab_mpi.h"
#endif

namespace moab{

  //Constructor
  /*1) Get Merge Data and tolerance*/
  ParallelMergeMesh::ParallelMergeMesh(ParallelComm *pc, 
				       const double epsilon) :
    myPcomm(pc), myEps(epsilon)
  {
  }

  //Perform Merge
  ErrorCode ParallelMergeMesh::merge() 
  {
    Interface *mb = myPcomm->get_moab();
    int dim;
    ErrorCode rval = mb->get_dimension(dim);
    if(rval != MB_SUCCESS){
      return rval;
    }

    /*2)Merge Mesh Locally*/
    //Get all dim dimensional entities
    Range ents;
    rval = mb->get_entities_by_dimension(0,dim,ents);
    if(rval != MB_SUCCESS){
      return rval;
    }

    //Merge Mesh Locally
    MergeMesh merger(mb);
    merger.merge_entities(ents,myEps);
    //We can return if there is only 1 proc
    if(rval != MB_SUCCESS || myPcomm->size() == 1){
      return rval;
    }

    //Rebuild the ents range
    ents.clear();
    rval = mb->get_entities_by_dimension(0,dim,ents);
    if(rval != MB_SUCCESS){
      return rval;
    }

    /*3)Get Skin
      -Get Range of 0 dimensional entities*/
    //Look into improving by using higher skin
    std::vector<Range> skin_ents(4);
    Skinner skinner(mb);
    for(int skin_dim = dim; skin_dim >= 0; skin_dim--){
      rval = skinner.find_skin(ents,skin_dim,skin_ents[skin_dim]);
      if(rval != MB_SUCCESS){
	return rval;
      }
      /*ents = skin_ents[skin_dim];
      ents.clear();
      rval = mb->get_entities_by_dimension(0,dim,ents);
      if(rval != MB_SUCCESS){
	return rval;
	}*/
    }
 
    /*4)Get Bounding Box*/
    AdaptiveKDTree kd(mb);
    double box[6], gbox[6];
    rval = kd.bounding_box(skin_ents[0],box, box+3);
    if(rval != MB_SUCCESS){
      return rval;
    }

    //Invert the max
    for(int i=3; i<6;i++){
      box[i] *= -1;
    }

    /* 5)Communicate to all processors*/
    MPI_Allreduce(box, gbox, 6, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);

    /* 6)Assemble Global Bounding Box*/
    //Flip the max back
    for(int i=3; i<6; i++){
      gbox[i] *= -1;
    }

    /* 7)Partition Global Bounding Box 
       -Simple 1 Dimensional Partition
       -An element (x,y,z) will be sent to floor((x-min(x))/length) */
    double length = gbox[3]-gbox[0];
    length /= static_cast<double>(myPcomm->size());

    /* 8)Get Skin Coordinates, Vertices */
    double *x = new double[skin_ents[0].size()]; 
    double *y = new double[skin_ents[0].size()]; 
    double *z = new double[skin_ents[0].size()];
    rval = mb->get_coords(skin_ents[0],x,y,z);
    if(rval != MB_SUCCESS){
      //Prevent Memory Leak
      delete []x; delete []y; delete []z;
      return rval;
    }

    /* 9)Assemble Tuples */
    double eps2 = myEps*myEps;
    tuple_list tup;
    tuple_list_init_max(&tup,1,0,1,3,skin_ents[0].size());
    int maxProc = myPcomm->size()-1;
    unsigned long j=0, tup_i=0,tup_ul=0,tup_r=0;
    for(Range::iterator it = skin_ents[0].begin();it != skin_ents[0].end();it++){
      //Calculate the toprocessor
      int toproc1 = static_cast<int>(floor((x[j]-gbox[0])/length));    
      int toproc2 = static_cast<int>(floor((x[j]+eps2-gbox[0])/length));
      
      //Make sure no entities go to an invalid processor
      toproc1 = toproc1<=maxProc?toproc1:maxProc;
      toproc2 = toproc2<=maxProc?toproc2:maxProc;

      //Make sure we have room for at least 2 more tuples
      while(tup.n+1 >= tup.max){
	tuple_list_grow(&tup);
      }

      //Assemble a tuple
      //Tuple is of the form (toProc, handle, x,y, z)
      tup.vi[tup_i++]=toproc1; //toProc
      tup.vul[tup_ul++]=*it; //Handle
      tup.vr[tup_r++]=x[j]; //x
      tup.vr[tup_r++]=y[j]; //y
      tup.vr[tup_r++]=z[j]; //z
      tup.n++;
      //Assemble a second tuple if necessary
      if(toproc1 != toproc2){
	tup.vi[tup_i++]=toproc2;// 2nd toProc
	tup.vul[tup_ul++]=*it;// handle
	tup.vr[tup_r++]=x[j];// x 
	tup.vr[tup_r++]=y[j];// y
	tup.vr[tup_r++]=z[j];// z
	tup.n++;
      }
      j++;
    }

    //Delete the coord arrays
    delete []x; delete []y; delete []z;
    /* 10)Gather-Scatter Tuple
       -tup comes out as (remProc,handle,x,y,z) */
    crystal_data cd; 
    moab_crystal_init(&cd,myPcomm->comm());
    //1 represents dynamic tuple, 0 represents index of the processor to send to
    moab_gs_transfer(1, &tup, 0, &cd);
    /* 11)Sort By X,Y,Z
       -Currently Utilizing a custom sort*/
    buffer buf;
    unsigned long long max_size = 
      skin_ents[0].size()*(MAX_SHARING_PROCS+1)*sizeof(double);
    buffer_init(&buf, max_size);
    //Sort by x,y,z
    temp_tuple_sort_real(&tup,eps2);
    //Print out the tuples to check for order
    if(myPcomm->rank()==0 && false){
      int g = 0;
      for(unsigned a=0;a<tup.n-1;a++){
	std::cout<<tup.vr[g]<<" : "<<tup.vr[g+1]<<" : "<<tup.vr[g+2]<<std::endl;
	g += tup.mr;
      }
    }

    /* 12)Match: new tuple list*/
    tuple_list matches;
    tuple_list_init_max(&matches,2,0,2,0,skin_ents[0].size());
    //Counters for accessing tuples more efficiently
    unsigned long i = 0, mat_i=0, mat_ul=0;
    j=0; tup_r=0;
    while((i+1)<tup.n){
      //Proximity Comparison
      double xi = tup.vr[tup_r], 
	yi = tup.vr[tup_r+1],
	zi = tup.vr[tup_r+2];

      bool done = false;
      while(!done && (j+1)<tup.n){
	j++; tup_r+= tup.mr;
	CartVect cv(tup.vr[tup_r]-xi,
		    tup.vr[tup_r+1]-yi,
		    tup.vr[tup_r+2]-zi);
	if(cv.length_squared() > eps2){
	  done = true;
	}
      }
      //Allocate the tuple list before adding matches
      while(matches.n+(j-i)*(j-i-1) >= matches.max){
	tuple_list_grow(&matches);
      }
 
      //We now know that tuples [i to j) exclusice match.  
      //If n tuples match, n*(n-1) match tuples will be made
      //tuples are of the form (proc1,proc2,handle1,handle2)
      int kproc = i*tup.mi;
      unsigned khand = i*tup.mul;
      for(unsigned k = i; k<j; k++){
	int lproc = kproc+tup.mi;
	unsigned lhand = khand+tup.mul;
	for(unsigned l=k+1; l<j; l++){
	  matches.vi[mat_i++]=tup.vi[kproc];//proc1
	  matches.vi[mat_i++]=tup.vi[lproc];//proc2
	  matches.vul[mat_ul++]=tup.vul[khand];//handle1
	  matches.vul[mat_ul++]=tup.vul[lhand];//handle2
	  matches.n++;

	  matches.vi[mat_i++]=tup.vi[lproc];//proc1
	  matches.vi[mat_i++]=tup.vi[kproc];//proc2
	  matches.vul[mat_ul++]=tup.vul[lhand];//handle1
	  matches.vul[mat_ul++]=tup.vul[khand];//handle2
	  matches.n++;
	  lproc += tup.mi;
	  lhand += tup.mul;
	}
	kproc += tup.mi;
	khand += tup.mul;
      }//End for(int k...
      i = j;
    }//End while(i<tup.n)
    //Cleanup
    tuple_list_free(&tup);

    /* 13)Gather-Scatter Again */
    //1 represents dynamic list, 0 represents proc index to send tuple to
    moab_gs_transfer(1,&matches,0,&cd);
    moab_crystal_free(&cd);

    //Sorts are necessary to check for doubles
    //Sort by remote handle
    moab_tuple_list_sort(&matches,3,&buf);
    //Sort by matching proc
    moab_tuple_list_sort(&matches,1,&buf);
    //Sort by local handle
    moab_tuple_list_sort(&matches,2,&buf);
    
    //Manipulate the matches list to tag vertices and entities
    //Set up proc ents
    Range proc_ents;
    // get the entities in the partition sets
    for (Range::iterator rit = myPcomm->partitionSets.begin(); rit != myPcomm->partitionSets.end(); rit++) {
      Range tmp_ents;
      rval = mb->get_entities_by_handle(*rit, tmp_ents, true);
      if (MB_SUCCESS != rval){
	tuple_list_free(&matches);
	return rval;
      }
      proc_ents.merge(tmp_ents);
    }
    if (mb->dimension_from_handle(*proc_ents.rbegin()) !=
	mb->dimension_from_handle(*proc_ents.begin())) {
      Range::iterator lower = proc_ents.lower_bound(CN::TypeDimensionMap[0].first),
	upper = proc_ents.upper_bound(CN::TypeDimensionMap[dim-1].second);
      proc_ents.erase(lower, upper);
    }

    //Build up a shared verts tup to utilize tag_shared_verts
    tuple_list shared_verts;
    tuple_list_init_max(&shared_verts,2,0,1,0,matches.n);
    tup_i=0; tup_ul=0; i=0; mat_ul = 0; mat_i = 0;
    unsigned n = 0;
    while(n<matches.n){
      unsigned localHand = matches.vul[mat_ul], remHand = matches.vul[mat_ul+1];
      int remProc = matches.vi[mat_i+1];
      assert(remProc != (int) myPcomm->rank());
      //Add to shared verts list
      shared_verts.vi[tup_i++] = skin_ents[0].index(localHand);
      shared_verts.vi[tup_i++] = remProc;
      shared_verts.vul[tup_ul++] = remHand;
      n++; 
      mat_ul+=matches.mul; 
      mat_i += matches.mi;
      shared_verts.n++;
      //Move past duplicates
      while(n < matches.n && 
	    localHand == matches.vul[mat_ul] &&
	    remHand == matches.vul[mat_ul+1] &&
	    remProc == matches.vi[mat_i+1]){
	n++; 
	mat_ul+=matches.mul; 
	mat_i += matches.mi;
      }
    }
    tuple_list_free(&matches);

    //Sort the shared_verts tuples
    moab_tuple_list_sort(&shared_verts,0,&buf);
    buffer_free(&buf);

    //This vector doesnt appear to be used but its in resolve_shared
    int maxp = -1;
    std::vector<int> sharing_procs(MAX_SHARING_PROCS);
    std::fill(sharing_procs.begin(), sharing_procs.end(), maxp);
    j = 0; i = 0; 

    // get ents shared by 1 or n procs
    std::map<std::vector<int>, std::vector<EntityHandle> > proc_nranges;
    Range proc_verts;
    rval = mb->get_adjacencies(proc_ents, 0, false, proc_verts,
				   Interface::UNION);
    if(rval != MB_SUCCESS){
      tuple_list_free(&shared_verts);
      return rval;
    }
    rval = myPcomm->tag_shared_verts(shared_verts,&skin_ents[0],
				     proc_nranges, proc_verts);
    tuple_list_free(&shared_verts);
    if(rval != MB_SUCCESS){
      return rval;
    }
    
    // get entities shared by 1 or n procs
    rval = myPcomm->tag_shared_ents(dim,dim-1, &skin_ents[0],proc_nranges);
    if(rval != MB_SUCCESS){
      return rval;
    }
    
    // create the sets for each interface; store them as tags on
    // the interface instance
    Range iface_sets;
    rval = myPcomm->create_interface_sets(proc_nranges, dim, dim-1);
    if(rval != MB_SUCCESS){
      return rval;
    }
    // establish comm procs and buffers for them
    std::set<unsigned int> procs;
    rval = myPcomm->get_interface_procs(procs, true);
    if(rval != MB_SUCCESS){
      return rval;
    }

    // resolve shared entity remote handles; implemented in ghost cell exchange
    // code because it's so similar
    rval = myPcomm->exchange_ghost_cells(-1, -1, 0, true, true);
    if(rval != MB_SUCCESS){
      return rval;
    }
    // now build parent/child links for interface sets
    rval = myPcomm->create_iface_pc_links();
    return rval;
  }

  //Swap around tuples
  void ParallelMergeMesh::temp_tuple_swap_real(tuple_list *tup, 
					       unsigned a, unsigned b)
  {
    if(a==b) return;
    //Swap mi
    int a_val = a*tup->mi, b_val=b*tup->mi;
    for(unsigned int i=0; i< tup->mi;i++){
      sint t =tup->vi[a_val];
      tup->vi[a_val] = tup->vi[b_val];
      tup->vi[b_val] = t; 
      a_val++;
      b_val++;
    }
    //Swap ml
    a_val = a*tup->ml;
    b_val = b*tup->ml;
    for(unsigned int i=0; i< tup->ml;i++){
      slong t =tup->vl[a_val];
      tup->vl[a_val] = tup->vl[b_val];
      tup->vl[b_val] = t;
      a_val++;
      b_val++;
    }
    //Swap mul
    a_val = a*tup->mul;
    b_val = b*tup->mul;
    for(unsigned int i=0; i< tup->mul;i++){
      ulong t =tup->vul[a_val];
      tup->vul[a_val] = tup->vul[b_val];
      tup->vul[b_val] = t; 
      a_val++;
      b_val++;
    }
    //Swap mr
    a_val = a*tup->mr;
    b_val = b*tup->mr;
    for(unsigned int i=0; i< tup->mr;i++){
      real t =tup->vr[a_val];
      tup->vr[a_val] = tup->vr[b_val];
      tup->vr[b_val] = t; 
      a_val++;
      b_val++;
    }
  }

  //Simple selection sort to test real
  void ParallelMergeMesh::temp_tuple_sort_real(tuple_list *tup,
					       double eps2)
  {
    //Call the recursive function
    temp_perform_sort_real(tup, 0, tup->n,eps2);
  }

  //Perform the sorting of a tuple by real
  //To sort an entire tuple_list, call (tup,0,tup.n.epsilon) 
  void ParallelMergeMesh::temp_perform_sort_real(tuple_list *tup, 
						 int left, 
						 int right,
						 double eps2)
  {  
    //If list size is only 1 return
    if(left+1 >= right){
      return;
    }
    int swap = left, tup_l = left*tup->mr, tup_t = tup_l + tup->mr;

    //Swap the median with the left position for a (hopefully) better split
    temp_tuple_swap_real(tup,left,(left+right)/2);

    //Partition the data
    for(int t=left+1;t<right;t++){
      //If the left value(pivot) is greater than t_val, swap it into swap
      if(greaterThan(tup,tup_l,tup_t,eps2)){
	swap++;
	temp_tuple_swap_real(tup,swap,t);
      }
      tup_t+=tup->mr;
    }
    //Swap so that position swap is in the correct position
    temp_tuple_swap_real(tup,left,swap);

    //Sort left and right of swap
    temp_perform_sort_real(tup,left,swap,eps2);
    temp_perform_sort_real(tup,swap+1,right,eps2);
  }

  //Note, this takes the actual tup->vr[] index (aka i*tup->mr)
  bool ParallelMergeMesh::greaterThan(tuple_list *tup, int vrI,int vrJ, double eps2){
    unsigned check=0;
    while(check < tup->mr){
      //If the values are the same
      if(fabs(tup->vr[vrI+check]-tup->vr[vrJ+check]) <= eps2){
	check++;
	continue;
      }
      //If the I greater than J 
      else if(tup->vr[vrI+check] > tup->vr[vrJ+check]){
	return true;
      }
      //If J greater than I
      else{
	return false;
      }
    }
    //Values are the same
    return false;
  }
}//End namespace moab

