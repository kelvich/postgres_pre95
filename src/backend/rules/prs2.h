/*
 * FILE:
 *   prs2.h
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   All you need to include if you are a PRS2 person!
 */

/*
 * Include this file only once...
 */
#ifndef Prs2Included
#define Prs2Included

#include "c.h"
#include "pg_lisp.h"
#include "datum.h"
#include "heapam.h"
#include "rel.h"
#include "tupdesc.h"
#include "buf.h"
#include "params.h"

/*------------------------------------------------------------------
 * Include PRS2 lock definition
 * These defs were put in a separate file becuase they are included
 * bit "htup.h" and that created a circular dependency...
 */
#include "prs2locks.h"

/*------------------------------------------------------------------
 * Comment out the following line to supress debugging output
 */
#define PRS2_DEBUG 1

/*==================================================================
 * PRS2 MAIN ROUTINES
 *
 * These are almost all the routines a non Prs2 person wants to know
 * about!
 *==================================================================
 */

/*------------------------------------------------------------------
 * prs2DefineTupleRule
 *    Define a PRS2 rule (tuple level proccessing)
 */
extern
void
prs2DefineTupleRule ARGS((
    LispValue	parseTree,
    char	*ruleText
));

/*------------------------------------------------------------------
 * prs2RemoveTupleRule
 *    Remove a prs2 rule given its name.
 */
extern
void
prs2RemoveTupleRule ARGS((
    Name	ruleName
));

/*------------------------------------------------------------------
 * prs2Main
 * The rule manager itself! Normally this should only be called
 * from within the executor...
 */

typedef int Prs2Status;

#define PRS2_STATUS_TUPLE_UNCHANGED	1
#define PRS2_STATUS_TUPLE_CHANGED	2
#define PRS2_STATUS_INSTEAD	3

extern
Prs2Status
prs2Main ARGS((
    int                 operation,
    int                 userId,
    Relation            relation,
    HeapTuple           oldTuple,
    Buffer              oldBuffer,
    HeapTuple           newTuple,
    Buffer              newBuffer,
    AttributeNumberPtr  attributeArray,
    int                 numberOfAttributes,
    HeapTuple           *returnedTupleP,
    Buffer              *returnedBufferP
));

/*==================================================================
 * PRS2 LOCKS
 *==================================================================
 */

/*------------------------------------------------------------------
 * Routines to access/set Prs2Lock data...
 * It is highly recommended that these routines are used instead
 * of directly manipulating the variosu structures invloved..
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
#define prs2GetNumberOfLocks(x)	((x)->numberOfLocks)

/*------------------------------------------------------------------
 * prs2FreeLocks
 *    free the space occupied by a rule lock
 */
extern
void
prs2FreeLocks ARGS((
    RuleLock lock
));

/*------------------------------------------------------------------
 * prs2MakeLocks
 *    return a pointer to a 'Prs2LocksData' structure containing
 *    no locks, i.e. numberOfLocks = 0;
 */
extern
RuleLock
prs2MakeLocks ARGS((
));

/*------------------------------------------------------------------
 * prs2AddLock
 *    Add a new lock (filled in with the given data) to a 'prs2Locks'
 *    Note that this frees the space occupied by 'prs2Locks' and reallocates
 *    some other. So this routine should be used with caution!
 */
extern
RuleLock
prs2AddLock ARGS((
    RuleLock       oldLocks,
    ObjectId	    ruleId,
    Prs2LockType    lockType,
    AttributeNumber attributeNumber,
    Prs2PlanNumber  planNumber
));

/*------------------------------------------------------------------
 * prs2GetOneLockFromLocks
 *    Given a 'RuleLock' return a pointer to its Nth lock..
 *    (the first locks is lock number 0).
 */
extern
Prs2OneLock
prs2GetOneLockFromLocks ARGS((
    RuleLock	locks,
    int		n
));

/*------------------------------------------------------------------
 * prs2GetLocksFromTuple
 *    Extract the locks from a tuple. It returns a 'RuleLock',
 *    (NOTE:it will never return NULL! Even if the tuple has no
 *    locks in it, it will return a 'RuleLock' with 'numberOfLocks'
 *    equal to 0.
 */
extern
RuleLock
prs2GetLocksFromTuple ARGS((
    HeapTuple		tuple,
    Buffer		buffer,
    TupleDescriptorData	tupleDescriptor
));

/*------------------------------------------------------------------
 * prs2PutLocksInTuple
 *    given a tuple, create a copy of it and put the given locks in its
 *    t_lock field...
 */
extern
HeapTuple
prs2PutLocksInTuple ARGS((
    HeapTuple	tuple,
    Buffer	buffer,
    Relation	relation,
    RuleLock	newLocks
));

/*------------------------------------------------------------------
 * prs2PrintLocks
 *    print the prs2 locks in stdout. Used for debugging...
 */
extern
void
prs2PrintLocks ARGS((
    RuleLock	locks
));

/*------------------------------------------------------------------
 * prs2RemoveAllLocksOfRule
 *    remove all the locks that have a ruleId equal to the given.
 *    the old `RuleLock' is destroyed and should never be
 *    referenced again.
 *    The new lock is returned.
 */
extern
RuleLock
prs2RemoveAllLocksOfRule ARGS((
    RuleLock	oldLocks,
    ObjectId	ruleId
));

/*------------------------------------------------------------------
 * prs2CopyLocks
 *    Make a copy of a prs2 lock..
 */
extern
RuleLock
prs2CopyLocks ARGS((
    RuleLock	locks;
));

/*------------------------------------------------------------------
 * prs2GetLocksFromRelation
 *   Get locks from the RelationRelation
 */
extern
RuleLock
prs2GetLocksFromRelation ARGS((
    Relation relation
));

/*------------------------------------------------------------------
 * prs2LockUnion
 *   Create the union of two RuleLock.
 */
extern
RuleLock
prs2LockUnion ARGS((
    RuleLock	lock1,
    RuleLock	lock1
));

/*------------------------------------------------------------------
 * RuleLockToString
 *   greate a string containing a representation of the given
 *   lock, more suitable for the human brain & eyes than a
 *   sequence of bytes.
 */
extern
char *
RuleLockToString ARGS((
    RuleLock	lock
));

/*------------------------------------------------------------------
 * StringToRuleLock
 *   the opposite of 'RuleLockToString()'
 */
extern
RuleLock
StringToRuleLock ARGS((
    char 	*string
));

/*==================================================================
 * Routine to extract rule info from the ParseTree
 *==================================================================
 */
#define GetRuleNameFromParse(p)		(nth(2,p))
#define GetRuleEventTypeFromParse(p)	(nth(0,nth(3,p)))
#define GetRuleEventTargetFromParse(p)	(nth(1,nth(3,p)))
#define GetRuleQualFromParse(p)		(nth(2,nth(3,p)))
#define GetRuleInsteadFromParse(p)	(nth(3,nth(3,p)))
#define GetRuleActionFromParse(p)	(nth(4,nth(3,p)))
#define GetRuleRangeTableFromParse(p)	(nth(4,p))

/*------------------------------------------------------------------
 * Various routines...
 *------------------------------------------------------------------
 */

/*
 * prs2InsertRuleInfoInCatalog
 *    add information about the rule to the system catalogs.
 *    returns the OID of the rule.
 */
extern
ObjectId
prs2InsertRuleInfoInCatalog ARGS((
    Name	ruleName
));


/*
 * prs2GenerateActionPlans
 *    Generate the plans for the action part of the rule
 *    The input argument is a list with the parsetrees for all
 *    the commands specified in the action part of the rule + the
 *    rule qualification.
 *    The returned value is a list with one item per command.
 *    Each one of them is a list with a (possibly modified)
 *    parse tree and the corresponding plan.
 */
extern
LispValue
prs2GenerateActionPlans ARGS((
    int		isRuleInstead,
    LispValue	ruleQual,
    LispValue	ruleAction
));

/*
 * prs2InsertRulePlanInCatalog
 *    Insert a rule plan into the appropriate system catalogs.
 */
extern
void
prs2InsertRulePlanInCatalog ARGS((
    ObjectId		ruleId,		/* the OID of the rule */
    Prs2PlanNumber	planNumber,	/* the PlanNumber for this plan*/
    LispValue		rulePlan
));

/*
 * prs2PutLocksInRelation
 *    Put all the appropriate locks in the RelationRelation
 *    (relation level locking...).
 */
extern
void
prs2PutLocksInRelation ARGS((
    ObjectId		ruleId,		/* the OID of the rule */
    Prs2LockType	lockType	/* the type of lock */
    ObjectId		eventRelationOid; /* the OID of the locked relation */
    AttributeNumber	attrNo;		/* the attribute to be locked */
));

/*
 * prs2RemoveLocks
 *    Remove the lock put by 'prs2PutLocks'
 *    For the time being only relation level locking is implemented.
 */
extern
void
prs2RemoveLocks ARGS((
    ObjectId	ruleId,			/* the OID of the rule */
    ObjectId	eventRelationOid	/* OID of the locked relation */
));


/*===================================================================
 * RULE PLANS
 *
 * The rule plans are stored in the "pg_prs2plans" relation.
 * They are a list with at least two items:
 *
 * The first item is a list giving some info for the rule.
 * Currently this can have the following elements:
 * The first element is an integer, equal to 1 if the rule is an
 * instead rule, 0 otherwise.
 *
 * The second item is the qualification of the rule.
 * This should be tested before and only if it succeeds should the
 * action part of the rule be executed.
 * This qualifcation can be either "nil" which means that
 * there is no qualifcation and the action part of the rule should be
 * executed, or a 2 item list, the first item beeing the parse tree
 * corresponding to the qualifcation and the second one the plan.
 *
 * Finally the (optional) rest items are the actions of the rule.
 * Each one of them can be either a 2 item list (parse tree + plan)
 * or the string "abort" which means that the current Xaction should be
 * aborted.
 *===================================================================
 */

#define prs2GetRuleInfoFromRulePlan(x)	(CAR(x))
#define prs2GetQualFromRulePlan(x)	(CADR(x))
#define prs2GetActionsFromRulePlan(x)	(CDR(CDR(x)))

extern
Boolean
prs2IsRuleInsteadFromRuleInfo ARGS((
    LispValue ruleInfo
));

extern
AttributeNumber
prs2GetEventAttributeNumberFromRuleInfo ARGS((
    LispValue ruleInfo
));

extern
AttributeNumber
prs2GetUpdatedAttributeNumberFromRuleInfo ARGS((
    LispValue ruleInfo
));

#define prs2GetParseTreeFromOneActionPlan(x)	(CAR(x))
#define prs2GetPlanFromOneActionPlan(x)		(CADR(x))

/*
 * prs2GetRulePlanFromCatalog
 * Given a rule id and a plan number get the appropriate plan
 * from the system catalogs. At the same time construct a list
 * with all the Param nodes contained in this plan.
 */
extern
LispValue
prs2GetRulePlanFromCatalog ARGS((
    ObjectId		ruleId,
    Prs2PlanNumber	planNumber,
    ParamListInfo	*paramListP
));

/*==================================================================
 * AttributeValues stuff...
 *==================================================================
 *
 * These definitions & routines make possible to use an array of
 * attribute values instead of a tuple, thus making easier to change
 * these values without having to make multiple copies of the tuple.
 * It also makes easier to keep some extra information about how the value
 * of attributes were calculated.
 * 
 * Explanation of the fileds of a 'AttributesValuesData':
 * For each attribute of the tuple there is a corresponding
 * 'AttributeValuesData' structure, with the following fields:
 *	value:	the value of this attribute. This is initially
 *		initialized to the value stroed in the tuple.
 *	isNull: true if the coresponding attribute in the tuple is
 *		null.
 *	isCalculated: Initially false. It becomes true if we have
 *		checked this attribute for possibly applicable
 *		rules, and we have finally calculated its correct value.
 *	isChanged: If an attribute has been checked for rules (i.e.
 *		has its 'isCalculated' field equal to true), then two
 *		things can happen. Either thre was no applicable rule
 *		found, in which case we have used the value stored
 *		in the tuple and 'isChanged' is equal to false,
 *		or there was a rule that calculated a value for
 *		this attribute, in which case 'isChanged' is true, and
 *		'value' is this new value.
 */

typedef struct AttributeValuesData {
    Datum	value;
    Boolean	isNull;
    Boolean	isCalculated;
    Boolean	isChanged;
} AttributeValuesData;

typedef AttributeValuesData *AttributeValues;
#define InvalidAttributeValues ((AttributeValues)NULL)

/*---------------------------------------------------------------------
 * attributeValuesCreate
 *    Given a tuple create the corresponding 'AttributeValues' array.
 *    Note that in the beginning all the 'value' entries are copied
 *    from the tuple and that 'isCalculated' are all false.
 */
extern
AttributeValues
attributeValuesCreate ARGS((
    HeapTuple tuple;
    Buffer buffer;
    Relation relation;
));

/*---------------------------------------------------------------------
 * attributeValuesFree
 *    Free a previously allocated 'AttributeValues' array.
extern
void
attributeValuesFree ARGS((
    AttributeValues a;
));

/*--------------------------------------------------------------------
 * attributeValuesMakeNewTuple
 *    Given the old tuple and the values that the new tuple should have
 *    create a new tuple.
 *    Returns 1 if a new tuple has been created (stored in *newTupleP)
 *    and 0 if the old tuple is the same as the new tuple, in which
 *    case we can use 'tuple' instead, in order to avoid a redudant
 *    copy operation.
 */
extern
int
attributeValuesMakeNewTuple ARGS((
    HeapTuple tuple;
    Buffer buffer;
    AttributeValues attrValues;
    RuleLock locks;
    Relation relation;
    HeapTuple *newTupleP;
));

/*--------------------------------------------------------------------
 * attributeValuesCombineNewAndOldTuple
 *    In the case of a replace command, given the 'raw' tuple, i.e.
 *    the tuple as retrieved by the AM (*not* changed by any backward
 *    chaining rules) and the 'new' tuple (the one with all backward
 *    chaning rules activated + with all user updates)
 *    form the tuple that will finally replace the old one.
 */
extern
HeapTuple
attributeValuesCombineNewAndOldTuple ARGS((
    AttributeValues rawAttrValues,
    AttributeValues newAttrValues,
    Relation relation,
    AttributeNumberPtr attributeArray,
    AttributeNumber numberOfAttributes
));

/*========================================================================
 * EState Rule Info + Rule Stack
 *
 * This structure (which is kept inside the executor state node EState)
 * contains some information used by the rule manager.
 *
 * RULE DETECTION MECHANISM:
 * prs2Stack: A stack where enough information is ketp in order to detect
 *	rule loops.
 * prs2StackPointer: the next (free) stack entry
 * prs2MaxStackSize: number of stack entries allocated. If
 *      prs2StackPointer is >= prs2MaxStackSize the stack is full
 *	and we have to reallocate some more memory...
 *
 * MISC INFO:
 * 
 *========================================================================
 */
typedef struct Prs2StackData {
    ObjectId		ruleId;		/* OID of the rule */
    ObjectId		tupleOid;	/* the tuple that activated the rule*/
    AttributeNumber	attrNo;		/* the locked attribute */
} Prs2StackData;

typedef Prs2StackData *Prs2Stack;

typedef struct Prs2EStateInfoData {
    Prs2Stack	prs2Stack;	/* the stack used for loop detection */
    int		prs2StackPointer;	/* the next free stack entry */
    int		prs2MaxStackSize;	/* the max number of entries */
} Prs2EStateInfoData;

typedef  Prs2EStateInfoData *Prs2EStateInfo;

/*
 * prs2RuleStackPush
 *    Add a new entry to the rule stack. First check if there is enough
 *    stack space. otherwise reallocate some more memory...
 */
extern
void
prs2RuleStackPush ARGS((
    Prs2EStateInfo p,
    ObjectId ruleId,
    ObjectId tupleOid,
    AttributeNumber attributeNumber
));

/*
 * prs2RuleStackPop
 *    Discard the top entry of the stack
 */
extern
void
prsRuleStackPop ARGS((
    Prs2EStateInfo p;
));

/*
 * prs2RuleStackSearch
 *   Search for a stack entry matching the given arguments.
 *   Return true if found, false otherwise...
 */
extern
bool
prs2RuleStackSearch ARGS((
    Prs2EStateInfo p,
    ObjectId ruleId,
    ObjectId tupleOid;
    AttributeNumber attributeNumber
));

/*
 * prs2RuleStackInitialize
 *   Intialize the stack.
 */
extern
Prs2EStateInfo
prs2RuleStackInitialize ARGS((
));

/*
 * prs2RuleStackFree
 *   Free the memory occupied by the stack.
 */
extern
void
prs2RuleStackFree ARGS((
    Prs2EStateInfo p;
));

/*========================================================================
 * VARIOUS ROUTINES....
 *========================================================================

/*
 * PlanToString
 *
 * Given a plan (or an arbritary (?) lisp structure, transform it into a
 * a strign which has the following two properties
 *  a) it is readable by (some) humans
 *  b) it can be used to recreate the original plan/lisp structure
 *     (see routine "StringToPlan").
 *
 *
 * XXX Maybe this should be placed in another header file...
 */
extern
char *
PlanToString ARGS((
    LispValue lispStructure
));

/*========================================================================
 * These routines are some 'internal' routines. 
 *========================================================================
 */

extern LispValue StringToPlanWithParams();
extern LispValue MakeQueryDesc();
/* XXX this causes circular dependency!
extern EState CreateExecutorState();
*/

extern Prs2Status prs2Retrieve();
extern Prs2Status prs2Append();
extern Prs2Status prs2Delete();
extern Prs2Status prs2Replace();
extern EventType prs2FindEventTypeFromParse();
extern ActionType prs2FindActionTypeFromParse();
extern void Prs2PutLocks();

/*
 * These are functions in prs2plans.c
 */
extern LispValue prs2GetRulePlanFromCatalog();
extern int prs2CheckQual();
extern void prs2RunActionPlans();
extern int prs2RunOnePlanAndGetValue();
extern LispValue prs2RunOnePlan();
extern LispValue prs2MakeQueryDescriptorFromPlan();

/*
 * functions in prs2bkwd.c
 */
extern void prs2CalculateAttributesOfParamNodes();
extern void prs2ActivateBackwardChainingRules();
extern void prs2ActivateForwardChainingRules();

#endif Prs2Included

