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

/* ----------------
 *	parallel state variables
 * ----------------
 */
int     MyPid;			/* int representing the process id */

int 	*MasterProcessIdP;	/* master backend process id */
int 	*SlaveProcessIdsP;	/* array of slave backend process id's */
Pointer *SlaveQueryDescsP;	/* array of pointers to slave query descs */
Pointer *SlaveRetStateP;	/* array of pointers to slave return states */
int     *SlaveAbortFlagP;	/* flag set during a transaction abort */

TransactionState SharedTransactionState; /* current transaction info */

#define CONDITION_NORMAL 0
#define CONDITION_ABORT  1

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
	    p = SlaveProcessIdsP[i];
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
	I_SharedMemoryMutex();
	ExecSMClean();
	
	SLAVE_elog(DEBUG, "Master Backend signaling slaves abort");
	V_Abort();

	SLAVE_elog(DEBUG, "Master Backend waiting for slave aborts");
	P_Finished();

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
	V_Finished();
    }
}

/* --------------------------------
 *	SlaveMain is the main loop executed by the slave backends
 * --------------------------------
 */
void
SlaveMain()
{
    List queryDesc;
    int i;
    int nproc;

    /* ----------------
     *  before we begin processing we have to register a SIGHUP
     *  handler so that if a problem occurs in one slave backend,
     *  all the backends resynchronize.
     * ----------------
     */
    if (setjmp(SlaveRestartPoint) != 0) {
	SlaveWarnings++;
	SLAVE1_elog(DEBUG, "Slave Backend %d SlaveBackendsAbort()",
		    MyPid);
	
	SlaveBackendsAbort();
	
	SLAVE1_elog(DEBUG, "Slave Backend %d SlaveBackendsAbort() done",
		    MyPid);
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

	/* ----------------
	 *  get the query descriptor to execute.
	 * ----------------
	 */
	nproc = 0;
	for (i=0; i<GetNumberSlaveBackends(); i++) {
	    nproc += get_parallel(QdGetPlan((List) SlaveQueryDescsP[i]));
	    if (MyPid < nproc) {
	        queryDesc = (List)CopyObject((List)SlaveQueryDescsP[i]);
		break;
	      }
	  }
	Assert(i < GetNumberSlaveBackends());

#if 0
	/* ----------------
	 *	for now, just test synchronization.
	 * ----------------
	 */
	srandom(MyPid);
	{
	    int sleeptime = 5 + (random() & 017);
	    
	    elog(DEBUG, "Slave Backend %d executing (ETA:%d)",
		 MyPid, sleeptime);
	    
	    sleep(sleeptime);
	    
	    elog(DEBUG, "Slave Backend %d finished", MyPid);
	}
#endif	
#if 0
	/* ----------------
	 *  If the query desc is NULL, then we assume the master
	 *  backend has finished execution.
	 * ----------------
	 */
	if (queryDesc == NULL)
	    exitpg();
#endif	
	/* ----------------
	 *  process the query descriptor
	 * ----------------
	 */
	if (queryDesc != NULL)
	    ProcessQueryDesc(queryDesc);

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
	V_Finished();
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
	
    SharedTransactionState = (TransactionState) ExecSMHighAlloc(nbytes);
	
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
    MasterProcessIdP =  (int *)     ExecSMHighAlloc(sizeof(int));
    SlaveProcessIdsP =  (int *)     ExecSMHighAlloc(nslaves * sizeof(int));
    SlaveAbortFlagP  =  (int *)     ExecSMHighAlloc(sizeof(int));
    SlaveQueryDescsP =  (Pointer *) ExecSMHighAlloc(nslaves * sizeof(Pointer));
    SlaveRetStateP =  (Pointer *) ExecSMHighAlloc(nslaves * sizeof(Pointer));

    /* ----------------
     *	move the transaction system state data into shared memory
     *  before we fork so that all backends share the transaction state.
     * ----------------
     */
    MoveTransactionState();
    
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
    MyPid = 0;
    
    for (i=1; i<nslaves; i++)
	if (IsMaster) {
	    if ((p = fork()) != 0) {
		SlaveProcessIdsP[i] = p;
		SlaveQueryDescsP[i] = NULL;
		SlaveRetStateP[i] = NULL;
	    } else {
		MyPid = i;
		
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
    if (! IsMaster)
	SlaveMain();

    /* ----------------
     *	here we're in the master and we've completed initialization
     *  so we return to the read..parse..plan..execute loop.
     * ----------------
     */
    return;
}
