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
#include "storage/lmgr.h"
#include "catalog/pg_relation.h"

 RcsId("$Header$");

extern ScanTemps RMakeScanTemps();
extern Fragment RMakeFragment();
extern Relation CopyRelDescUsing();

/* ------------------------------------
 *	FingFragments
 *
 *	find all the plan fragments under plan node, mark the fragments starting
 *	with fragmentNo
 *	plan fragments are obtained by decomposing the plan tree across all
 *	blocking edges, i.e., edges out of Hash nodes and Sort nodes
 * ------------------------------------
 */
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
	  /* -----------------------------
	   * detected a blocking edge, fragment boundary
	   * -----------------------------
	   */
          fragment = RMakeFragment();
          set_frag_root(fragment, get_lefttree(node));
          set_frag_parent_op(fragment, node);
          set_frag_parallel(fragment, 1);
          set_frag_subtrees(fragment, LispNil);
	  set_frag_parsetree(fragment, parsetree);
	  set_frag_is_inprocess(fragment, false);
          subFragments = nappend1(subFragments, fragment);
        }
       else {
          subFragments = FindFragments(parsetree,get_lefttree(node),fragmentNo);
        }
    if (get_righttree(node) != NULL)
       if (ExecIsHash(get_righttree(node)) || ExecIsSort(get_righttree(node))) {
	  /* -----------------------------
	   * detected a blocking edge, fragment boundary
	   * -----------------------------
	   */
          fragment = RMakeFragment();
          set_frag_root(fragment, get_righttree(node));
          set_frag_parent_op(fragment, node);
          set_frag_parallel(fragment, 1);
          set_frag_subtrees(fragment, LispNil);
	  set_frag_parsetree(fragment, parsetree);
	  set_frag_is_inprocess(fragment, false);
          subFragments = nappend1(subFragments, fragment);
         }
       else {
         newFragments = FindFragments(parsetree,get_righttree(node),fragmentNo);
         subFragments = nconc(subFragments, newFragments);
        }
    
    return subFragments;
}

/* --------------------------------
 *	InitialPlanFragments
 *
 *	calls FindFragments() recursively to obtain the initial set of
 *	plan fragments -- the largest possible, further decomposition
 *	might be necessary in DecomposeFragments().
 * --------------------------------
 */
Fragment
InitialPlanFragments(parsetree, plan)
Plan plan;
List parsetree;
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
    set_frag_is_inprocess(rootFragment, false);

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

/* -----------------------------
 *	GetCurrentMemSize
 *
 *	get the current amount of available memory
 * -----------------------------
 */
int
GetCurrentMemSize()
{
   return NBuffers;  /* YYY functionalities to be added later */
}

/* -----------------------------
 *	GetCurrentLoadAverage
 *
 *	get the current load average of the system
 * -----------------------------
 */
float
GetCurrentLoadAverage()
{
    return 0.0;  /* YYY functionalities to be added later */
}

/* ------------------------------
 *	GetReadyFragments
 *
 *	get the set of fragments that are ready to fire, i.e., those that
 *	have no children
 * ------------------------------
 */
List
GetReadyFragments(fragments)
Fragment fragments;
{
    LispValue x;
    Fragment frag;
    List readyFragments = LispNil;
    List readyFrags;

    if (lispNullp(get_frag_subtrees(fragments)) && 
	!get_frag_is_inprocess(fragments))
       return lispCons(fragments, LispNil);
    foreach (x, get_frag_subtrees(fragments)) {
	frag = (Fragment)CAR(x);
	readyFrags = GetReadyFragments(frag);
	readyFragments = nconc(readyFragments, readyFrags);
      }
    return readyFragments;
}

/* ---------------------------------
 *	GetFitFragments
 *
 *	get the set of fragments that can fit in the current available memory
 * ---------------------------------
 */
List
GetFitFragments(fragmentlist, memsize)
List fragmentlist;
int memsize;
{
    return fragmentlist; /* YYY functionalities to be added later */
}

/* ----------------------------------
 *	DecomposeFragments
 *
 *	decompose fragments into smaller fragments to fit in memsize amount
 *	of memory
 * ----------------------------------
 */
List
DecomposeFragments(fragmentlist, memsize)
List fragmentlist;
int memsize;
{
    return fragmentlist;  /* YYY functionalities to be added later */
}

/* -----------------------------------
 *	ChooseToFire
 *
 *	choose among all the ready-to-fire fragments which
 *	to execute in parallel
 * -----------------------------------
 */
List
ChooseToFire(fragmentlist, memsize)
List fragmentlist;
int memsize;
{
    return lispCons(CAR(fragmentlist), LispNil);   
    /* YYY functionalities to be added later */
}

/* ----------------------------------
 *	ChooseFragments
 *
 *	choose the fragments to execute in parallel
 * -----------------------------------
 */
List
ChooseFragments(fragments, memsize)
Fragment fragments;
int memsize;
{
    List readyFragments;
    List fitFragments;
    List fireFragments;

    readyFragments = GetReadyFragments(fragments);
    if (lispNullp(readyFragments)) return LispNil;
    fitFragments = GetFitFragments(readyFragments, memsize);
    if (lispNullp(fitFragments)) {
	fitFragments = DecomposeFragments(fitFragments, memsize);
	if (lispNullp(fitFragments))
	   elog(WARN, "memory below hashjoin threshold.");
      }
    fireFragments = ChooseToFire(fitFragments, memsize);
    return fireFragments;
}

/* -----------------------------
 *	set_plan_parallel
 *
 *	set the degree of parallelism for each node in plan
 * -----------------------------
 */
void
set_plan_parallel(plan, nparallel)
Plan plan;
int nparallel;
{
    if (plan == NULL)
       return;
    if (ExecIsNestLoop(plan))  {
       /* --------------------
	* inner path of nestloop join plan should have parallelism 1
	* --------------------
	*/
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

/* ------------------------------
 *	SetParallelDegree
 *
 *	set the degree of parallelism for fragments in fragmentlist
 * ------------------------------
 */
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
    int nfreeslaves;
    List parsetree;
    LispValue k;
    int parallel;

    fireFragmentList = LispNil;
    nfreeslaves = NumberOfFreeSlaves;
    foreach (x, fragmentlist) {
	fragment = (Fragment)CAR(x);
	parsetree = get_frag_parsetree(fragment);
	k = parse_parallel(parsetree);
	if (lispNullp(k)) {
	    if (lispNullp(CDR(x)))
		parallel = nfreeslaves;
	    else
	        parallel = 1;
	  }
	else {
	    parallel = CInteger(k);
	    if (parallel > nfreeslaves || parallel == 0)
		parallel = nfreeslaves;
	  }
        memAvail = GetCurrentMemSize();
        loadAvg = GetCurrentLoadAverage();
        fireFragments = ChooseFragments(fragment, memAvail);
        SetParallelDegree(fireFragments, parallel);
	nfreeslaves -= parallel;
	if (parallel > 0)
	    fireFragmentList = nconc(fireFragmentList, fireFragments);
      }
    return fireFragmentList;
}

#define MINHASHTABLEMEMORYKEY	1000
static IpcMemoryKey nextHashTableMemoryKey = 0;

/* -------------------------
 *	getNextHashTableMemoryKey
 *
 *	get the next hash table key
 * -------------------------
 */
IpcMemoryKey
getNextHashTableMemoryKey()
{
    extern int MasterPid;
    return (nextHashTableMemoryKey++ + MINHASHTABLEMEMORYKEY + MasterPid);
}

/* ------------------------
 *	sizeofTmpRelDesc
 *
 *	calculate the size of reldesc of the temporary relation of plan
 * ------------------------
 */
static int
sizeofTmpRelDesc(plan)
Plan plan;
{
    List targetList;
    int natts;
    int size;

    targetList = get_qptargetlist(plan);
    natts = length(targetList);
    /* ------------------
     * see CopyRelDescUsing() in lib/C/copyfuncs.c if you want to know
     * how size if derived.
     * ------------------
     */
    size = sizeof(RelationData) + (natts - 1) * sizeof(TupleDescriptorData) +
	   sizeof(RuleLock) + sizeof(RelationTupleFormD) +
	   sizeof(LockInfoData) + 
	   natts * (sizeof(AttributeTupleFormData) + sizeof(RuleLock)) +
	   12; /* some extra for possible LONGALIGN() */
    return size;
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
	   ProcGroupSMBeginAlloc(groupid);
	   ProcGroupInfoP[groupid].queryDesc = (List)
			CopyObjectUsing(fragQueryDesc, ProcGroupSMAlloc);
	   for (p = ProcGroupLocalInfoP[groupid].memberProc;
		p != NULL;
		p = p->next) {
	       SlaveInfoP[p->pid].resultTmpRelDesc = 
		 (Relation)ProcGroupSMAlloc(sizeofTmpRelDesc(plan));
	      }
	   ProcGroupSMEndAlloc();
	   ProcGroupInfoP[groupid].countdown = nparallel;
	   wakeupProcGroup(groupid);
	   set_frag_is_inprocess(fragment, true);
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
	*  free shared memory
	*  free the finished processed group
	* -----------------
	*/
       ProcGroupSMClean(groupid);
       freeProcGroup(groupid);
     }
}
