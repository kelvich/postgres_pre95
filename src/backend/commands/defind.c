/*
 * defind.c --
 *	POSTGRES define and remove index code.
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/attnum.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/funcindex.h"
#include "catalog/syscache.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "utils/palloc.h"

#include "commands/defrem.h"

#define IsFuncIndex(ATTR_LIST) (listp(CAAR(ATTR_LIST)))

void
DefineIndex(heapRelationName, indexRelationName, accessMethodName,
		attributeList, parameterList)

	Name		heapRelationName;
	Name		indexRelationName;
	Name		accessMethodName;
	LispValue	attributeList;
	LispValue	parameterList;
{
	ObjectId	*classObjectId;
	ObjectId	accessMethodId;
	ObjectId	relationId;
	AttributeNumber	numberOfAttributes;
	AttributeNumber	*attributeNumberA;
	HeapTuple	tuple;
	uint16		parameterCount;
	String		*parameterA;
	String		*nextP;
	FuncIndexInfo	fInfo;

	AssertArg(NameIsValid(heapRelationName));
	AssertArg(NameIsValid(indexRelationName));
	AssertArg(NameIsValid(accessMethodName));
	AssertArg(listp(attributeList));
	AssertArg(listp(parameterList));

	/*
	 * Handle attributes
	 */
	numberOfAttributes = length(attributeList);
	if (numberOfAttributes <= 0) {
		elog(WARN, "DefineIndex: must specify at least one attribute");
	}



	/*
	 * compute heap relation id
	 */
	tuple = SearchSysCacheTuple(RELNAME, heapRelationName);
	if (!HeapTupleIsValid(tuple)) {
		elog(WARN, "DefineIndex: %s relation not found",
			heapRelationName);
	}
	relationId = tuple->t_oid;

	/*
	 * compute access method id
	 */
	tuple = SearchSysCacheTuple(AMNAME, accessMethodName);
	if (!HeapTupleIsValid(tuple)) {
		elog(WARN, "DefineIndex: %s access method not found",
			accessMethodName);
	}
	accessMethodId = tuple->t_oid;


	/*
	 * Handle parameters
	 */
	parameterCount = length(parameterList) / 2;
#ifndef	PERFECTPARSER
	AssertArg(length(parameterList) == 2 * parameterCount);
#endif
	if (parameterCount >= 1) {
		parameterA = LintCast(String *,
			palloc(2 * parameterCount * sizeof *parameterA));

		nextP = &parameterA[0];
		while (!lispNullp(parameterList)) {
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CAR(parameterList)));
#endif
			*nextP = (String)CString(CAR(parameterList));
			parameterList = CDR(parameterList);
			nextP += 1;
		}
	}

	if (IsFuncIndex(attributeList))
	{
		int nargs;

		nargs = length(CDR(CAAR(attributeList)));
		FIgetnArgs(&fInfo) = nargs;
		strncpy(FIgetname(&fInfo), 
			CString(CAAR(CAR(attributeList))), 
			sizeof(NameData));

		attributeNumberA = LintCast(AttributeNumber *,
			palloc(nargs * sizeof attributeNumberA[0]));
		classObjectId = LintCast(ObjectId *,
			palloc(sizeof classObjectId[0]));

		FuncIndexArgs(attributeList, attributeNumberA, 
			classObjectId, relationId);

		index_create(heapRelationName, indexRelationName,
			&fInfo, accessMethodId, 
			numberOfAttributes, attributeNumberA,
			classObjectId, parameterCount, parameterA);
	}
	else
	{
		attributeNumberA = LintCast(AttributeNumber *,
			palloc(numberOfAttributes*sizeof attributeNumberA[0]));
		classObjectId = LintCast(ObjectId *,
			palloc(numberOfAttributes * sizeof classObjectId[0]));

		NormIndexAttrs(attributeList, attributeNumberA, 
			classObjectId, relationId);

		index_create(heapRelationName, indexRelationName, NULL,
			accessMethodId, numberOfAttributes, attributeNumberA,
			classObjectId, parameterCount, parameterA);
	}
}

FuncIndexArgs(attList, attNumP, opOidP, relId)
	LispValue	attList;
	AttributeNumber *attNumP;
	ObjectId	*opOidP;
	ObjectId	relId;
{
	LispValue	rest;
	HeapTuple	tuple;

	tuple = SearchSysCacheTuple(CLANAME, CString(CADR(CAR(attList))));

	if (!HeapTupleIsValid(tuple)) 
	{
		elog(WARN, "DefineIndex: %s class not found",
			CString(CADR(CAR(attList))));
	}
	*opOidP = tuple->t_oid;

	/*
	 * process the function arguments 
	 */
	for (rest=CDR(CAAR(attList)); rest != LispNil; rest = CDR(rest)) 
	{
		LispValue	arg;

		AssertArg(listp(rest));

		arg = CAR(rest);
		AssertArg(lispStringp(arg));

		tuple = SearchSysCacheTuple(ATTNAME, relId, CString(arg));

		if (!HeapTupleIsValid(tuple)) 
		{
			elog(WARN, 
			     "DefineIndex: attribute \"%s\" not found",
			     CString(arg));
		}
		*attNumP++ = ((Attribute)GETSTRUCT(tuple))->attnum;
	}
}

NormIndexAttrs(attList, attNumP, opOidP, relId)
	LispValue	attList;
	AttributeNumber *attNumP;
	ObjectId	*opOidP;
	ObjectId	relId;
{
	LispValue	rest;
	HeapTuple	tuple;

	/*
	 * process attributeList
	 */
	
	for (rest=attList; rest != LispNil; rest = CDR(rest)) {

		LispValue	attribute;

		AssertArg(listp(rest));

		attribute = CAR(rest);
		AssertArg(listp(attribute));

		if (length(attribute) != 2) {
			if (length(attribute) != 1) {
				elog(WARN, "DefineIndex: malformed att");
			}
			elog(WARN,
			     "DefineIndex: default index class unsupported");
		}

		if (CADR(attribute) == LispNil || !lispStringp(CADR(attribute)))
			elog(WARN, "missing opclass for define index");
		if (CAR(attribute) == LispNil)
			elog(WARN, "missing attribute for define index");

		tuple = SearchSysCacheTuple(ATTNAME, relId,
			CString(CAR(attribute)));
		if (!HeapTupleIsValid(tuple)) {
			elog(WARN, 
			     "DefineIndex: attribute \"%s\" not found",
			     CString(CAR(attribute)));
		}
		*attNumP++ = ((Attribute)GETSTRUCT(tuple))->attnum;

		tuple = SearchSysCacheTuple(CLANAME, CString(CADR(attribute)));

		if (!HeapTupleIsValid(tuple)) {
			elog(WARN, "DefineIndex: %s class not found",
				CString(CADR(attribute)));
		}
		*opOidP++ = tuple->t_oid;
	}
}

void
RemoveIndex(name)
	Name	name;
{
	ObjectId	id;
	HeapTuple	tuple;

	tuple = SearchSysCacheTuple(RELNAME, name);

	if (!HeapTupleIsValid(tuple)) {
		elog(WARN, "index \"%s\" nonexistant", name);
	}

	if (((RelationTupleForm)GETSTRUCT(tuple))->relkind != 'i') {
		elog(WARN, "relation \"%s\" is of type \"%c\"", name,
			((RelationTupleForm)GETSTRUCT(tuple))->relkind);
	}

	index_destroy(tuple->t_oid);
}
