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

#include <signal.h>
#include <math.h>
#include "tmp/postgres.h"
#include "tcop/tcopdebug.h"
#include "tcop/slaves.h"
#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "executor/execmisc.h"
#include "executor/x_execinit.h"
#include "executor/x_hash.h"
#include "tcop/dest.h"
#include "tmp/portal.h"
#include "commands/command.h"
#include "parser/parsetree.h"
#include "storage/lmgr.h"
#include "catalog/pg_relation.h"
#include "utils/lsyscache.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "nodes/relation.h"
#include "lib/copyfuncs.h"
#include "lib/catalog.h"
#include "tcop/pquery.h"

 RcsId("$Header$");

int AdjustParallelismEnabled = 1;
extern int NStriping;
static MasterSchedulingInfoData MasterSchedulingInfoD = {-1, NULL, -1, NULL};
extern ParallelismModes ParallelismMode;

/* ------------------------------------
 *	FingFragments
 *
 *	find all the plan fragments under plan node, mark the fragments starting
 *	with fragmentNo
 *	plan fragments are obtained by decomposing the plan tree across all
 *	blocking edges, i.e., edges out of Hash nodes and Sort nodes
 * ------------------------------------
 */
static List
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
      if (ExecIsHash(get_lefttree(node))||ExecIsMergeJoin(get_lefttree(node))) {
	  /* -----------------------------
	   * detected a blocking edge, fragment boundary
	   * -----------------------------
	   */
          fragment = RMakeFragment();
          set_frag_root(fragment, (Plan)get_lefttree(node));
          set_frag_parent_op(fragment, node);
          set_frag_parallel(fragment, 1);
          set_frag_subtrees(fragment, LispNil);
	  set_frag_parsetree(fragment, parsetree);
	  set_frag_is_inprocess(fragment, false);
	  set_frag_iorate(fragment, 0.0);
          subFragments = nappend1(subFragments, (LispValue)fragment);
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
          set_frag_root(fragment, (Plan)get_righttree(node));
          set_frag_parent_op(fragment, node);
          set_frag_parallel(fragment, 1);
          set_frag_subtrees(fragment, LispNil);
	  set_frag_parsetree(fragment, parsetree);
	  set_frag_is_inprocess(fragment, false);
	  set_frag_iorate(fragment, 0.0);
          subFragments = nappend1(subFragments, (LispValue)fragment);
         }
       else {
         newFragments = FindFragments(parsetree,get_righttree(node),fragmentNo);
         subFragments = nconc(subFragments, newFragments);
        }
    
    return subFragments;
}

#define SEQPERTUPTIME	2e-3 	/* second */
#define INDPERTUPTIME	0.1
#define DEFPERTUPTIME	4e-3

static float
get_pertuptime(plan)
Plan plan;
{
    switch (NodeType(plan)) {
    case classTag(SeqScan):
	return SEQPERTUPTIME;
    case classTag(IndexScan):
	return INDPERTUPTIME;
    default:
	return DEFPERTUPTIME;
      }
}

#define AVGINDTUPS	5

static float
compute_frag_iorate(fragment)
Fragment fragment;
{
    Plan plan;
    int fragmentno;
    float pertupletime;
    Plan p;
    float iorate;
    int tupsize;

    plan = get_frag_root(fragment);
    fragmentno = get_fragment(plan);
    pertupletime = 0.0;
    for (;;) {
	pertupletime += get_pertuptime(plan);
	p = get_outerPlan(plan); /* walk down the outer path only for now */
	if (p == NULL || get_fragment(p) != fragmentno)
	    break;
	else
	    plan = p;
      }
    if (ExecIsSeqScan(plan) || ExecIsScanTemps(plan)) {
	iorate = 1.0/(pertupletime * get_plan_tupperpage(plan));
      }
    else if (ExecIsIndexScan(plan)) {
	iorate = 1.0/(pertupletime * AVGINDTUPS);
      }
    return iorate;
}

/* -------------------------------
 *	SetIoRate
 *
 *	compute and set the io rate of each fragment
 * -------------------------------
 */
static void
SetIoRate(fragment)
Fragment fragment;
{
    LispValue x;
    Fragment f;

    set_frag_iorate(fragment, compute_frag_iorate(fragment));
    foreach (x, get_frag_subtrees(fragment)) {
	f = (Fragment)CAR(x);
	SetIoRate(f);
      }
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
    set_frag_iorate(rootFragment, 0.0);

    fragmentlist = lispCons((LispValue)rootFragment, LispNil);

    while (!lispNullp(fragmentlist)) {
	newFragmentList = LispNil;
	foreach (x, fragmentlist) {
	   fragment = (Fragment)CAR(x);
	   node = get_frag_root(fragment);
	   subFragments = FindFragments(parsetree, node, fragmentNo++);
	   set_frag_subtrees(fragment, subFragments);
	   foreach (y, subFragments) {
	      frag = (Fragment)CAR(y);
	      set_frag_parent_frag(frag, (FragmentPtr)fragment);
	     }
	   newFragmentList = nconc(newFragmentList, subFragments);
	  }
	fragmentlist = newFragmentList;
      }
    SetIoRate(rootFragment);
    return rootFragment;
}

extern int NBuffers;

/* -----------------------------
 *	GetCurrentMemSize
 *
 *	get the current amount of available memory
 * -----------------------------
 */
static int
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
static float
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
static List
GetReadyFragments(fragments)
Fragment fragments;
{
    LispValue x;
    Fragment frag;
    List readyFragments = LispNil;
    List readyFrags;

    if (lispNullp(get_frag_subtrees(fragments)) && 
	!get_frag_is_inprocess(fragments))
       return lispCons((LispValue)fragments, LispNil);
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
static List
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
static List
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
static List
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
static List
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

/* ------------------------------
 *	SetParallelDegree
 *
 *	set the degree of parallelism for fragments in fragmentlist
 * ------------------------------
 */
static void
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
	  }
       else {
           set_frag_parallel(fragment, nfreeslaves);  /* YYY */
	 }
     }
}

/* ---------------------------------
 *	plan_is_parallelizable
 *
 *	returns true if plan is parallelizable, false otherwise
 * ---------------------------------
 */
static bool
plan_is_parallelizable(plan)
Plan plan;
{
    if (plan == NULL)
	return true;
    if (ExecIsMergeJoin(plan))
	return false;
    if (ExecIsSort(plan))
	return false;
    if (ExecIsIndexScan(plan)) {
	List indxqual;
	LispValue x;
	NameData opname;
	Oper op;
	indxqual = get_indxqual((IndexScan)plan);
	if (length(CAR(indxqual)) < 2)
	    return false;
	foreach (x, CAR(indxqual)) {
	    op = (Oper)CAR(CAR(x));
	    opname = get_opname(get_opno(op));
	    if (strcmp(&opname, "=") == 0)
		return false;
	 }
      }
    if (plan_is_parallelizable(get_outerPlan(plan)))
	return true;
    return false;
}

/* ----------------------------------------
 *	nappend1iobound
 *
 *	insert an io-bound fragment into a list in 
 *	descending io rate order.
 * ----------------------------------------
 */
static List
nappend1iobound(ioboundlist, frag)
List ioboundlist;
Fragment frag;
{
    LispValue x;
    Fragment f;

    if (lispNullp(ioboundlist))
	return lispCons((LispValue)frag, LispNil);
    f = (Fragment)CAR(ioboundlist);
    if (get_frag_iorate(frag) > get_frag_iorate(f)) {
	return(nconc(lispCons((LispValue)frag, LispNil), ioboundlist));
      }
    else {
	return(nconc(lispCons((LispValue)f, LispNil), 
		     nappend1iobound(CDR(ioboundlist), frag)));
      }
}

/* ----------------------------------------
 *	nappend1cpubound
 *
 *	insert a cpu-bound fragment into a list in 
 *	ascending io rate order.
 * ----------------------------------------
 */
static List
nappend1cpubound(cpuboundlist, frag)
List cpuboundlist;
Fragment frag;
{
    LispValue x;
    Fragment f;

    if (lispNullp(cpuboundlist))
	return lispCons((LispValue)frag, LispNil);
    f = (Fragment)CAR(cpuboundlist);
    if (get_frag_iorate(frag) < get_frag_iorate(f)) {
	return(nconc(lispCons((LispValue)frag, LispNil), cpuboundlist));
      }
    else {
	return(nconc(lispCons((LispValue)f, LispNil), 
		     nappend1cpubound(CDR(cpuboundlist), frag)));
      }
}

#define DISKBANDWIDTH	60 	/* IO per second */

/* -------------------------------------
 *	ClassifyFragments
 *
 *	classify fragments into io-bound, cpu-bound, unparallelizable or
 *	parallelism-preset.
 * -------------------------------------
 */
static void
ClassifyFragments(fraglist, ioboundlist, cpuboundlist, unparallelizablelist, presetlist)
List fraglist;
List *ioboundlist, *cpuboundlist, *unparallelizablelist, *presetlist;
{
    LispValue x;
    Fragment f;
    Plan p;
    float iorate;
    float diagonal;

    *ioboundlist = LispNil;
    *cpuboundlist = LispNil;
    *unparallelizablelist = LispNil;
    *presetlist = LispNil;
    diagonal = (float)NStriping * DISKBANDWIDTH/(float)GetNumberSlaveBackends();
    foreach (x, fraglist) {
	f = (Fragment)CAR(x);
	p = get_frag_root(f);
	if (!plan_is_parallelizable(p)) {
	    *unparallelizablelist = nappend1(*unparallelizablelist, 
					     (LispValue)f);
	  }
	else if (!lispNullp(parse_parallel(get_frag_parsetree(f)))) {
	    *presetlist = nappend1(*presetlist, (LispValue)f);
	  }
	else {
	    iorate = get_frag_iorate(f);
	    if (iorate > diagonal) {
	        *ioboundlist = nappend1iobound(*ioboundlist, f);
	      }
	    else {
		*cpuboundlist = nappend1cpubound(*cpuboundlist, f);
	      }
	  }
      }
}

/* ---------------------------------
 *	ComputeIoCpuBalancePoint
 *
 *	compute io/cpu balance point of two fragments:
 *	f1, io-bound
 *	f2, cpu-bound
 * ---------------------------------
 */
static void
ComputeIoCpuBalancePoint(f1, f2, x1, x2)
Fragment f1, f2;
int *x1, *x2;
{
    float bandwidth;
    float iorate1, iorate2;
    int nfreeslaves;

    nfreeslaves = GetNumberSlaveBackends();
    bandwidth = NStriping * DISKBANDWIDTH;
    iorate1 = get_frag_iorate(f1);
    iorate2 = get_frag_iorate(f2);
    *x1=(int)floor((double)((bandwidth-iorate2*nfreeslaves)/(iorate1-iorate2)));
    *x2=(int)ceil((double)((iorate1*nfreeslaves-bandwidth)/(iorate1-iorate2)));
}

/* --------------------------------------
 *	MaxFragParallelism
 *
 *	return the maximum parallelism for a fragment
 *	within the limit of number of free processors and disk bandwidth
 * -------------------------------------
 */
static int
MaxFragParallelism(frag)
Fragment frag;
{
    float ioRate;
    int par;

    if (!plan_is_parallelizable(get_frag_root(frag)))
	return 1;
    if (!lispNullp(parse_parallel(get_frag_parsetree(frag))))
	return CInteger(parse_parallel(get_frag_parsetree(frag)));
    ioRate = (float)get_frag_iorate(frag);
    par = MIN((int)floor((double)NStriping*DISKBANDWIDTH/ioRate),
	      NumberOfFreeSlaves);
    return par;
}

/* --------------------------------------
 *	CurMaxFragParallelism
 *
 *	return the maximum parallelism for a fragment
 *	within the limit of current number of free processors and 
 *	available disk bandwidth
 * -------------------------------------
 */
static int
CurMaxFragParallelism(frag, curbandwidth, nfreeslaves)
Fragment frag;
float curbandwidth;
int nfreeslaves;
{
    float ioRate;
    int par;

    if (!plan_is_parallelizable(get_frag_root(frag)))
	return 1;
    if (!lispNullp(parse_parallel(get_frag_parsetree(frag))))
	return CInteger(parse_parallel(get_frag_parsetree(frag)));
    ioRate = (float)get_frag_iorate(frag);
    par = MIN((int)floor((double)curbandwidth/ioRate),
	      nfreeslaves);
    return par;
}

/* ------------------------
 *	AdjustParallelism
 *
 *	dynamically adjust degrees of parallelism of the fragments that
 *	are already in process to take advantage of the extra processors
 * ------------------------
 */
static void
AdjustParallelism(pardelta, groupid)
int pardelta;
int groupid;
{
    int j;
    int slave;
    int max_curpage;
    int size;
    int oldnproc;

    Assert(pardelta != 0);
    SLAVE_elog(DEBUG, "master trying to adjust degrees of parallelism");
    SLAVE1_elog(DEBUG, "master sending signal to process group %d", groupid);
    signalProcGroup(groupid, SIGPARADJ);
    max_curpage = getProcGroupMaxPage(groupid);
    SLAVE1_elog(DEBUG, "master gets maxpage = %d", max_curpage);
    oldnproc = ProcGroupInfoP[groupid].nprocess;
    if (max_curpage == NOPARADJ) {
	/* --------------------------
	 *  forget about adjustment to parallelism
	 *  in this case -- the fragment is almost finished
	 * ---------------------------
	 */
	SLAVE_elog(DEBUG, "master changes mind on adjusting parallelism");
	ProcGroupInfoP[groupid].paradjpage = NOPARADJ;
        OneSignalM(&(ProcGroupInfoP[groupid].m1lock), oldnproc);
	return;
      }
    ProcGroupInfoP[groupid].paradjpage = max_curpage + 1; 
				   /* page on which to adjust par. */
    if (pardelta > 0) {
        ProcGroupInfoP[groupid].nprocess += pardelta;
        ProcGroupInfoP[groupid].scounter.count = 
					      ProcGroupInfoP[groupid].nprocess;
        ProcGroupInfoP[groupid].newparallel = ProcGroupInfoP[groupid].nprocess;
        SLAVE2_elog(DEBUG,
		    "master signals waiting slaves with adjpage=%d,newpar=%d",
	            ProcGroupInfoP[groupid].paradjpage,
		    ProcGroupInfoP[groupid].newparallel);
        OneSignalM(&(ProcGroupInfoP[groupid].m1lock), oldnproc);
        set_frag_parallel(ProcGroupLocalInfoP[groupid].fragment,
		      get_frag_parallel(ProcGroupLocalInfoP[groupid].fragment)+
		      pardelta);
        ProcGroupSMBeginAlloc(groupid);
        size = sizeofTmpRelDesc(QdGetPlan(ProcGroupInfoP[groupid].queryDesc));
        for (j=0; j<pardelta; j++) {
	    if (NumberOfFreeSlaves == 0) {
		elog(WARN, 
		     "trying to adjust to too much parallelism: out of slaves");
	      }
	    slave = getFreeSlave();
	    SLAVE2_elog(DEBUG, "master adding slave %d to procgroup %d", 
			slave, groupid);
            SlaveInfoP[slave].resultTmpRelDesc = 
					(Relation)ProcGroupSMAlloc(size);
            addSlaveToProcGroup(slave, groupid, oldnproc+j);
	    V_Start(slave);
          }
        ProcGroupSMEndAlloc();
      }
    else {
        ProcGroupInfoP[groupid].newparallel = 
				 ProcGroupInfoP[groupid].nprocess + pardelta;
        SLAVE2_elog(DEBUG,
		    "master signals waiting slaves with adjpage=%d,newpar=%d",
	            ProcGroupInfoP[groupid].paradjpage,
		    ProcGroupInfoP[groupid].newparallel);
	ProcGroupInfoP[groupid].dropoutcounter.count = -pardelta;
        OneSignalM(&(ProcGroupInfoP[groupid].m1lock), oldnproc);
      }
}

/* ----------------------------------------------------------------
 *	ParallelOptimize
 *	
 *	analyzes plan fragments and determines what fragments to execute
 *	and with how much parallelism
 *	
 * ----------------------------------------------------------------
 */
static List
ParallelOptimize(fragmentlist)
List fragmentlist;
{
    LispValue y;
    Fragment fragment;
    int memAvail;
    float loadAvg;
    List readyFragmentList;
    List flist;
    List ioBoundFragList, cpuBoundFragList, unparallelizableFragList;
    List presetFraglist;
    List newIoBoundFragList, newCpuBoundFragList;
    Fragment f1, f2, f;
    int x1, x2;
    List fireFragmentList;
    int nfreeslaves;
    LispValue k, x;
    int parallel;
    Plan plan;
    bool io_running, cpu_running;
    int curpar;
    int pardelta;

    fireFragmentList = LispNil;
    nfreeslaves = NumberOfFreeSlaves;

    /* ------------------------------
     *  find those plan fragments that are ready to run, i.e.,
     *  those with all input data ready.
     * ------------------------------
     */
    readyFragmentList = LispNil;
    foreach (y, fragmentlist) {
	fragment = (Fragment)CAR(y);
	flist = GetReadyFragments(fragment);
	readyFragmentList = nconc(readyFragmentList, flist);
      }
    if (ParallelismMode == INTRA_ONLY) {
	f = (Fragment)CAR(readyFragmentList);
	fireFragmentList = lispCons((LispValue)f, LispNil);
	SetParallelDegree(fireFragmentList, MaxFragParallelism(f));
	return fireFragmentList;
      }
    /* -------------------------------
     *  classify the ready fragments into io-bound, cpu-bound and
     *  unparallelizable
     * -------------------------------
     */
    ClassifyFragments(readyFragmentList, &ioBoundFragList, &cpuBoundFragList, 
		      &unparallelizableFragList, &presetFraglist);
    fireFragmentList = LispNil;
    /* -------------------------------
     *  take care of the unparallelizable fragments first
     * -------------------------------
     */
    if (!lispNullp(unparallelizableFragList)) {
	fireFragmentList = lispCons(CAR(unparallelizableFragList), LispNil);
        SetParallelDegree(fireFragmentList, 1);
	elog(NOTICE, "nonparallelizable fragment, running sequentially\n");
	return fireFragmentList;
      }
    /* --------------------------------
     * deal with those fragments with parallelism preset
     * --------------------------------
     */
    if (!lispNullp(presetFraglist)) {
	foreach (x, presetFraglist) {
	    f = (Fragment)CAR(x);
	    k = parse_parallel(get_frag_parsetree(f));
	    parallel = CInteger(k);
	    SetParallelDegree((y=lispCons((LispValue)f, LispNil)), parallel);
	    fireFragmentList = nconc(fireFragmentList, y);
	  }
	SLAVE_elog(DEBUG, "executing fragments with preset parallelism.");
	return fireFragmentList;
      }
    /* ---------------------------------
     *  now we deal with the parallelizable plan fragments
     * ---------------------------------
     */
    if (MasterSchedulingInfoD.ioBoundFrag != NULL) {
	f1 = MasterSchedulingInfoD.ioBoundFrag;
	io_running = true;
      }
    else {
	io_running = false;
	if (lispNullp(ioBoundFragList))
	    f1 = NULL;
	else
	    f1 = (Fragment)CAR(ioBoundFragList);
      }
    if (MasterSchedulingInfoD.cpuBoundFrag != NULL) {
	f2 = MasterSchedulingInfoD.cpuBoundFrag;
	cpu_running = true;
      }
    else {
	cpu_running = false;
	if (lispNullp(cpuBoundFragList))
	    f2 = NULL;
	else
	    f2 = (Fragment)CAR(cpuBoundFragList);
      }
    if (f1 != NULL && f2 != NULL) {
	if (ParallelismMode != INTER_WO_ADJ || (!io_running && !cpu_running)) {
	    ComputeIoCpuBalancePoint(f1, f2, &x1, &x2);
	    SLAVE2_elog(DEBUG, "executing two fragments at balance point (%d, %d).",
		    x1, x2);
	  }
	if (io_running) {
	    curpar = get_frag_parallel(f1);
	    if (ParallelismMode == INTER_WO_ADJ) {
		int newpar;
		float curband;
		curband = NStriping*DISKBANDWIDTH - curpar*get_frag_iorate(f1);
		newpar = CurMaxFragParallelism(f2, curband, NumberOfFreeSlaves);
		if (newpar == 0) return LispNil;
		fireFragmentList = lispCons((LispValue)f2, LispNil);
		SetParallelDegree(fireFragmentList, newpar);
	      }
	    else {
		pardelta = x1 - curpar;
		if (pardelta != 0) {
		   SLAVE_elog(DEBUG, "adjusting parallelism of io-bound task.");
		   AdjustParallelism(pardelta,
				     MasterSchedulingInfoD.ioBoundGroupId);
		  }
		fireFragmentList = lispCons((LispValue)f2, LispNil);
		if (pardelta >= 0) {
		    SetParallelDegree(fireFragmentList, x2);
		  }
		else {
		    SetParallelDegree(fireFragmentList, NumberOfFreeSlaves);
		  }
	      }
	    MasterSchedulingInfoD.cpuBoundFrag = f2;
	  }
	else if (cpu_running) {
	    curpar = get_frag_parallel(f2);
	    if (ParallelismMode == INTER_WO_ADJ) {
		int newpar;
		float curband;
		curband = NStriping*DISKBANDWIDTH - curpar*get_frag_iorate(f2);
		newpar = CurMaxFragParallelism(f1, curband, NumberOfFreeSlaves);
		if (newpar == 0) return LispNil;
		fireFragmentList = lispCons((LispValue)f1, LispNil);
		SetParallelDegree(fireFragmentList, newpar);
	      }
	    else {
		pardelta = x2 - curpar;
		if (pardelta != 0) {
		  SLAVE_elog(DEBUG, "adjusting parallelism of cpu-bound task.");
		  AdjustParallelism(pardelta,
				    MasterSchedulingInfoD.cpuBoundGroupId);
		  }
		fireFragmentList = lispCons((LispValue)f1, LispNil);
		if (pardelta >= 0) {
		    SetParallelDegree(fireFragmentList, x1);
		  }
		else {
		    SetParallelDegree(fireFragmentList, NumberOfFreeSlaves);
		  }
	      }
	    MasterSchedulingInfoD.ioBoundFrag = f1;
	  }
	else {
	    SetParallelDegree((y=lispCons((LispValue)f1, LispNil)), x1);
	    fireFragmentList = nconc(fireFragmentList, y);
	    SetParallelDegree((y=lispCons((LispValue)f2, LispNil)), x2);
	    fireFragmentList = nconc(fireFragmentList, y);
	    MasterSchedulingInfoD.ioBoundFrag = f1;
	    MasterSchedulingInfoD.cpuBoundFrag = f2;
	  }
      }
    else if (f1 == NULL && f2 != NULL) {
	/* -----------------------------
	 *  out of io-bound fragments, use intra-operation parallelism only
	 *  for cpu-bound fragments.
	 * ------------------------------
	 */
	fireFragmentList = lispCons((LispValue)f2, LispNil);
	SetParallelDegree(fireFragmentList, nfreeslaves);
	MasterSchedulingInfoD.cpuBoundFrag = f2;
	SLAVE1_elog(DEBUG, 
            "out of io-bound tasks, running cpu-bound task with parallelism %d",
            nfreeslaves);
      }
    else if (f1 != NULL && f2 == NULL) {
	nfreeslaves = MaxFragParallelism(f1);
	fireFragmentList = lispCons((LispValue)f1, LispNil);
	SetParallelDegree(fireFragmentList, nfreeslaves);
	fireFragmentList = nconc(fireFragmentList, y);
	MasterSchedulingInfoD.ioBoundFrag = f1;
	SLAVE1_elog(DEBUG, 
           "out of cpu-bound tasks, running io-bound task with parallelism %d",
            nfreeslaves);
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
static IpcMemoryKey
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
	   48; /* some extra for possible LONGALIGN() */
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
    int			size;
    
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
	   parentFragment = (Fragment)get_frag_parent_frag(fragment);
	   finalResultRelation = parse_tree_result_relation(parsetree);
	   dest = destination;
	   dest = None; /* WWW */
	   if (ExecIsHash(plan))  {
	      /* ------------
	       *  if it is hashjoin, create the hash table
	       *  so that the slaves can share it
	       * ------------
	       */
	      hashTableMemoryKey = getNextHashTableMemoryKey();
	      set_hashtablekey((Hash)plan, hashTableMemoryKey);
	      hashtable = ExecHashTableCreate(plan);
	      set_hashtable((Hash)plan, hashtable);
	      hashTableMemorySize = get_hashtablesize((Hash)plan);
	      parse_tree_result_relation(parsetree) = LispNil;
	      }
	   else if (get_fragment(plan) >= 0) {
	      /* ------------
	       *  this means that this an intermediate fragment, so
	       *  the result should be kept in some temporary relation
	       * ------------
	       */
	      /* WWW
	      parse_tree_result_relation(parsetree) =
		  lispCons(lispAtom("intotemp"), LispNil);
	       */
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
	   if (fragment == MasterSchedulingInfoD.ioBoundFrag)
	       MasterSchedulingInfoD.ioBoundGroupId = groupid;
	   else if (fragment == MasterSchedulingInfoD.cpuBoundFrag)
	       MasterSchedulingInfoD.cpuBoundGroupId = groupid;
	   ProcGroupLocalInfoP[groupid].fragment = fragment;
	   ProcGroupInfoP[groupid].status = WORKING;
	   ProcGroupSMBeginAlloc(groupid);
	   ProcGroupInfoP[groupid].queryDesc = (List)
			CopyObjectUsing((Node)fragQueryDesc, ProcGroupSMAlloc);
	   size = sizeofTmpRelDesc(plan);
	   for (p = ProcGroupLocalInfoP[groupid].memberProc;
		p != NULL;
		p = p->next) {
	       SlaveInfoP[p->pid].resultTmpRelDesc = 
		 (Relation)ProcGroupSMAlloc(size);
	      }
	   ProcGroupSMEndAlloc();
	   ProcGroupInfoP[groupid].scounter.count = nparallel;
	   ProcGroupInfoP[groupid].nprocess = nparallel;
#ifdef 	   TCOP_SLAVESYNCDEBUG
	   {
	       char procs[100];
	       p = ProcGroupLocalInfoP[groupid].memberProc;
	       sprintf(procs, "%d", p->pid);
	       for (p = p->next; p != NULL; p = p->next)
		   sprintf(procs+strlen(procs), ",%d", p->pid);
	       SLAVE2_elog(DEBUG, "master to wake up procgroup %d {%s} for",
			   groupid, procs);
	       set_query_range_table(parsetree);
	       pplan(plan);
	       fprintf(stderr, "\n");
	    }
#endif
	   wakeupProcGroup(groupid);
	   set_frag_is_inprocess(fragment, true);
	   /* ---------------
	    * restore the original result relation descriptor
	    * ---------------
	    */
	   parse_tree_result_relation(parsetree) = finalResultRelation;
	 }

       /* ------------
	* if there are extra processors lying around,
	* dynamically adjust degrees of parallelism of
	* fragments that are already in process.
       if (NumberOfFreeSlaves > 0 && AdjustParallelismEnabled) {
	    AdjustParallelism(NumberOfFreeSlaves, -1);
	 }
	* ------------
	*/

       /* ----------------
	* wait for some process group to complete execution
	* ----------------
	*/
MasterWait:
       P_Finished();

       /* --------------
	* some process group has finished processing a fragment,
	* find that group
	* --------------
	*/
       groupid = getFinishedProcGroup();
       if (ProcGroupInfoP[groupid].status == PARADJPENDING) {
	   /* -----------------------------
	    * master decided earlier than process group, groupid's parallelism
	    * was going to be reduced.  now the adjustment point is reached.
	    * master is ready to collect those slaves spared from the
	    * group.
	    * -----------------------------
	    */
	   ProcessNode *p, *prev, *nextp;
	   int newparallel = ProcGroupInfoP[groupid].newparallel;
	   List tmpreldesclist = LispNil;
	   int nfreeslave;

         SLAVE1_elog(DEBUG, "master woken up by paradjpending process group %d",
		       groupid);
	   ProcGroupInfoP[groupid].status = WORKING;
	   tempRelationDescList = 
			     ProcGroupLocalInfoP[groupid].resultTmpRelDescList;
	   prev = NULL;
	   for (p = ProcGroupLocalInfoP[groupid].memberProc;
		p != NULL; p = nextp) {
	       nextp = p->next;
	       if (SlaveInfoP[p->pid].groupPid >= newparallel) {
		   /* ------------------------
		    * before freeing this slave, we have to save its
		    * resultTmpRelDesc somewhere.
		    * -------------------------
		    */
#ifndef PALLOC_DEBUG
		    tempRelationDesc = CopyRelDescUsing(
					    SlaveInfoP[p->pid].resultTmpRelDesc,
					    palloc);
#else
		    tempRelationDesc = CopyRelDescUsing(
					    SlaveInfoP[p->pid].resultTmpRelDesc,
					    palloc_debug);
#endif
		    tempRelationDescList = nappend1(tempRelationDescList,
						    (LispValue)tempRelationDesc);
		    if (prev == NULL) {
			ProcGroupLocalInfoP[groupid].memberProc = nextp;
		      }
		    else {
			prev->next = nextp;
		      }
		    freeSlave(p->pid);
		  }
	     }
	        ProcGroupLocalInfoP[groupid].resultTmpRelDescList =
							tempRelationDescList;
		/* --------------------------
		 * now we have to adjust nprocess and countdown of group groupid
		 * --------------------------
		 */
		nfreeslave = ProcGroupInfoP[groupid].nprocess - 
			     ProcGroupInfoP[groupid].newparallel;
		ProcGroupInfoP[groupid].nprocess = 
				ProcGroupInfoP[groupid].newparallel;
		ProcGroupInfoP[groupid].countdown -= nfreeslave;
		/* ---------------------------
		 * adjust parallelism with the freed slaves
		 * ---------------------------
		 */
		if (MasterSchedulingInfoD.ioBoundGroupId == groupid)
		    AdjustParallelism(nfreeslave, 
				      MasterSchedulingInfoD.cpuBoundGroupId);
		else
		    AdjustParallelism(nfreeslave,
				      MasterSchedulingInfoD.ioBoundGroupId);
		if (ProcGroupInfoP[groupid].countdown == 0) {
		    /* -----------------------
		     * this means that this group has actually finished
		     * go down to process the group
		     * -----------------------
		     */
		   }
		else {
		    /* ------------------------
		     * otherwise, go to P_Finished()
		     * ------------------------
		     */
		    goto MasterWait;
		  }
	  }
       SLAVE1_elog(DEBUG, "master woken up by finished process group %d", 
		   groupid);
       fragment = ProcGroupLocalInfoP[groupid].fragment;
       if (fragment == MasterSchedulingInfoD.ioBoundFrag) {
	   MasterSchedulingInfoD.ioBoundFrag = NULL;
	   MasterSchedulingInfoD.ioBoundGroupId = -1;
	 }
       else if (fragment == MasterSchedulingInfoD.cpuBoundFrag) {
	   MasterSchedulingInfoD.cpuBoundFrag = NULL;
	   MasterSchedulingInfoD.cpuBoundGroupId = -1;
	 }
       nparallel = get_frag_parallel(fragment);
       plan = get_frag_root(fragment);
       parentPlan = get_frag_parent_op(fragment);
       parentFragment = (Fragment)get_frag_parent_frag(fragment);
       /* ---------------
	* delete the finished fragment from the subtree list of its
	* parent fragment
	* ---------------
	*/
       if (parentFragment == NULL)
	  subtrees = LispNil;
       else {
	  subtrees = get_frag_subtrees(parentFragment);
	  set_frag_subtrees(parentFragment,
			    nLispRemove(subtrees, (LispValue)fragment));
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
	   set_hashjointable((HashJoin)parentPlan, hashtable);
	   set_hashjointablekey((HashJoin)parentPlan, hashTableMemoryKey);
	   set_hashjointablesize((HashJoin)parentPlan, hashTableMemorySize);
	   set_hashdone((HashJoin)parentPlan, true);
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

	       tempRelDescs = get_temprelDescs((ScanTemps)plan);
	       foreach (y, tempRelDescs) {
		   tempreldesc = (Relation)CAR(y);
		   ReleaseTmpRelBuffers(tempreldesc);
		   if (FileNameUnlink(
			  relpath((char*)&(tempreldesc->rd_rel->relname))) < 0)
		       elog(WARN, "ExecEndScanTemp: unlink: %m");
		}
	     }
	   if (parentPlan == NULL /* WWW && nparallel == 1 */)
	      /* in this case the whole plan has been finished */
	      fraglist = nLispRemove(fraglist, (LispValue)fragment);
	   else {
	      /* -----------------
	       *  make a ScanTemps node to let the parent collect the tuples
	       *  from a set of temporary relations
	       * -----------------
	       */
	      tempRelationDescList = 
			ProcGroupLocalInfoP[groupid].resultTmpRelDescList;
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
						 (LispValue)tempRelationDesc);
		 }
	      scantempNode = RMakeScanTemps();
	      set_qptargetlist((Plan)scantempNode, get_qptargetlist(plan));
	      set_temprelDescs(scantempNode, tempRelationDescList);
	      if (parentPlan == NULL) {
		 set_frag_root(fragment, (Plan)scantempNode);
		 set_frag_subtrees(fragment, LispNil);
		 set_fragment((Plan)scantempNode,-1);
					    /*means end of parallelism */
		 set_frag_is_inprocess(fragment, false);
		 set_frag_iorate(fragment, 0.0);
		}
	      else {
	      if (plan == (Plan)get_lefttree(parentPlan)) {
		 set_lefttree(parentPlan, (PlanPtr)scantempNode);
		}
	      else {
		 set_righttree(parentPlan, (PlanPtr)scantempNode);
		}
	      set_fragment((Plan)scantempNode, get_fragment(parentPlan));
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
