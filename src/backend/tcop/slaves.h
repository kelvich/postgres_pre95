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

extern int MyPid;
#define IsMaster	(MyPid == -1)

extern int ParallelExecutorEnableState;
#define ParallelExecutorEnabled()	ParallelExecutorEnableState

/* slave info will stay in shared memory */
struct slaveinfo {
    int		unixPid;		/* the unix process id -- for sending
					   signals */
    int		groupId;		/* Id of the process group this slave
					   belongs to */
    int		groupPid;		/* virtual pid within the process 
					   group */
    Relation	resultTmpRelDesc;	/* the reldesc of the tmp relation
					   that holds the result of this
					   slave backend */
};
typedef struct slaveinfo SlaveInfoData;
typedef SlaveInfoData *SlaveInfo;
extern SlaveInfo SlaveInfoP;

/* pgroupinfo will stay in shared memory */
struct pgroupinfo {
    enum { IDLE, FINISHED, WORKING }
		status;		/* status of the process group */
    List	queryDesc;	/* querydesc for the process group */
    int		countdown;	/* countdown of # of proc in progress */
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
    int			nmembers;
    struct lpgroupinfo	*nextfree;
};
typedef struct lpgroupinfo ProcGroupLocalInfoData;
typedef ProcGroupLocalInfoData *ProcGroupLocalInfo;
extern ProcGroupLocalInfo ProcGroupLocalInfoP;
extern int getFreeProcGroup();
extern int getFinishedProcGroup();
extern void freeProcGroup();
extern void wakeupProcGroup();

#endif  TcopIncluded
