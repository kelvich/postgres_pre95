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
/* XXX had to break up executor.h, otherwise symbol table full */
#include "nodes/pg_lisp.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/plannodes.a.h"
#include "executor/x_execinit.h"

/* XXX the following #define's are copied from executor.h */
#define ExecIsNestLoop(node)    IsA(node,NestLoop)
#define ExecIsHash(node)        IsA(node,Hash)
#define ExecIsSort(node)        IsA(node,Sort)

#define QdGetParseTree(queryDesc)    (List) CAR(CDR(queryDesc))
#define QdGetPlan(queryDesc)         (Plan) CAR(CDR(CDR(queryDesc)))

#define parse_tree_root(parse_tree) \
    CAR(parse_tree)

#define parse_tree_root_result_relation(parse_tree_root) \
    CAR(CDR(CDR(parse_tree_root)))

#define parse_tree_result_relation(parse_tree) \
    parse_tree_root_result_relation(parse_tree_root(parse_tree))


 RcsId("$Header$");

/* ----------------------------------------------------------------
 *	ExecuteFragments
 *
 *	Read the comments for ProcessQuery() below...
 *
 *	Since we do no parallism, planFragments is totally
 *	ignored for now.. -cim 9/18/89
 * ----------------------------------------------------------------
 */
extern Pointer *SlaveQueryDescsP;
extern ScanTemp RMakeScanTemp();
extern Append RMakeAppend();
extern Fragment RMakeFragment();

Fragment
ExecuteFragments(queryDesc, fragmentlist, rootFragment)
List 	queryDesc;
List 	fragmentlist;
Fragment rootFragment;
{
    int			nparallel;
    int			i;
    int			nproc;
    LispValue		x;
    Fragment		fragment;
    Plan		plan;
    Plan		parentPlan;
    Plan		shmPlan;
    List		parseTree;
    HashJoinTable	hashtable;
    List		subtrees;
    Fragment		parentFragment;
    List		fragQueryDesc;
    ScanTemp		scantempNode;
    Relation		tempRelationDesc;
    Append		appendNode;
    
    /* ----------------
     *	execute the query appropriately if we are running one or
     *  several backend processes.
     * ----------------
     */
    
    if (! ParallelExecutorEnabled()) {
	/* ----------------
	 *   single-backend case. just execute the query
	 *   and return NULL.
	 * ----------------
	 */
	ProcessQueryDesc(queryDesc);
	return NULL;
    } else {
	/* ----------------
	 *   parallel-backend case.  place each plan fragment
	 *   in shared memory and signal slave-backend execution.
	 *   When slave execution is completed, form a new plan
	 *   representing the query with some of the work materialized
	 *   and return this to ProcessQuery().
	 *
	 * ----------------
	 */

	nproc = 0;
	foreach (x, fragmentlist) {
	   fragment = (Fragment)CAR(x);
	   nparallel = get_frag_parallel(fragment);
	   plan = get_frag_root(fragment);
	   parseTree = QdGetParseTree(queryDesc);
	   if (ExecIsHash(plan))  {
	      int nbatch;
	      nbatch = ExecHashPartition(plan);
	      hashtable = ExecHashTableCreate(plan, nbatch);
	      /* set_hashjointable(plan, hashtable);  YYY */
	      }
	   else if (fragment != rootFragment || nparallel > 1) {
	      parse_tree_result_relation(parseTree) =
		  lispCons(lispAtom("intotemp"), LispNil);
	     }
	   fragQueryDesc = CreateQueryDesc(parseTree, plan);

	/* ----------------
	 *	place fragments in shared memory here.  
	 * ----------------
	 */
	   for (i=0; i<nparallel; i++) {
	      SlaveQueryDescsP[nproc+i] = (Pointer)
	    		CopyObjectUsing(fragQueryDesc, ExecSMAlloc);
	     }
	   nproc += nparallel;
	  }

	/* ----------------
	 *	signal slave execution start
	 * ----------------
	 */
	for (i=0; i<nproc; i++) {
	    elog(DEBUG, "Master Backend: signaling slave %d", i);
	    V_Start(i);
	}

	/* ----------------
	 *	wait for slaves to complete execution
	 * ----------------
	 */
	elog(DEBUG, "Master Backend: waiting for slaves...");
	P_Finished();
	elog(DEBUG, "Master Backend: slaves execution complete!");

	nproc = 0;
	foreach (x, fragmentlist) {
	   fragment = (Fragment)CAR(x);
	   nparallel = get_frag_parallel(fragment);
	   plan = get_frag_root(fragment);
	   parentPlan = get_frag_parent_op(fragment);
	   parentFragment = get_frag_parent_frag(fragment);
	   if (parentFragment == NULL)
	      subtrees = LispNil;
	   else {
	      subtrees = get_frag_subtrees(parentFragment);
	      set_frag_subtrees(parentFragment, set_difference(subtrees,
	                                     lispCons(fragment, LispNil)));
	    }
	   if (ExecIsHash(plan)) {
	       /* set_hashjointable(parentPlan, hashtable);  */
	       /* YYY more info. needed. */
	      }
   	   else {
	       List unionplans = LispNil;
	       if (parentPlan == NULL && nparallel == 1)
		  rootFragment = NULL;
	       else {
		  for (i=0; i<nparallel; i++) {
		     shmPlan = QdGetPlan((List)SlaveQueryDescsP[nproc+i]);
		     tempRelationDesc=get_resultTmpRelDesc(get_retstate(shmPlan));
		     scantempNode = RMakeScanTemp();
		     set_temprelDesc(scantempNode, tempRelationDesc);
		     unionplans = nappend1(unionplans, scantempNode);
		     }
		  appendNode = RMakeAppend();
		  set_unionplans(appendNode, unionplans);
		  if (parentPlan == NULL) {
		     set_frag_root(rootFragment, appendNode);
		     set_frag_subtrees(rootFragment, LispNil);
		     set_fragment(appendNode,-1); /* means end of parallelism */
		    }
	          else {
	          if (plan == (Plan)get_lefttree(parentPlan)) {
		     set_lefttree(parentPlan, appendNode);
		    }
	          else {
		     set_righttree(parentPlan, appendNode);
		    }
	          set_fragment(appendNode, get_fragment(parentPlan));
	          }
	       }
	   nproc += nparallel;
	 }
       }
	    
	/* ----------------
	 *	Clean Shared Memory used during the query
	 * ----------------
	 */
	ExecSMClean();
	
	/* ----------------
	 *	replace fragments with materialized results and
	 *	return new plan to ProcessQuery.
	 * ----------------
	 */
	return rootFragment;
    }
}

List
FindFragments(node, fragmentNo)
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
          subFragments = nappend1(subFragments, fragment);
         }
       else {
         newFragments = FindFragments(get_righttree(node), fragmentNo);
         subFragments = nconc(subFragments, newFragments);
        }
    
    return subFragments;
}

Fragment
InitialPlanFragments(originalPlan)
Plan originalPlan;
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
    set_frag_root(rootFragment, originalPlan);
    set_frag_parent_op(rootFragment, NULL);
    set_frag_parallel(rootFragment, 1);
    set_frag_subtrees(rootFragment, LispNil);
    set_frag_parent_frag(rootFragment, NULL);

    fragmentlist = lispCons(rootFragment, LispNil);

    while (!lispNullp(fragmentlist)) {
	newFragmentList = LispNil;
	foreach (x, fragmentlist) {
	   fragment = (Fragment)CAR(x);
	   node = get_frag_root(fragment);
	   subFragments = FindFragments(node, fragmentNo++);
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
SetParallelDegree(fragmentlist, loadavg)
List fragmentlist;
float loadavg;
{
    LispValue x;
    Fragment fragment;
    int nslaves;
    Plan plan;
    int fragmentno;

    nslaves = GetNumberSlaveBackends();
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
           set_frag_parallel(fragment, nslaves);  /* YYY */
           set_plan_parallel(plan, nslaves);
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
ParallelOptimize(queryDesc, planFragments)
List		queryDesc;
Fragment	planFragments;
{
    int memAvail;
    float loadAvg;
    List fireFragments;

    memAvail = GetCurrentMemSize();
    fireFragments = ChooseFragments(planFragments, memAvail);
    loadAvg = GetCurrentLoadAverage();
    SetParallelDegree(fireFragments, loadAvg);
    return fireFragments;
}

