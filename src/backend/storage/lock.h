/* $Header$ */

#ifndef _LOCK_H_
#define _LOCK_H_

#include "tmp/postgres.h"
#include "storage/itemptr.h"
#include "storage/spin.h"
#include "storage/procq.h"
#include "storage/backendid.h"
#include "utils/hsearch.h"

extern SPINLOCK LockMgrLock;
typedef int MASK;

#define INIT_TABLE_SIZE		100
#define MAX_TABLE_SIZE 		1000

/* ----------------------
 * The following defines are used to estimate how much shared
 * memory the lock manager is going to require.  
 * 
 * NBACKENDS - The number of concurrently running backends
 * NLOCKS_PER_XACT - The number of unique locks acquired in a transaction
 * NLOCKENTS - The maximum number of lock entries in the lock table.
 * ----------------------
 */
#define NBACKENDS 50
#define NLOCKS_PER_XACT 40
#define NLOCKENTS NLOCKS_PER_XACT*NBACKENDS

typedef int LOCK_TYPE;
typedef int LOCKT;
typedef int LockTableId;

/* MAX_LOCKTYPES cannot be larger than the bits in MASK */
#define MAX_LOCKTYPES 6

/*
 * MAX_TABLES corresponds to the number of spin locks allocated in
 * CreateSpinLocks() or the number of shared memory locations allocated
 * for lock table spin locks in the case of machines with TAS instructions.
 */
#define MAX_TABLES 2

#define INVALID_TABLEID 0

/*typedef struct LOCK LOCK; */


typedef struct ltag {
  oid			relId;
  oid			dbId;
  ItemPointerData	tupleId;
} LOCKTAG;

#define TAGSIZE (sizeof(LOCKTAG))

/* This is the control structure for a lock table.  It
 * lives in shared memory:
 *
 * tableID -- the handle used by the lock table's clients to
 *	refer to the table.
 *
 * nLockTypes -- number of lock types (READ,WRITE,etc) that
 *	are defined on this lock table
 *
 * conflictTab -- this is an array of bitmasks showing lock
 *	type conflicts. conflictTab[i] is a mask with the j-th bit
 *	turned on if lock types i and j conflict.
 *
 * prio -- each locktype has a priority, so, for example, waiting
 *	writers can be given priority over readers (to avoid
 *	starvation).
 *
 * masterlock -- synchronizes access to the table
 *
 */
typedef struct lockctl {
  LockTableId	tableId;
  int		nLockTypes;
  int		conflictTab[MAX_LOCKTYPES];
  int		prio[MAX_LOCKTYPES];
  SPINLOCK	masterLock;
} LOCKCTL;

/*
 * lockHash -- hash table on lock Ids,
 * xidHash -- hash on xid and lockId in case
 *	multiple processes are holding the lock
 * ctl - control structure described above.
 */
typedef struct ltable {
  HTAB		*lockHash;
  HTAB		*xidHash;
  LOCKCTL	*ctl;
} LOCKTAB;

/* -----------------------
 * A transaction never conflicts with its own locks.  Hence, if
 * multiple transactions hold non-conflicting locks on the same
 * data, private per-transaction information must be stored in the
 * XID table.  The tag is XID + shared memory lock address so that
 * all locks can use the same XID table.  The private information
 * we store is the number of locks of each type (holders) and the
 * total number of locks (nHolding) held by the transaction.
 *
 * NOTE: --
 * There were some problems with the fact that currently TransactionIdData
 * is a 5 byte entity and compilers long word aligning of structure fields.
 * If the 3 byte padding is put in front of the actual xid data then the
 * hash function (which uses XID_TAGSIZE when deciding how many bytes of a
 * struct to look at for the key) might only see the last two bytes of the xid.
 *
 * Clearly this is not good since its likely that these bytes will be the
 * same for many transactions and hence they will share the same entry in
 * hash table causing the entry to be corrupted.  For this long-winded
 * reason I have put the tag in a struct of its own to ensure that the
 * XID_TAGSIZE is computed correctly.  It used to be sizeof (SHMEM_OFFSET) +
 * sizeof(TransactionIdData) which != sizeof(XIDTAG).
 *
 * Finally since the hash function will now look at all 12 bytes of the tag
 * the padding bytes MUST be zero'd before use in hash_search() as they
 * will have random values otherwise.  Jeff 22 July 1991.
 * -----------------------
 */

typedef struct XIDTAG {
  SHMEM_OFFSET		lock;
  int			pid;
  TransactionIdData	xid;
} XIDTAG;

typedef struct XIDLookupEnt {
  /* tag */
  XIDTAG tag;

  /* data */
  int			holders[MAX_LOCKTYPES];
  int			nHolding;
  SHM_QUEUE		queue;

} XIDLookupEnt;

#define XID_TAGSIZE (sizeof(XIDTAG))

/*
 * lock information:
 *
 * tag -- uniquely identifies the object being locked
 * mask -- union of the conflict masks of all lock types
 *	currently held on this object.
 * waitProcs -- queue of processes waiting for this lock
 * holders -- count of each lock type currently held on the
 *	lock.
 * nHolding -- total locks of all types.
 */
typedef struct Lock {
  /* hash key */
  LOCKTAG		tag;

  /* data */
  int			mask;
  PROC_QUEUE		waitProcs;
  int			holders[MAX_LOCKTYPES];
  int			nHolding;
  int			activeHolders[MAX_LOCKTYPES];
  int			nActive;
} LOCK;

#define LockGetLock_nHolders(l) l->nHolders

#define LockDecrWaitHolders(lock, lockt) \
  lock->nHolding--; \
  lock->holders[lockt]--

#define LockLockTable() SpinAcquire(LockMgrLock);
#define UnlockLockTable() SpinRelease(LockMgrLock);

#define LOCK_PRINT(where,tag,type)

/*
#define LOCK_PRINT(where,tag,type)\
  elog(DEBUG, "%s: rel (%d) dbid (%d) tid (%d,%d) type (%d)\n",where, \
	 tag->relId, tag->dbId, \
	 ( (tag->tupleId.blockData.data[0] >= 0) ? \
		BlockIdGetBlockNumber(&tag->tupleId.blockData) : -1 ), \
	 tag->tupleId.positionData, \
	 type);
*/

/*
 * function prototypes
 */
void InitLocks();
void GrantLock ARGS((LOCK *lock, LOCKT lockt));
void LockDisable ARGS((int status));
bool LockAcquire ARGS((LockTableId tableId, LOCKTAG *lockName, LOCKT lockt));
bool LockRelease ARGS((LockTableId tableId, LOCKTAG *lockName, LOCKT lockt));
bool LockReleaseAll ARGS((LockTableId tableId, SHM_QUEUE *lockQueue));
bool LockingDisabled();
LockTableId LockTabRename ARGS((LockTableId tableId));
int LockShmemSize();

int WaitOnLock ARGS((
	LOCKTAB *ltable, 
	LockTableId tableId, 
	LOCK *lock, 
	LOCKT lockt
));

int LockResolveConflicts ARGS((
	LOCKTAB *ltable,
	LOCK *lock,
	LOCKT lockt,
	TransactionId xid,
	int pid
));

LockTableId LockTabInit ARGS((
	char *tabName, 
	MASK *conflictsP, 
	int *prioP, 
	int ntypes
));

#endif /* ndef _LOCK_H_ */
