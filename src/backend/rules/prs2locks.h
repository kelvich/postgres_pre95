/*
 * FILE:
 *   prs2locks.h
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   include stuff for PRS2 locks.
 *   This file contain only the definition for the prs2Locks, so that
 *   it can be easily included in "htup.h".
 *   For declarations of routines that manipulate locks, see
 *   "prs2.h"
 */

/*
 * Include this file only once...
 */
#ifndef Prs2LocksIncluded
#define Prs2LocksIncluded
#define PRS2LOCKS_H

#include "tmp/postgres.h"
#include "access/attnum.h"

#define PRS2_DEBUG 1

/*==================================================================
 * PRS2 LOCKS
 *==================================================================
 */

/*------------------------------------------------------------------
 * event types: the types of event that can cause rule activations...
 *------------------------------------------------------------------
 */
typedef char EventType;
#define EventTypeRetrieve	'R'
#define EventTypeReplace	'U'
#define EventTypeAppend		'A'
#define EventTypeDelete		'D'
#define EventTypeInvalid	'*'

/*------------------------------------------------------------------
 * Action types. These are the possible actions that can be defined
 * in the 'do' part of a rule.
 * Currently the actiosn suuported are:
 * ActionTypeRetrieveValue: the action part of the rule is a
 *   'retrieve (expression) where ....'. (a 'retrieve  into relation'
 *   query is NOT of this type!
 *   NOTE: under the current implementation, if such an action is
 *   specified in the action part of a rule, no other action can
 *   be specified!
 * ActionTypeReplaceCurrent: the action is a 'replace CURRENT(x=..)'
 *   NOTE: under the current implementation, if such an action is
 *   specified in the action part of a rule, no other action can
 *   be specified!
 * ActionTypeReplaceNew: the action is a 'replace NEW(x=..)'
 *   NOTE: under the current implementation, if such an action is
 *   specified in the action part of a rule, no other action can
 *   be specified!
 * ActionTypeOther: Any other action... (including 'retrieve into...')
 *------------------------------------------------------------------
 */
typedef char ActionType;
#define ActionTypeRetrieveValue		'r'
#define ActionTypeReplaceCurrent	'u'
#define ActionTypeReplaceNew		'U'
#define ActionTypeOther			'o'
#define ActionTypeInvalid		'*'

/*------------------------------------------------------------------
 * Plan numbers for various rule plans (as stored in the system catalogs)
 *------------------------------------------------------------------
 */
typedef uint16 Prs2PlanNumber;

/* #define QualificationPlanNumber		((Prs2PlanNumber)0) */
#define ActionPlanNumber		((Prs2PlanNumber)1)

/*------------------------------------------------------------------
 * Used to distinguish between 'old' and 'new' tuples...
 *------------------------------------------------------------------
 */
#define PRS2_OLD_TUPLE 1
#define PRS2_NEW_TUPLE 2

/*------------------------------------------------------------------
 * Types of locks..
 *
 * We distinguish between two types of rules: the 'backward' & 'forward'
 * chaning rules. The 'backward' chaining rules are the ones that
 * calculate a value for the current tuple, like:
 *    on retrieve to EMP.salary where EMP.name = "mike"
 *    do instead retrieve (salary=1000)
 * or
 *    on append to EMP where EMP.age<25
 *    do replace CURRENT(salary=2000)
 *
 * The forward chaining rules are the ones that do some action but do not
 * update the current tuple, like:
 *    on replace to EMP.salary where EMP.name = "mike"
 *    do replace EMP(salary=NEW.salary) where EMP.name = "john"
 * or
 *    on retrieve to EMP.age where EMP.name = "mike"
 *    do append TEMP(username = user(), age = OLD.age)
 *
 * The first type of rules gets a `LockTypeXXXWrite' lock (where XXX is
 * the event specified in the "on ..." clause and can be one of
 * retrieve, append, delete or replace) and the forward chinign rules
 * get locks of the form `LockTypeXXXAction'
 *
 * There is also the LockTypeRetrieveRelation, put by 'view' rules, 
 * i.e. rules of the form
 *	ON retrieve to relation
 *      DO [ instead ] retrieve .... <some other tuples> ...
 * These rules are used to implement views.
 *
 * NOTE: as we have 2 rule systems (tuple-level & query rewrite)
 * we use different lock types for each one of them.
 * Otherwise it wouldn't be possible to distinguish whether a rule
 * is a tuple-level rule or of query-rewrite flavor.
 *
 *------------------------------------------------------------------
 */
typedef char Prs2LockType;

#define LockTypeInvalid			((Prs2LockType) '*')

/*--- TUPLE LEVEL LOCK TYPES --------------------------------------*/
#define LockTypeTupleRetrieveAction		((Prs2LockType) '1')
#define LockTypeTupleAppendAction		((Prs2LockType) '2')
#define LockTypeTupleDeleteAction		((Prs2LockType) '3')
#define LockTypeTupleReplaceAction		((Prs2LockType) '4')
#define LockTypeTupleRetrieveWrite		((Prs2LockType) '5')
#define LockTypeTupleAppendWrite		((Prs2LockType) '6')
#define LockTypeTupleDeleteWrite		((Prs2LockType) '7')
#define LockTypeTupleReplaceWrite		((Prs2LockType) '8')
#define LockTypeTupleRetrieveRelation		((Prs2LockType) '9')

/*--- QUERY REWRITE LOCK TYPES ------------------------------------*/
#define LockTypeRetrieveAction		((Prs2LockType) 'r')
#define LockTypeAppendAction		((Prs2LockType) 'a')
#define LockTypeDeleteAction		((Prs2LockType) 'd')
#define LockTypeReplaceAction		((Prs2LockType) 'u')
#define LockTypeRetrieveWrite		((Prs2LockType) 'R')
#define LockTypeAppendWrite		((Prs2LockType) 'A')
#define LockTypeDeleteWrite		((Prs2LockType) 'D')
#define LockTypeReplaceWrite		((Prs2LockType) 'U')
#define LockTypeRetrieveRelation	((Prs2LockType) 'V')

/*------------------------------------------------------------------
 * Every single lock (`Prs2OneLock') has the following fields:
 *	ruleId:		OID of the rule
 *	lockType:	the type of locks
 *	attributeNumber:the attribute that is locked. A value
 *			equal to 'InvalidAttributeNumber' means that
 *                      all the attributes of the tuple are locked.
 *	planNumber:	the plan number of the plan (which is stored
 *			in the system catalogs) that must be executed
 *                      if the rule is activated. This is usually
 *                      equal to 'ActionPlanNumber', but we might add
 *                      others as well when we'll implement a 
 *                      more complex locking scheme.
 *
 * Every tuple can have more than one rule locks, so we need
 * a structure 'Prs2LocksData' to hold them.
 *   	numberOfLocks:	the number of entries in the 'locks' array.
 *	locks[]:	A *VARIABLE LENGTH* array with 0 or more
 *                      entries of type Prs2OneLock.
 *------------------------------------------------------------------
 */
typedef struct Prs2OneLockData {
    ObjectId		ruleId;	
    Prs2LockType	lockType;
    AttributeNumber	attributeNumber;
    Prs2PlanNumber		planNumber;
} Prs2OneLockData;

typedef Prs2OneLockData *Prs2OneLock;

typedef struct Prs2LocksData {
    int			numberOfLocks;
    Prs2OneLockData	locks[1];	/* XXX VARIABLE LENGTH DATA */
} Prs2LocksData;

typedef Prs2LocksData	*RuleLock;


/*------------------------------------------------------------------
 * INVALID RULE LOCK:
 *
 * This is an illegal value for a (main memory representation) of a
 * rule lock. You must not confuse it with the "empty" lock, i.e.
 * the lock: "(numOfLocs: 0)"::lock.
 *
 * However note that (as even an empty rule lock is 4 bytes long)
 * in order to save disk space, when we 
 * store an empty lock in the disk, we use an InvalidItemPointer
 *
 * NOTE: `RuleLockIsValid' only applies for the main memory
 * representation of a lock!
 */
#define InvalidRuleLock		((RuleLock) NULL)
#define RuleLockIsValid(x)    PointerIsValid(x)

/*------------------------------------------------------------------
 * #defines to access/set Prs2Lock data...
 * It is highly recommended that these routines are used instead
 * of directly manipulating the various structures invloved..
 *------------------------------------------------------------------
 */

#define prs2OneLockGetRuleId(l)			((l)->ruleId)
#define prs2OneLockGetLockType(l)		((l)->lockType)
#define prs2OneLockGetAttributeNumber(l)	((l)->attributeNumber)
#define prs2OneLockGetPlanNumber(l)		((l)->planNumber)

#define prs2OneLockSetRuleId(l, x)		((l)->ruleId = (x))
#define prs2OneLockSetLockType(l, x)		((l)->lockType = (x))
#define prs2OneLockSetAttributeNumber(l, x)	((l)->attributeNumber = (x))
#define prs2OneLockSetPlanNumber(l, x)		((l)->planNumber = (x))

/*------------------------------------------------------------------
 * prs2LockSize
 *    return the size needed for a 'Prs2LocksData' structure big enough
 *    to hold 'n' of 'Prs2OneLockData' structures...
 */
#define prs2LockSize(n) (sizeof(Prs2LocksData) \
			+ ((n)-1)*sizeof(Prs2OneLockData))

/*------------------------------------------------------------------
 * prs2GetNumberOfLocks
 *    return the number of locks contained in a 'RuleLock' structure.
 */
#define prs2GetNumberOfLocks(x)	((x)==NULL ? 0 : (x)->numberOfLocks)

/*------------------------------------------------------------------
 * prs2RuleLockIsEmpty
 * 	return true if this is an empty rule lock.
 */
#define prs2RuleLockIsEmpty(l)	(prs2GetNumberOfLocks(l) == 0)

/*=================================================================
 *
 * VARIOUS SYSTEM RELATION CONSTANTS
 *
 *=================================================================
 */

/*----
 * pg_type.oid for the "lock" type
 */
#define PRS2_LOCK_TYPEID	((ObjectId) 31)
#define PRS2_BOOL_TYPEID	((ObjectId) 16)
/*---
 * varnos for the NEW & CURRENT tuple.
 * these are hardwired in the parser code...
 */
#define PRS2_CURRENT_VARNO	1
#define PRS2_NEW_VARNO		2

#endif Prs2LocksIncluded
