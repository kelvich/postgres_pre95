/*-----------------------------------------------------------------------
 * FILE:
 *   prs2define.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 *   All the routines needed to define a (tuple level) PRS2 rule
 */

#include "tmp/postgres.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "utils/relcache.h"	/* RelationNameGetRelation() defined here...*/
#include "rules/prs2.h"
#include "access/skey.h"	/* 'ScanKeyEntryData' defined here... */
#include "access/tqual.h"	/* 'NowTimeQual' defined here.. */
#include "access/heapam.h"	/* heap AM calls defined here */
#include "utils/lsyscache.h"	/* get_attnum()  defined here...*/
#include "parser/parse.h"	/* RETRIEVE, APPEND etc defined here.. */
#include "parser/parsetree.h"
#include "catalog/catname.h"	/* names of various system relations */
#include "utils/fmgr.h"	/* for F_CHAR16EQ, F_CHAR16IN etc. */
#include "access/ftup.h"	/* for FormHeapTuple() */
#include "utils/palloc.h"

#include "catalog/pg_proc.h"
#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2plans.h"

extern HeapTuple palloctup();
extern LispValue planner();
extern LispValue lispCopy();


/*-----------------------------------------------------------------------
 *
 * prs2DefineTupleRule
 *
 * Define a tuple-level rule. This is the high-level interface.
 *
 *-----------------------------------------------------------------------
 */

void
prs2DefineTupleRule(parseTree, ruleText)
LispValue parseTree;
char *ruleText;
{

    Prs2RuleData ruleData;
    List hint;

#ifdef PRS2_DEBUG
    printf("PRS2: ---prs2DefineTupleRule called, with argument =");
    lispDisplay(parseTree, 0);
    printf("\n");
    printf("PRS2:   ruletext = %s\n", ruleText);
#endif PRS2_DEBUG
    
    /*
     * find the rule "hint", i.e. see if the user explicitly
     * stated what kind of lock he/she/it/ wnated to use.
     */
    hint = GetRuleHintFromParse(parseTree);
    
    /*
     * extract some rule info form the parsetree...
     */
    ruleData = prs2FindRuleData(parseTree, ruleText);

    /*
     * now put locks/stubs and update system catalogs...
     */
    prs2AddTheNewRule(ruleData, hint);

#ifdef PRS2_DEBUG
    printf("PRS2: --- DEFINE PRS2 RULE: Done.\n");
#endif PRS2_DEBUG
}

/*-----------------------------------------------------------------------
 * prs2RemoveTupleRule
 *
 * Remove a tuple-system rule given its name.
 *
 *-----------------------------------------------------------------------
 */
void
prs2RemoveTupleRule(ruleName)
Name ruleName;
{
    ObjectId ruleId;

    /*
     * first find its oid & then remove it...
     */
    ruleId = get_ruleid(ruleName);
    if (ruleId == InvalidObjectId) {
	elog(WARN, "Rule '%s' does not exist!", ruleName);
    }

    prs2DeleteTheOldRule(ruleId);
}

/*-----------------------------------------------------------------------
 * prs2FindRuleData
 *
 * create a 'Prs2RuleData' struct & fill it with all the information
 * about this rule we will ever need to know...
 *
 *-----------------------------------------------------------------------
 */
Prs2RuleData
prs2FindRuleData(parseTree, ruleText)
List parseTree;
char *ruleText;
{
    Prs2RuleData r;
    List eventTarget;
    Relation rel;
    bool newUsed, currentUsed;

    /*
     * create the structure where we hold all the rule
     * information we need to know...
     */
    r = (Prs2RuleData) palloc(sizeof(Prs2RuleDataData));
    if (r==NULL) {
	elog(WARN, "prs2FindRuleInfo: Out of memory");
    }
    r->ruleName = (Name) palloc(sizeof(NameData));
    if (r->ruleName == NULL) {
	elog(WARN, "prs2FindRuleInfo: Out of memory");
    }
    r->eventRelationName = (Name) palloc(sizeof(NameData));
    if (r->eventRelationName == NULL) {
	elog(WARN, "prs2FindRuleInfo: Out of memory");
    }
    r->eventAttributeName = (Name) palloc(sizeof(NameData));
    if (r->eventAttributeName == NULL) {
	elog(WARN, "prs2FindRuleInfo: Out of memory");
    }
    
    /*
     * start filling it in...
     * NOTE: we do not know yet the rule oid...
     */
    r->ruleId = InvalidObjectId;
    r->parseTree = parseTree;
    r->ruleText = ruleText;
    strcpy(r->ruleName->data, CString(GetRuleNameFromParse(parseTree)));

    if  (CInteger(GetRuleInsteadFromParse(parseTree)) != 0)
	r->isInstead = true;
    else
	r->isInstead = false;
    r->eventType = prs2FindEventTypeFromParse(parseTree);

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
    eventTarget = GetRuleEventTargetFromParse(parseTree);
    strcpy(r->eventRelationName->data, CString(CAR(eventTarget)));
    rel = RelationNameGetRelation(r->eventRelationName);
    r->eventRelationOid = rel->rd_id;
    if (null(CDR(eventTarget))) {
	/*
	 * no attribute is specified
	 */
	r->eventAttributeNumber = InvalidAttributeNumber;
	r->eventAttributeName = NULL;
    } else {
	strcpy(r->eventAttributeName->data, CString(CADR(eventTarget)));
	r->eventAttributeNumber = get_attnum(r->eventRelationOid,
					    r->eventAttributeName);
	if (r->eventAttributeNumber == InvalidAttributeNumber) {
	    elog(WARN,"Illegal attribute in event target list");
	}
	/*
	 * XXX currently we only allow tuple-level rules to
	 * be defined in attributes of basic types (i.e. not of type
	 * relation etc....)
	 */
	if (!prs2AttributeIsOfBasicType(r->eventRelationOid,
					r->eventAttributeNumber)) {
		elog(WARN,
		"Can not define tuple rules in non-base type attributes");
	}
    }


    /*
     * check if we use CURRENT on a 'on append' rule, or NEW
     * on a 'on retrieve' or 'on delete' rule.
     * If yes, then complain...
     */
    r->ruleQual = GetRuleQualFromParse(parseTree);
    r->ruleAction = GetRuleActionFromParse(parseTree);
    IsPlanUsingNewOrCurrent(r->ruleQual, &currentUsed, &newUsed);
    if (currentUsed && r->eventType == EventTypeAppend) {
	elog(WARN,
	    "An `on append' rule, can not reference the 'current' tuple");
    }
    if (newUsed && r->eventType == EventTypeRetrieve) {
	elog(WARN,
	    "An `on retrieve' rule, can not reference the 'new' tuple");
    }
    if (newUsed && r->eventType == EventTypeDelete) {
	elog(WARN,
	    "An `on delete' rule, can not reference the 'new' tuple");
    }
    IsPlanUsingNewOrCurrent(r->ruleAction, &currentUsed, &newUsed);
    if (currentUsed && r->eventType == EventTypeAppend) {
	elog(WARN,
	    "An `on append' rule, can not reference the 'current' tuple");
    }
    if (newUsed && r->eventType == EventTypeRetrieve) {
	elog(WARN,
	    "An `on retrieve' rule, can not reference the 'new' tuple");
    }
    if (newUsed && r->eventType == EventTypeDelete) {
	elog(WARN,
	    "An `on delete' rule, can not reference the 'new' tuple");
    }

    /*
     * The parsetree generated by the parser has a NEW and/or CURRENT
     * entries in the range table. In order to find the rule qualification
     * and the action plans to be executed at rule activation time,
     * we have to change all the corresponding "Var" nodes to the
     * appropriate "Param" nodes.
     * Note however, that we must also keep the original rule qualification 
     * (i.e. the one with the "Var" nodes) because it is needed to
     * calculate the appropriate stub records etc.
     * nodes to the equivalent
     */
    r->paramParseTree = lispCopy(parseTree);
    SubstituteParamForNewOrCurrent(r->paramParseTree, r->eventRelationOid);
    r->paramRuleQual = GetRuleQualFromParse(r->paramParseTree);
    r->paramRuleAction = GetRuleActionFromParse(r->paramParseTree);

    /*
     * Now, find the type of action. (i.e. ActionTypeRetrieve, 
     * ActionTypeReplaceCurrent or ActionTypeOther).
     * In the first 2 cases,  `updatedAttributeNumber' is the number
     * of attribute beeing updated.
     */
    r->actionType = prs2FindActionTypeFromParse(r->parseTree,
			r->eventAttributeNumber,
			&(r->updatedAttributeNumber),
			r->eventType);
    /*
     * Hm... for the time being we do NOT allow 
     *     ON RETRIEVE ... DO RETRIEVE ....
     * rules without an INSTEAD
     * unless this is a "view" rule (i.e. something
     * like "on retrieve to TOYEMP do retrieve...")
     * In this case the event target object must be a
     * relation and not a "relation.attribute"
     */
    if (!(r->isInstead) && r->actionType == ActionTypeRetrieveValue &&
	r->eventAttributeNumber != InvalidAttributeNumber ) {
	elog(WARN,
	"`on retrieve ... do retrieve' tuple rules must have an `instead'");
    }


#ifdef PRS2_DEBUG
    {
    printf("PRS2: ---DEFINE TUPLE RULE:\n");
    printf("PRS2:    RuleName = %s\n", r->ruleName->data);
    printf("PRS2:    event type = %d ", r->eventType);
    switch (r->eventType) {
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
    printf("PRS2:    event target:");
    lispDisplay(eventTarget, 0);
    printf("\n");
    printf("PRS2:    event target relation OID = %ld\n", r->eventRelationOid);
    printf("PRS2:    event target attr no = %d\n", r->eventAttributeNumber);
    printf("PRS2:    Instead flag = %d\n", r->isInstead);
    printf("PRS2:    ruleAction:");
    lispDisplay(r->paramRuleAction);
    printf("\n");
    }
#endif PRS2_DEBUG
    
    return(r);
}


/*-----------------------------------------------------------------------
 *
 * prs2InsertRuleInfoInCatalog
 *
 * Insert information about a rule in the rules system catalog.
 * As the rule names are unique, we have first to make sure
 * that no other rule with the same names exists.
 *
 *-----------------------------------------------------------------------
 */
ObjectId
prs2InsertRuleInfoInCatalog(r)
Prs2RuleData r;
{
    register		i;
    Relation		prs2RuleRelation;
    HeapTuple		heapTuple;
    char		*values[Prs2RuleRelationNumberOfAttributes];
    char		nulls[Prs2RuleRelationNumberOfAttributes];
    ObjectId		objectId;
    HeapScanDesc	heapScan;
    ScanKeyEntryData	ruleNameKey[1];


    
    /*
     * Open the system catalog relation
     * where we keep the rule info (pg_prs2rule)
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
    ruleNameKey[0].argument = NameGetDatum(r->ruleName);

    heapScan = RelationBeginHeapScan(prs2RuleRelation, false, NowTimeQual,
				     1, (ScanKey) ruleNameKey);
    heapTuple = HeapScanGetNextTuple(heapScan, false, (Buffer *) NULL);
    if (HeapTupleIsValid(heapTuple)) {
	HeapScanEnd(heapScan);
	RelationCloseHeapRelation(prs2RuleRelation);
	elog(WARN, "Sorry, rule %s is already defined",
	     r->ruleName);
    }
    HeapScanEnd(heapScan);

    /*
     * Now create a tuple with the new rule info
     */
    for (i = 0; i < Prs2RuleRelationNumberOfAttributes; ++i) {
	values[i] = NULL;
	nulls[i] = 'n';
    }

    values[Prs2RuleNameAttributeNumber-1] =
			    fmgr(F_CHAR16IN,(char *)(r->ruleName));
    nulls[Prs2RuleNameAttributeNumber-1] = ' ';

    values[Prs2RuleEventTypeAttributeNumber-1] = (char *) r->eventType;
    nulls[Prs2RuleEventTypeAttributeNumber-1] = ' ';

    values[Prs2RuleEventTargetRelationAttributeNumber-1] =
			(char *) r->eventRelationOid;
    nulls[Prs2RuleEventTargetRelationAttributeNumber-1] = ' ';

    values[Prs2RuleEventTargetAttributeAttributeNumber-1] = 
			(char *) r->eventAttributeNumber;
    nulls[Prs2RuleEventTargetAttributeAttributeNumber-1] = ' ';

    values[Prs2RuleTextAttributeNumber-1] = 
			fmgr(F_TEXTIN, r->ruleText);
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
 * prs2DeleteRuleInfoFromCatalog
 *
 * remove all rule info stored in 'pg_prs2rule' about the given
 * rule.
 *
 *-----------------------------------------------------------------------
 */
void
prs2DeleteRuleInfoFromCatalog(ruleId)
ObjectId ruleId;
{
    Relation prs2RuleRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;

    /*
     * Open the pg_rule relation. 
     */
    prs2RuleRelation = RelationNameOpenHeapRelation(Prs2RuleRelationName);

     /*
      * Scan the RuleRelation ('pg_prs2rule') until we find a tuple
      */
    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = ObjectIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(ruleId);
    scanDesc = RelationBeginHeapScan(prs2RuleRelation,
				    false, NowTimeQual, 1, &scanKey);

    tuple = HeapScanGetNextTuple(scanDesc, false, &buffer);

    /*
     * complain if no such rule existed
     */
    if (!HeapTupleIsValid(tuple)) {
	RelationCloseHeapRelation(prs2RuleRelation);
	elog(WARN, "No rule with id = '%d' was found.\n", ruleId);
    }

    /*
     * Now delete the tuple...
     */
    RelationDeleteHeapTuple(prs2RuleRelation, &(tuple->t_ctid));
    RelationCloseHeapRelation(prs2RuleRelation);
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
 *
 *-----------------------------------------------------------------------
 */
void
prs2InsertRulePlanInCatalog(ruleId, planNumber, rulePlan)
ObjectId ruleId;
Prs2PlanNumber planNumber;
List rulePlan;
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
    pfree(rulePlanString);

}

/*--------------------------------------------------------------------------
 * prs2DeleteRulePlanFromCatalog
 *
 * Delete all rule plans for the given rule from the "pg_prs2plans"
 * system catalog.
 *--------------------------------------------------------------------------
 */
void
prs2DeleteRulePlanFromCatalog(ruleId)
ObjectId ruleId;
{
    Relation prs2PlansRelation;
    HeapScanDesc scanDesc;
    ScanKeyData scanKey;
    HeapTuple tuple;
    Buffer buffer;

    /*
     * Delete all tuples of the Prs2Plans relation (pg_prs2plans)
     * corresponding to this rule...
     */
    prs2PlansRelation = RelationNameOpenHeapRelation(Prs2PlansRelationName);

    scanKey.data[0].flags = 0;
    scanKey.data[0].attributeNumber = Prs2PlansRuleIdAttributeNumber;
    scanKey.data[0].procedure = ObjectIdEqualRegProcedure;
    scanKey.data[0].argument = ObjectIdGetDatum(ruleId);
    scanDesc = RelationBeginHeapScan(prs2PlansRelation,
				    false, NowTimeQual, 1, &scanKey);


    while((tuple=HeapScanGetNextTuple(scanDesc,false,(Buffer *)NULL)) !=NULL) {
	/*
	 * delete the prs2PlansRelation tuple...
	 */
	RelationDeleteHeapTuple(prs2PlansRelation, &(tuple->t_ctid));
    }
	
    RelationCloseHeapRelation(prs2PlansRelation);

}

/*----------------------------------------------------------------------
 *
 * prs2GenerateActionPlans
 *
 * generate the plan+parse trees for the action part of the rule
 *
 *----------------------------------------------------------------------
 */
LispValue
prs2GenerateActionPlans(r)
Prs2RuleData r;
{
    LispValue result;
    LispValue action;
    LispValue oneParse;
    LispValue onePlan;
    LispValue oneEntry;
    LispValue qualQuery;
    LispValue ruleInfo;
    LispValue rangeTable;
    AttributeNumber currentAttributeNo;


    rangeTable = GetRuleRangeTableFromParse(r->parseTree);

    /*
     * the first entry in the result is some rule info
     * This consists of the following fields:
     * a) "instead" or "not-instead"
     * b) the attribute number of the attribute specified
     *    in the ON EVENT TO REL.attribute clause
     *    (or InvalidAttributeNumber)
     * c) either InvalidAttributeNumber, or in the case of a rule
     *    of the form:
     *       ON REPLACE TO REL.x WHERE ....
     *       DO REPLACE CURRENT.attribute
     *    the 'attribute' that is replced by the rule.
     */
    if (r->isInstead) {
	ruleInfo = lispCons(lispString("instead"), LispNil);
    } else {
	ruleInfo = lispCons(lispString("not-instead"), LispNil);
    }

    ruleInfo = nappend1(ruleInfo, lispInteger(r->eventAttributeNumber));
    ruleInfo = nappend1(ruleInfo, lispInteger(r->updatedAttributeNumber));

    result = lispCons(ruleInfo, LispNil);


    /*
     * now append the qual entry (parse tree + plan)
     */
    if (null(r->ruleQual)) {
	oneEntry = LispNil;
    } else {
	/*
	 * the 'ruleQual' is just a qualification.
	 * Transform it into a query of the form:
	 * "retrieve (foo=1) where QUAL".
	 * The rule manager will run this query and if there is 
	 * a tuple returned, then we know that the qual is true,
	 * otherwise we know it is false
	 */
	LispValue MakeRoot();
	LispValue root, targetList;
	Resdom resdom;
	Const constant;
	Name name;
	Datum value;

	/*
	 * construct the target list
	 */
	name = (Name) palloc(sizeof(NameData));
	if (name == NULL) {
	    elog(WARN, "prs2GenerateActionPlans: Out of memory");
	}
	strcpy(name->data,"foo");
	resdom = MakeResdom((AttributeNumber)1,
			    (ObjectId) 23,
			    (Size) 4,
			    name,
			    (Index) 0,
			    (OperatorTupleForm) 0,
			    0);
	value = Int32GetDatum(1);
	constant = MakeConst((ObjectId) 23,	/* type */
			    (Size) 4,		/* size */
			    value,		/* value */
			    false,		/* isnull */
			    true);		/* byval */
	targetList = lispCons(
			lispCons(resdom, lispCons(constant, LispNil)),
			LispNil);
	/*
	 * now the root
	 * XXX NumLevels == 1 ??? IS THIS CORRECT ????
	 */
	root = MakeRoot(
			1,			/* num levels */
			lispAtom("retrieve"),	/* command */
			LispNil,		/* result relation */
			rangeTable,		/* range table */
			lispInteger(0),		/* priority */
			LispNil,		/* rule info */
			LispNil,		/* unique flag */
			LispNil,		/* sort_clause */
			targetList);		/* targetList */
	/*
	 * Finally, construct the parse tree...
	 */
	qualQuery = lispCons(root,
			lispCons(targetList,
			    lispCons(r->paramRuleQual, LispNil)));
	onePlan = planner(qualQuery);
	oneEntry = lispCons(onePlan, LispNil);
	oneEntry = lispCons(qualQuery, oneEntry);
    }
    result = nappend1(result, oneEntry);

    /*
     * Now for each action append the corresponding entry
     * (parse tree + plan)
     */
    foreach (action, r->paramRuleAction) {
	oneParse = CAR(action);
	/*
	 * XXX: THIS IS A HACK !!!
	 *      (but it works, so what the hell....) 
	 *
	 * if this is a REPLACE CURRENT (X = ...)
	 * change it to a RETRIEVE (X = ...)
	 */
	changeReplaceToRetrieve(oneParse);
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

/*------------------------------------------------------------------
 *
 * prs2FindEventTypeFromParse
 *
 * Given a rule's parse tree find its event type.
 *------------------------------------------------------------------
 */
EventType
prs2FindEventTypeFromParse(parseTree)
LispValue parseTree;
{

    int eventTypeInt;
    EventType eventType;

    eventTypeInt = CInteger(GetRuleEventTypeFromParse(parseTree));

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

    return(eventType);
}

/*------------------------------------------------------------------
 *
 * prs2FindActionTypeFromParse
 *
 * find the ActionType of a rule.
 *
 * if the action is in the form 'REPLACE CURRENT(x = ....)'
 * then (*updatedAttributeNumber) is the attribute number of the
 * updated field, otherwise it is InvalidAttributeNumber.
 *------------------------------------------------------------------
 */
ActionType
prs2FindActionTypeFromParse(parseTree,
			    eventTargetAttributeNumber,
			    updatedAttributeNumberP,
			    eventType)
LispValue parseTree;
AttributeNumber eventTargetAttributeNumber;
AttributeNumberPtr updatedAttributeNumberP;
EventType eventType;
{
    
    LispValue ruleActions;
    LispValue t, oneRuleAction;
    LispValue root;
    int commandType;
    LispValue resultRelation;
    LispValue rangeTable;
    ActionType actionType;
    Name relationName;
    NameData nameData;
    LispValue targetList;
    int resultRelationNo;
    LispValue resultRelationEntry;

    *updatedAttributeNumberP = InvalidAttributeNumber;

    ruleActions = GetRuleActionFromParse(parseTree);

    if (null(ruleActions)) {
	if (eventType != EventTypeRetrieve) {
	    /*
	     * then this is probably an `instead' rule with no action
	     * specified, e.g. "ON delete to DEPT WHERE ... DO INSTEAD"
	     */
	    return(ActionTypeOther);
	} else {
	    /*
	     * However, if we have something like:
	     * ON RETRIEVE THEN DO INSTEAD NOTHING
	     * we assume that this means do NOT retrieve a value,
	     * i.e. return a null attribute.
	     */
	    *updatedAttributeNumberP = eventTargetAttributeNumber;
	    return(ActionTypeRetrieveValue);
	}
    }

    foreach(t, ruleActions) {
	oneRuleAction = CAR(t);
	/*
	 * find the type of query (retrieve, delete etc...)
	 */
	root = parse_root(oneRuleAction);
	commandType = root_command_type(root);
	resultRelation = root_result_relation(root);
	rangeTable = root_rangetable(root);
	if (!null(resultRelation)) {
	    resultRelationNo = CInteger(resultRelation);
	    resultRelationEntry = nth(resultRelationNo-1, rangeTable);
	    strcpy(&(nameData.data[0]),
		    CString(rt_relname(resultRelationEntry)));
	    relationName = &nameData;
	} else {
	    relationName = InvalidName;
	}
	if (commandType == RETRIEVE && null(resultRelation)) {
	    /*
	     * this is a "retrieve" (NOT a "retrieve into...")
	     * action.
	     * It can be either something like:
	     *    ON retrieve to EMP.salary 
	     *    WHERE ....
	     *    DO retrieve (salary = ....) where .....
	     *
	     * In which case only one 'retrieve' command is allowed
	     * in the action part of the rule, 
	     * or it can be a view type rule:
	     *    ON retrieve to TOYEMP
	     *    DO retrieve (EMP.all) where EMP.dept = "toy"
	     */
	    actionType = ActionTypeRetrieveValue;
	    break; 	/* exit 'foreach' loop */

	} else if (commandType == REPLACE &&
		    NameIsEqual("*CURRENT*", relationName)) {
	    /*
	     * this is a replace CURRENT(...)
	     */
	    actionType = ActionTypeReplaceCurrent;
	    /*
	     * find the updated attribute number...
	     */
	    targetList = parse_targetlist(oneRuleAction);
	    if (length(targetList) != 1) {
		elog(WARN,
		" a 'replace CURRENT(...)' must replace ONLY 1 attribute");
	    }
	    *updatedAttributeNumberP = get_resno(tl_resdom(CAR(targetList)));
	    break; 	/* exit 'foreach' loop */
	} else if (commandType == REPLACE &&
		    NameIsEqual("*NEW*", relationName)) {
	    /*
	     * this is a replace NEW(...)
	     */
	    actionType = ActionTypeReplaceNew;
	    /*
	     * find the updated attribute number...
	     */
	    targetList = parse_targetlist(oneRuleAction);
	    if (length(targetList) != 1) {
		elog(WARN,
		" a 'replace NEW(...)' must replace ONLY 1 attribute");
	    }
	    *updatedAttributeNumberP = get_resno(tl_resdom(CAR(targetList)));
	    break; 	/* exit 'foreach' loop */
	} else {
	    actionType = ActionTypeOther;
	}
    } /* foreach */

    if (actionType == ActionTypeRetrieveValue) {
	/*
	 * then this must be the ONLY statement in the rule actions!
	 */
	if (length(ruleActions) != 1) {
	    elog(WARN,
	    "a 'retrieve (..)' must be the only action of a PRS2 rule!");
	}
	*updatedAttributeNumberP = eventTargetAttributeNumber;
    }
	
    if (actionType == ActionTypeReplaceCurrent) {
	/*
	 * then this must be the ONLY statement in the rule actions!
	 */
	if (length(ruleActions) != 1) {
	    elog(WARN,
	    "a 'replace CURRENT(..)' must be the only action of a PRS2 rule!");
	}
    }

    if (actionType == ActionTypeReplaceNew) {
	/*
	 * then this must be the ONLY statement in the rule actions!
	 */
	if (length(ruleActions) != 1) {
	    elog(WARN,
	    "a 'replace NEW(..)' must be the only action of a PRS2 rule!");
	}
    }

    return(actionType);
}

/*----------------------------------------------------------------
 *
 * changeReplaceToRetrieve
 *
 * Ghange the parse tree of a 'REPLACE CURRENT(X = ...)'
 * or 'REPLACE NEW(X = ...)' command to a
 * 'RETRIEVE (X = ...)'
 *----------------------------------------------------------------
 */
void
changeReplaceToRetrieve(parseTree)
LispValue parseTree;
{

    LispValue root;
    LispValue targetList;
    int commandType;
    LispValue resultRelation;
    LispValue rangeTable;
    Name relationName;
    NameData nameData;
    int resultRelationNo;
    LispValue resultRelationEntry;


    root = parse_root(parseTree);
    commandType = root_command_type(root);
    resultRelation = root_result_relation(root);
    rangeTable = root_rangetable(root);
    if (!null(resultRelation)) {
	resultRelationNo = CInteger(resultRelation);
	resultRelationEntry = nth(resultRelationNo-1, rangeTable);
	strcpy(&(nameData.data[0]),
		CString(rt_relname(resultRelationEntry)));
	relationName = &nameData;
    } else {
	relationName = InvalidName;
    }

    /*
     * Is this a REPLACE CURRENT command?
     */
    if (commandType!=REPLACE ||
       (!NameIsEqual("*CURRENT*", relationName) &&
       !NameIsEqual("*NEW*", relationName))) {
	/*
	 * NO, this is not a REPLACE CURRENT or a REPLACE NEW command
	 */
	return;
    }

    /*
     * Yes it is!
     *
     * Now *DESTRUCTIVELY* change the parse tree...
     * 
     * Change the command from 'replace' to 'retrieve'
     * and the result relation to 'nil'
     */
    root = parse_root(parseTree);
    root_command_type_atom(root) = lispAtom("retrieve");
    root_result_relation(root) = LispNil;

    /*
     * Now go to the target list (which must have exactly 1 Resdom
     * node, and change the 'resno' to 1
     */
    targetList = parse_targetlist(parseTree);
    set_resno(tl_resdom(CAR(targetList)), (AttributeNumber)1);
}

/*-----------------------------------------------------------------
 *
 * prs2AttributeIsOfBasicType
 *
 * check if the given attribute is of a "basic" type.
 *-----------------------------------------------------------------
 */
bool
prs2AttributeIsOfBasicType(relOid, attrNo)
ObjectId relOid;
AttributeNumber attrNo;
{
    ObjectId typeId;
    char typtype;

    typeId = get_atttype(relOid, attrNo);
    typtype = get_typtype(typeId);
    if (typtype == '\0') {
	elog(WARN, "Cache lookup for type %ld failed...", typeId);
    }

    if (typtype == 'b') {
	return(true);
    } else {
	return(false);
    }
}

/*=============================== DEBUGGINF STUFF ================*/
LispValue
prs2ReadRule(fname)
char *fname;
{
    FILE *fp;
    int c;
    int count, maxcount;
    char *s1, *s2;
    LispValue l;
    LispValue StringToPlan();

    printf("--- prsReadRule.\n");
    AllocateFile();
    if (fname==NULL) {
	fp = stdin;
	printf(" reading rule from stdin.\n");
    } else {
	AllocateFile();
	fp = fopen(fname, "r");
	if (fp==NULL){
	    printf(" Can not open rule file %s\n", fname);
	    elog(WARN,"prs2ReadRule.... exiting...\n");
	} else {
	    printf("Reading rule from file %s\n", fname);
	}
    }

    maxcount = 100;
    s1 = palloc(maxcount);
    if (s1 == NULL) {
	elog(WARN,"prs2ReadRule : out of memory!");
    }

    count = 0;
    while((c=getc(fp)) != EOF) {
	if (count >= maxcount) {
	    maxcount = maxcount + 1000;
	    s2 = palloc(maxcount);
	    if (s2 == NULL) {
		elog(WARN,"prs2ReadRule : out of memory!");
	    }
	    bcopy(s1, s2, count);
	    pfree(s1);
	    s1 = s2;
	}
	s1[count] = c;
	count++;
    }
    s1[count+1] = '\0';

    l = StringToPlan(s1);

    printf("  list = \n");
    lispDisplay(l,0);

    if (fp!=stdin) {
	fclose(fp);
	FreeFile();
    }

    return(l);
}
