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

/* XXX had to break up executor.h, otherwise symbol table full */

#include "nodes/pg_lisp.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/plannodes.a.h"
#include "executor/execmisc.h"
#include "executor/x_execinit.h"

 RcsId("$Header$");

/* ----------------------------------------------------------------
 *	ExecuteFragments
 *
 *	This is only executed from the Master backend.  Remember the
 *	slave backends have their own special processing loop.
 *
 *	In the single backend case, this just executes the planned
 *	query.  In the multiple backend case, this divides the query
 *	plan into plan fragments and copys them into shared memory.
 *	Each backend process then goes to work on its fragment.
 *
 *	Read the comments for ProcessQuery() below...
 * ----------------------------------------------------------------
 */
extern Pointer *SlaveQueryDescsP;
extern ScanTemps RMakeScanTemps();
extern Fragment RMakeFragment();
extern Relation CopyRelDescUsing();

Fragment
ExecuteFragments(queryDesc, fragmentlist, rootFragment)
    List 	queryDesc;
    List 	fragmentlist;
    Fragment	rootFragment;
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
    ScanTemps		scantempNode;
    Relation		tempRelationDesc;
    List		tempRelationDescList;
    Relation		shmTempRelationDesc;
    
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
	 *   in shared memory, signal slave-backend execution and
	 *   go to work on our fragment.
	 *
	 *   When slave execution is completed, form a new plan
	 *   representing the query with some of the work materialized
	 *   and return this to ProcessQuery().
	 * ----------------
	 */

	nproc = 0;
	foreach (x, fragmentlist) {

	    /* ----------------
	     *	XXX what are we doing here? (comment me)
	     * ----------------
	     */
	    fragment = 	(Fragment) CAR(x);
	    nparallel = get_frag_parallel(fragment);
	    plan = 	get_frag_root(fragment);
	    parseTree = QdGetParseTree(queryDesc);
	    
	    parse_tree_result_relation(parseTree) = LispNil;
	    
	    /* ----------------
	     *	XXX what are we doing here? (comment me)
	     *  XXX why are we treating hash nodes specially here?
	     * ----------------
	     */
	    if (ExecIsHash(plan))  {
		int nbatch;
		nbatch =    ExecHashPartition(plan);
		hashtable = (HashJoinTable) ExecHashTableCreate(plan, nbatch);
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
	 *  all the fragments are in shared memory now.. 
	 *  signal slave execution start, and then go to work
	 *  ourselves.
	 * ----------------
	 */
	for (i=1; i<nproc; i++) {
	    SLAVE1_elog(DEBUG, "Master Backend: signaling slave %d", i);
	    V_Start(i);
	}

	ProcessQueryDesc((List)SlaveQueryDescsP[0]);

	/* ----------------
	 *  now that we're done, let's wait for slaves to complete execution
	 * ----------------
	 */
	SLAVE_elog(DEBUG, "Master Backend: waiting for slaves...");
	P_Finished(nproc - 1);
	SLAVE_elog(DEBUG, "Master Backend: slaves execution complete!");

	/* ----------------
	 *	XXX what are we doing here? (comment me)
	 * ----------------
	 */
	nproc = 0;
	foreach (x, fragmentlist) {
	    fragment = 		(Fragment) CAR(x);
	    nparallel = 		get_frag_parallel(fragment);
	    plan = 		get_frag_root(fragment);
	    parentPlan = 	get_frag_parent_op(fragment);
	    parentFragment = get_frag_parent_frag(fragment);
	   
	    if (parentFragment == NULL)
		subtrees = LispNil;
	    else {
		subtrees = get_frag_subtrees(parentFragment);
		set_frag_subtrees(parentFragment,
				  set_difference(subtrees,
						 lispCons(fragment, LispNil)));
	   }

	    /* ----------------
	     *	XXX what are we doing here? (comment me)
	     *  XXX why are hash nodes treated special?
	     * ----------------
	     */
	    if (ExecIsHash(plan)) {
		/* set_hashjointable(parentPlan, hashtable);  */
		/* YYY more info. needed. */
	    } else {
		List unionplans = LispNil;
	       
		/* ----------------
		 *	XXX what are we doing here? (comment me)
		 *	XXX why are we treating ScanTemp nodes special?
		 * ----------------
		 */
		if (ExecIsScanTemps(plan)) {
		    Relation 	tempreldesc;
		    List	tempRelDescs;
		    LispValue 	y;

		    tempRelDescs = get_temprelDescs(plan);
		    foreach (y, tempRelDescs) {
			tempreldesc = (Relation)CAR(y);
			ReleaseTmpRelBuffers(tempreldesc);
			if (FileNameUnlink(
			      relpath(&(tempreldesc->rd_rel->relname))) < 0)
			    elog(WARN, "ExecuteFragments: unlink: %m");
		    }
		}
		
		/* ----------------
		 *	XXX what are we doing here? (comment me)
		 * ----------------
		 */
		if (parentPlan == NULL && nparallel == 1)
		    rootFragment = NULL;
		else {
		    tempRelationDescList = LispNil;
		    for (i=0; i<nparallel; i++) {
			shmPlan = QdGetPlan((List)SlaveQueryDescsP[nproc+i]);
			shmTempRelationDesc=
			    get_resultTmpRelDesc(get_retstate(shmPlan));

			tempRelationDesc =
			    CopyRelDescUsing(shmTempRelationDesc, palloc);
			tempRelationDescList =
			    nappend1(tempRelationDescList, tempRelationDesc);
		    }
		    scantempNode = RMakeScanTemps();
		    set_qptargetlist(scantempNode, get_qptargetlist(plan));
		    set_temprelDescs(scantempNode, tempRelationDescList);

		    /* ----------------
		     *	XXX what are we doing here? (comment me)
		     *  XXX why are we getting very deep in if statements?
		     * ----------------
		     */
		    if (parentPlan == NULL) {
			set_frag_root(rootFragment, scantempNode);
			set_frag_subtrees(rootFragment, LispNil);
			set_fragment(scantempNode,-1); /*means end of parallelism */
		    } else {
			if (plan == (Plan) get_lefttree(parentPlan)) {
			    set_lefttree(parentPlan, scantempNode);
			}
			else {
			    set_righttree(parentPlan, scantempNode);
			}
			set_fragment(scantempNode, get_fragment(parentPlan));
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

/* --------------------------------
 *	FindFragments
 *
 *	XXX comment me
 * --------------------------------
 */
List
FindFragments(node, fragmentNo)
    Plan node;
    int fragmentNo;
{
    List subFragments = LispNil;
    List newFragments;
    Fragment fragment;

    /* ----------------
     *	XXX what are we doing here? (comment me)
     *  XXX why are hash/sort nodes special
     * ----------------
     */
    set_fragment(node, fragmentNo);
    if (get_lefttree(node) != NULL) {
	if (ExecIsHash(get_lefttree(node)) ||
	    ExecIsSort(get_lefttree(node))) {
	    
	    fragment = RMakeFragment();
	    set_frag_root(fragment, get_lefttree(node));
	    set_frag_parent_op(fragment, node);
	    set_frag_parallel(fragment, 1);
	    set_frag_subtrees(fragment, LispNil);
	    subFragments = nappend1(subFragments, fragment);
        } else {
	    subFragments = FindFragments(get_lefttree(node), fragmentNo);
        }
    }
    
    /* ----------------
     *	XXX what are we doing here? (comment me)
     *  XXX why are hash/sort nodes in the right tree special?
     *  XXX why is this code practically identical to the previous code?
     * ----------------
     */
    if (get_righttree(node) != NULL) {
	if (ExecIsHash(get_righttree(node)) ||
	    ExecIsSort(get_righttree(node))) {
	    
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
    }
    
    return subFragments;
}

/* --------------------------------
 *	InitialPlanFragments(originalPlan)
 *
 *	XXX comment me
 * --------------------------------
 */
Fragment
InitialPlanFragments(originalPlan)
    Plan originalPlan;
{
    Plan 	node;
    List 	x, y;
    List 	fragmentlist;
    List 	subFragments;
    List 	newFragmentList;
    int 	fragmentNo = 0;
    Fragment 	rootFragment;
    Fragment 	fragment, frag;

    /* ----------------
     *	XXX what are we doing here? (comment me)
     * ----------------
     */
    rootFragment = RMakeFragment();
    
    set_frag_root(rootFragment, originalPlan);
    set_frag_parent_op(rootFragment, NULL);
    set_frag_parallel(rootFragment, 1);
    set_frag_subtrees(rootFragment, LispNil);
    set_frag_parent_frag(rootFragment, NULL);

    fragmentlist = lispCons(rootFragment, LispNil);

    /* ----------------
     *	XXX what are we doing here? (comment me)
     * ----------------
     */
    while (!lispNullp(fragmentlist)) {
	newFragmentList = LispNil;
	
	foreach (x, fragmentlist) {
	    fragment = 		(Fragment)CAR(x);
	    node = 		get_frag_root(fragment);
	    
	    subFragments = 	FindFragments(node, fragmentNo++);
	    
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

/* --------------------------------
 *	GetCurrentMemSize
 * --------------------------------
 */

extern int NBuffers;

int
GetCurrentMemSize()
{
    /* ----------------
     *	XXX comment me (what will be added later???)
     * ----------------
     */
    return NBuffers;  /* YYY functionalities to be added later */
}

/* --------------------------------
 *	GetCurrentLoadAverage
 * --------------------------------
 */
float
GetCurrentLoadAverage()
{
    /* ----------------
     *	XXX comment me (what will be added later???)
     * ----------------
     */
    return 0.0;  /* YYY functionalities to be added later */
}

/* --------------------------------
 *	GetReadyFragments
 * --------------------------------
 */
List
GetReadyFragments(fragments)
Fragment fragments;
{
    List 	x;
    Fragment 	frag;
    List 	readyFragments = LispNil;
    List 	readyFrags;

    /* ----------------
     *	XXX what are we doing here? (comment me)
     * ----------------
     */
    if (lispNullp(get_frag_subtrees(fragments)))
	return lispCons(fragments, LispNil);
    
    /* ----------------
     *	XXX what are we doing here? (comment me)
     * ----------------
     */
    foreach (x, get_frag_subtrees(fragments)) {
	frag = (Fragment)CAR(x);
	readyFrags = GetReadyFragments(frag);
	readyFragments = nconc(readyFragments, readyFrags);
    }
    
    return readyFragments;
}

/* --------------------------------
 *	GetFitFragments(fragmentlist, memsize)
 * --------------------------------
 */
List
GetFitFragments(fragmentlist, memsize)
    List fragmentlist;
    int memsize;
{
    /* YYY functionalities to be added later */
    /* ----------------
     *	XXX comment me (what will be added later???)
     * ----------------
     */
    
    return fragmentlist;
}

/* --------------------------------
 *	DecomposeFragments(fragmentlist, memsize)
 * --------------------------------
 */
List
DecomposeFragments(fragmentlist, memsize)
    List fragmentlist;
    int memsize;
{
    /* YYY functionalities to be added later */
    /* ----------------
     *	XXX comment me (what will be added later???)
     * ----------------
     */
    
    return fragmentlist; 
}

/* --------------------------------
 *	ChooseToFire
 *
 *	XXX comment me
 * --------------------------------
 */
List
ChooseToFire(fragmentlist, memsize)
    List fragmentlist;
    int memsize;
{
    /* YYY functionalities to be added later */
    /* ----------------
     *	XXX comment me (what will be added later???)
     * ----------------
     */
    
    return
	lispCons(CAR(fragmentlist), LispNil);   
    
}

/* --------------------------------
 *	ChooseFragments
 *
 *	XXX comment me
 * --------------------------------
 */
List
ChooseFragments(fragments, memsize)
    Fragment fragments;
    int memsize;
{
    List readyFragments;
    List fitFragments;
    List fireFragments;

    /* ----------------
     *	XXX what are we doing here? (comment me)
     * ----------------
     */
    readyFragments = GetReadyFragments(fragments);
    fitFragments = GetFitFragments(readyFragments, memsize);
    
    /* ----------------
     *	XXX what are we doing here? (comment me)
     *  XXX what does this have to do with hash joins?
     * ----------------
     */
    if (lispNullp(fitFragments)) {
	fitFragments = DecomposeFragments(fitFragments, memsize);
	if (lispNullp(fitFragments))
	    elog(WARN, "memory below hashjoin threshold.");
    }
    
    fireFragments = ChooseToFire(fitFragments, memsize);
    
    return fireFragments;
}

/* --------------------------------
 *	set_plan_parallel
 *
 *	XXX comment me
 * --------------------------------
 */
void
set_plan_parallel(plan, nparallel)
    Plan plan;
    int nparallel;
{
    if (plan == NULL)
       return;

    /* ----------------
     *	XXX what are we doing here? (comment me)
     *  XXX why are we treating nest loops specially
     * ----------------
     */    
    if (ExecIsNestLoop(plan))  {
	set_parallel(plan, nparallel);
	set_plan_parallel(get_outerPlan(plan), nparallel);
	set_plan_parallel(get_innerPlan(plan), 1);
    } else  {
	set_parallel(plan, nparallel);
	set_plan_parallel(get_lefttree(plan), nparallel);
	set_plan_parallel(get_righttree(plan), nparallel);
    }
}

/* --------------------------------
 *	SetParallelDegree
 *
 *	XXX comment me
 * --------------------------------
 */
void
SetParallelDegree(fragmentlist, loadavg)
    List fragmentlist;
    float loadavg;
{
    LispValue 	x;
    Fragment 	fragment;
    int 	nslaves;
    Plan 	plan;
    int 	fragmentno;

    /* ----------------
     *	XXX what are we doing here? (comment me)
     * ----------------
     */    
    nslaves = GetNumberSlaveBackends();
    /* YYY more functionalities to be added later */
    
    foreach (x,fragmentlist) {
	fragment = 	(Fragment)CAR(x);
	plan = 		get_frag_root(fragment);
	fragmentno = 	get_fragment(plan);
	
	/* ----------------
	 *	XXX what are we doing here? (comment me)
	 * ----------------
	 */    
	if (fragmentno < 0) {
	    set_frag_parallel(fragment, 1);  /* YYY */
	    set_plan_parallel(plan, 1);
	} else {
	    set_frag_parallel(fragment, nslaves);  /* YYY */
	    set_plan_parallel(plan, nslaves);
	}
    }
}


/* --------------------------------
 *	ParallelOptimize
 *	
 *	this analyzes the plan in the query descriptor and determines
 *	which fragments to execute based on available virtual
 *	memory resources...
 *	
 * --------------------------------
 */
List
ParallelOptimize(queryDesc, planFragments)
    List	queryDesc;
    Fragment	planFragments;
{
    int 	memAvail;
    float	loadAvg;
    List 	fireFragments;

    memAvail = 		GetCurrentMemSize();
    fireFragments = 	ChooseFragments(planFragments, memAvail);
    loadAvg = 		GetCurrentLoadAverage();
    
    SetParallelDegree(fireFragments, loadAvg);
    
    return fireFragments;
}

