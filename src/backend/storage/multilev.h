/*
 * multilev.h -- multi level lock table consts/defs
 *
 * $Header$
 *
 * for single.c and multi.c and their clients
 */
#ifndef _INC_MULTILEV_
#define _INC_MULTILEV_

#include "storage/lock.h"

#define READ_LOCK  	2
#define WRITE_LOCK 	1

/* any time a small granularity READ/WRITE lock is set.  
 * Higher granularity READ_INTENT/WRITE_INTENT locks must
 * also be set.  A read intent lock is has value READ+INTENT.
 * in this implementation.
 */
#define NO_LOCK		0
#define INTENT		2
#define READ_INTENT	(READ_LOCK+INTENT)
#define WRITE_INTENT	(WRITE_LOCK+INTENT)

#define EXTEND_LOCK	5

#define SHORT_TERM	1
#define LONG_TERM	2
#define UNLOCK		0

#define N_LEVELS 3
#define RELN_LEVEL 0
#define PAGE_LEVEL 1
#define TUPLE_LEVEL 2
typedef int LOCK_LEVEL;

/* multi.c */

/*
 * function prototypes
 */
LockTableId InitMultiLevelLockm();
bool MultiLockReln ARGS((LockInfo linfo, LOCKT lockt));
bool MultiLockTuple ARGS((LockInfo linfo, ItemPointer tidPtr, LOCKT lockt));
bool MultiLockPage ARGS((LockInfo linfo, ItemPointer tidPtr, LOCKT lockt));
bool MultiReleaseReln ARGS((LockInfo linfo, LOCKT lockt));
bool MultiReleasePage ARGS((LockInfo linfo, ItemPointer tidPtr, LOCKT lockt));

bool MultiAcquire ARGS((
	TableId tableId, 
	LOCKTAG *tag, 
	LOCK_LEVEL level, 
	LOCKT lockt
));

bool MultiRelease ARGS((
	TableId tableId, 
	LOCKTAG *tag, 
	LOCK_LEVEL level, 
	LOCKT lockt
));


#endif _INC_MULTILEV_
