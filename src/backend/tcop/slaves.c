/* ----------------------------------------------------------------
 *   FILE
 *	slaves.c
 *	
 *   DESCRIPTION
 *	slave backend management routines
 *
 *   INTERFACE ROUTINES
 *	SlaveMain()
 *	SlaveBackendsInit()
 *	SlaveBackendsAbort()
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include <signal.h>
#include <setjmp.h>

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) tcopdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "tcop/tcopdebug.h"
#include "tcop/slaves.h"

#include "access/xact.h"
#include "utils/log.h"
#include "catalog/syscache.h"
#include "executor/execdesc.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"

/* ----------------
 *	parallel state variables
 * ----------------
 */
/*
 *	local data structures
 */
extern int MyPid; /* int representing the process id, defined in execipc.c */
static ProcessNode *SlaveArray, *FreeSlaveP;
int NumberOfFreeSlaves;
ProcGroupLocalInfo	ProcGroupLocalInfoP; /* process group local info */
static ProcGroupLocalInfo FreeProcGroupP;
extern SlaveLocalInfoData SlaveLocalInfoD;  /* defined in execipc.c */
extern int AdjustParallelismEnabled;
FILE *StatFp;
static bool RestartForParAdj = false;  /* indicating that the longjmp to
					  SlaveRestartPoint is for paradj */
static List QueryDesc;

/*
 *	shared data structures
 */
int 	*MasterProcessIdP;	/* master backend process id */
int	*SlaveAbortFlagP;	/* flag set during a transaction abort */
extern SlaveInfo	SlaveInfoP;	/* slave backend info */
extern ProcGroupInfo  ProcGroupInfoP; /* process group info */
                                      /* defined in execipc.c to make
                                         postmaster happy */

TransactionState SharedTransactionState; /* current transaction info */

#define CONDITION_NORMAL	0
#define CONDITION_ABORT		1

/* --------------------------------
 *	SendAbortSignals
 *
 *	This sends a SIGHUP to every other backend in order
 *	to cause them to preform their abort processing when
 *	we discover a reason to abort the current transaction.
 * --------------------------------
 */
void
SendAbortSignals()    
{
    int nslaves;		/* number of slaves */
    int i;			/* counter */
    int p;			/* process id */

#ifdef TCOP_SLAVESYNCDEBUG    
    if (IsMaster)
	elog(DEBUG, "Master Backend sending abort signals");
    else
	elog(DEBUG, "Slave Backend %d sending abort signals", MyPid);
#endif TCOP_SLAVESYNCDEBUG    

    nslaves = GetNumberSlaveBackends();
    for (i=0; i<nslaves; i++)
	if (i != MyPid) {
	    p = SlaveInfoP[i].unixPid;
	    if (kill(p, SIGHUP) != 0) {
		fprintf(stderr, "signaling slave %d (pid %d): ", i, p);
		perror("kill");
	    }
	}
    
    if (! IsMaster) {
	p = (*MasterProcessIdP);
	if (kill(p, SIGHUP) != 0) {
	    fprintf(stderr, "signaling master (pid %d): ", p);
	    perror("kill");
	}
    }
}

/* --------------------------------
 *	SlaveRestart
 *
 *	This is the signal / exception handler for the slave
 *	backends.  It causes processing to resume at the top
 *	of SlaveMain(), right before SlaveBackendsAbort().
 * --------------------------------
 */
jmp_buf	SlaveRestartPoint;
int	SlaveWarnings = 0;

void
SlaveRestart()
{
    SLAVE1_elog(DEBUG, "Slave Backend %d *** SlaveRestart ***", MyPid);

    longjmp(SlaveRestartPoint, 1);
}

/* --------------------------------
 *	SlaveBackendsAbort
 *
 *	this is called in each process when trouble arises.
 * --------------------------------
 */
void
SlaveBackendsAbort()
{
    int nslaves;		/* number of slaves */
    int i;			/* counter */
    
    /* ----------------
     *	if the abort flag is not set, then it means the problem
     *  occurred within our process.  So set the flag and tell
     *  all the other backends to abort.
     *
     *  only the aborting process does this:
     * ----------------
     */
    if ((*SlaveAbortFlagP) != CONDITION_ABORT) {
	(*SlaveAbortFlagP) = CONDITION_ABORT;
	
	SendAbortSignals();
    }

    /* ----------------
     *	all processes do this:
     * ----------------
     */
    if (IsMaster) {
	/* ----------------
	 *  in the master:
	 *
	 *	+ we record the aborted transaction,
	 *  	+ reinitialize the synchronization semaphores
	 *  	+ reinitialize the executor shared memory
	 *  	+ signal the slave backends to cleanup.
	 *
	 *  We then wait for all the slaves to finish doing their
	 *  cleanup and then clear the abort flag.
	 * ----------------
	 */
	SLAVE_elog(DEBUG, "Master Backend aborting current transaction");
	AbortCurrentTransaction();

	SLAVE_elog(DEBUG, "Master Backend reinitializing semaphores");
	nslaves = GetNumberSlaveBackends();
	for (i=0; i<nslaves; i++) {
	    I_Start(i);
	}
	I_Finished();
	
	SLAVE_elog(DEBUG, "Master Backend reinitializing shared memory");
	ExecSMInit();
	
	SLAVE_elog(DEBUG, "Master Backend signaling slaves abort");
	V_Abort();

	SLAVE_elog(DEBUG, "Master Backend waiting for slave aborts");
	P_FinishedAbort();

	SLAVE_elog(DEBUG, "Master Backend reinitializing abort semaphore");
	I_Abort();

	SLAVE_elog(DEBUG, "Master Backend abort processing finished");
	(*SlaveAbortFlagP) = CONDITION_NORMAL;

    } else {
	/* ----------------
	 *  the slave backends preform their cleanup processing
	 *  after the master backend records the abort.  This is
	 *  guaranteed because the slaves P on the semaphore which
	 *  isn't V'ed by the master until after its done aborting.
	 * ----------------
	 */
	SLAVE1_elog(DEBUG, "Slave Backend %d waiting to abort..",
		    MyPid);
	P_Abort();

	SLAVE1_elog(DEBUG, "Slave Backend %d processing abort..",
		    MyPid);
	SlaveAbortTransaction();

	SLAVE1_elog(DEBUG, "Slave Backend %d abort finished, signaling..",
		    MyPid);
	
	V_Abort();
	V_FinishedAbort();
    }
}

/* --------------------------------
 *	SlaveMain is the main loop executed by the slave backends
 * --------------------------------
 */
void
SlaveMain()
{
    int i;
    int groupid;
    extern int ShowExecutorStats;

    /* -----------------
     * set the flag to false in case the SIGPARADJ is received
     * -----------------
     */
    SlaveLocalInfoD.isworking = false;
    /* ------------------
     * set stat file pointer if required
     * ------------------
     */
    if (ShowExecutorStats) {
	char fname[30];
	sprintf(fname, "/usr/tmp/hong/slave%d.stat", MyPid);
        StatFp = fopen(fname, "w");
      }
    /* ----------------
     *  before we begin processing we have to register a SIGHUP
     *  handler so that if a problem occurs in one slave backend,
     *  all the backends resynchronize.
     * ----------------
     */
    if (setjmp(SlaveRestartPoint) != 0) {
	if (RestartForParAdj) {
	    /* restart for parallelism adjustment */
	    RestartForParAdj = false;
	  }
	else {
	    /* restart for transaction abort */
	    SlaveWarnings++;
	    SLAVE1_elog(DEBUG, "Slave Backend %d SlaveBackendsAbort()",
		        MyPid);
	
	    SlaveBackendsAbort();
	    SLAVE1_elog(DEBUG, "Slave Backend %d SlaveBackendsAbort() done",
		        MyPid);
	  }
      }	

    signal(SIGHUP, SlaveRestart);

    /* ----------------
     *	POSTGRES slave processing loop begins here
     * ----------------
     */
    SLAVE1_elog(DEBUG, "Slave Backend %d entering slave loop...", MyPid);
    
    for(;;) {
	/* ----------------
	 *  setup caches, lock tables and memory stuff just
	 *  as if we were running within a transaction.
	 * ----------------
	 */
	SlaveStartTransaction();
	
	/* ----------------
	 *  The master V's on the slave start semaphores after
	 *  placing the plan fragment in shared memory.
	 *  Meanwhile each slave waits on its start semaphore
	 *  until signaled by the master.
	 * ----------------
	 */
	SLAVE1_elog(DEBUG, "Slave Backend %d waiting...", MyPid);
	
	P_Start(MyPid);
	
	SLAVE1_elog(DEBUG, "Slave Backend %d starting task.", MyPid);

	if (ShowExecutorStats)
	    ResetUsage();
	/* ------------------
	 * initialize slave local info
	 * ------------------
	 */
	groupid = SlaveInfoP[MyPid].groupId;
	SlaveLocalInfoD.startpage = SlaveInfoP[MyPid].groupPid +
				    (SlaveInfoP[MyPid].isAddOnSlave ? 
				     ProcGroupInfoP[groupid].paradjpage : 0);
	SlaveLocalInfoD.nparallel = ProcGroupInfoP[groupid].nprocess;
	SlaveLocalInfoD.paradjpending = false;
	SlaveLocalInfoD.paradjpage = -1;
	SlaveLocalInfoD.newparallel = -1;
	SlaveLocalInfoD.heapscandesc = NULL;
	SlaveLocalInfoD.indexscandesc = NULL;
	SlaveLocalInfoD.isworking = true;

	/* ----------------
	 *  get the query descriptor to execute.
	 * ----------------
	 */
	QueryDesc = (List)CopyObject((List)ProcGroupInfoP[groupid].queryDesc);

	/* ----------------
	 *  process the query descriptor
	 * ----------------
	 */
	if (QueryDesc != NULL)
	    ProcessQueryDesc(QueryDesc);

	/* ---------------
	 * it is important to set isDone to true before
	 * SlaveCommitTransaction(), because it may
	 * free the memory of SlaveInfoP[MyPid].heapscandesc
	 * ---------------
	 */
	SlaveInfoP[MyPid].isDone = true;

	if (ShowExecutorStats)
	    ShowUsage();
	/* ----------------
	 *  clean caches, lock tables and memory stuff just
	 *  as if we were running within a transaction.
	 * ----------------
	 */
	SlaveCommitTransaction();
	    
	/* ----------------
	 *  when the slave finishes, it signals the master
	 *  backend by V'ing on the master's finished semaphore.
	 *
	 *  Since the master started by placing nslaves locks
	 *  on the semaphore, the semaphore will be decremented
	 *  each time a slave finishes.  the last slave to finish
	 *  should bring the finished count to 1;
	 * ----------------
	 */
	SLAVE1_elog(DEBUG, "Slave Backend %d task complete.", MyPid);
	SlaveLocalInfoD.isworking = false;
	V_Finished(groupid, &(ProcGroupInfoP[groupid].scounter), FINISHED);
    }
}

/* --------------------------------
 *	MoveTransactionState copies the transaction system's
 *	CurrentTransactionState information into permanent shared
 *	memory and then sets the transaction system's state
 *	pointer to the shared memory copy.  This is necessary to
 *	have the transaction system use the shared state in each
 *	of the parallel backends.
 * --------------------------------
 */
void
MoveTransactionState()
{
    int nbytes = sizeof(TransactionStateData);
    extern TransactionState CurrentTransactionState;
	
    bcopy(CurrentTransactionState, SharedTransactionState, nbytes);
    CurrentTransactionState = SharedTransactionState;
}
	
/* --------------------------------
 *	SlaveBackendsInit
 *
 *	SlaveBackendsInit initializes the communication structures,
 *	forks several "worker" backends and returns.
 *
 *	Note: this function only returns in the master
 *	      backend.  The worker backends run forever in
 *	      a separate execution loop.
 * --------------------------------
 */

void
SlaveBackendsInit()
{
    int nslaves;		/* number of slaves */
    int i;			/* counter */
    int p;			/* process id returned by fork() */
    int paradj_handler(); 	/* intr handler for adjusting parallelism */
    extern int ProcessAffinityOn; /* process affinity on flag */
    
    /* ----------------
     *  first initialize shared memory and get the number of
     *  slave backends.
     * ----------------
     */
    nslaves = GetNumberSlaveBackends();

    /* -----------------
     * the following calls to SearchSysCacheTuple() are total hacks
     * what they do is to pre-initialize all the caches so that
     * performance numbers will look better.  they can be removed at
     * any time.
     * -----------------
     */
    SearchSysCacheTuple(AMOPOPID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(AMOPSTRATEGY, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(ATTNAME, "");
    SearchSysCacheTuple(ATTNUM, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(INDEXRELID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(LANNAME, "");
    SearchSysCacheTuple(OPRNAME, "");
    SearchSysCacheTuple(OPROID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(PRONAME, "");
    SearchSysCacheTuple(PROOID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(RELNAME, "");
    SearchSysCacheTuple(RELOID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(TYPNAME, "");
    SearchSysCacheTuple(TYPOID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(AMNAME, "");
    SearchSysCacheTuple(CLANAME, "");
    SearchSysCacheTuple(INDRELIDKEY, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(INHRELID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(PRS2PLANCODE, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(RULOID, NULL, NULL, NULL, NULL);
    SearchSysCacheTuple(PRS2STUB, NULL, NULL, NULL, NULL);
    
    /* --------------------
     * set signal for dynamically adjusting degrees of parallelism
     * --------------------
     */
    if (AdjustParallelismEnabled)
        signal(SIGPARADJ, paradj_handler);

    /* ----------------
     *	initialize Start, Finished, and Abort semaphores
     * ----------------
     */
    for (i=0; i<nslaves; i++)
	I_Start(i);
    I_Finished();
    I_Abort();
    
    /* ----------------
     *	allocate area in shared memory for process ids and
     *  communication mechanisms.  All backends share these pointers.
     * ----------------
     */
    MasterProcessIdP = (int*)ExecSMReserve(sizeof(int));
    SlaveAbortFlagP = (int*)ExecSMReserve(sizeof(int));
    SlaveInfoP = (SlaveInfo)ExecSMReserve(nslaves * sizeof(SlaveInfoData));
    ProcGroupInfoP = (ProcGroupInfo)ExecSMReserve(nslaves *
						    sizeof(ProcGroupInfoData));
    SharedTransactionState = (TransactionState)ExecSMReserve(
						  sizeof(TransactionStateData));

    /* ----------------
     *	move the transaction system state data into shared memory
     *  before we fork so that all backends share the transaction state.
     * ----------------
     */
    MoveTransactionState();
    
    /* ----------------
     *	initialize executor shared memory
     * ----------------
     */
    ExecSMInit();
    /* ----------------
     *	initialize shared memory variables
     * ----------------
     */
    (*MasterProcessIdP) = getpid();
    (*SlaveAbortFlagP) = CONDITION_NORMAL;
    /* ----------------
     *	fork several slave processes and save the process id's
     * ----------------
     */
    MyPid = -1;
    
    for (i=0; i<nslaves; i++)
	if (IsMaster) {
	    if ((p = fork()) != 0) {
		if (p < 0) {
		    perror("fork");
		    exitpg(1);
		  }
		/* initialize shared data structures */
		SlaveInfoP[i].unixPid = p;
		SlaveInfoP[i].groupId = -1;
		SlaveInfoP[i].groupPid = -1;
		SlaveInfoP[i].resultTmpRelDesc = NULL;
#ifdef sequent
		S_INIT_LOCK(&(SlaveInfoP[i].lock));
		S_LOCK(&(SlaveInfoP[i].lock));
#endif
		ProcGroupInfoP[i].status = IDLE;
		ProcGroupInfoP[i].queryDesc = NULL;
		ProcGroupInfoP[i].scounter.count = 0;
		ProcGroupInfoP[i].dropoutcounter.count = 0;
#ifdef sequent
		S_INIT_LOCK(&(ProcGroupInfoP[i].scounter.exlock));
		S_INIT_LOCK(&(ProcGroupInfoP[i].dropoutcounter.exlock));
#endif
		ProcGroupInfoP[i].paradjpage = NULLPAGE;
		InitMWaitOneLock(&(ProcGroupInfoP[i].m1lock));
	    } else {
		MyPid = i;
#ifdef sequent
		if (ProcessAffinityOn && GetNumberSlaveBackends() <= 12) {
		    int prev;
		    /* ----------------------
		     * bind slave to a specific processor
		     * ----------------------
		     */
		    prev = tmp_affinity(MyPid);
		    if (prev == -1) {
			fprintf(stderr, "slave %d ", MyPid);
			perror("tmp_affinity");
		      }
		  }
#endif
		SLAVE1_elog(DEBUG, "Slave Backend %d alive!", MyPid);
	    }
	}
    
    /* ----------------
     *	now ExecIsMaster is true in the master backend and is false
     *  in each of the slaves.  In addition, each slave is branded
     *  with a slave id to identify it.  So, if we're a slave, we now
     *  get sent off to the labor camp, never to return..
     * ----------------
     */
    if (IsMaster) {
	/* initialize local data structures of the master */
	SlaveArray = (ProcessNode*)malloc(nslaves * sizeof(ProcessNode));
	FreeSlaveP = SlaveArray;
	NumberOfFreeSlaves = nslaves;
	for (i=0; i<nslaves; i++) {
	    SlaveArray[i].pid = i;
	    SlaveArray[i].next = SlaveArray + i + 1;
	  }
	SlaveArray[nslaves-1].next = NULL;
	ProcGroupLocalInfoP = (ProcGroupLocalInfo)malloc(nslaves *
						sizeof(ProcGroupLocalInfoData));
	FreeProcGroupP = ProcGroupLocalInfoP;
	for (i=0; i<nslaves; i++) {
	    ProcGroupLocalInfoP[i].id = i;
	    ProcGroupLocalInfoP[i].fragment = NULL;
	    ProcGroupLocalInfoP[i].memberProc = NULL;
	    ProcGroupLocalInfoP[i].nextfree = ProcGroupLocalInfoP + i + 1;
	    ProcGroupLocalInfoP[i].resultTmpRelDescList = LispNil;
	  }
	ProcGroupLocalInfoP[nslaves - 1].nextfree = NULL;
      }
    else {
	SlaveMain();
      }

    /* ----------------
     *	here we're in the master and we've completed initialization
     *  so we return to the read..parse..plan..execute loop.
     * ----------------
     */
    return;
}

/* ----------------------
 *	getFreeSlave
 *
 *	get a free slave backend from the FreeSlaveP queue
 *	also decrements NumberOfFreeSlaves
 * ----------------------
 */
int
getFreeSlave()
{
    ProcessNode *p;

    p = FreeSlaveP;
    FreeSlaveP = p->next;
    NumberOfFreeSlaves--;

    return p->pid;
}

/* ------------------------
 *	freeSlave
 *
 *	frees a slave to FreeSlaveP queue
 *	increments NumberOfFreeSlaves
 * ------------------------
 */
void
freeSlave(i)
int i;
{
    SlaveArray[i].next = FreeSlaveP;
    FreeSlaveP = SlaveArray + i;
    SlaveInfoP[i].groupId = -1;
    SlaveInfoP[i].groupPid = -1;
    NumberOfFreeSlaves++;
}

/* ------------------------
 *	getFreeProcGroup
 *
 *	get a free process group with nproc free slave processes
 * ------------------------
 */
int
getFreeProcGroup(nproc)
int nproc;
{
    ProcGroupLocalInfo p;
    ProcessNode *slavep;
    int i;
    int pid;

    p = FreeProcGroupP;
    FreeProcGroupP = p->nextfree;
    pid = getFreeSlave();
    SlaveInfoP[pid].groupId = p->id;
    SlaveInfoP[pid].groupPid = 0;
    SlaveInfoP[pid].isAddOnSlave = false;
    SlaveInfoP[pid].isDone = false;
    p->memberProc = SlaveArray + pid;
    slavep = p->memberProc;
    for (i=1; i<nproc; i++) {
	pid = getFreeSlave();
	SlaveInfoP[pid].groupId = p->id;
	SlaveInfoP[pid].groupPid = i;
        SlaveInfoP[pid].isAddOnSlave = false;
        SlaveInfoP[pid].isDone = false;
	slavep->next = SlaveArray + pid;
	slavep = slavep->next;
      }
    slavep->next = NULL;
    return p->id;
}

/* --------------------------
 *	addSlaveToProcGroup
 *
 *	add a free slave to an existing process group
 * --------------------------
 */
void
addSlaveToProcGroup(slave, group, groupid)
int slave;
int group;
int groupid;
{
    SlaveInfoP[slave].groupId = group;
    SlaveInfoP[slave].groupPid = groupid;
    SlaveInfoP[slave].isAddOnSlave = true;
    SlaveArray[slave].next = ProcGroupLocalInfoP[group].memberProc;
    ProcGroupLocalInfoP[group].memberProc = SlaveArray + slave;
}

/* -------------------------
 *	freeProcGroup
 *
 *	frees a process group and all the slaves in the group
 * -------------------------
 */
void
freeProcGroup(gid)
int gid;
{
    ProcessNode *p, *nextp;

    p=ProcGroupLocalInfoP[gid].memberProc;
    while (p != NULL) {
	nextp = p->next;
	freeSlave(p->pid);
	p = nextp;
      }
    ProcGroupInfoP[gid].status = IDLE;
    ProcGroupLocalInfoP[gid].fragment = NULL;
    ProcGroupLocalInfoP[gid].nextfree = FreeProcGroupP;
    ProcGroupLocalInfoP[gid].resultTmpRelDescList = LispNil;
    FreeProcGroupP = ProcGroupLocalInfoP + gid;
}

/* ----------------------------
 *	getFinishedProcGroup
 *
 *	walks the array of processes group and find the first
 *	process group with status = FINISHED or PARADJPENDING
 * -----------------------------
 */
int
getFinishedProcGroup()
{
    int i;

    for (i=0; i<GetNumberSlaveBackends(); i++) {
	if (ProcGroupInfoP[i].status == FINISHED ||
	    ProcGroupInfoP[i].status == PARADJPENDING)
	    return i;
      }
    return -1;
}

/* -------------------------------
 *	wakeupProcGroup
 *
 *	wake up the processes in process group
 * -------------------------------
 */
void
wakeupProcGroup(groupid)
int groupid;
{
    ProcessNode *p;

    for (p = ProcGroupLocalInfoP[groupid].memberProc;
	 p != NULL;
	 p = p->next) {
      V_Start(p->pid);
     }
}

/* ---------------------------------
 *	signalProcGroup
 *
 *	send a signal to a process group
 * ---------------------------------
 */
void
signalProcGroup(groupid, sig)
int groupid;
int sig;
{
    ProcessNode *p;

    for (p = ProcGroupLocalInfoP[groupid].memberProc;
	 p != NULL;
	 p = p->next) {
	kill(SlaveInfoP[p->pid].unixPid, sig);
      }
}

/* ------------------------------
 * the following routines are a specialized memory allocator for 
 * the process groups.  they only supposed to be called by the master
 * backend.  no mutex is done.
 * ------------------------------
 */
static char *CurrentSMSegmentStart;
static char *CurrentSMSegmentEnd;
static int CurrentSMGroupid;
static char *CurrentSMSegmentPointer;

/* --------------------------------
 *	ProcGroupSMBeginAlloc
 *
 *	begins shared memory allocation for process group
 * --------------------------------
 */
void
ProcGroupSMBeginAlloc(groupid)
int groupid;
{
    MemoryHeader mp;

    mp = ExecGetSMSegment();
    ProcGroupLocalInfoP[groupid].groupSMQueue = mp;
    mp->next = NULL;
    CurrentSMSegmentStart = mp->beginaddr;
    CurrentSMSegmentEnd =  CurrentSMSegmentStart + mp->size;
    CurrentSMGroupid = groupid;
    CurrentSMSegmentPointer = CurrentSMSegmentStart;
}

/* -------------------------------
 *	ProcGroupSMEndAlloc
 *
 *	ends shared memory allocation for process group
 *	frees the leftover memory from current memory segment
 * -------------------------------
 */
void
ProcGroupSMEndAlloc()
{
    int usedsize;
    MemoryHeader mp;

    usedsize = CurrentSMSegmentPointer - CurrentSMSegmentStart;
    mp = ProcGroupLocalInfoP[CurrentSMGroupid].groupSMQueue;
    CurrentSMSegmentStart=CurrentSMSegmentPointer=CurrentSMSegmentEnd = NULL;
    ExecSMSegmentFreeUnused(mp, usedsize);
}

/* --------------------------------
 *	ProcGroupSMAlloc
 *
 *	allocate shared memory within a process group
 *	if the current memory segment runs out, allocate a new segment
 * ---------------------------------
 */
char *
ProcGroupSMAlloc(size)
int size;
{
    MemoryHeader mp;
    char *retP;

    while (CurrentSMSegmentPointer + size > CurrentSMSegmentEnd) {
	mp = ExecGetSMSegment();
	if (mp == NULL)
	    elog(WARN, "out of executor shared memory, got to die.");
	mp->next = ProcGroupLocalInfoP[CurrentSMGroupid].groupSMQueue;
	ProcGroupLocalInfoP[CurrentSMGroupid].groupSMQueue = mp;
	CurrentSMSegmentStart = mp->beginaddr;
	CurrentSMSegmentEnd =  CurrentSMSegmentStart + mp->size;
	CurrentSMSegmentPointer = CurrentSMSegmentStart;
      }
    retP = CurrentSMSegmentPointer;
    CurrentSMSegmentPointer = (char*)LONGALIGN(CurrentSMSegmentPointer + size);
    return retP;
}
    
/* -------------------------------
 *	ProcGroupSMClean
 *
 *	frees the shared memory allocated for a process group
 * -------------------------------
 */
void
ProcGroupSMClean(groupid)
int groupid;
{
    MemoryHeader mp, nextp;

    mp = ProcGroupLocalInfoP[groupid].groupSMQueue;
    while (mp != NULL) {
	nextp = mp->next;
	ExecSMSegmentFree(mp);
	mp = nextp;
      }
}

/* -----------------------------
 * the following routines are special functions for copying reldescs to
 * shared memory
 * -----------------------------
 */
static char *SlaveTmpRelDescMemoryP;

/* -------------------------------
 *	SlaveTmpRelDescInit
 *
 *	initialize shared memory preallocated for temporary relation descriptors
 * -------------------------------
 */
void
SlaveTmpRelDescInit()
{
    SlaveTmpRelDescMemoryP = (char*)SlaveInfoP[MyPid].resultTmpRelDesc;
}

/* -------------------------------
 *	SlaveTmpRelDescAlloc
 *
 *	memory allocation for reldesc copying
 *	Note: there is no boundary checking, so had better pre-allocate
 *	enough memory!
 * -------------------------------
 */
char *
SlaveTmpRelDescAlloc(size)
int size;
{
    char *retP;

    retP = SlaveTmpRelDescMemoryP;
    SlaveTmpRelDescMemoryP = (char*)LONGALIGN(SlaveTmpRelDescMemoryP + size);

    return retP;
}

/* ---------------------------------
 *	getProcGroupMaxPage
 *
 *	find out the largest page number the slaves are scanning
 *	used only after SIGPARADJ signal has been sent to the
 *	process group.
 * ---------------------------------
 */
int
getProcGroupMaxPage(groupid)
int groupid;
{
    ProcessNode *p;
    int maxpage = NULLPAGE;
    int page;

    for (p = ProcGroupLocalInfoP[groupid].memberProc;
	 p != NULL;
	 p = p->next) {
#ifdef HAS_TEST_AND_SET
	S_LOCK(&(SlaveInfoP[p->pid].lock));
#endif
	page = SlaveInfoP[p->pid].curpage;
	if (page == NOPARADJ)
	    maxpage = NOPARADJ;
	if (maxpage < page && maxpage != NOPARADJ)
	    maxpage = page;
      }
    return maxpage;
}

/* ---------------------------------------
 *	paradj_handler
 *
 *	signal handler for dynamically adjusting degrees of parallelism
 *	XXX only handle heap scan now.
 * ---------------------------------------
 */
int
paradj_handler()
{
    BlockNumber curpage;
    HeapTuple curtuple;
    ItemPointer tid;
    int groupid;

    SLAVE1_elog(DEBUG, "slave %d got SIGPARADJ", MyPid);
    if (SlaveInfoP[MyPid].isDone) {
	/* -----------------------
	 * this means that the whole job is almost done
	 * no adjustment to parallelism should be made
	 * ------------------------
	 */
	SlaveInfoP[MyPid].curpage = NOPARADJ;
	curpage = NOPARADJ;
      }
    else
    if (!SlaveLocalInfoD.isworking || SlaveLocalInfoD.heapscandesc == NULL) {
	if (SlaveInfoP[MyPid].isAddOnSlave) {
	    SlaveInfoP[MyPid].curpage = SlaveLocalInfoD.startpage;
	    curpage = SlaveLocalInfoD.startpage;
	  }
	else {
	    SlaveInfoP[MyPid].curpage = NULLPAGE;
	    curpage = NULLPAGE;
	  }
      }
    else {
        curtuple = SlaveLocalInfoD.heapscandesc->rs_ctup;
        tid = &(curtuple->t_ctid);
        curpage = ItemPointerGetBlockNumber(tid);
        SlaveInfoP[MyPid].curpage = curpage;
      }
#ifdef HAS_TEST_AND_SET
    S_UNLOCK(&(SlaveInfoP[MyPid].lock));
#endif
    SLAVE2_elog(DEBUG, "slave %d sending back curpage = %d", MyPid, curpage);
    groupid = SlaveInfoP[MyPid].groupId;
    MWaitOne(&(ProcGroupInfoP[groupid].m1lock));
    SLAVE1_elog(DEBUG, "slave %d complete handshaking with master", MyPid);
    if (ProcGroupInfoP[groupid].paradjpage == NOPARADJ) {
	/* ----------------------
	 * this means that the master changed his/her mind
	 * no adjustment to parallelism will be done
	 * ----------------------
	 */
	return;
      }
    SlaveLocalInfoD.paradjpending = true;
    SlaveLocalInfoD.paradjpage = ProcGroupInfoP[groupid].paradjpage;
    SlaveLocalInfoD.newparallel = ProcGroupInfoP[groupid].newparallel;
    return;
}

/* ------------------------------------
 *	paradj_nextpage
 *
 *	check if parallelism adjustment point is reached, if so
 *	figure out and return the next page to scan.
 *	XXX only works for heapscan right now.
 * -------------------------------------
 */
int
paradj_nextpage(page, dir)
int page;
int dir;
{
    if (SlaveLocalInfoD.paradjpending) {
        if (page >= SlaveLocalInfoD.paradjpage) {
            SLAVE2_elog(DEBUG, "slave %d adjusting page skip to %d",
                        MyPid, SlaveLocalInfoD.newparallel);
            if (SlaveLocalInfoD.newparallel >= SlaveLocalInfoD.nparallel ||
                SlaveInfoP[MyPid].groupPid < SlaveLocalInfoD.newparallel) {
                SlaveLocalInfoD.nparallel = SlaveLocalInfoD.newparallel;
                SlaveLocalInfoD.paradjpending = false;
		if (dir < 0)
                    return 
			SlaveLocalInfoD.paradjpage-SlaveInfoP[MyPid].groupPid;
		else
                    return
			SlaveLocalInfoD.paradjpage+SlaveInfoP[MyPid].groupPid;
              }
            else {
                int groupid = SlaveInfoP[MyPid].groupId;
		Plan plan = QdGetPlan(QueryDesc);
		EState estate = get_state(plan);
		EndPlan(plan, estate);
                V_Finished(groupid, &(ProcGroupInfoP[groupid].dropoutcounter),
                           PARADJPENDING);
		RestartForParAdj = true;
		SlaveRestart();
              }
          }
      }
    else
	return NULLPAGE;
}
