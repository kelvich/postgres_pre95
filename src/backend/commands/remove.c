/*
 * remove.c --
 *	remove {procedure, operator, type} system commands
 */

#include <strings.h>
#include "access.h"
#include "anum.h"
#include "attnum.h"
#include "catname.h"
#include "fmgr.h"
#include "heapam.h"
#include "ftup.h"
#include "log.h"
#include "syscache.h"
#include "tqual.h"

RcsId("$Header$");

extern           TypeRemove();
extern           OperatorRemove();
extern           ProcedureRemove();


/*
 *  OperatorRemove
 */

OperatorRemove(operatorName, firstTypeName, lastTypeName)
	Name 	operatorName;             /* operator name */
	Name 	firstTypeName;            /* first type name */
	Name 	lastTypeName;             /* last type name  */
{
	Relation 	relation;
	HeapScanDesc 	scan;
	HeapTuple 	tup;
	ObjectId	firstTypeId = InvalidObjectId;
	ObjectId        lastTypeId  = InvalidObjectId;
	int 		defined;
	ItemPointerData	itemPointerData;
	Buffer          buffer;
	static ScanKeyEntryData	operatorKey[3] = {
		{ 0, OperatorNameAttributeNumber, F_CHAR16EQ },
		{ 0, OperatorLeftAttributeNumber, F_OIDEQ },
		{ 0, OperatorRightAttributeNumber, F_OIDEQ }
	};

	Assert(NameIsValid(operatorName));
	Assert(PointerIsValid(firstTypeName) || PointerIsValid(lastTypeName));

	
	firstTypeId = TypeGet(firstTypeName, &defined);
	if (!ObjectIdIsValid(firstTypeId)) {
		elog(WARN, "OperatorRemove: type %s does not exist",
		     firstTypeName);
		return;
	}

	if (PointerIsValid(lastTypeName)) {
		lastTypeId = TypeGet(lastTypeName, &defined);
		if (!ObjectIdIsValid(lastTypeId)) {
			elog(WARN, "OperatorRemove: type %s does not exist",
			     lastTypeName);
			return;
		}
	}
	operatorKey[0].argument.name.value = operatorName;
	operatorKey[1].argument.objectId.value = firstTypeId;
	operatorKey[2].argument.objectId.value = lastTypeId;

	relation = RelationNameOpenHeapRelation(OperatorRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 3,
				      (ScanKey) operatorKey);
	tup = HeapScanGetNextTuple(scan, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);
		RelationDeleteHeapTuple(relation, &itemPointerData);
	} else {
		elog(DEBUG,
		     "OperatorRemove: no such operator %s with types %s, %s",
		     operatorName, firstTypeName, lastTypeName);		
	}
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
}


/*
 *  SingleOpOperatorRemove
 *	Removes all operators that have operands or a result of type 'typeOid'.
 */
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
	
	key[0].sk_flags  = 0;
	key[0].sk_opr    = F_OIDEQ;
	key[0].sk_data   = (char *) typeOid;
	rdesc = amopenr(OperatorRelationName->data);
	for (i = 0; i < 3; ++i) {
		key[0].sk_attnum = attnums[i];
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 1, key);
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
	key[0].sk_flags  = 0;
	key[0].sk_attnum = 3;
	key[0].sk_opr    = F_OIDEQ;
	key[0].sk_data   = (DATUM) typeOid;
	oidptr = (struct oidlist *) palloc(sizeof(*oidptr));
	oidptr->next = NULL;
	optr = oidptr; 
	rdesc = amopenr(AttributeRelationName->data);
	sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 1, key);
	while (PointerIsValid(tup = amgetnext(sdesc, 0, &buffer))) {
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);   
		optr->reloid = ((struct attribute *) GETSTRUCT(tup))->attrelid;
		optr->next = (struct oidlist *) palloc(sizeof(*oidptr));     
		optr = optr->next;
        }
	optr->next = NULL;
	amendscan(sdesc);
	amclose(rdesc);
	
	key[0].sk_flags  = NULL;
	key[0].sk_attnum = T_OID;
	key[0].sk_opr    = F_OIDEQ;
	optr = oidptr;
	rdesc = amopenr(RelationRelationName->data);
	while (PointerIsValid((char *) optr->next)) {
		key[0].sk_data = (DATUM) (optr++)->reloid;
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 1, key);
		tup = amgetnext(sdesc, 0, &buffer);
		if (PointerIsValid(tup))
			amdestroy(((struct relation *)
				   GETSTRUCT(tup))->relname);
	}
	amendscan(sdesc);
	amclose(rdesc);
}


/*
 *  TypeRemove
 *	Removes the type 'typeName' and all attributes and relations that
 *	use it.
 */
TypeRemove(typeName)
	Name      typeName;             /* type name to be removed */
{
	Relation 	relation;  
	HeapScanDesc 	scan;      
	HeapTuple 	tup;
	ObjectId	typeOid;
	ItemPointerData	itemPointerData;
	Buffer          buffer;
	static ScanKeyEntryData typeKey[1] = {
		{ 0, TypeNameAttributeNumber, F_CHAR16EQ }
	};

	Assert(NameIsValid(typeName));
	
	typeKey[0].argument.name.value = typeName;
	relation = RelationNameOpenHeapRelation(TypeRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, "TypeRemove: type %s nonexistant",  typeName);
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


/*
 * ProcedureRemove
 *	Removes a procedure.
 */
ProcedureRemove(procedureName)
	Name 	procedureName;             /* type name to be removed */
{
	Relation         relation;
	HeapScanDesc         scan;
	HeapTuple        tup;
	Buffer           buffer;
#ifdef USEPARGS
	ObjectId         oid;
#endif
	ItemPointerData  itemPointerData;
	static ScanKeyEntryData procedureKey[3] = {
		{ 0, ProcedureNameAttributeNumber, F_CHAR16EQ }
	};
	
	Assert(NameIsValid(procedureName));

	procedureKey[0].argument.name.value = procedureName;
	relation = RelationNameOpenHeapRelation(ProcedureRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) procedureKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {	
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, "ProcedureRemove: procedure %s nonexistant",
		     procedureName);
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
	typeKey[0].flags = 0;
	typeKey[0].attributeNumber = 1; /* place an entry in anum.h if ever used */
	typeKey[0].procedure = F_OIDEQ;
	typeKey[0].argument.objectId.value = oid;
	procedureKey[0].argument.name.value = procedureName;
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
