/*-----------------------------------------------------------------------
 * FILE:
 *   prs2define.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   All teh routines needed to define a (tuple) PRS2 rule
 */

#include "c.h"
#include "pg_lisp.h"
#include "log.h"
#include "relcache.h"	/* RelationNameGetRelation() defined here...*/
#include "prs2.h"
#include "anum.h"	/* RuleRelationNumberOfAttributes etc. defined here */
#include "ruledef.h"	/* PlanToString() defined here... */
#include "skey.h"	/* 'ScanKeyEntryData' defined here... */
#include "tqual.h"	/* 'NowTimeQual' defined here.. */
#include "heapam.h"	/* heap AM calls defined here */
#include "lsyscache.h"	/* get_attnum()  defined here...*/
#include "parse.h"	/* RETRIEVE, APPEND etc defined here.. */
#include "catname.h"	/* names of various system relations defined here */
#include "rproc.h"	/* for ObjectIdEqualRegProcedure etc. */
#include "fmgr.h"	/* for F_CHAR16EQ, F_CHAR16IN etc. */
#include "ftup.h"	/* for FormHeapTuple() */

extern HeapTuple palloctup();
extern LispValue planner();

/*-----------------------------------------------------------------------
 *
 * prs2DefineTupleRule
 *
 */

void
prs2DefineTupleRule(parseTree, ruleText)
LispValue parseTree;
char *ruleText;
{
    NameData ruleName;
    int	eventTypeInt;
    EventType eventType;
    LispValue eventTarget;
    NameData eventTargetRelationNameData;
    Relation eventTargetRelation;
    ObjectId eventTargetRelationOid;
    NameData eventTargetAttributeNameData;
    AttributeNumber eventTargetAttributeNumber;
    LispValue ruleQual;
    LispValue ruleAction;
    int isRuleInstead;
    LispValue ruleActionPlans;
    ObjectId ruleId;		/*the OID for the new rule*/
    Prs2LockType lockType;


#ifdef PRS2_DEBUG
    printf("---prs2DefineTupleRule called, with argument =");
    lispDisplay(parseTree, 0);
    printf("\n");
    printf("   ruletext = %s\n", ruleText);
#endif PRS2_DEBUG

    strcpy(ruleName.data, CString(GetRuleNameFromParse(parseTree)));
    eventTypeInt = CInteger(GetRuleEventTypeFromParse(parseTree));
    eventTarget = GetRuleEventTargetFromParse(parseTree);
    ruleQual = GetRuleQualFromParse(parseTree);
    isRuleInstead = CInteger(GetRuleInsteadFromParse(parseTree));
    ruleAction = GetRuleActionFromParse(parseTree);

    /*
     * Note that the event type as stored in the parse tree is one of
     * RETRIEVE, REPLACE, APPEND or DELETE. All these symbols are
     * defined in "parse.h". So, we have to change them
     * into the appropriate "EventType" type.
     */
    switch (eventTypeInt) {
	case RETRIEVE:
	    eventType = EventTypeRetrieve;
	    break;
	case REPLACE:
	    eventType = EventTypeReplace;
	    break;
	case APPEND:
	    eventType = EventTypeAppend;
	    break;
	case DELETE:
	    eventType = EventTypeDelete;
	    break;
	default:
	    eventType = EventTypeInvalid;
	    elog(WARN, "prs2DefineTupleRule: Illegal event type (int) %d",
		eventTypeInt);
    } /* switch*/
    

    /*
     * 
     * Find the OID of the relation to be locked, and the
     * attribute number of the locked attribute. If no such 
     * attribute has been specified, use 'InvalidAttributeNumber'
     * instead.
     *
     * NOTE: 'eventTarget' is a list of one or two items.
     * The first one is the name of the relation and the optional second
     * one is the attribute name
     *
     * XXX: this should change in the future! It would be better
     * that the parser will return a list with the relation OID
     * and the attibute number instead of their names!
     */
    strcpy(eventTargetRelationNameData.data, CString(CAR(eventTarget)));
    eventTargetRelation = RelationNameGetRelation(&eventTargetRelationNameData);
    eventTargetRelationOid = eventTargetRelation->rd_id;
    if (null(CDR(eventTarget))) {
	/*
	 * no attribute is specified
	 */
	eventTargetAttributeNumber = InvalidAttributeNumber;
    } else {
	strcpy(eventTargetAttributeNameData.data, CString(CADR(eventTarget)));
	eventTargetAttributeNumber = get_attnum(eventTargetRelationOid,
					    &eventTargetAttributeNameData);
	if (eventTargetAttributeNumber == InvalidAttributeNumber) {
	    elog(WARN,"Illegal attribute in event target list");
	}
    }
    

#ifdef PRS2_DEBUG
    printf("---DEFINE TUPLE RULE:\n");
    printf("   RuleName = %s\n", ruleName.data);
    printf("   event type = %d ", eventType);
    switch (eventType) {
	case EventTypeRetrieve:
	    printf("(retrieve)\n");
	    break;
	case EventTypeReplace:
	    printf("(replace)\n");
	    break;
	case EventTypeAppend:
	    printf("(append)\n");
	    break;
	case EventTypeDelete:
	    printf("(delete)\n");
	    break;
	default:
	    printf("(*** UNKNOWN ***)\n");
    }
    printf("   event target:");
    lispDisplay(eventTarget, 0);
    printf("\n");
    printf("   event target relation OID = %ld\n", eventTargetRelationOid);
    printf("   event target attr no = %d\n", eventTargetAttributeNumber);
    printf("   Instead flag = %d\n", isRuleInstead);
    printf("   ruleAction:");
    lispDisplay(ruleAction, 0);
    printf("\n");
#endif PRS2_DEBUG

    /*
     * Insert information about the rule in the catalogs.
     * The routine returns the rule's OID.
     *
     */
    ruleId = prs2InsertRuleInfoInCatalog(&ruleName,
					eventType,
					eventTargetRelationOid,
					eventTargetAttributeNumber,
					ruleText);
					
    /*
     * Now generate the plans for the action part of the rule
     */
    ruleActionPlans = prs2GenerateActionPlans(isRuleInstead,
					    ruleQual,ruleAction);

    /*
     * Store the plans generated above in the system 
     * catalogs.
     */
    prs2InsertRulePlanInCatalog(ruleId, 
			    ActionPlanNumber,
			    ruleActionPlans);
    
    /*
     * Now set the appropriate locks.
     *
     * Try first to findthe appropriate type of lock.
     * Normally if the rule updates the current tuple
     * we must put a LockTypeWrite lock.
     */

    /* XXX Thsi wil not work! */
    lockType = LockTypeWrite;
    prs2PutLocks(ruleId, lockType,
		    eventTargetRelationOid,
		    eventTargetAttributeNumber);

#ifdef PRS2_DEBUG
    printf("--- DEFINE PRS2 RULE: Done.\n");
#endif PRS2_DEBUG

}

/*-----------------------------------------------------------------------
 * prs2PutLocks
 *
 * Put the appropriate locks for the rule. For the time
 * being we just put a relation level lock in pg_relation.
 * All the tuples of the target relation are assumed to be locked
 */

void
prs2PutLocks(ruleId, lockType, eventRelationOid, eventAttributeNumber)
ObjectId ruleId;
Prs2LockType lockType;
ObjectId eventRelationOid;
AttributeNumber eventAttributeNumber;
{

    Relation relationRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    Prs2Locks currentLock;

    /*
     * Lock a relation given its ObjectId.
     * Go to the RelationRelation (i.e. pg_relation), find the
     * appropriate tuple, and add the specified lock to it.
     */
    relationRelation = RelationNameOpenHeapRelation(RelationRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument.objectId.value = eventRelationOid;
    scanDesc = RelationBeginHeapScan(relationRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (!HeapTupleIsValid(tuple)) {
	elog(WARN, "Invalid rel OID %ld", eventRelationOid);
    }

    /*
     * We have found the appropriate tuple of the RelationRelation.
     * Now find its old locks, and add the new one
     */
    currentLock = prs2GetLocksFromTuple(tuple, buffer,
					relationRelation->rd_att);

#ifdef PRS2_DEBUG
    /*-- DEBUG --*/
    printf("previous Lock:");
    prs2PrintLocks(currentLock);
    printf("\n");
#endif /* PRS2_DEBUG */

    currentLock = prs2AddLock(currentLock,
			ruleId,
			lockType,
			eventAttributeNumber,
			ActionPlanNumber);

#ifdef PRS2_DEBUG
    /*-- DEBUG --*/
    printf("new Lock:");
    prs2PrintLocks(currentLock);
    printf("\n");
#endif /* PRS2_DEBUG */

    /*
     * Create a new tuple (i.e. a copy of the old tuple
     * with its rule lock field changed and replace the old
     * tuple in the Relationrelation
     */
    newTuple = prs2PutLocksInTuple(tuple, buffer,
		    relationRelation,
		    currentLock);

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);

}


/*-----------------------------------------------------------------------
 *
 * prs2InsertRuleInfoInCatalog
 *
 * Insert information about a rule in the rules system catalog.
 * As the rule names are unique, we have first to make sure
 * that no other rule with the same names exists.
 *
 */

ObjectId
prs2InsertRuleInfoInCatalog(ruleName, eventType, eventRelationOid,
			    eventAttributeNumber, ruleText)
Name ruleName;
EventType eventType;
ObjectId eventRelationOid;
AttributeNumber eventAttributeNumber;
char * ruleText;
{
    register		i;
    Relation		prs2RuleRelation;
    HeapTuple		heapTuple;
    char		*values[RuleRelationNumberOfAttributes];
    char		nulls[RuleRelationNumberOfAttributes];
    ObjectId		objectId;
    HeapScanDesc	heapScan;
    ScanKeyEntryData	ruleNameKey[1];


    
    /*
     * Open the system catalog relation
     */
    prs2RuleRelation = RelationNameOpenHeapRelation(Prs2RuleRelationName);
    if (!RelationIsValid(prs2RuleRelation)) {
	elog(WARN, "prs2InsertRuleInfoCatalog: could not open relation %s",
	     Prs2RuleRelationName);
    }

    /*
     * First make sure that no other rule with the same name exists!
     */
    ruleNameKey[0].flags = 0;
    ruleNameKey[0].attributeNumber= Prs2RuleNameAttributeNumber;
    ruleNameKey[0].procedure = F_CHAR16EQ;
    ruleNameKey[0].argument.name.value = ruleName;

    heapScan = RelationBeginHeapScan(prs2RuleRelation, 0, NowTimeQual,
				     1, (ScanKey) ruleNameKey);
    heapTuple = HeapScanGetNextTuple(heapScan, 0, (Buffer *) NULL);
    if (HeapTupleIsValid(heapTuple)) {
	HeapScanEnd(heapScan);
	RelationCloseHeapRelation(prs2RuleRelation);
	elog(WARN, "prs2InsertRuleInfoInCatalog: rule %s already defined",
	     ruleName);
    }
    HeapScanEnd(heapScan);

    /*
     * Now create a tuple with the new rule info
     */
    for (i = 0; i < Prs2RuleRelationNumberOfAttributes; ++i) {
	values[i] = NULL;
	nulls[i] = 'n';
    }

    values[Prs2RuleNameAttributeNumber-1] = fmgr(F_CHAR16IN,(char *)ruleName);
    nulls[Prs2RuleNameAttributeNumber-1] = ' ';

    values[Prs2RuleEventTypeAttributeNumber-1] = (char *) eventType;
    nulls[Prs2RuleEventTypeAttributeNumber-1] = ' ';

    values[Prs2RuleEventTargetRelationAttributeNumber-1] =
			(char *) eventRelationOid;
    nulls[Prs2RuleEventTargetRelationAttributeNumber-1] = ' ';

    values[Prs2RuleEventTargetAttributeAttributeNumber-1] = 
			(char *) eventAttributeNumber;
    nulls[Prs2RuleEventTargetAttributeAttributeNumber-1] = ' ';

    values[Prs2RuleTextAttributeNumber-1] = 
			fmgr(F_TEXTIN, ruleText);
    nulls[Prs2RuleTextAttributeNumber-1] = ' ';

    heapTuple = FormHeapTuple(Prs2RuleRelationNumberOfAttributes,
			      &prs2RuleRelation->rd_att, values, nulls);

    /*
     * Now insert the tuple in the system catalog
     * and return its OID (which form now on is the rule's id.
     */
    (void) RelationInsertHeapTuple(prs2RuleRelation, heapTuple,
				   (double *) NULL);
    objectId = heapTuple->t_oid;
    RelationCloseHeapRelation(prs2RuleRelation);
    return(objectId);
}

/*-----------------------------------------------------------------------
 *
 * prs2InsertRulePlanInCatalog
 *
 *    Insert a rule plan into the appropriate system catalogs.
 *    Arguments:
 *	ruleId		the OID of the rule (as returned by routine
 *			'prs2InsertRuleInfoInCatalog').
 *	planNumber	the corresponding planNumber.
 *	rulePlan	the rule plan itself!
 */
void
prs2InsertRulePlanInCatalog(ruleId, planNumber, rulePlan)
ObjectId	ruleId;
Prs2PlanNumber	planNumber;
LispValue	rulePlan;
{
    register		i;
    char		*rulePlanString;
    Relation		prs2PlansRelation;
    HeapTuple		heapTuple;
    char		*values[Prs2PlansRelationNumberOfAttributes];
    char		nulls[Prs2PlansRelationNumberOfAttributes];

    
    /*
     * Open the system catalog relation
     */
    prs2PlansRelation=RelationNameOpenHeapRelation(Prs2PlansRelationName);
    if (!RelationIsValid(prs2PlansRelation)) {
	elog(WARN, "prs2InsertRulePlanInCatalog: could not open relation %s",
	     Prs2PlansRelationName);
    }

    /*
     * Create the appropriate tuple.
     * NOTE: 'rulePlan' is of type LispValue, so we have first
     * to transform it to a string using 'PlanToString()'
     */
    rulePlanString = PlanToString(rulePlan);

    for (i = 0; i < Prs2PlansRelationNumberOfAttributes; ++i) {
	nulls[i] = ' ';
    }

    values[Prs2PlansRuleIdAttributeNumber-1] = (char *) ruleId;
    values[Prs2PlansPlanNumberAttributeNumber-1] = (char *) planNumber;
    values[Prs2PlansCodeAttributeNumber-1] = fmgr(F_TEXTIN,rulePlanString);
    
    heapTuple = FormHeapTuple(Prs2PlansRelationNumberOfAttributes,
			      &prs2PlansRelation->rd_att, values, nulls);

    /*
     * Now insert the tuple to the system catalog.
     */
    (void) RelationInsertHeapTuple(prs2PlansRelation,
				heapTuple, (double *) NULL);
    RelationCloseHeapRelation(prs2PlansRelation);
}

/*----------------------------------------------------------------------
 *
 * prs2GenerateActionPlans
 *
 * generate the plan+parse trees for the action part of the rule
 */
LispValue
prs2GenerateActionPlans(isRuleInstead, ruleQual,ruleAction)
int isRuleInstead;
LispValue ruleQual;
LispValue ruleAction;
{
    LispValue result;
    LispValue action;
    LispValue oneParse;
    LispValue onePlan;
    LispValue oneEntry;

    result = LispNil;

    /*
     * the first entry in the result is some rule info
     */
    if (isRuleInstead) {
	oneEntry = lispCons(lispString("instead"), LispNil);
    } else {
	oneEntry = lispCons(lispString("not-instead"), LispNil);
    }

    result = nappend1(result, oneEntry);


    /*
     * now append the qual entry (parse tree + plan)
     */
    if (null(ruleQual)) {
	oneEntry = LispNil;
    } else {
	onePlan = planner(ruleQual);
	oneEntry = lispCons(onePlan, LispNil);
	oneEntry = lispCons(ruleQual, oneEntry);
    }
    result = nappend1(result, oneEntry);

    /*
     * Now for each action append the corresponding entry
     * (parse tree + plan)
     */
    foreach (action, ruleAction) {
	oneParse = CAR(action);
	/*
	 * call the planner to create a plan for this parse tree
	 */
	onePlan = planner(oneParse);
	oneEntry = lispCons(onePlan, LispNil);
	oneEntry = lispCons(oneParse, oneEntry);
	result = nappend1(result, oneEntry);
    }

    return(result);

}

/* ----------------------------------------------------------------
 *
 * prs2RemoveTupleRule
 *
 * Delete a rule given its rulename.
 *
 * There are three steps.
 *   1) Find the corresponding tuple in 'pg_prs2rule' relation.
 *      Find the rule Id (i.e. the Oid of the tuple) and finally delete
 *      the tuple.
 *   2) Given the rule Id find and delete all corresonding tuples from
 *      'pg_prs2plans' relation
 *   3) (Optional) Delete the locks from the 'pg_relation' relation.
 *
 *
 * ----------------------------------------------------------------
 */

void
prs2RemoveTupleRule(ruleName)
Name ruleName;
{
    Relation prs2RuleRelation;
    Relation prs2PlansRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    ObjectId ruleId;
    ObjectId eventRelationOid;
    Datum eventRelationOidDatum;
    Buffer buffer;
    Boolean isNull;

    /*
     * Open the pg_rule relation. 
     */
    prs2RuleRelation = RelationNameOpenHeapRelation(Prs2RuleRelationName);

     /*
      * Scan the RuleRelation ('pg_prs2rule') until we find a tuple
      */
    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = Prs2RuleNameAttributeNumber;
    scanKey.data[0].procedure = Character16EqualRegProcedure;
    scanKey.data[0].argument.name.value = ruleName;
    scanDesc = RelationBeginHeapScan(prs2RuleRelation,
				    0, NowTimeQual, 1, &scanKey);

    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);

    /*
     * complain if no rule with such name existed
     */
    if (!HeapTupleIsValid(tuple)) {
	RelationCloseHeapRelation(prs2RuleRelation);
	elog(WARN, "No rule with name = '%s' was found.\n", ruleName);
    }

    /*
     * Store the OID of the rule (i.e. the tuple's OID)
     * and the event relation's OID
     */
    ruleId = tuple->t_oid;
    eventRelationOidDatum = HeapTupleGetAttributeValue(
				tuple,
				buffer,
				Prs2RuleEventTargetRelationAttributeNumber,
				&(prs2RuleRelation->rd_att),
				&isNull);
    if (isNull) {
	/* XXX strange!!! */
	elog(WARN, "prs2RemoveTupleRule: null event target relation!");
    }
    eventRelationOid = DatumGetObjectId(eventRelationOidDatum);

    /*
     * Now delete the tuple...
     */
    RelationDeleteHeapTuple(prs2RuleRelation, &(tuple->t_ctid));
    RelationCloseHeapRelation(prs2RuleRelation);

    /*
     * Now delete all tuples of the Prs2Plans relation (pg_prs2plans)
     * corresponding to this rule...
     */
    prs2PlansRelation = RelationNameOpenHeapRelation(Prs2PlansRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = Prs2PlansRuleIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument.objectId.value = ruleId;
    scanDesc = RelationBeginHeapScan(prs2PlansRelation,
				    0, NowTimeQual, 1, &scanKey);


    while((tuple=HeapScanGetNextTuple(scanDesc, 0, (Buffer *)NULL)) != NULL) {
	/*
	 * delete the prs2PlansRelation tuple...
	 */
	RelationDeleteHeapTuple(prs2PlansRelation, &(tuple->t_ctid));
    }
	
    RelationCloseHeapRelation(prs2PlansRelation);

    /*
     * Now delete the relation level locks from the updated relation
     */
    prs2RemoveLocks(ruleId, eventRelationOid);

    elog(DEBUG, "---Rule '%s' deleted.\n", ruleName);

}

/*--------------------------------------------------------------
 *
 * RuleUnlockRelation
 *
 * Unlock a relation given its ObjectId ant the Objectid of the rule
 * Go to the RelationRelation (i.e. pg_relation), find the
 * appropriate tuple, and remove all locks of this rule from it.
 */

void
prs2RemoveLocks(ruleId, eventRelationOid)
ObjectId ruleId;
ObjectId eventRelationOid;
{

    Relation relationRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;
    HeapTuple newTuple;
    HeapTuple newTuple2;
    Prs2Locks currentLocks;
    Boolean isNull;
    int i;
    int numberOfLocks;
    Prs2OneLock oneLock;

    relationRelation = RelationNameOpenHeapRelation(RelationRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument.objectId.value = eventRelationOid;
    scanDesc = RelationBeginHeapScan(relationRelation,
					0, NowTimeQual,
					1, &scanKey);
    
    tuple = HeapScanGetNextTuple(scanDesc, 0, &buffer);
    if (!HeapTupleIsValid(tuple)) {
	elog(WARN, "Invalid rel Oid %ld\n", eventRelationOid);
    }

    currentLocks = prs2GetLocksFromTuple(
			    tuple,
			    buffer,
			    &(relationRelation->rd_att));

#ifdef PRS2_DEBUG
    printf("previous Lock:");
    prs2PrintLocks(currentLocks);
    printf("\n");
#endif PRS2_DEBUG
    
    /*
     * find all the locks with the given rule id and remove them
     */
    currentLocks = prs2RemoveAllLocksOfRule(currentLocks, ruleId);

#ifdef PRS2_DEBUG
    printf("new Lock:");
    prs2PrintLocks(currentLocks);
    printf("\n");
#endif PRS2_DEBUG

    /*
     * Create a new tuple (i.e. a copy of the old tuple
     * with its rule lock field changed and replace the old
     * tuple in the RelationRelation
     */
    newTuple = prs2PutLocksInTuple(tuple, buffer,
				relationRelation,
				currentLocks);

    RelationReplaceHeapTuple(relationRelation, &(tuple->t_ctid),
			    newTuple, (double *)NULL);
    
    RelationCloseHeapRelation(relationRelation);

}
