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
 *	The semaphore operations should be moved elsewhere
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "executor.h"
 RcsId("$Header$");

/* ----------------
 *	parallel state variables
 * ----------------
 */
bool    IsMaster;		/* true if this process is the master */
int     SlaveId;		/* int representing the slave id */

int 	*MasterProcessIdP;	/* master backend process id */
int 	*SlaveProcessIdsP;	/* array of slave backend process id's */
Pointer *SlaveQueryDescsP;	/* array of pointers to slave query descs */

/* --------------------------------
 *	SlaveMain is the main loop executed by the slave backends
 *
 *	Note: have to add code to communicate transaction information
 *	      in slaves and synchronize aborts and stuff.
 * --------------------------------
 */
void
SlaveMain()
{
    List queryDesc;
    
    /* ----------------
     *	XXX have to initialize the transaction exception handlers
     *  here so that aborts and signals are processed correctly..
     * ----------------
     */
    
    for(;;) {
	/* ----------------
	 *  The master V's on the start semaphore after
	 *  placing the plan fragment in shared memory.
	 *  Meanwhile slaves wait on their start semaphore
	 *  until the master signals them.
	 * ----------------
	 */
	P_Start(SlaveId);

	/* ----------------
	 *  get the query descriptor to execute.
	 *  If the query desc is NULL, then we assume the master
	 *  backend has finished execution.
	 * ----------------
	 */
	queryDesc = (List) SlaveQueryDescsP[SlaveId];
	if (queryDesc == NULL)
	    exitpg();

	/* ----------------
	 *  process the query descriptor
	 * ----------------
	 */
	ProcessQueryDesc(queryDesc);
	
	/* ----------------
	 *  when the slave finishes, it signals the master
	 *  backend by V'ing on the master's finished semaphore.
	 *
	 *  Since the master started by placing nslaves locks
	 *  on the semaphore, the semaphore will be decremented
	 *  each time a slave finishes.  the last slave to finish
	 *  should bring the finished count to -1;
	 * ----------------
	 */
	V_Finished();
    }
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
    
    /* ----------------
     *	initialize Start[] and Finished semaphores to 0
     * ----------------
     */
    for (i=0; i<nslaves; i++)
	I_Start(i);
    I_Finished();
    
    /* ----------------
     *	allocate area in shared memory for process ids and
     *  communication mechanisms.  All backends share these pointers.
     * ----------------
     */
    MasterProcessIdP =  (int *)     ExecSMHighAlloc(sizeof(int));
    SlaveProcessIdsP =  (int *)     ExecSMHighAlloc(nslaves * sizeof(int));
    SlaveQueryDescsP =  (Pointer *) ExecSMHighAlloc(nslaves * sizeof(Pointer));
    
    /* ----------------
     *	save master backend's process id.
     * ----------------
     */
    (*MasterProcessIdP) = getpid();
    
    /* ----------------
     *	fork several slave processes and save the process id's
     * ----------------
     */
    for (i=0; i<nslaves; i++) {
	if ((p = fork()) != 0) {
	    IsMaster = true;
	    SlaveProcessIdsP[i] = p;
	    SlaveQueryDescsP[i] = NULL;
	} else {
	    SlaveId = i;
	    IsMaster = false;
	}
    }

    /* ----------------
     *	now IsMaster is true in the master backend and is false
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


/* --------------------------------
 *	SlaveBackendsAbort tells the slave backends to abandon
 *	their task and wait for new instructions.
 * --------------------------------
 */
void
SlaveBackendsAbort()
{
    return;
}

