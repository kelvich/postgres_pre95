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
#include "executor/execshmem.h"

extern int MyPid;
#define IsMaster	(MyPid == -1)

extern int ParallelExecutorEnableState;
#define ParallelExecutorEnabled()	ParallelExecutorEnableState
extern int NumberSlaveBackends;
#define GetNumberSlaveBackends()	NumberSlaveBackends

/* slave info will stay in shared memory */
struct slaveinfo {
    int		unixPid;		/* the unix process id -- for sending
					   signals */
    int		groupId;		/* Id of the process group this slave
					   belongs to */
    int		groupPid;		/* virtual pid within the process 
					   group */
    bool	isAddOnSlave;		/* true if is add-on slave */
    bool	isDone;			/* true if slave has finished task and
					   is waiting for others to finish */
    Relation	resultTmpRelDesc;	/* the reldesc of the tmp relation
					   that holds the result of this
					   slave backend */
    int		curpage;		/* current page being scanned:
					   for communication with master */
#ifdef HAS_TEST_AND_SET
    slock_t	lock;			/* mutex lock for comm. with master */
#endif
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

typedef enum { IDLE, FINISHED, WORKING, PARADJPENDING }		ProcGroupStatus;
/* pgroupinfo will stay in shared memory */
struct pgroupinfo {
    ProcGroupStatus status;	/* status of the process group */
    List	queryDesc;	/* querydesc for the process group */
    int		countdown;	/* countdown of # of proc in progress */
    int		nprocess;	/* # of processes in group */
    SharedCounter scounter;	/* a shared counter for sync purpose */
    SharedCounter dropoutcounter; /* a counter for drop out slaves */
    M1Lock	m1lock; /* one producer multiple consumer lock for comm. */
    int         paradjpage;   /* page on which to adjust parallelism */
    int         newparallel;  /* new page skip */
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
    List		resultTmpRelDescList;
};
typedef struct lpgroupinfo ProcGroupLocalInfoData;
typedef ProcGroupLocalInfoData *ProcGroupLocalInfo;
extern ProcGroupLocalInfo ProcGroupLocalInfoP;

/* slaves.c */
void SendAbortSignals ARGS((void ));
void SlaveRestart ARGS((void ));
void SlaveBackendsAbort ARGS((void ));
void SlaveMain ARGS((void ));
void MoveTransactionState ARGS((void ));
void SlaveBackendsInit ARGS((void ));
int getFreeSlave ARGS((void ));
void freeSlave ARGS((int i ));
int getFreeProcGroup ARGS((int nproc ));
void addSlaveToProcGroup ARGS((int slave , int group , int groupid ));
void freeProcGroup ARGS((int gid ));
int getFinishedProcGroup ARGS((void ));
void wakeupProcGroup ARGS((int groupid ));
void signalProcGroup ARGS((int groupid , int sig ));
void ProcGroupSMBeginAlloc ARGS((int groupid ));
void ProcGroupSMEndAlloc ARGS((void ));
char *ProcGroupSMAlloc ARGS((int size ));
void ProcGroupSMClean ARGS((int groupid ));
void SlaveTmpRelDescInit ARGS((void ));
char *SlaveTmpRelDescAlloc ARGS((int size ));
int getProcGroupMaxPage ARGS((int groupid ));
int paradj_handler ARGS((void ));
int paradj_nextpage ARGS((int page , int dir ));

#define SIGPARADJ	SIGUSR1

#define NULLPAGE	-1
#define NOPARADJ	-2

#endif  TcopIncluded
