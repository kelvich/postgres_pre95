/*=======================================================================
 *
 * FILE: stubjoin.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 * Routines used when the executor runs in 'rule lock set/remove' mode.
 *
 */
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "access/heapam.h"
#include "utils/log.h"
#include "tmp/datum.h"

/*----------------------------------------------------------------
 *
 * prs2MakeStubForInnerRelation
 *
 * Create a `PrsOneStub'. This routine is called during the proccessing
 * of a Join node. For every tuple of the Outer relation, we have to
 * create (and insert in the inner relation) a rule stub.
 *
 *----------------------------------------------------------------
 */
Prs2OneStub
prs2MakeStubForInnerRelation(ruleInfo, tuple, buffer,
			    outerTupleDesc, innerTupleDesc)
JoinRuleInfo ruleInfo;
HeapTuple tuple;
Buffer buffer;
TupleDescriptor outerTupleDesc;
TupleDescriptor innerTupleDesc;
{
    Prs2OneStub oneStub;
    ObjectId ruleId;
    Prs2StubId stubId;
    RuleLock lock;
    LispValue qual;
    AttributeNumber innerAttrNo, outerAttrNo;
    ObjectId operator, type;
    Size len;
    bool byval;
    Datum value, cpvalue;
    Boolean isNull;
    Oper opr;
    Var var;
    Const cnst;
    LispValue varid;


    operator = get_jri_operator(ruleInfo);
    innerAttrNo = get_jri_inattrno(ruleInfo);
    outerAttrNo = get_jri_outattrno(ruleInfo);
    ruleId = get_jri_ruleid(ruleInfo);
    stubId = get_jri_stubid(ruleInfo);
    lock = get_jri_lock(ruleInfo);

    /*
     * now form the stub's qualification. This will correspond to
     * the qualification:
     *    ( <operator> <innerAttrNo> <constant> )
     * where <constant> is the value of the "outerAttrno" of the
     * outer relation tuple "tuple".
     */
    value = HeapTupleGetAttributeValue(
			tuple,
			buffer,
			outerAttrNo,
			outerTupleDesc,
			&isNull);
    if (isNull) {
	/*
	 * the outer tuple has a null attribute, so do not do
	 * anything.
	 */
	return((Prs2OneStub)NULL);
    }
    /*
     * NOTE: make a "copy" of the datum (i.e. of the data pointed
     * by, if any)... Beter be safe (and slow) then sorry...
     */
    type = outerTupleDesc->data[outerAttrNo-1]->atttypid;
    len = get_typlen(type);
    byval = get_typbyval(type);
    cpvalue = datumCopy(value, type, byval, len);
    cnst = MakeConst(
	    type,	/* consttype */
	    len,	/* constlen */
	    cpvalue,	/* constvalue */
	    false,	/* constisnull */
	    byval);	/* constbyval */

    type = innerTupleDesc->data[innerAttrNo-1]->atttypid;
    len = get_typlen(type);
    byval = get_typbyval(type);
    varid = lispCons(lispInteger(0),
		    lispCons(lispInteger(innerAttrNo), LispNil));
    var =  MakeVar(
	    (Index) 0,		/* varno */
	    innerAttrNo,	/* varattno */
	    type,		/* vartype */
	    LispNil,		/* vardotfields */
	    LispNil,		/* vararraylist */
	    varid,		/* varid */
	    InvalidObjectId);	/* varelemtype */
    opr = MakeOper(
	    operator,		/* opno */
	    InvalidObjectId,	/* opid */
	    false,		/* oprelationlevel */
	    PRS2_BOOL_TYPEID,	/* opresulttype */
	    0,			/* opsize */
	    NULL);		/* op_fcache */
    (void) replace_opid(opr);

    qual = lispCons((LispValue)opr,
		    lispCons((LispValue)var,
			     lispCons((LispValue)cnst, LispNil)));

    qual = lispCons(qual, LispNil);

    /*
     * OK, now form the 'Prs2OneStub'
     */
    oneStub = prs2MakeOneStub();
    oneStub->ruleId = ruleId;
    oneStub->stubId = stubId;
    oneStub->counter = 1;
    oneStub->lock = lock;
    oneStub->qualification = qual;
    return(oneStub);
}

/*----------------------------------------------------------------
 *
 * prs2AddLocksAndReplaceTuple
 *
 * If the qualification associated with the `oneStub' is satisfied
 * by the given tuple, then add a rule lock and return true.
 * Otherwise return false.
 *
 *----------------------------------------------------------------
 */
bool
prs2AddLocksAndReplaceTuple(tuple, buffer, relation, oneStub, newExpLocksFlag)
HeapTuple tuple;
Buffer buffer;
Relation relation;
Prs2OneStub oneStub;
bool *newExpLocksFlag;
{
    RuleLock oldLocks, newLocks, expLocks, temp;
    TupleDescriptor tupDesc;
    ItemPointer tupleId;
    HeapTuple newTuple;

    tupDesc = RelationGetTupleDescriptor(relation);
    *newExpLocksFlag = false;

    if (prs2StubQualTestTuple(tuple, buffer, tupDesc, oneStub->qualification)) {
	oldLocks = prs2GetLocksFromTuple(tuple, buffer);
	newLocks = prs2LockUnion(oneStub->lock, oldLocks);
	prs2FreeLocks(oldLocks);
	/*
	 * If the stub lock was an "import" lock, then this addition
	 * might create a new "export" lock for the tuple...
	 */
	expLocks = prs2FindNewExportLocksFromLocks(newLocks);
	if (!prs2RuleLockIsEmpty(expLocks)) {
	    *newExpLocksFlag = true;
	    temp = prs2LockUnion(newLocks, expLocks);
	    prs2FreeLocks(newLocks);
	    prs2FreeLocks(expLocks);
	    newLocks = temp;
	}
	/*
	 * create a copy of the tuple & add the locks
	 */
	newTuple = palloctup(tuple, buffer, relation);
	prs2PutLocksInTuple(newTuple, buffer, relation, newLocks);
	/*
	 * XXX: what happens if this tuple has already been replaced ?
	 * Should we check with: TupleUpdatedByCurXactAndCmd() ???
	 */
	tupleId = &(tuple->t_ctid);
	amreplace(relation, tupleId, newTuple);
	/*
	 * free the new tuple...
	 */
	HeapTupleFreeRuleLock(newTuple);
	pfree(newTuple);
	return(true);
    } else {
	return(false);
    }
}
	
/*----------------------------------------------------------------
 *
 * prs2UpdateStats
 *
 * Update statistics associated with rule stub records.
 *----------------------------------------------------------------
 */
void
prs2UpdateStats(ruleInfo, operation)
JoinRuleInfo ruleInfo;
int operation;
{
    Prs2StubStats stats;

    stats = get_jri_stats(ruleInfo);
    if (stats == NULL) {
	stats = (Prs2StubStats) palloc(sizeof(Prs2StubStatsData));
	if (stats == NULL) {
	    elog(WARN, "prs2UpdateStats: run out of memory");
	}
	stats->stubsAdded = 0;
	stats->stubsDeleted = 0;
	stats->locksAdded = 0;
	stats->locksDeleted = 0;
    }

    switch (operation) {
	case PRS2_ADDSTUB:
	    stats->stubsAdded += 1;
	    break;
	case PRS2_DELETESTUB:
	    stats->stubsDeleted += 1;
	    break;
	case PRS2_ADDLOCK:
	    stats->locksAdded += 1;
	    break;
	case PRS2_DELETELOCK:
	    stats->locksDeleted += 1;
	    break;
	default:
	    elog(WARN, "prs2UpdateStats:illegal operation %d",operation);
    }

    set_jri_stats(ruleInfo, stats);
}

