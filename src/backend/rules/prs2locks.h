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
#include "postgres.h"
#include "attnum.h"

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
 * ActionTypeOther: Any other action... (including 'retrieve into...')
 *------------------------------------------------------------------
 */
typedef char ActionType;
#define ActionTypeRetrieveValue		'r'
#define ActionTypeReplaceCurrent	'u'
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
 *------------------------------------------------------------------
 */
typedef char Prs2LockType;

#define LockTypeInvalid			((Prs2LockType) '*')
#define LockTypeRetrieveAction		((Prs2LockType) 'r')
#define LockTypeAppendAction		((Prs2LockType) 'a')
#define LockTypeDeleteAction		((Prs2LockType) 'd')
#define LockTypeReplaceAction		((Prs2LockType) 'u')
#define LockTypeRetrieveWrite		((Prs2LockType) 'R')
#define LockTypeAppendWrite		((Prs2LockType) 'A')
#define LockTypeDeleteWrite		((Prs2LockType) 'D')
#define LockTypeReplaceWrite		((Prs2LockType) 'U')

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


#define InvalidRuleLock		((RuleLock) NULL)
#define RuleLockIsValid(x)	PointerIsValid(x)
#endif Prs2LocksIncluded
