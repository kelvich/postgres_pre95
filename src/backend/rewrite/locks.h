/* 
 * $Header$
 */

#define EventIsRetrieve		0x0 		/* %00000000 */
#define EventIsAppend		0x1		/* %00000001 */
#define EventIsDelete		0x2		/* %00000010 */
#define EventIsReplace		0x3		/* %00000011 */

#define LockIsRewrite		0x80		/* %10000000 */

#define LockIsActive		0x40		/* %01000000 */

#define DoReplaceCurrentOrNew	0x4		/* %00000100 */
#define DoExecuteProc		0x8		/* %00001000 */
#define DoOther			0x10		/* %00010000 */
#define DoInstead		0x20		/* %00100000 */
#define DoInsteadOther	        (DoOther | DoInstead)

#define LockEventMask		0x3		/* %00000011 */
#define LockSystemMask		0x80		/* %10000000 */
#define LockActionMask		0x1c		/* %00111100 */
#define LockActiveMask		0x40		/* %01000000 */

#define Event(lock)		(lock->lockType & LockEventMask)
#define Action(lock)		(lock->lockType & LockActionMask)
#define IsActive(lock)		(lock->lockType & LockActiveMask)
#define IsRewrite(lock)		(lock->lockType & LockIsRewrite )
#define IsInstead(lock)		(lock->lockType & DoInstead )

#define LockEventIsRetrieve(lock) 	(Event(lock)==EventIsRetrieve)
#define LockEventIsAppend(lock) 	(Event(lock)==EventIsAppend)
#define LockEventIsDelete(lock) 	(Event(lock)==EventIsDelete)
#define LockEventIsReplace(lock) 	(Event(lock)==EventIsReplace)

struct _LispValue *MatchRetrieveLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
char ThisLockWasTriggered ARGS((int, short, struct _LispValue *));
struct _LispValue *MatchLocks ARGS((char, struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *MatchReplaceLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *MatchAppendLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *MatchDeleteLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct Prs2LocksData *RelationGetRelationLocks ARGS((struct RelationData *));


