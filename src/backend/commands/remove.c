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
#include "catalog/pg_aggregate.h"
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
	rdesc = heap_openr(OperatorRelationName->data);
	for (i = 0; i < 3; ++i) {
		key[0].sk_attnum = attnums[i];
		sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 1, key);
		while (PointerIsValid(tup = heap_getnext(sdesc, 0, &buffer))) {
			ItemPointerCopy(&tup->t_ctid, &itemPointerData);   
			/* XXX LOCK not being passed */
			heap_delete(rdesc, &itemPointerData);
		}
		heap_endscan(sdesc);
	}
	heap_close(rdesc);
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
	rdesc = heap_openr(AttributeRelationName->data);
	sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 1, key);
	while (PointerIsValid(tup = heap_getnext(sdesc, 0, &buffer))) {
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);   
		optr->reloid = ((AttributeTupleForm)GETSTRUCT(tup))->attrelid;
		optr->next = (struct oidlist *) palloc(sizeof(*oidptr));     
		optr = optr->next;
        }
	optr->next = NULL;
	heap_endscan(sdesc);
	heap_close(rdesc);
	

	ScanKeyEntryInitialize(&key[0], ObjectIdAttributeNumber,
						   ObjectIdEqualRegProcedure, 0);
	optr = oidptr;
	rdesc = heap_openr(RelationRelationName->data);
	while (PointerIsValid((char *) optr->next)) {
		key[0].sk_data = (DATUM) (optr++)->reloid;
		sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 1, key);
		tup = heap_getnext(sdesc, 0, &buffer);
		if (PointerIsValid(tup)) {
			Name	name;

			name = &((RelationTupleForm)GETSTRUCT(tup))->relname;
			RelationNameDestroyHeapRelation(name);
		}
	}
	heap_endscan(sdesc);
	heap_close(rdesc);
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
	Name bb;

	Assert(NameIsValid(typeName));
	
	relation = RelationNameOpenHeapRelation(TypeRelationName);
	fmgr_info(typeKey[0].procedure, &typeKey[0].func, &typeKey[0].nargs);

	/* Delete the primary type */

	typeKey[0].argument = NameGetDatum(typeName);

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

	/* Now, Delete the "array of" that type */

	strcpy(bb, "_");
	strncat(bb, typeName, 15);

	typeKey[0].argument = NameGetDatum(bb);

	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);

	typeOid = tup->t_oid;
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
        RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);

	RelationCloseHeapRelation(relation);
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
	static ScanKeyEntryData key[3] = {
		{ 0, ProcedureNameAttributeNumber, NameEqualRegProcedure }
	};
	
	Assert(NameIsValid(functionName));

	key[0].argument = NameGetDatum(functionName);

	fmgr_info(key[0].procedure, &key[0].func, &key[0].nargs);

	relation = RelationNameOpenHeapRelation(ProcedureRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 1,
		(ScanKey)key);
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
	key[0].argument = NameGetDatum(functionName);
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

void
RemoveAggregate(aggName)
	Name	aggName;
{
	Relation 	relation;
	HeapScanDesc	scan;
	HeapTuple	tup;
	Buffer		buffer;
	ItemPointerData	  itemPointerData;
	static ScanKeyEntryData key[3] = {
		{ 0, AggregateNameAttributeNumber, NameEqualRegProcedure }
	};

	Assert(NameIsValid(aggName));
	key[0].argument = NameGetDatum(aggName);

	fmgr_info(key[0].procedure, &key[0].func, &key[0].nargs);
	relation = RelationNameOpenHeapRelation(AggregateRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 1,
		 (ScanKey)key);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, "Remove: aggregate %s nonexistant", aggName);
	}
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
	RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);

}


