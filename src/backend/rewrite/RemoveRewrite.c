/*
 * $Header$
 */
#include "access/skey.h"
#include "catalog/pg_rewrite.h"
#include "catalog/catname.h"	/* for RewriteRelationName */
#include "catalog/syscache.h"
#include "utils/log.h"		/* for elog stuff */
#include "access/tqual.h"	/* 'NowTimeQual' defined here.. */
#include "access/heapam.h"	/* heap AM calls defined here */
#include "access/ftup.h"	/* for FormHeapTuple */
#include "fmgr.h"		/* for CHAR_16_EQ */

extern void prs2RemoveRelationLevelLocksOfRule();

/*-----------------------------------------------------------------------
 * RewriteGetRuleEventRel
 *-----------------------------------------------------------------------
 */
Name
RewriteGetRuleEventRel(rulename)
	char *rulename;
{
	HeapTuple htp;
	ObjectId eventrel;

	htp = SearchSysCacheTuple(REWRITENAME, rulename, NULL, NULL, NULL);
	if (!HeapTupleIsValid(htp))
		elog(WARN, "RewriteGetRuleEventRel: rule \"%s\" not found",
		     rulename);
	eventrel = ((Form_pg_rewrite) GETSTRUCT(htp))->ev_class;
	htp = SearchSysCacheTuple(RELOID, eventrel, NULL, NULL, NULL);
	if (!HeapTupleIsValid(htp))
		elog(WARN, "RewriteGetRuleEventRel: class %d not found",
		     eventrel);
	return(&((Form_pg_relation) GETSTRUCT(htp))->relname);
}

/* ----------------------------------------------------------------
 *
 * RemoveRewriteRule
 *
 * Delete a rule given its rulename.
 *
 * There are three steps.
 *   1) Find the corresponding tuple in 'pg_rewrite' relation.
 *      Find the rule Id (i.e. the Oid of the tuple) and finally delete
 *      the tuple.
 *   3) Delete the locks from the 'pg_relation' relation.
 *
 *
 * ----------------------------------------------------------------
 */

void
RemoveRewriteRule(ruleName)
     Name ruleName;
{
    Relation RewriteRelation	= NULL;
    HeapScanDesc scanDesc	= NULL;
    ScanKey scanKey		= (ScanKey)palloc(sizeof (ScanKeyData));
    HeapTuple tuple		= NULL;
    ObjectId ruleId		= NULL;
    ObjectId eventRelationOid	= NULL;
    Datum eventRelationOidDatum	= NULL;
    Buffer buffer		= NULL;
    Boolean isNull		= false;


    /*
     * Open the pg_rewrite relation. 
     */
    RewriteRelation = RelationNameOpenHeapRelation(RewriteRelationName);

     /*
      * Scan the RuleRelation ('pg_rewrite') until we find a tuple
      */
    ScanKeyEntryInitialize(&scanKey->data[0], 0, Anum_pg_rewrite_rulename,
			       F_CHAR16EQ, NameGetDatum(ruleName));
    scanDesc = RelationBeginHeapScan(RewriteRelation,
				    0, NowTimeQual, 1, scanKey);

    tuple = HeapScanGetNextTuple(scanDesc, 0, (Buffer *)NULL);

    /*
     * complain if no rule with such name existed
     */
    if (!HeapTupleIsValid(tuple)) {
	RelationCloseHeapRelation(RewriteRelation);
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
				Anum_pg_rewrite_ev_class,
				&(RewriteRelation->rd_att),
				&isNull);
    if (isNull) {
	/* XXX strange!!! */
	elog(WARN, "RemoveRewriteRule: null event target relation!");
    }
    eventRelationOid = DatumGetObjectId(eventRelationOidDatum);

    /*
     * Now delete the tuple...
     */
    RelationDeleteHeapTuple(RewriteRelation, &(tuple->t_ctid));
    RelationCloseHeapRelation(RewriteRelation);
    HeapScanEnd(scanDesc);

    /*
     * Now delete the relation level locks from the updated relation
     */
    prs2RemoveRelationLevelLocksOfRule(ruleId, eventRelationOid);

    elog(DEBUG, "---Rule '%s' deleted.\n", ruleName);

}

/* temporarily here */
int IsDefinedRewriteRule(ruleName)
     Name ruleName;
{
    Relation RewriteRelation	= NULL;
    HeapScanDesc scanDesc	= NULL;
    ScanKey scanKey		= (ScanKey)palloc(sizeof (ScanKeyData));
    HeapTuple tuple		= NULL;
    ObjectId ruleId		= NULL;
    ObjectId eventRelationOid	= NULL;
    Datum eventRelationOidDatum	= NULL;
    Buffer buffer		= NULL;
    Boolean isNull		= false;


    /*
     * Open the pg_rewrite relation. 
     */
    RewriteRelation = RelationNameOpenHeapRelation(RewriteRelationName);

     /*
      * Scan the RuleRelation ('pg_rewrite') until we find a tuple
      */
    ScanKeyEntryInitialize(&scanKey->data[0], 0, Anum_pg_rewrite_rulename,
			       F_CHAR16EQ, NameGetDatum(ruleName));
    scanDesc = RelationBeginHeapScan(RewriteRelation,
				    0, NowTimeQual, 1, scanKey);

    tuple = HeapScanGetNextTuple(scanDesc, 0, (Buffer *)NULL);

    /*
     * return whether or not the rewrite rule existed
     */
    RelationCloseHeapRelation(RewriteRelation);
    HeapScanEnd(scanDesc);
    return (HeapTupleIsValid(tuple));
}
