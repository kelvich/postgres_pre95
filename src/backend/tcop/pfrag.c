/* ----------------------------------------------------------------
 *   FILE
 *	pfrag.c
 *	
 *   DESCRIPTION
 *	POSTGRES process query command code
 *
 *   INTERFACE ROUTINES
 *	ProcessQuery
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tcop/tcop.h"
#include "tcop/tcopdebug.h"
#include "tcop/slaves.h"
#include "nodes/pg_lisp.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.a.h"
#include "nodes/plannodes.a.h"
#include "executor/execmisc.h"
#include "executor/x_execinit.h"
#include "executor/x_hash.h"
#include "tcop/dest.h"
#include "parser/parsetree.h"

 RcsId("$Header$");

extern ScanTemps RMakeScanTemps();
extern Fragment RMakeFragment();
extern Relation CopyRelDescUsing();

List
FindFragments(parsetree, node, fragmentNo)
List parsetree;
Plan node;
int fragmentNo;
{
    List subFragments = LispNil;
    List newFragments;
    Fragment fragment;

    set_fragment(node, fragmentNo);
    if (get_lefttree(node) != NULL) 
       if (ExecIsHash(get_lefttree(node)) || ExecIsSort(get_lefttree(node))) {
          fragment = RMakeFragment();
          set_frag_root(fragment, get_lefttree(node));
          set_frag_parent_op(fragment, node);
          set_frag_parallel(fragment, 1);
          set_frag_subtrees(fragment, LispNil);
	  set_frag_parsetree(fragment, parsetree);
          subFragments = nappend1(subFragments, fragment);
        }
       else {
          subFragments = FindFragments(get_lefttree(node), fragmentNo);
        }
    if (get_righttree(node) != NULL)
       if (ExecIsHash(get_righttree(node)) || ExecIsSort(get_righttree(node))) {
          fragment = RMakeFragment();
          set_frag_root(fragment, get_righttree(node));
          set_frag_parent_op(fragment, node);
          set_frag_parallel(fragment, 1);
          set_frag_subtrees(fragment, LispNil);
	  set_frag_parsetree(fragment, parsetree);
          subFragments = nappend1(subFragments, fragment);
         }
       else {
         newFragments = FindFragments(get_righttree(node), fragmentNo);
         subFragments = nconc(subFragments, newFragments);
        }
    
    return subFragments;
}

Fragment
InitialPlanFragments(parsetree, plan)
Plan plan;
{
    Plan node;
    LispValue x, y;
    List fragmentlist;
    List subFragments;
    List newFragmentList;
    int fragmentNo = 0;
    Fragment rootFragment;
    Fragment fragment, frag;

    rootFragment = RMakeFragment();
    set_frag_root(rootFragment, plan);
    set_frag_parent_op(rootFragment, NULL);
    set_frag_parallel(rootFragment, 1);
    set_frag_subtrees(rootFragment, LispNil);
    set_frag_parent_frag(rootFragment, NULL);
    set_frag_parsetree(rootFragment, parsetree);

    fragmentlist = lispCons(rootFragment, LispNil);

    while (!lispNullp(fragmentlist)) {
	newFragmentList = LispNil;
	foreach (x, fragmentlist) {
	   fragment = (Fragment)CAR(x);
	   node = get_frag_root(fragment);
	   subFragments = FindFragments(parsetree, node, fragmentNo++);
	   set_frag_subtrees(fragment, subFragments);
	   foreach (y, subFragments) {
	      frag = (Fragment)CAR(y);
	      set_frag_parent_frag(frag, fragment);
	     }
	   newFragmentList = nconc(newFragmentList, subFragments);
	  }
	fragmentlist = newFragmentList;
      }
    return rootFragment;
}

extern int NBuffers;

int
GetCurrentMemSize()
{
   return NBuffers;  /* YYY functionalities to be added later */
}

float
GetCurrentLoadAverage()
{
    return 0.0;  /* YYY functionalities to be added later */
}

List
GetReadyFragments(fragments)
Fragment fragments;
{
    LispValue x;
    Fragment frag;
    List readyFragments = LispNil;
    List readyFrags;

    if (lispNullp(get_frag_subtrees(fragments)))
       return lispCons(fragments, LispNil);
    foreach (x, get_frag_subtrees(fragments)) {
	frag = (Fragment)CAR(x);
	readyFrags = GetReadyFragments(frag);
	readyFragments = nconc(readyFragments, readyFrags);
      }
    return readyFragments;
}

List
GetFitFragments(fragmentlist, memsize)
List fragmentlist;
int memsize;
{
    return fragmentlist; /* YYY functionalities to be added later */
}

List
DecomposeFragments(fragmentlist, memsize)
List fragmentlist;
int memsize;
{
    return fragmentlist;  /* YYY functionalities to be added later */
}

List
ChooseToFire(fragmentlist, memsize)
List fragmentlist;
int memsize;
{
    return lispCons(CAR(fragmentlist), LispNil);   
    /* YYY functionalities to be added later */
}

List
ChooseFragments(fragments, memsize)
Fragment fragments;
int memsize;
{
    List readyFragments;
    List fitFragments;
    List fireFragments;

    readyFragments = GetReadyFragments(fragments);
    fitFragments = GetFitFragments(readyFragments, memsize);
    if (lispNullp(fitFragments)) {
	fitFragments = DecomposeFragments(fitFragments, memsize);
	if (lispNullp(fitFragments))
	   elog(WARN, "memory below hashjoin threshold.");
      }
    fireFragments = ChooseToFire(fitFragments, memsize);
    return fireFragments;
}

void
set_plan_parallel(plan, nparallel)
Plan plan;
int nparallel;
{
    if (plan == NULL)
       return;
    if (ExecIsNestLoop(plan))  {
       set_parallel(plan, nparallel);
       set_plan_parallel(get_outerPlan(plan), nparallel);
       set_plan_parallel(get_innerPlan(plan), 1);
      }
    else  {
       set_parallel(plan, nparallel);
       set_plan_parallel(get_lefttree(plan), nparallel);
       set_plan_parallel(get_righttree(plan), nparallel);
      }
}

void
SetParallelDegree(fragmentlist, nfreeslaves)
List fragmentlist;
int nfreeslaves;
{
    LispValue x;
    Fragment fragment;
    Plan plan;
    int fragmentno;

    /* YYY more functionalities to be added later */
    foreach (x,fragmentlist) {
       fragment = (Fragment)CAR(x);
       plan = get_frag_root(fragment);
       fragmentno = get_fragment(plan);
       if (fragmentno < 0) {
           set_frag_parallel(fragment, 1);  /* YYY */
           set_plan_parallel(plan, 1);
	  }
       else {
           set_frag_parallel(fragment, nfreeslaves);  /* YYY */
           set_plan_parallel(plan, nfreeslaves);
	 }
     }
}


/* ----------------------------------------------------------------
 *	ParallelOptimize
 *	
 *	this analyzes the plan in the query descriptor and determines
 *	which fragments to execute based on available virtual
 *	memory resources...
 *	
 * ----------------------------------------------------------------
 */
List
ParallelOptimize(fragmentlist)
List fragmentlist;
{
    LispValue x;
    Fragment fragment;
    int memAvail;
    float loadAvg;
    List fireFragments;
    List fireFragmentList;

    fireFragmentList = LispNil;
    foreach (x, fragmentlist) {
	fragment = (Fragment)CAR(x);
        memAvail = GetCurrentMemSize();
        loadAvg = GetCurrentLoadAverage();
        fireFragments = ChooseFragments(fragment, memAvail);
        SetParallelDegree(fireFragments, NumberOfFreeSlaves);
	fireFragmentList = nconc(fireFragmentList, fireFragments);
      }
    return fireFragmentList;
}

#define MINHASHTABLEMEMORYKEY	1000
static IpcMemoryKey nextHashTableMemoryKey = 0;

IpcMemoryKey
getNextHashTableMemoryKey()
{
    extern int MasterPid;
    return (nextHashTableMemoryKey++ + MINHASHTABLEMEMORYKEY + MasterPid);
}

/* ----------------------------------------------------------------
 *	OptimizeAndExecuteFragments
 *
 *	Optimize plan fragments to explore both intra-fragment
 *	and inter-fragment parallelism and execute the "optimal"
 *	parallel plan
 *
 * ----------------------------------------------------------------
 */
void
OptimizeAndExecuteFragments(fragmentlist, destination)
List 		fragmentlist;
CommandDest	destination;
{
    LispValue		x;
    int			i;
    List		currentFragmentList;
    Fragment		fragment;
    int			nparallel;
    List		finalResultRelation;
    Plan		plan;
    List		parsetree;
    Plan		parentPlan;
    Fragment		parentFragment;
    int			groupid;
    ProcessNode		*p;
    List		subtrees;
    List		fragQueryDesc;
    HashJoinTable	hashtable;
    int			hashTableMemorySize;
    IpcMemoryKey	hashTableMemoryKey;
    IpcMemoryKey	getNextHashTableMemoryKey();
    ScanTemps		scantempNode;
    Relation		tempRelationDesc;
    List		tempRelationDescList;
    Relation		shmTempRelationDesc;
    List		fraglist;
    CommandDest		dest;
    
    fraglist = fragmentlist;
    while (!lispNullp(fraglist)) {
	/* ------------
	 * choose the set of fragments to execute and parallelism
	 * for each fragment.
	 * ------------
	 */
        currentFragmentList = ParallelOptimize(fraglist);
	foreach (x, currentFragmentList) {
	   fragment = (Fragment)CAR(x);
	   nparallel = get_frag_parallel(fragment);
	   plan = get_frag_root(fragment);
	   parsetree = get_frag_parsetree(fragment);
	   parentFragment = get_frag_parent_frag(fragment);
	   finalResultRelation = parse_tree_result_relation(parsetree);
	   dest = destination;
	   if (ExecIsHash(plan))  {
	      /* ------------
	       *  if it is hashjoin, create the hash table
	       *  so that the slaves can share it
	       * ------------
	       */
	      hashTableMemoryKey = getNextHashTableMemoryKey();
	      set_hashtablekey(plan, hashTableMemoryKey);
	      hashtable = ExecHashTableCreate(plan);
	      set_hashtable(plan, hashtable);
	      hashTableMemorySize = get_hashtablesize(plan);
	      parse_tree_result_relation(parsetree) = LispNil;
	      }
	   else if (parentFragment != NULL || nparallel > 1) {
	      /* ------------
	       *  this means that this an intermediate fragment, so
	       *  the result should be kept in some temporary relation
	       * ------------
	       */
	      parse_tree_result_relation(parsetree) =
		  lispCons(lispAtom("intotemp"), LispNil);
	      dest = None;
	     }
	   /* ---------------
	    * create query descriptor for the fragment
	    * ---------------
	    */
	   fragQueryDesc = CreateQueryDesc(parsetree, plan, dest);

	   /* ---------------
	    * assign a process group to work on the fragment
	    * ---------------
	    */
	   groupid = getFreeProcGroup(nparallel);
	   ProcGroupLocalInfoP[groupid].fragment = fragment;
	   ProcGroupInfoP[groupid].status = WORKING;
	   ProcGroupInfoP[groupid].queryDesc = (List)
			CopyObjectUsing(fragQueryDesc, ExecSMAlloc);
	   ProcGroupInfoP[groupid].countdown = nparallel;
	   wakeupProcGroup(groupid);
	   /* ---------------
	    * restore the original result relation descriptor
	    * ---------------
	    */
	   parse_tree_result_relation(parsetree) = finalResultRelation;
	 }

       /* ----------------
	* wait for some process group to complete execution
	* ----------------
	*/
       P_Finished();

       /* --------------
	* some process group has finished processing a fragment,
	* find that group
	* --------------
	*/
       groupid = getFinishedProcGroup();
       fragment = ProcGroupLocalInfoP[groupid].fragment;
       nparallel = get_frag_parallel(fragment);
       plan = get_frag_root(fragment);
       parentPlan = get_frag_parent_op(fragment);
       parentFragment = get_frag_parent_frag(fragment);
       /* ---------------
	* delete the finished fragment from the subtree list of its
	* parent fragment
	* ---------------
	*/
       if (parentFragment == NULL)
	  subtrees = LispNil;
       else {
	  subtrees = get_frag_subtrees(parentFragment);
	  set_frag_subtrees(parentFragment, nLispRemove(subtrees, fragment));
	 }
       /* ----------------
	* let the parent fragment know where the result is
	* ----------------
	*/
       if (ExecIsHash(plan)) {
	   /* ----------------
	    *  if it is hashjoin, let the parent know where the hash table is
	    * ----------------
	    */
	   set_hashjointable(parentPlan, hashtable);
	   set_hashjointablekey(parentPlan, hashTableMemoryKey);
	   set_hashjointablesize(parentPlan, hashTableMemorySize);
	   set_hashdone(parentPlan, true);
	  }
       else {
	   List unionplans = LispNil;
	   /* ------------------
	    * if it is ScanTemps node, clean up the temporary relations
	    * they are not needed any more
	    * ------------------
	    */
	   if (ExecIsScanTemps(plan)) {
	       Relation tempreldesc;
	       List	tempRelDescs;
	       LispValue y;

	       tempRelDescs = get_temprelDescs(plan);
	       foreach (y, tempRelDescs) {
		   tempreldesc = (Relation)CAR(y);
		   ReleaseTmpRelBuffers(tempreldesc);
		   if (FileNameUnlink(
			  relpath(&(tempreldesc->rd_rel->relname))) < 0)
		       elog(WARN, "ExecEndScanTemp: unlink: %m");
		}
	     }
	   if (parentPlan == NULL && nparallel == 1)
	      /* in this case the whole plan has been finished */
	      fraglist = nLispRemove(fraglist, fragment);
	   else {
	      /* -----------------
	       *  make a ScanTemps node to let the parent collect the tuples
	       *  from a set of temporary relations
	       * -----------------
	       */
	      tempRelationDescList = LispNil;
	      p = ProcGroupLocalInfoP[groupid].memberProc;
	      for (p = ProcGroupLocalInfoP[groupid].memberProc;
		   p != NULL;
		   p = p->next) {
		 shmTempRelationDesc = SlaveInfoP[p->pid].resultTmpRelDesc;
#ifndef PALLOC_DEBUG		     
		 tempRelationDesc = CopyRelDescUsing(shmTempRelationDesc,
						     palloc);
#else
		 tempRelationDesc = CopyRelDescUsing(shmTempRelationDesc,
						     palloc_debug);
#endif PALLOC_DEBUG		     
		 tempRelationDescList = nappend1(tempRelationDescList, 
						 tempRelationDesc);
		 }
	      scantempNode = RMakeScanTemps();
	      set_qptargetlist(scantempNode, get_qptargetlist(plan));
	      set_temprelDescs(scantempNode, tempRelationDescList);
	      if (parentPlan == NULL) {
		 set_frag_root(fragment, scantempNode);
		 set_frag_subtrees(fragment, LispNil);
		 set_fragment(scantempNode,-1);/*means end of parallelism */
		}
	      else {
	      if (plan == (Plan)get_lefttree(parentPlan)) {
		 set_lefttree(parentPlan, scantempNode);
		}
	      else {
		 set_righttree(parentPlan, scantempNode);
		}
	      set_fragment(scantempNode, get_fragment(parentPlan));
	      }
	   }
         }
       /* -----------------
	*  free the finished processed group
	* -----------------
	*/
       freeProcGroup(groupid);
     }
	    
	/* ----------------
	 *	Clean Shared Memory used during the query
	 * ----------------
	 */
	ExecSMClean();
}
