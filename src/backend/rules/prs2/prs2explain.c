/*---------------------------------------------------------------------
 *
 * IDENTIFICATION:
 * $Header$
 *
 */

#include <stdio.h>
#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/tupdesc.h"
#include "access/ftup.h"
#include "utils/log.h"
#include "catalog/syscache.h"

/*---------------------------------------------------------------------
 *
 * createExplainRelation
 *
 * create the relation wehre the output of the explain command
 * will be stored.
 *
 * XXX: If the relation exists, then we destroy it and create a
 * new one. We might want to change that....
 */

Relation
createExplainRelation(relationName) 
Name relationName;
{
    ArchiveMode archiveMode;
    AttributeNumber numberOfAttributes;
    TupleDesc tupleDesc;
    ObjectId relationOid;
    NameData attrName;
    NameData typeName;
    Relation reldesc;
    HeapTuple tuple;
    Relation relDesc;

    /*
     * check the arguments for stupid values...
     */
    if (!NameIsValid(relationName)) {
	elog(WARN, "Illegal relation name %s", relationName->data);
    }

    /*
     * If the relation exists, destroy it
     */
    tuple = SearchSysCacheTuple(RELNAME, relationName);
    if (HeapTupleIsValid(tuple)) {
	RelationNameDestroyHeapRelation(relationName);
    }

    /*
     * Create the new relation schema
     */
    archiveMode = 'n';
    numberOfAttributes = 4;

    tupleDesc = CreateTemplateTupleDesc(numberOfAttributes);

    strcpy(attrName.data, "rulename");
    strcpy(typeName.data, "char16");
    TupleDescInitEntry(tupleDesc,1, &attrName, &typeName, 0);

    strcpy(attrName.data, "depth");
    strcpy(typeName.data, "int4");
    TupleDescInitEntry(tupleDesc,2, &attrName, &typeName, 0);

    strcpy(attrName.data, "tupleoid");
    strcpy(typeName.data, "oid");
    TupleDescInitEntry(tupleDesc,3, &attrName, &typeName, 0);
    
    strcpy(attrName.data, "relname");
    strcpy(typeName.data, "char16");
    TupleDescInitEntry(tupleDesc,4, &attrName, &typeName, 0);

    /*
     * Create the relation.
     * NOTE: `TupleDescriptorData' is the same thing
     * as a `TupleDesc'.
     */
    relationOid = RelationNameCreateHeapRelation(
			relationName,
			archiveMode,
			numberOfAttributes,
			(TupleDescriptor)tupleDesc);

    if (!ObjectIdIsValid(relationOid)) {
	elog(WARN,"Can not create relation `%s'.", relationName->data);
    }

    /*
     * OK, now open the relation.
     * XXX: what does this `setheapoverride' do ??
     *      (everybody is using it....)
     */
    setheapoverride(true);
    relDesc = ObjectIdOpenHeapRelation(relationOid);
    setheapoverride(false);
    if (!RelationIsValid(relDesc)) {
	elog(WARN,"Can not open created relation `%s'.", relationName->data);
    }

    return(relDesc);
}

/*--------------------------------------------------------------------
 *
 * storeExplainInfo
 *
 * Append a tuple to the 'explain' relation
 */
void
storeExplainInfo(explainRelation, ruleId, relation, tupleOid)
Relation explainRelation;
ObjectId ruleId;
Relation relation;
ObjectId tupleOid;
{
    NameData relName;
    NameData ruleName;
    HeapTuple explainTuple;
    TupleDescriptor explainTupleDesc;
    Datum data[4];
    char null[4];
    int i;


    /*
     * check the arguments....
     */
    if (!RelationIsValid(explainRelation)) {
	elog(WARN, "storeExplainInfo: ivnalid explain relation");
    }
    if (!RelationIsValid(relation)) {
	elog(WARN, "storeExplainInfo: ivnalid scan relation");
    }
    if (!ObjectIdIsValid(ruleId)) {
	elog(WARN, "storeExplainInfo: invalid rule Id");
    }
    if (!ObjectIdIsValid(tupleOid)) {
	elog(WARN, "storeExplainInfo: invalid tuple oid");
    }

    /*
     * find the rule's name
     */
    strcpy(ruleName.data, "No-Name");

    /*
     * find the scan relation's name
     */
    strncpy(relName.data,
	RelationGetRelationTupleForm(relation)->relname, 16);
    
    
    /*
     * Now form the tuple that will be stored in the
     * `explain' relation.
     */
    for (i=0; i<4; i++) {
	null[i] = ' ';
    }
    data[0] = NameGetDatum(&ruleName);
    data[1] = Int32GetDatum(0); /* XXXX THIS IS THE RECURSION DEPTH */
    data[2] = ObjectIdGetDatum(tupleOid);
    data[3] = NameGetDatum(&relName);

    explainTupleDesc = RelationGetTupleDescriptor(explainRelation);
    explainTuple = FormHeapTuple((AttributeNumber) 4,
				    explainTupleDesc,
				    data,
				    null);
    
    /*
     * Insert the tuple
     */
    (void) RelationInsertHeapTuple(explainRelation, explainTuple,
				    (double *) NULL);
}
