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

bool
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

/* ----------------------------------------------------------------
 *	ParallelOptimize
 *	
 *	this analyzes the plan in the query descriptor and determines
 *	which fragments to execute based on available virtual
 *	memory resources...
 *	
 * ----------------------------------------------------------------
 */
static List
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
    Plan plan;

    fireFragmentList = LispNil;
    nfreeslaves = NumberOfFreeSlaves;
    foreach (x, fragmentlist) {
	fragment = (Fragment)CAR(x);
	plan = get_frag_root(fragment);
	if (!plan_is_parallelizable(plan)) {
	    parallel = 1;
	    elog(NOTICE, "nonparallelizable fragment, running sequentially\n");
	  }
	else {
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

/* ------------------------
 *	AdjustParallelism
 *
 *	dynamically adjust degrees of parallelism of the fragments that
 *	are already in process to take advantage of the extra processors
 * ------------------------
 */
static void
AdjustParallelism(pardelta, notgroupid)
int pardelta;
int notgroupid; /* do not adjust this proc group */
{
    int i, j;
    int slave;
    int max_curpage;
    int size;
    int oldnproc;

    Assert(pardelta != 0);
    SLAVE_elog(DEBUG, "master trying to adjust degrees of parallelism");
    for (i=0; i<GetNumberSlaveBackends(); i++) {
	/* ---------------
	 * for now we only adjust the parallelism of the
	 * first fragment in process, will become
	 * more elaborate later.
	 * ---------------
	 */
	if (i != notgroupid &&
	    ProcGroupInfoP[i].status == WORKING &&
	    get_fragment(QdGetPlan(ProcGroupInfoP[i].queryDesc)) >= 0)
	    break;
      };
    if (i == GetNumberSlaveBackends()) {
	SLAVE_elog(DEBUG, "master finds no fragment to adjust parallelism to");
	return;
      }
    SLAVE1_elog(DEBUG, "master sending signal to process group %d", i);
    signalProcGroup(i, SIGPARADJ);
    max_curpage = getProcGroupMaxPage(i);
    SLAVE1_elog(DEBUG, "master gets maxpage = %d", max_curpage);
    oldnproc = ProcGroupInfoP[i].nprocess;
    if (max_curpage == NOPARADJ) {
	/* --------------------------
	 *  forget about adjustment to parallelism
	 *  in this case -- the fragment is almost finished
	 * ---------------------------
	 */
	SLAVE_elog(DEBUG, "master changes mind on adjusting parallelism");
	ProcGroupInfoP[i].paradjpage = NOPARADJ;
        OneSignalM(&(ProcGroupInfoP[i].m1lock), oldnproc);
	return;
      }
    ProcGroupInfoP[i].paradjpage = max_curpage + 1; 
				   /* page on which to adjust par. */
    if (pardelta > 0) {
        ProcGroupInfoP[i].nprocess += pardelta;
        ProcGroupInfoP[i].scounter.count = ProcGroupInfoP[i].nprocess;
        ProcGroupInfoP[i].newparallel = ProcGroupInfoP[i].nprocess;
        SLAVE2_elog(DEBUG,
		    "master signals waiting slaves with adjpage=%d,newpar=%d",
	            ProcGroupInfoP[i].paradjpage,ProcGroupInfoP[i].newparallel);
        OneSignalM(&(ProcGroupInfoP[i].m1lock), oldnproc);
        set_frag_parallel(ProcGroupLocalInfoP[i].fragment,
		          get_frag_parallel(ProcGroupLocalInfoP[i].fragment)+
		          pardelta);
        ProcGroupSMBeginAlloc(i);
        size = sizeofTmpRelDesc(QdGetPlan(ProcGroupInfoP[i].queryDesc));
        for (j=0; j<pardelta; j++) {
	    if (NumberOfFreeSlaves == 0) {
		elog(WARN, 
		     "trying to adjust to too much parallelism: out of slaves");
	      }
	    slave = getFreeSlave();
	    SLAVE2_elog(DEBUG, "master adding slave %d to procgroup %d", 
			slave, i);
            SlaveInfoP[slave].resultTmpRelDesc = 
					(Relation)ProcGroupSMAlloc(size);
            addSlaveToProcGroup(slave, i, oldnproc+j);
	    V_Start(slave);
          }
        ProcGroupSMEndAlloc();
      }
    else {
        ProcGroupInfoP[i].newparallel = ProcGroupInfoP[i].nprocess + pardelta;
        SLAVE2_elog(DEBUG,
		    "master signals waiting slaves with adjpage=%d,newpar=%d",
	            ProcGroupInfoP[i].paradjpage,ProcGroupInfoP[i].newparallel);
	ProcGroupInfoP[i].dropoutcounter.count = -pardelta;
        OneSignalM(&(ProcGroupInfoP[i].m1lock), oldnproc);
      }
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
	* ------------
	*/
       if (NumberOfFreeSlaves > 0 && AdjustParallelismEnabled) {
	    AdjustParallelism(NumberOfFreeSlaves, -1);
	 }

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
						    tempRelationDesc);
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
		AdjustParallelism(nfreeslave, groupid);
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
	   if (parentPlan == NULL && nparallel == 1)
	      /* in this case the whole plan has been finished */
	      fraglist = nLispRemove(fraglist, fragment);
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
						 tempRelationDesc);
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
		}
	      else {
	      if (plan == get_lefttree(parentPlan)) {
		 set_lefttree(parentPlan, (Plan)scantempNode);
		}
	      else {
		 set_righttree(parentPlan, (Plan)scantempNode);
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
