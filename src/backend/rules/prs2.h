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

#include "tmp/postgres.h"

#include "access/heapam.h"
#include "access/tupdesc.h"
#include "nodes/pg_lisp.h"
#include "storage/buf.h"
#include "utils/rel.h"

#include "rules/params.h"

/*------------------------------------------------------------------
 * Include PRS2 lock definition
 * These defs were put in a separate file because they are included
 * bit "htup.h" and that created a circular dependency...
 */
#include "rules/prs2locks.h"

/*------------------------------------------------------------------
 * Comment out the following line to supress debugging output
 */
#define PRS2_DEBUG 1

/*==================================================================
 * --Prs2RuleData--
 *
 * This is the structure where we keep all the information
 * needed to define a rule.
 *
 * ruleText:
 *     the text of the rule (i.e. a string)
 * ruleName:
 *     the name of the rule
 * ruleId:
 *     the oid of the rule.
 *     NOTE: we do NOT know it until we add the rule in the system
 *     catalogs!
 * isInstead:
 *     is this an "instead" rule ??
 * eventType:
 *     the event type that triggers the rule
 * actionType:
 *     the type of the action that will be executed if the rule istriggered.
 * eventRelationOid:
 * eventRelationName:
 *     oid & name of the relation referenced in the "event" clause of the rule.
 * eventAttributeNumber:
 * eventAttributeName:
 *     the number & name of the attribute referenced in the "event" clause
 *     of the rule. If no such attribute exists (i.e. "on retrieve to EMP" -
 *     a view rule) then this is an InvalidAttributeNumber & NULL
 *     respectively.
 * updatedAttributeNumber:
 *     The attribute no of the attribute that is updated from the rule.
 *     For instance "on retrieve ... then do replace CURRENT(attr = ...)"
 *     or "on retrieve to ... then do retrieve (attr = ....)"
 * parseTree:
 *     the parse tree for the rule definition.
 * ruleQual:
 *     the qualification of the rule
 * ruleAction:
 *     a list of all the actions.
 * paramParseTree:
 * paramRuleQual:
 * paramRuleAction:
 *     same as the parseTree, ruleQual & ruleAction but with all the
 *     Var nodes that reference the CURRENT and/or NEW relations changed
 *     into the appropriate Param nodes...
 * 
 */
typedef struct Prs2RuleDataData {
    ObjectId		ruleId;
    char		*ruleText;
    Name		ruleName;
    bool		isInstead;
    EventType		eventType;
    ActionType		actionType;
    ObjectId		eventRelationOid;
    Name		eventRelationName;
    AttributeNumber	eventAttributeNumber;
    Name		eventAttributeName;
    AttributeNumber	updatedAttributeNumber;
    List		parseTree;
    List		ruleQual;
    List		ruleAction;
    List		paramParseTree;
    List		paramRuleQual;
    List		paramRuleAction;
} Prs2RuleDataData;

typedef Prs2RuleDataData *Prs2RuleData;

/*==================================================================
 * PRS2 MAIN ROUTINES
 *
 * These are almost all the routines a non Prs2 person wants to know
 * about!
 *==================================================================
 */


/*------------------------------------------------------------------
 * return true if the executor must call the rule manager, or
 * false if there is no need to do so (no rules defined).
 */
extern
bool
prs2MustCallRuleManager ARGS((
    RelationInfo	relationInfo,
    HeapTuple           oldTuple,
    Buffer              oldBuffer,
    int			operation
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
    EState		estate,
    RelationInfo	scanRelInfo,
    int                 operation,
    int                 userId,
    Relation            relation,
    HeapTuple           oldTuple,
    Buffer              oldBuffer,
    HeapTuple           newTuple,
    Buffer              newBuffer,
    HeapTuple           rawTuple,
    Buffer              rawBuffer,
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
    RuleLock		oldLocks,
    ObjectId		ruleId,
    Prs2LockType	lockType,
    AttributeNumber	attributeNumber,
    Prs2PlanNumber	planNumber
    int			partialindx,
    int		 	npartial
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
 * prs2OneLocksAreTheSame
 * return true iff the two 'Prs2OneLock' are the same...
 */
extern
bool
prs2OneLocksAreTheSame ARGS((
    Prs2OneLock	oneLock1,
    Prs2OneLock	oneLock2,
));

/*------------------------------------------------------------------
 * prs2OneLockIsMemberOfLocks
 * return true iff the given `Prs2OneLock' is one of the locks of
 * `locks'
 */
extern
bool
prs2OneLockIsMemberOfLocks ARGS((
    Prs2OneLock	oneLock,
    RuleLock	locks;
));

/*------------------------------------------------------------------
 * prs2GetLocksFromTuple
 *  Extract the locks from a tuple. It returns a 'RuleLock',
 *  (NOTE:it will never return NULL! Even if the tuple has no
 *  locks in it, it will return a 'RuleLock' with 'numberOfLocks'
 *  equal to 0.
 *  The returned rule lock is a copy, and must be pfreed to avoid memory leaks.
 */
extern
RuleLock
prs2GetLocksFromTuple ARGS((
    HeapTuple		tuple,
    Buffer		buffer,
));

/*------------------------------------------------------------------
 * prs2PutLocksInTuple
 *    given a tuple, update its rule lock.
 *    NOTE: the old rule lock is pfreed!
 */
extern
void
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
 * prs2RemoveOneLockInPlace
 *
 * remove a rule lock in place (i.e. no copies)
 */
extern
void
prs2RemoveOneLockInPlace ARGS((
    RuleLock	locks,
    int		n
));

/*------------------------------------------------------------------
 * prs2RemoveAllLocksOfRuleInPlace
 *
 * remove all locks of a given rule in place (i.e. no copies)
 * returns true if the lcoks have changed, false otherwise (i.e.
 * if no locks for this rule were found).
 */
extern
bool
prs2RemoveAllLocksOfRuleInPlace ARGS((
    RuleLock	locks,
    ObjectId	ruleId
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
 * prs2LockDifference
 *   Create the difference of two RuleLock.
 */
extern
RuleLock
prs2LockDifference ARGS((
    RuleLock	lock1,
    RuleLock	lock1
));

/*------------------------------------------------------------------
 * prs2FindLocksOfType
 *	return a copy of all the locks of a given type.
 */
extern
RuleLock
prs2FindLocksOfType ARGS((
    RuleLock		locks,
    Prs2LockType	locktype;
));

/*------------------------------------------------------------------
 * prs2RemoveLocksOfTypeInPlace
 * remove all the lcoks of the given type in place (i.e. No copies)
 */
extern
void
prs2RemoveLocksOfTypeInPlace ARGS((
    	RuleLock	locks,
	Prs2LockType	lockType
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
#define GetRuleHintFromParse(p)		(CADR(nth(1,p)))
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

/*====================== FILE: prs2define.c ============================*/
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

extern Prs2RuleData prs2FindRuleData();
extern ObjectId prs2InsertRuleInfoInCatalog();
extern void prs2DeleteRuleInfoFromCatalog();
extern void prs2InsertRulePlanInCatalog();
extern void prs2DeleteRulePlanFromCatalog();
extern LispValue prs2GenerateActionPlans();
extern EventType prs2FindEventTypeFromParse();
extern ActionType prs2FindActionTypeFromParse();
extern void changeReplaceToRetrieve();
extern bool prs2AttributeIsOfBasicType();

/*====================== FILE: prs2putlocks.c ============================*/

/*------------------------
 * prs2AddTheNewRule
 * 	add a new rule. Decide what kind of lock to use (tuple-level or
 * 	relation-level lock). Add locks and/or stubs and the appropriate
 *	system catalog info.
 *------------------------
 */
extern
void
prs2AddTheNewRule ARGS((
    Prs2RuleData	r
));

/*------------------------
 * prs2DeleteTheOldRule
 *	delete a rule given its rule oid. Find if it was a tuple-level
 *	lock or a relation-level lock rule and do the right thing
 *	(delete locks/stubs & system catalog info).
 *------------------------
 */
extern
void
prs2DeleteTheOldRule ARGS((
	ObjectId	ruleId
));

extern void prs2FindLockTypeAndAttrNo();
extern LispValue prs2FindConstantQual();
extern LispValue prs2FindConstantClause();
extern bool prs2IsATupleLevelLockRule();

/*====================== FILE: prs2tup.c ============================*/
extern bool prs2DefTupleLevelLockRule();
extern RuleLock prs2FindLocksThatWeMustPut();
extern bool prs2DoesStubDependsOnAttribute();
extern bool prs2LockWritesAttributes();
extern void prs2FindAttributesOfQual();
extern void prs2UndefTupleLeveLockRule();
extern void prs2RemoveTupleLeveLocksAndStubsOfManyRules();

/*====================== FILE: prs2rel.c ============================*/
extern void prs2DefRelationLevelLockRule();
extern void prs2UndefRelationLevelLockRule();
extern void prs2RemoveRelationLevelLocksOfRule();
extern void prs2SetRelationLevelLocks();
extern void prs2AddRelationLevelLock();
 
/*===================================================================
 * RULE PLANS
 *
 * The rule plans are stored in the "pg_prs2plans" relation.
 * There are different types of plans.
 * Each "plan" contains appart from one or more parsetrees/plans
 * some rule information.
 * The first item of all the plans is a string describing the type of the
 * plan. This string can be "action" for the ACTION plans, and "export"
 * for the EXPORT plans.
 * The second item is the plan information itself, and its format depends
 * on the type of plan:
 *
 * a) ACTION PLANS:
 *	They are a list with at least two items:
 *
 *	The first item is a list giving some info for the rule.
 *	Currently this can have the following elements:
 *	The first element is an integer, equal to 1 if the rule is an
 *	instead rule, 0 otherwise.
 *	The second is the "event attribute" number, i.e. the attrno
 *	of the attribute specified in the "on <action> to <rel>.<attr>"
 *	rule clause.
 *	The third is the "updated attribute" number, i.e. the attrno 
 * 	of the attribute updated by a backward chaining rule.
 *
 *	The second item is the qualification of the rule.
 *	This should be tested before and only if it succeeds should the
 *	action part of the rule be executed.
 *	This qualifcation can be either "nil" which means that
 *	there is no qualification and the action part of the rule should be
 *	always executed, or a 2 item list, the first item beeing the parse
 *	tree corresponding to the qualification and the second one the plan.
 *
 *	Finally the (optional) rest items are the actions of the rule.
 *	Each one of them can be either a 2 item list (parse tree + plan)
 *	or the string "abort" which means that the current Xaction should be
 *	aborted.
 *
 * b) EXPORT PLANS:
 *	this is the kind of plan to be executed when an export
 *	lock is broken.
 *	The first item of the plan is a list with information
 *	about this export lock. This information consists of the
 *	following items:
 *		1) a string containing the lock (use `lockin' to recreate
 *		the lock).
 *	Then there is the plan to be run in order to add/delete
 *	rule locks when this export lock is broken. This plan is actually
 * 	a two item list conatining the parsetree and the actual plan.
 *	
 *===================================================================
 */

#define Prs2RulePlanType_EXPORT			("export")
#define Prs2RulePlanType_ACTION			("action")

#define prs2GetTypeOfRulePlan(x)		(CAR(x))
#define prs2GetPlanInfoFromRulePlan(x)		(CDR(x))

#define prs2GetRuleInfoFromActionPlan(x)	(CAR(CDR(x)))
#define prs2GetQualFromActionPlan(x)		(CADR(CDR(x)))
#define prs2GetActionsFromActionPlan(x)		(CDR(CDR(CDR(x))))

#define prs2GetLockInfoFromExportPlan(x)	(CAR(CDR(x)))
#define prs2GetActionPlanFromExportPlan(x)	(CADR(CDR(x)))

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

extern LispValue StringToPlan();
extern LispValue StringToPlanWithParams();

extern LispValue MakeQueryDesc();
/* XXX this causes circular dependency!
extern EState CreateExecutorState();
*/

extern Prs2Status prs2Retrieve();
extern Prs2Status prs2Append();
extern Prs2Status prs2Delete();
extern Prs2Status prs2Replace();
extern RuleLock prs2FindLocksForNewTupleFromStubs();

/*
 * These are functions in prs2plans.c
 */
extern LispValue prs2GetRulePlanFromCatalog();
extern int prs2CheckQual();
extern void prs2RunActionPlans();
extern int prs2RunOnePlanAndGetValue();
extern void prs2RunOnePlan();
extern LispValue prs2MakeQueryDescriptorFromPlan();

/*
 * functions in prs2bkwd.c
 */
extern void prs2CalculateAttributesOfParamNodes();
extern void prs2ActivateBackwardChainingRules();
extern void prs2ActivateForwardChainingRules();

/*
 * functions in prs2impexp.c
 */
extern RuleLock prs2FindNewExportLocksFromLocks();
extern RuleLock prs2GetExportLockFromCatalog();
extern void prs2ActivateExportLockRulePlan();

/*============== RULE STATISTICS ============================*/
/*
 * the following variables/routines are used to print stats about
 * the tuple level system usage.
 * (all this stuff is defined in "rules/prs2/prs2main.c")
 */
extern int Prs2Stats_rulesActivated;
extern int Prs2Stats_rulesTested;
extern void ResetPrs2Stats();
extern void ShowPrs2Stats();

#endif Prs2Included

