
/*====================================================================
 *
 * FILE: prs2tup.c
 *
 * $Header$
 *
 * DESCRIPTION:
 * Routines to insert/remove "tuple-level" locks in the tuples of a relation.
 *
 *====================================================================
 */

#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "utils/fmgr.h"
#include "executor/execdefs.h"	/* for the EXEC_RESULT */
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "rules/rac.h"
#include "parser/parsetree.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "utils/lsyscache.h"
#include "utils/palloc.h"

extern HeapTuple palloctup();
extern LispValue cnfify();
extern bool IsPlanUsingCurrentAttr();
extern LispValue FindVarNodes();
Prs2Stub prs2FindStubsThatDependOnAttribute();

/*--------------------------------------------------------------------
 *
 * prs2DefTupleLevelLockRule
 *
 * Read the following at your own risk... If you are not me, then
 * you have my sympathy.
 * 
 * This routine puts "tuple-level" locks to the tuple of a relation.
 *
 * "constQual" is a qualification which involves only
 * attributes of the CURRENT relation (i.e. of the relation to be locked)
 * and constants. (i.e. it has no joins, etc.).
 *
 * Now the tricky part... Which tuples to lock ????
 * Yes, I know, you think that's a trivial question, right?
 * We only need to lock the tuples that satisfy the `constQual', eh?
 * Well, I have bad news for you, my dear hacker, who were naive enough
 * to be dragged into the gloomy world of rules.
 * Consider the following example:
 *
 * RULE 1:	on retrieve to EMP.salary
 *		where current.name = "spyros"
 *		do instead retrieve (salary=1)
 *
 * RULE 2:	on retrieve to EMP.desk
 *		where current.salary = 1
 *		do instead retrieve (desk = "what desk?")
 *
 * Lets say that we have a tuple:
 *   [ NAME	SALARY	DESK  ]
 *   [ spyros	2	steel ]
 * Well, if we define RULE_1 first and RULE_2 second, everything is
 * OK. However, if we define RULE_2 first, then we will NOT put
 * a lock to spyros' tuple, because his salary is 2.
 * So, when later on we define RULE_1 we must put in spyros' tuple not
 * only the locks of RULE_1, but also the locks for RULE_2!
 *
 * To make things even worse, lets rewrite rule 1:
 * RULE 1:	on retrieve to EMP.salary
 *		where current.name = "spyros"
 *		do instead retrieve (E.salary) where E.name = "mike"
 * And lets say that initially mike has a salary equal to 2.
 * Now even if we define RULE_1 first and RULE_2 second, still
 * spyros' tuple will not get any locks. If later on we change mike's
 * salary from 2 to 1, then we are in trouble....
 *
 * So, to cut a long story longer:
 * a) we have to lock all the tuples that satisfy 'constQual' OR
 *    have a "write" lock in any attribute referenced by this 'constQual'
 * b) when we put a lock to a tuple, then if this lock is a "write"
 *    lock (i.e. if our new rule is something like "on retrieve ... do
 *    retrieve" we have also to check all the relation level stub records.
 *    For every stub if its qualification references the attribute
 *    we are currently putting this "write" lock on, then we have also to
 *    add to this tuple the stub's lock...
 *
 *
 * NOTE: If `hintFlag' is false, then we will try to use a tuple level
 * lock, but if we believe that that is not such a good idea after all
 * we return 'false' without doing anything (to indicate that
 * we shopuld use a relation level lock).
 * If 'hintFlag' is true however we go ahead & use a tuple level lock,
 * even if this is going to be really inefficient...
 *--------------------------------------------------------------------
 */

bool
prs2DefTupleLevelLockRule(r, hintFlag)
Prs2RuleData r;
bool hintFlag;
{
    LispValue actionPlans;
    LispValue constQual, planQual;
    Relation rel;
    RuleLock relLocks;
    TupleDescriptor tdesc;
    HeapScanDesc scanDesc;
    HeapTuple tuple, newTuple;
    Buffer buffer;
    Prs2Stub oldStubs;
    RuleLock newLocks, thislock, tlocks, newtlocks;
    AttributeNumber *attrList;
    int attrListSize;
    Prs2OneStub oneStub;
    Prs2LockType lockType;
    AttributeNumber attributeNo;

    /*
     * First find the constant qual, i.e.a qualification
     * that only involves constants & attributes of the
     * "current" tuple.
     * NOTE: we assume that the varno of the current tuple
     * is a constant (this is hardcoded in the parser).
     */
    constQual = prs2FindConstantQual(r->ruleQual, PRS2_CURRENT_VARNO);

    /*
     * If this qual is null, then use relation level locks.
     * Return 'false' to indicate that it is not a good idea to
     * use tuple level locks.
     */
    if (!hintFlag && null(constQual)) {
	return(false);
    }

    /*
     * Find all the attributes of the CURRENT tupel 
     * referenced by the the constant qual.
     */
    prs2FindAttributesOfQual(constQual, PRS2_CURRENT_VARNO, 
			    &attrList, &attrListSize);

    /*
     * Another reason we should use relation level locks, is if the
     * 'constQual' references an attribute which is calculated by
     * a rule using a relation-level lock. (i.e. if the attribute
     * has a relation level "LockTypeTupleRetrieveWrite" lock.
     */
    rel = ObjectIdOpenHeapRelation(r->eventRelationOid);
    relLocks = prs2GetLocksFromRelation(RelationGetRelationName(rel));

    if (prs2LockWritesAttributes(relLocks, attrList, attrListSize)) {
	if (!hintFlag) {
	    /*
	     * ooops! we have to use a relation level lock.
	     */
	    RelationCloseHeapRelation(rel);
	    return(false);
	} else {
	    /*
	     * we can't use a relation level lock, because the user
	     * wants us to use a tuple level lock (stupid user...)
	     * So lets do it!
	     * However as we have to lock ALL the tuples currently in
	     * the relation and make suer that ALL other tuples
	     * appended to the relation will also get the lock,
	     * we have to change the 'constQual' to 'nil' (which
	     * always evaluates to true).
	     */
	    constQual = LispNil;
	}
    }

    /*
     * OK, go ahead & use tuple level locks....
     *
     * First add the appropriate info to the system catalogs
     * Start with 'pg_prs2rule'... This will give us back the rule oid.
     */
    r->ruleId = prs2InsertRuleInfoInCatalog(r);
    elog(DEBUG,
	"Rule %s (id=%ld) will be implemented using tuple-level-locks",
	r->ruleName, r->ruleId);

    /*
     * Now generate & append the appropriate rule plans in the
     * 'pg_prs2plans' relation.
     * NOTE: 'ActionPlanNUmber' is a constant...
     */
    actionPlans = prs2GenerateActionPlans(r);
    prs2InsertRulePlanInCatalog(r->ruleId, ActionPlanNumber, actionPlans);

    /*
     * Find what type of lock to use and in which attribute to put it
     */
    prs2FindLockTypeAndAttrNo(r, &lockType, &attributeNo);

    /*
     * OK, start putting the rule locks in the tuples of the relation.
     * Yes, yes, we are going to do a sequential scan of the
     * relation, checking ALL the tuples one by one, and if you don't
     * like it, so what? It works and no planner can do better
     * (remember, it is not enough to test the 'constQual'!)
     */
    tdesc = RelationGetTupleDescriptor(rel);
    scanDesc = RelationBeginHeapScan(rel, false, NowTimeQual, 0, NULL);

    /*
     * create the new lock (for the new rule).
     */
    thislock = prs2MakeLocks();
    thislock = prs2AddLock(thislock, r->ruleId, lockType,
			attributeNo, ActionPlanNumber);

    /*
     * Find the locks that we must put to the tuple.
     * These locks include of course the 'newLock' but they might also
     * inlcude the locks of all the stubs that "depend" on the
     * attribute locked by 'newlock' iff of course this is a
     * "write" lock...
     */
    oldStubs = prs2GetRelationStubs(r->eventRelationOid);
    if (lockType == LockTypeTupleRetrieveWrite)
	newLocks = prs2FindLocksThatWeMustPut(oldStubs, thislock, attributeNo);
    else
	newLocks = prs2CopyLocks(thislock);

    /*
     * Now scan one by one the tuples & put locks to all the
     * tuples that satisfy the qualification, or have a "write"
     * tuple level lock to an attribute referenced by the current
     * "constQual"
     *
     * NOTE: the 'constQual' is the qualification in a 'parsetree'
     * format. In order to call 'prs2StubQualTestTuple' (which in turns
     * calls the executor) we must change it to 'plan' format.
     */
    if (null(constQual)) {
	planQual = LispNil;
    } else {
	planQual = cnfify(constQual,true);
	fix_opids(planQual);
    }

    while((tuple = HeapScanGetNextTuple(scanDesc, false, &buffer)) != NULL) {
	/*
	 * shall we put a lock to this tuple?
	 */
	tlocks = HeapTupleGetRuleLock(tuple, buffer);
	/*
	 * NOTE: the first test (planQual==lispNil) is not really
	 * required, because 'prs2StubQualTestTuple' will succeed
	 * if called with a nil qual, but we do it for efficiency
	 * (what a joke!)
	 */
	if (planQual == LispNil
	   ||prs2LockWritesAttributes(tlocks, attrList, attrListSize)
	   || prs2StubQualTestTuple(tuple, buffer, tdesc, planQual)) {
	    /*
	     * yes!
	     * Create a new tuple (i.e. a copy of the old tuple with the
	     * new lock, and replace the old tuple).
	     * NOTE: do we really need to make that copy ???
	     */
	    newTuple = palloctup(tuple, buffer, rel);
	    newtlocks = prs2LockUnion(tlocks, newLocks);
	    prs2PutLocksInTuple(newTuple, InvalidBuffer, rel, newtlocks);
	    RelationReplaceHeapTuple(rel, &(tuple->t_ctid),
					newTuple, (double *)NULL);
	    /*
	     * Free all tuples/locks.
	     * Do NOT free `tuple' because it is probably a pointer
	     * to a buffer page and not to 'palloced' data.
	     * But free 'newtlocks' and 'newTuple' because the are
	     * guaranteed to be palloced.
	     * We should also free 'tlocks' because 'HeapTupleGetRuleLock'
	     * returns a palloced memory representation of locks...
	     */
	    prs2FreeLocks(newtlocks);
	    prs2FreeLocks(tlocks);
	    pfree(newTuple);
	}
    }

    /*
     * But don't forget to put the relation level stub !
     * NOTE: we can not use the qualification as produced by the
     * parser. We have to preprocess it a little bit...
     *
     * NOTE: the lock of this stub is the original lock for this
     * rule only and NOT the locks that we put in the
     * tuples (which contain the original lock + locks
     * for all other rules that depend on the one we define)!
     */
    oneStub = prs2MakeOneStub();
    oneStub->ruleId = r->ruleId;
    oneStub->stubId = (Prs2StubId) 0;
    oneStub->counter = 1;
    oneStub->lock = thislock;
    oneStub->qualification = planQual;
    prs2AddRelationStub(rel, oneStub);

    /*
     * OK, we are done, cleanup & return true...
     */
    RelationCloseHeapRelation(rel);
    if (attrList != NULL)
	pfree(attrList);
    pfree(thislock);
    pfree(newLocks);
    return(true);
}

/*----------------------------------------------------------------------
 * prs2FindLocksThatWeMustPut
 *
 * (I like that name.... I couldn't find a better one though...)
 *
 * Given some relation stubs and that the given attribute
 * is going to be locked by a 'LockTypeReplaceWrite' lock,
 * find all the new locks that the tuple must get.
 * 
 * More precisely when we add such a lock to an attribute,
 * we have to find all the stubs that their qualification
 * references this attribute, and add their locks to the se
 * of the new locks for that tuple.
 * As these locks can be of 'LockTypeReplaceWrite' we have
 * to repeat this procedure for every such new lock.
 *
 *----------------------------------------------------------------------
 */
RuleLock
prs2FindLocksThatWeMustPut(stubs, newlock, attrno)
Prs2Stub stubs;
RuleLock newlock;
AttributeNumber attrno;
{
    RuleLock resultLocks;
    RuleLock thisLock;
    Prs2Stub s;
    int i;


    /*
     * find the stubs that depend on this 'attrno'
     */
    s = prs2FindStubsThatDependOnAttribute(stubs, attrno);

    /*
     * the locks that we must finally put is the 'newlock'
     * and all the locks of the stubs that depend on this
     * attribute.
     */
    resultLocks = prs2CopyLocks(newlock);
    for (i=0; i<s->numOfStubs; i++) {
	thisLock = s->stubRecords[i]->lock;
	resultLocks = prs2LockUnion(resultLocks, thisLock);
    }

    return(resultLocks);
}
	
/*------------------------------------------------------------------------
 * prs2FindStubsThatDependOnAttribute
 *
 * Given some stub records, return all the stubs that "depend" on the
 * given attribute.
 * A stub "depends" on the attribute if
 * a) it references in its qualification this attribute
 * b) refernces an attribute, where another second stub might place
 * a "write" lock, and this second stub "depends" on the original
 * attribute.
 *------------------------------------------------------------------------
 */
Prs2Stub
prs2FindStubsThatDependOnAttribute(stubs, attrno)
Prs2Stub stubs;
AttributeNumber attrno;
{
    
    bool *stubUsed;
    int i, j;
    Prs2OneStub thisStub;
    RuleLock thisStubLock;
    Prs2Stub result;
    List attrlist;
    AttributeNumber thisattrno;

    /*
     * 'stubUsed' is an array whith one entry per stub record.
     * If the entry is true, then we have already used this
     * stub record, so there is no need to 
     * check it again....
     */
    if (stubs->numOfStubs > 0) {
	stubUsed = (bool *) palloc(sizeof(bool) * stubs->numOfStubs);
	for (i=0; i<stubs->numOfStubs; i++) {
	    stubUsed[i] = false;
	}
    }

    /*
     * 'result' is the stubs that depend on "attrno"
     * 'attrList' is a list of the attributes to be examined.
     */
    result = prs2MakeStub();

    /*
     * I hate that, but I had to use a lisp list!
     */
    attrlist = lispCons(lispInteger((int)attrno), LispNil);

    /*
     * now the loop... One by one examine all the stubs.
     * and add the appropriate stubs to "result".
     */
    while (!null(attrlist)) {
	/*
	 * get the first attribute to be examined, & remove it
	 * from the list
	 */
	thisattrno = (AttributeNumber) CInteger(CAR(attrlist));
	attrlist = CDR(attrlist);
	for (i=0; i<stubs->numOfStubs; i++) {
	    if (!stubUsed[i]) {
		/*
		 * does this stub depend on the attribute
		 * we are currently examining ?
		 */
		thisStub = stubs->stubRecords[i];
		thisStubLock = thisStub->lock;
		if (prs2DoesStubDependsOnAttribute(thisStub, thisattrno)) {
		    /*
		     * add this stub to the 'result'
		     */
		    prs2AddOneStub(result, thisStub);
		    stubUsed[i] = true;
		    /*
		     * if the stub locks contain a  "write" lock,
		     * add the locked attribute to the list
		     * of attributes to be examined...
		     */
		    for (j=0; j<prs2GetNumberOfLocks(thisStubLock); j++){
			Prs2OneLock l;
			Prs2LockType lt;
			AttributeNumber a;
			l = prs2GetOneLockFromLocks(thisStubLock,j);
			lt = prs2OneLockGetLockType(l);
			a = prs2OneLockGetAttributeNumber(l);
			if (lt==LockTypeTupleRetrieveWrite) {
			    attrlist = lispCons(lispInteger((int)a),attrlist);
			}
		    }/*for*/
		}/*if*/
	    }/*if*/
	} /* for */
    } /* while */

    /*
     * clean up...
     */
    if (stubs->numOfStubs > 0) 
	pfree(stubUsed);

     /*
      * return the result stubs
      */
    return(result);
}
	

/*------------------------------------------------------------------------
 * prs2DoesStubDependsOnAttribute
 * 
 * return true iff 'stub' "depends" on attribute 'attrno', i.e. 
 * if this attribute is used in the stub's qualification.
 *------------------------------------------------------------------------
 */
bool
prs2DoesStubDependsOnAttribute(onestub, attrno)
Prs2OneStub onestub;
AttributeNumber attrno;
{
    LispValue qual;
    AttributeNumber *attrList;
    int attrListSize;
    int i;
    bool res;

    qual = onestub->qualification;

    prs2FindAttributesOfQual(qual, PRS2_CURRENT_VARNO,
			&attrList, &attrListSize);

    res = false;
    for (i=0; i<attrListSize; i++) {
	if (attrList[i] == attrno) {
	    res = true;
	    break;
	}
    }
		    
    if (attrList != NULL)
	pfree(attrList);
    return(res);

}

/*------------------------------------------------------------------------
 * prs2LockWritesAttributes
 *
 * return true if among the rule locks is a "write" lock that
 * "writes" any of the attributes given.
 *------------------------------------------------------------------------
 */
bool
prs2LockWritesAttributes(locks, attrList, attrListSize)
RuleLock locks;
AttributeNumber *attrList;
int attrListSize;
{

    register int i, j;
    register AttributeNumber attrno;
    Prs2OneLock oneLock;

    for (i=0; i< prs2GetNumberOfLocks(locks); i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	if (prs2OneLockGetLockType(oneLock) == LockTypeTupleRetrieveWrite) {
	    attrno = prs2OneLockGetAttributeNumber(oneLock);
	    for (j=0; j<attrListSize; j++) {
		if (attrno == attrList[j]) {
		    return(true);
		}
	    }/*foreach*/
	}/*if*/
    }/*for*/

    return(false);
}

/*------------------------------------------------------------------------
 * prs2FindAttributesOfQual
 *
 * Given a qualification, return a lsit of all the attributes
 * of the given "varno" used in this qual.
 * This list is actually a "palloced" array of AttributeNumbers.
 *
 * NOTE: duplicates are not removed...
 *------------------------------------------------------------------------
 */
void
prs2FindAttributesOfQual(qual, varno, attrListP, attrListSizeP)
LispValue qual;
int varno;
AttributeNumber **attrListP;
int *attrListSizeP;
{
    LispValue varnodes, t;
    Var v;
    int i,n;
    AttributeNumber *a;
    
    /*
     * first find the Var nodes of the qual
     */
    varnodes = FindVarNodes(qual);

    /*
     * first count the attributes
     */
    n = 0;
    foreach(t, varnodes) {
	v = (Var) CAR(t);
	if (get_varno(v) == varno)
	    n++;
    }

    /*
     * allocate memory for the array.
     */
    if (n>0) {
	a = (AttributeNumber *) palloc(n * sizeof(AttributeNumber));
	if (a==NULL) {
	    elog(WARN, "prs2FindAttributesOfQual: Out of memory");
	}
    } else {
	a = NULL;
    }

    /*
     * Now fill-in the array...
     */
    i=0;
    foreach(t, varnodes) {
	v = (Var) CAR(t);
	if (get_varno(v) == varno) {
	    a[i] = get_varattno(v);
	    i++;
	}
    }

    /*
     * now return the right values...
     */
    *attrListP = a;
    *attrListSizeP = n;
}

/*------------------------------------------------------------------------
 * prs2UndefTupleLevelLockRule
 *
 * remove all the locks & stubs a rule implemented with a tuple level
 * lock. Remove all system catalog info too...
 *------------------------------------------------------------------------
 */
void
prs2UndefTupleLevelLockRule(ruleId, relationOid)
ObjectId ruleId;
ObjectId relationOid;
{
    
    elog(DEBUG, "Removing tuple-level-lock rule %ld", ruleId);

    /*
     * remove all its locks/stubs
     */
    prs2RemoveTupleLeveLocksAndStubsOfManyRules(relationOid, &ruleId, 1);

    /*
     * remove info from the system catalogs
     */
    prs2DeleteRulePlanFromCatalog(ruleId);
    prs2DeleteRuleInfoFromCatalog(ruleId);

}

/*------------------------------------------------------------------------
 * prs2RemoveTupleLevelLocksAndStubsOfManyRules
 *
 * remove all the locks & stubs of the rules specified.
 * Do NOT update the system catalogs...
 *
 * We have this routine because sometimes we might to change
 * many tuple-level-lock rules to relation-level-lock.
 * If we do it separatelly for every rule we have two problmes:
 *
 * a) efficiency (one scan versus one scan per rule)
 * b) all these scans run under the same Xid & Cid, therefore we
 * are screwed, because we can not see the previous updates!
 *------------------------------------------------------------------------
 */
void
prs2RemoveTupleLeveLocksAndStubsOfManyRules(relationOid, ruleIds, nrules)
ObjectId relationOid;
ObjectId *ruleIds;
int nrules;
{
    
    bool status;
    Relation rel;
    HeapTuple tuple, newTuple;
    Buffer buffer;
    TupleDescriptor tdesc;
    HeapScanDesc scanDesc;
    RuleLock tlocks;
    Prs2Stub stubs;
    int i;

    /*
     * OK, first remove all the stubs for these rules
     */
    stubs = prs2GetRelationStubs(relationOid);
    for (i=0; i<nrules; i++) {
	status =  prs2RemoveStubsOfRule(stubs, ruleIds[i]);
	if (!status) {
	    /*
	     * something smells fishy here...
	     * that means that the rule didnt have any stubs,
	     * so it wasn't a tuple-level-lock rule...
	     */
	    elog(DEBUG,
	"prs2RemoveTupleLevelLocksOfManyRules: rule %d is not tuple-level-lock"
	    ,ruleIds[i]);
	}
    }

    /*
     * Now update the stub records for the relation.
     */
    prs2ReplaceRelationStub(relationOid, stubs);

    /*
     * now remove all locks for this rule from the tuples of the
     * relation...
     */
    rel = ObjectIdOpenHeapRelation(relationOid);
    tdesc = RelationGetTupleDescriptor(rel);
    scanDesc = RelationBeginHeapScan(rel, false, NowTimeQual, 0, NULL);

    while((tuple = HeapScanGetNextTuple(scanDesc, false, &buffer)) != NULL) {
	tlocks = HeapTupleGetRuleLock(tuple, buffer);
	status = false;
	for (i=0; i<nrules;  i++) {
	    if (prs2RemoveAllLocksOfRuleInPlace(tlocks, ruleIds[i]))
		status = true;
	}
	if (status) {
	    /*
	     * the locks of this tuple have changed.
	     * replace it...
	     */
	    newTuple = palloctup(tuple, buffer, rel);
	    prs2PutLocksInTuple(newTuple, InvalidBuffer, rel, tlocks);

	    RelationReplaceHeapTuple(rel, &(tuple->t_ctid),
					newTuple, (double *)NULL);
	    /* 
	     * NOTE: I tried to 'pfree(tuple)' and 
	     * I got an error message...
	     */
	    prs2FreeLocks(tlocks);
	    pfree(newTuple);
	}
    }

    RelationCloseHeapRelation(rel);
}
    
