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

#include "tmp/miscadmin.h"

#include "catalog/pg_aggregate.h"
#include "catalog/pg_language.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/syscache.h"
#include "parser/catalog_utils.h"

void
RemoveOperator(operatorName, typeName1, typeName2)
	Name 	operatorName;		/* operator name */
	Name 	typeName1;		/* first type name */
	Name 	typeName2;		/* optional second type name */
{
	Relation 	relation;
	HeapScanDesc 	scan;
	HeapTuple 	tup;
	ObjectId	typeId1 = InvalidObjectId;
	ObjectId        typeId2 = InvalidObjectId;
	int 		defined;
	ItemPointerData	itemPointerData;
	Buffer          buffer;
	ScanKeyEntryData	operatorKey[3];
	NameData	user;

	Assert(NameIsValid(operatorName));
	Assert(NameIsValid(typeName1) || NameIsValid(typeName2));

	if (NameIsValid(typeName1)) {
	    typeId1 = TypeGet(typeName1, &defined);
	    if (!ObjectIdIsValid(typeId1)) {
		elog(WARN, "RemoveOperator: type \"%-.*s\" does not exist",
		     sizeof(NameData), typeName1);
		return;
	    }
	}
	
	if (NameIsValid(typeName2)) {
	    typeId2 = TypeGet(typeName2, &defined);
	    if (!ObjectIdIsValid(typeId2)) {
		elog(WARN, "RemoveOperator: type \"%-.*s\" does not exist",
		     sizeof(NameData), typeName2);
		return;
	    }
	}
	
	ScanKeyEntryInitialize(&operatorKey[0], 0x0,
			       OperatorNameAttributeNumber,
			       NameEqualRegProcedure,
			       NameGetDatum(operatorName));

	ScanKeyEntryInitialize(&operatorKey[1], 0x0,
			       OperatorLeftAttributeNumber,
			       ObjectIdEqualRegProcedure,
			       ObjectIdGetDatum(typeId1));

	ScanKeyEntryInitialize(&operatorKey[2], 0x0,
			       OperatorRightAttributeNumber,
			       ObjectIdEqualRegProcedure,
			       ObjectIdGetDatum(typeId2));

	relation = RelationNameOpenHeapRelation(OperatorRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 3,
				      (ScanKey) operatorKey);
	tup = HeapScanGetNextTuple(scan, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
#ifndef NO_SECURITY
		GetUserName(&user);
		if (!pg_ownercheck(user.data,
				   (char *) ObjectIdGetDatum(tup->t_oid),
				   OPROID))
			elog(WARN, "RemoveOperator: operator \"%-.*s\": permission denied",
			     sizeof(NameData), operatorName);
#endif
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);
		RelationDeleteHeapTuple(relation, &itemPointerData);
	} else {
		if (ObjectIdIsValid(typeId1) && ObjectIdIsValid(typeId2)) {
			elog(WARN, "RemoveOperator: binary operator \"%-.*s\" taking \"%-.*s\" and \"%-.*s\" does not exist",
			     sizeof(NameData), operatorName,
			     sizeof(NameData), typeName1,
			     sizeof(NameData), typeName2);
		} else if (ObjectIdIsValid(typeId1)) {
			elog(WARN, "RemoveOperator: right unary operator \"%-.*s\" taking \"%-.*s\" does not exist",
			     sizeof(NameData), operatorName,
			     sizeof(NameData), typeName1);
		} else {
			elog(WARN, "RemoveOperator: left unary operator \"%-.*s\" taking \"%-.*s\" does not exist",
			     sizeof(NameData), operatorName,
			     sizeof(NameData), typeName2);
		}
	}
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
}

#ifdef NOTYET
/*
 * this stuff is to support removing all reference to a type
 * don't use it  - pma 2/1/94
 */
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
	
	ScanKeyEntryInitialize((ScanKeyEntry)&key[0],
			       0, 0, ObjectIdEqualRegProcedure, (Datum)typeOid);
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

	ScanKeyEntryInitialize((ScanKeyEntry)&key[0],
			       0, 3, ObjectIdEqualRegProcedure, (Datum)typeOid);

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
	

	ScanKeyEntryInitialize((ScanKeyEntry) &key[0], 0,
            /*bug fix from prototyping, unknown bug ---^ */
			       ObjectIdAttributeNumber,
			       ObjectIdEqualRegProcedure, (Datum)0);
	optr = oidptr;
	rdesc = heap_openr(RelationRelationName->data);
	while (PointerIsValid((char *) optr->next)) {
		key[0].sk_data = (DATUM) (optr++)->reloid;
		sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 1, key);
		tup = heap_getnext(sdesc, 0, &buffer);
		if (PointerIsValid(tup)) {
			Name	name;

			name = &((RelationTupleForm)GETSTRUCT(tup))->relname;
			RelationNameDestroyHeapRelation(name->data);
		}
	}
	heap_endscan(sdesc);
	heap_close(rdesc);
}
#endif /* NOTYET */

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
	NameData arrayNameData;
	Name arrayName = & arrayNameData;
	NameData	user;

	Assert(NameIsValid(typeName));
	
#ifndef NO_SECURITY
	GetUserName(&user);
	if (!pg_ownercheck(user.data, typeName->data, TYPNAME))
		elog(WARN, "RemoveType: type \"%-.*s\": permission denied",
		     sizeof(NameData), typeName);
#endif

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
		elog(WARN, "RemoveType: type \"%-.*s\" does not exist",
		     sizeof(NameData), typeName);
	}
	typeOid = tup->t_oid;
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
        RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);

	/* Now, Delete the "array of" that type */

	arrayName->data[0] = '_';
	arrayName->data[1] = '\0';

	strncat(arrayName, typeName, 15);

	typeKey[0].argument = NameGetDatum(arrayName);

	scan = RelationBeginHeapScan(relation, 0, NowTimeQual,
				     1, (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);

	if (!HeapTupleIsValid(tup))
	{
	    elog(WARN, "RemoveType: type \"%-.*s\": array stub not found",
		 sizeof(NameData), typeName->data);
	}
	typeOid = tup->t_oid;
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
        RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);

	RelationCloseHeapRelation(relation);
}

void
RemoveFunction(functionName, nargs, argNameList)
	Name 	   functionName;             /* type name to be removed */
        int	   nargs;
        List	   argNameList;
{
	Relation         relation;
	HeapScanDesc         scan;
	HeapTuple        tup;
	Buffer           buffer = InvalidBuffer;
	bool		 bufferUsed = FALSE;
	ObjectId	 argList[8];
	Form_pg_proc	 the_proc;
#ifdef USEPARGS
	ObjectId         oid;
#endif
	ItemPointerData  itemPointerData;
	static ScanKeyEntryData key[3] = {
		{ 0, ProcedureNameAttributeNumber, NameEqualRegProcedure }
	};
	NameData	user;
	NameData	typename;
	int 		i;
	
	Assert(NameIsValid(functionName));

	bzero(argList, 8 * sizeof(ObjectId));
	for (i=0; i<nargs; i++) {
	    (void) namestrcpy(&typename, CString(CAR(argNameList)));
	    argNameList = CDR(argNameList);
	    
	    if (strncmp(typename.data, "any", NAMEDATALEN) == 0)
		argList[i] = 0;
	    else {
		tup = SearchSysCacheTuple(TYPNAME, (char *) &typename,
					  (char *) NULL, (char *) NULL,
					  (char *) NULL);

		if (!HeapTupleIsValid(tup)) {
		    elog(WARN, "RemoveFunction: type \"%-.*s\" not found",
			 sizeof(NameData), &typename);
		}

		argList[i] = tup->t_oid;
	    }
	}

	tup = SearchSysCacheTuple(PRONAME, (char *) functionName,
				  (char *) Int32GetDatum(nargs),
				  (char *) argList, (char *) NULL);
	if (!HeapTupleIsValid(tup))
	        func_error("RemoveFunction", functionName, nargs, argList);

#ifndef NO_SECURITY
	GetUserName(&user);
	if (!pg_func_ownercheck(user.data, (char *) functionName, 
				nargs, argList)) {
		elog(WARN, "RemoveFunction: function \"%-.*s\": permission denied",
		     sizeof(NameData), functionName);
	}
#endif

	key[0].argument = NameGetDatum(functionName);

	fmgr_info(key[0].procedure, &key[0].func, &key[0].nargs);

	relation = RelationNameOpenHeapRelation(ProcedureRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 1,
		(ScanKey)key);

	do { /* hope this is ok because it's indexed */
	        if (bufferUsed) {
		        ReleaseBuffer(buffer);
			bufferUsed = FALSE;
		}
	        tup = HeapScanGetNextTuple(scan, 0, (Buffer *) &buffer);
		if (!HeapTupleIsValid(tup))
		        break;
		bufferUsed = TRUE;
		the_proc = (Form_pg_proc) GETSTRUCT(tup);
	} while (NameIsEqual(&(the_proc->proname), functionName) &&
		 (the_proc->pronargs != nargs ||
		  !oid8eq(&(the_proc->proargtypes.data[0]), &argList[0])));


	if (!HeapTupleIsValid(tup) || !NameIsEqual(&(the_proc->proname), 
						   functionName))
	        {	
		      HeapScanEnd(scan);
		      RelationCloseHeapRelation(relation);
		      func_error("RemoveFunction", functionName,
				 nargs, argList);
		}

/* ok, function has been found */
#ifdef USEPARGS
	oid = tup->t_oid;
#endif

	if (the_proc->prolang == INTERNALlanguageId)
		elog(WARN, "RemoveFunction: function \"%-.*s\" is built-in",
		     sizeof(NameData), functionName);

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
		{ 0, Anum_pg_aggregate_aggname, NameEqualRegProcedure }
	};

	Assert(NameIsValid(aggName));

#ifndef NO_SECURITY
	/* XXX there's no pg_aggregate.aggowner?? */
#endif

	key[0].argument = NameGetDatum(aggName);

	fmgr_info(key[0].procedure, &key[0].func, &key[0].nargs);
	relation = RelationNameOpenHeapRelation(AggregateRelationName);
	scan = RelationBeginHeapScan(relation, 0, NowTimeQual, 1,
		 (ScanKey)key);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, "RemoveAggregate: aggregate \"%-.*s\" does not exist",
		     sizeof(NameData), aggName);
	}
	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
	RelationDeleteHeapTuple(relation, &itemPointerData);
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);

}
