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

#include "c.h"

#define PRS2_DEBUG 1

/*==================================================================
 * PRS2 LOCKS
 *==================================================================
 */

/*------------------------------------------------------------------
 * event types: the types of event that can cause rule activations...
 */
typedef char EventType;
#define EventTypeRetrieve	'R'
#define EventTypeReplace	'U'
#define EventTypeAppend		'A'
#define EventTypeDelete		'D'
#define EventTypeInvalid	'*'

/*------------------------------------------------------------------
 * Plan numbers for various rule plans (as stored in the system catalogs)
 *------------------------------------------------------------------
 */
typedef uint16 Prs2PlanNumber;

/* #define QualificationPlanNumber		((Prs2PlanNumber)0) */
#define ActionPlanNumber		((Prs2PlanNumber)1)

/*------------------------------------------------------------------
 * Types of locks..
 *
 * If the event specified in the event clause of a rule (i.e. the "on ..."
 * clause) is a replace, append or delete then the corresponding lock
 * type is LockTypeOnReplace, LockTypeOnAppend or LockTypeOnDelete.
 * If we are dealing with an 'on retrieve' rule, then we distinguish
 * between 2 cases:
 *  a) either the action is a retrieves that updates the current tuple
 *  (as for example in 'on retrieve to EMP.salary do instead retrieve ...'
 *  in which case the lock type is 'LockTypeWrite' (i.e. we have
 *  a kind of backward chaining)
 *  b) or we hace something like 'on retrieve ... do append ....'
 *  in which case we use a LockTypeOnRetrieve.
 *
 *------------------------------------------------------------------
 */
typedef char Prs2LockType;

#define LockTypeInvalid			((Prs2LockType) '*')
#define LockTypeOnRetrieve		((Prs2LockType) 'r')
#define LockTypeOnAppend		((Prs2LockType) 'a')
#define LockTypeOnDelete		((Prs2LockType) 'd')
#define LockTypeOnReplace		((Prs2LockType) 'u')
#define LockTypeWrite			((Prs2LockType) 'w')

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

typedef Prs2LocksData	*Prs2Locks;


#define InvalidPrs2Locks	((Prs2Locks) NULL);
#define Prs2LocksIsValid(x)	PointerIsValid(x)
#endif Prs2LocksIncluded
