/*
 * remove.c --
 *	POSTGRES remove (function | type | operator ) utilty code.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "access/attnum.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/skey.h"
#include "access/tqual.h"	/* for NowTimeQual */
#include "catalog/catname.h"
#include "commands/defrem.h"
#include "utils/log.h"

#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"

static String	Messages[] = {
#define	NonexistantTypeMessage	(Messages[0])
	"Define: type %s nonexistant",
};

void
RemoveOperator(operatorName, typeName1, typeName2)
	Name 	operatorName;		/* operator name */
	Name 	typeName1;		/* first type name */
	Name 	typeName2;		/* optional second type name */
{
	Relation 	relation;
	HeapScanDesc 	scan;
	HeapTuple 	tup;
	ObjectId	typeId1;
	ObjectId        typeId2;
	int 		defined;
	ItemPointerData	itemPointerData;
	Buffer          buffer;
	static ScanKeyEntryData	operatorKey[3] = {
		{ 0, OperatorNameAttributeNumber, NameEqualRegProcedure },
		{ 0, OperatorLeftAttributeNumber, ObjectIdEqualRegProcedure },
		{ 0, OperatorRightAttributeNumber, ObjectIdEqualRegProcedure }
	};

	Assert(NameIsValid(operatorName));
	Assert(NameIsValid(typeName1));

	typeId1 = TypeGet(typeName1, &defined);
	if (!ObjectIdIsValid(typeId1)) {
		elog(WARN, NonexistantTypeMessage, typeName1);
		return;
	}

	typeId2 = InvalidObjectId;
	if (NameIsValid(typeName2)) {
		typeId2 = TypeGet(typeName2, &defined);
		if (!ObjectIdIsValid(typeId2)) {
			elog(WARN, NonexistantTypeMessage, typeName2);
			return;
		}
	}
	operatorKey[0].argument = NameGetDatum(operatorName);
	operatorKey[1].argument = ObjectIdGetDatum(typeId1);
	operatorKey[2].argument = ObjectIdGetDatum(typeId2);

	relation = RelationNameOpenHeapRelation(OperatorRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 3,
				      (ScanKey) operatorKey);
	tup = HeapScanGetNextTuple(scan, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);
		RelationDeleteHeapTuple(relation, &itemPointerData);
	} else {
		if (ObjectIdIsValid(typeId2)) {
			elog(WARN, "Remove: operator %s(%s, %s) nonexistant",
				operatorName, typeName1, typeName2);
		} else {
			elog(WARN, "Remove: operator %s(%s) nonexistant",
				operatorName, typeName1);
		}
	}
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
}

/*
 *  SingleOpOperatorRemove
 *	Removes all operators that have operands or a result of type 'typeOid'.
 */
static
void
SingleOpOperatorRemove(typeOid)
	OID 	typeOid;
{
	Relation 	rdesc;
	struct skey 	key[3];
	HeapScanDesc 	sdesc;
	HeapTuple 	tup;
	ItemPointerData itemPointerData;
	Buffer 		buffer;
	static 		attnums[3] = { 7, 8, 9 }; /* left, right, return */
	register	i;
	
	ScanKeyEntryInitialize(&key[0], 0, 0, ObjectIdEqualRegProcedure, typeOid);
	rdesc = amopenr(OperatorRelationName->data);
	for (i = 0; i < 3; ++i) {
		key[0].sk_attnum = attnums[i];
		sdesc = ambeginscan(rdesc, 0, NowTimeQual, 1, key);
		while (PointerIsValid(tup = amgetnext(sdesc, 0, &buffer))) {
			ItemPointerCopy(&tup->t_ctid, &itemPointerData);   
			/* XXX LOCK not being passed */
			amdelete(rdesc, &itemPointerData);
		}
		amendscan(sdesc);
	}
	amclose(rdesc);
}

/*
 *  AttributeAndRelationRemove
 *	Removes all entries in the attribute and relation relations
 *	that contain entries of type 'typeOid'.
 *      Currently nothing calls this code, it is untested.
 */
static
void
AttributeAndRelationRemove(typeOid)
	OID 	typeOid;
{
	struct oidlist {
		OID		reloid;
		struct oidlist	*next;
	};
	struct oidlist	*oidptr, *optr;
	Relation 	rdesc;
	struct skey	key[1];
	HeapScanDesc 	sdesc;
	HeapTuple 	tup;
	ItemPointerData	itemPointerData;
	Buffer 		buffer;
	
	/*
	 * Get the oid's of the relations to be removed by scanning the
	 * entire attribute relation.
	 * We don't need to remove the attributes here,
	 * because amdestroy will remove all attributes of the relation.
	 * XXX should check for duplicate relations
	 */

	ScanKeyEntryInitialize(&key[0], 0, 3, ObjectIdEqualRegProcedure, typeOid);

	oidptr = (struct oidlist *) palloc(sizeof(*oidptr));
	oidptr->next = NULL;
	optr = oidptr; 
	rdesc = amopenr(AttributeRelationName->data);
	sdesc = ambeginscan(rdesc, 0, NowTimeQual, 1, key);
	while (PointerIsValid(tup = amgetnext(sdesc, 0, &buffer))) {
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);   
		optr->reloid = ((AttributeTupleForm)GETSTRUCT(tup))->attrelid;
		optr->next = (struct oidlist *) palloc(sizeof(*oidptr));     
		optr = optr->next;
        }
	optr->next = NULL;
	amendscan(sdesc);
	amclose(rdesc);
	

	ScanKeyEntryInitialize(&key[0], ObjectIdAttributeNumber,
						   ObjectIdEqualRegProcedure, 0);
	optr = oidptr;
	rdesc = amopenr(RelationRelationName->data);
	while (PointerIsValid((char *) optr->next)) {
		key[0].sk_data = (DATUM) (optr++)->reloid;
		sdesc = ambeginscan(rdesc, 0, NowTimeQual, 1, key);
		tup = amgetnext(sdesc, 0, &buffer);
		if (PointerIsValid(tup)) {
			Name	name;

			name = &((RelationTupleForm)GETSTRUCT(tup))->relname;
			RelationNameDestroyHeapRelation(name);
		}
	}
	amendscan(sdesc);
	amclose(rdesc);
}


/*
 *  TypeRemove
 *	Removes the type 'typeName' and all attributes and relations that
 *	use it.
 */
void
RemoveType(typeName)
	Name      typeName;             /* type name to be removed */
{
	Relation 	relation;  
	HeapScanDesc 	scan;      
	HeapTuple 	tup;
	ObjectId	typeOid;
	ItemPointerData	itemPointerData;
	Buffer          buffer;
	static ScanKeyEntryData typeKey[1] = {
		{ 0, TypeNameAttributeNumber, NameEqualRegProcedure }
	};

	Assert(NameIsValid(typeName));
	
	typeKey[0].argument = NameGetDatum(typeName);
	relation = RelationNameOpenHeapRelation(TypeRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, NonexistantTypeMessage, typeName);
	}
	typeOid = tup->t_oid;
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
        RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
	
	/*
	 * If the type remove was successful, now need to remove all operators
	 * that have one or more operand of this type.  Need a special
	 * version of OperatorRemove that only requires one operand type
	 * to be specified.
	 */

	/* It was decided to remove only the type, and nothing else.
	 * Hence the following call is removed
         */

/*	SingleOpOperatorRemove(typeOid);     */
	
	/*
	 * Also remove any attributes of this type.  However, if an
	 * attribute is removed, the relation it is an attribute
	 * of must also be removed.
	 */
#ifdef CASCADINGTYPEDELETE
	AttributeAndRelationRemove(typeOid);
#endif
}

void
RemoveFunction(functionName)
	Name 	functionName;             /* type name to be removed */
{
	Relation         relation;
	HeapScanDesc         scan;
	HeapTuple        tup;
	Buffer           buffer;
#ifdef USEPARGS
	ObjectId         oid;
#endif
	ItemPointerData  itemPointerData;
	static ScanKeyEntryData functionKey[3] = {
		{ 0, ProcedureNameAttributeNumber, NameEqualRegProcedure }
	};
	
	Assert(NameIsValid(functionName));

	functionKey[0].argument = NameGetDatum(functionName);
	relation = RelationNameOpenHeapRelation(ProcedureRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 1,
		(ScanKey)functionKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {	
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, "Remove: function %s nonexistant", functionName);
	}
#ifdef USEPARGS
	oid = tup->t_oid;
#endif
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
        RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
	
	/*  
	 * Remove associated parameters from PARG catalog
	 *
	 * XXX NOT CURRENTLY DONE
	 */
#ifdef USEPARGS
	ScanKeyEntryInitialize(&typeKey, 0, 1, ObjectIdEqualRegProcedure,
						   ObjectIdGetDatum(oid));
	functionKey[0].argument = NameGetDatum(functionName);
	relation = RelationNameOpenHeapRelation(ProcedureArgumentRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	while (PointerIsValid(tup)) {
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);
		RelationDeleteHeapTuple(relation, &itemPointerData);
		tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	}                
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
#endif
}
