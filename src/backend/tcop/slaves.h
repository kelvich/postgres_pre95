/* ----------------------------------------------------------------
 *   FILE
 *	slaves.h
 *	
 *   DESCRIPTION
 *	macros and definitions for parallel backends
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef SlavesIncluded
#define SlavesIncluded
#include "utils/rel.h"
#include "nodes/plannodes.h"
#include "executor/x_shmem.h"

extern int MyPid;
#define IsMaster	(MyPid == -1)

extern int ParallelExecutorEnableState;
#define ParallelExecutorEnabled()	ParallelExecutorEnableState
extern int NumberSlaveBackends;
#define GetNumberSlaveBackends()	NumberSlaveBackends

struct mastercommunicationdata {
    long	data[2]; /* long for now, may be replaced by some union */
    M1Lock	m1lock; /* one producer multiple consumer lock */
};
typedef struct mastercommunicationdata MasterCommunicationData;

struct slavecommunicationdata {
    long	data; /* long for now, may be replaced by some union */
#ifdef HAS_TEST_AND_SET
    slock_t	lock;	/* mutex lock */
#endif
};
typedef struct slavecommunicationdata SlaveCommunicationData;

/* slave info will stay in shared memory */
struct slaveinfo {
    int		unixPid;		/* the unix process id -- for sending
					   signals */
    int		groupId;		/* Id of the process group this slave
					   belongs to */
    int		groupPid;		/* virtual pid within the process 
					   group */
    bool	isAddOnSlave;		/* true if is add-on slave */
    Relation	resultTmpRelDesc;	/* the reldesc of the tmp relation
					   that holds the result of this
					   slave backend */
    SlaveCommunicationData	comdata;	/* communication data area */
};
typedef struct slaveinfo SlaveInfoData;
typedef SlaveInfoData *SlaveInfo;
extern SlaveInfo SlaveInfoP;

struct slavelocalinfo {
    int			nparallel;	/* degree of parallelism */
    int			startpage;	/* heap scan starting page number */
    bool		paradjpending;	/* parallelism adjustment pending */
    int			paradjpage;   /* page on which to adjust parallelism */
    int			newparallel;  /* new page skip */
    bool		isworking; /* true if the slave is working */
    HeapScanDesc	heapscandesc; /* heap scan descriptor */
    IndexScanDesc	indexscandesc; /* index scan descriptor */
};
typedef struct slavelocalinfo SlaveLocalInfoData;
extern SlaveLocalInfoData SlaveLocalInfoD;

/* pgroupinfo will stay in shared memory */
struct pgroupinfo {
    enum { IDLE, FINISHED, WORKING }
		status;		/* status of the process group */
    List	queryDesc;	/* querydesc for the process group */
    int		countdown;	/* countdown of # of proc in progress */
    int		nprocess;	/* # of processes in group */
#ifdef sequent
    slock_t	lock;		/* for synchronization when dec. countdown */
#endif
};
typedef struct pgroupinfo ProcGroupInfoData;
typedef ProcGroupInfoData *ProcGroupInfo;
extern ProcGroupInfo ProcGroupInfoP;

/* the following structs will be kept in master's memory only */
struct procnode {
    int			pid;
    struct procnode	*next;
};
typedef struct procnode ProcessNode;
extern int NumberOfFreeSlaves;

struct lpgroupinfo {
    int			id;
    Fragment		fragment;
    ProcessNode		*memberProc;
    MemoryHeader	groupSMQueue;
    struct lpgroupinfo	*nextfree;
};
typedef struct lpgroupinfo ProcGroupLocalInfoData;
typedef ProcGroupLocalInfoData *ProcGroupLocalInfo;
extern ProcGroupLocalInfo ProcGroupLocalInfoP;
extern int getFreeProcGroup();
extern void addSlaveToProcGroup();
extern int getFinishedProcGroup();
extern void freeProcGroup();
extern void wakeupProcGroup();
extern void signalProcGroup();
extern void ProcGroupSMBeginAlloc();
extern void ProcGroupSMEndAlloc();
extern char *ProcGroupSMAlloc();
extern void ProcGroupSMClean();
extern void SlaveTmpRelDescInit();
extern char *SlaveTmpRelDescAlloc();
extern int getProcGroupMaxPage();

#define SIGPARADJ	SIGUSR1

#endif  TcopIncluded
