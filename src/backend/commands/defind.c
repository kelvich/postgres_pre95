/*
 * defind.c --
 *	POSTGRES define and remove index code.
 */

#include "c.h"

RcsId("$Header$");

#include "attnum.h"
#include "genam.h"
#include "heapam.h"
#include "htup.h"
#include "log.h"
#include "name.h"
#include "pg_lisp.h"
#include "oid.h"
#include "palloc.h"
#include "syscache.h"

#include "defrem.h"


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
	AttributeNumber	attributeNumber;
	AttributeNumber	numberOfAttributes;
	AttributeNumber	*attributeNumberA;
	HeapTuple	tuple;
	uint16		parameterCount;
	String		*parameterA;
	String		*nextP;
	LispValue	rest;

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

	attributeNumberA = LintCast(AttributeNumber *,
		palloc(numberOfAttributes * sizeof attributeNumberA[0]));

	classObjectId = LintCast(ObjectId *,
		palloc(numberOfAttributes * sizeof classObjectId[0]));

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
	 * process X
	 */
	rest = attributeList;
	for (attributeNumber = 1; attributeNumber <= numberOfAttributes;
			attributeNumber += 1) {

		LispValue	attribute;

		attribute = CAR(rest);
#ifndef	PERFECTPARSER
		rest = CDR(rest);
		AssertArg(listp(rest));
		AssertArg(listp(attribute));
#endif
		if (length(attribute) != 2) {
			if (length(attribute) != 1) {
				elog(WARN, "DefineIndex: malformed att");
			}
			elog(WARN,
				"DefineIndex: default index class unsupported");
		}
#ifndef	PERFECTPARSER
		AssertArg(lispStringp(CAR(attribute)));
		AssertArg(lispStringp(CADR(attribute)));
#endif
		tuple = SearchSysCacheTuple(ATTNAME, relationId,
			CString(CAR(attribute)));
		if (!HeapTupleIsValid(tuple)) {
			elog(WARN, "DefineIndex: %s attribute not found");
		}
		attributeNumberA[attributeNumber - 1] =
			((Attribute)GETSTRUCT(tuple))->attnum;

		tuple = SearchSysCacheTuple(CLANAME, CString(CADR(attribute)));
		if (!HeapTupleIsValid(tuple)) {
			elog(WARN, "DefineIndex: %s class not found");
		}
		classObjectId[attributeNumber - 1] = tuple->t_oid;
	}

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

	RelationNameCreateIndexRelation(heapRelationName, indexRelationName,
		accessMethodId, numberOfAttributes, attributeNumberA,
		classObjectId, parameterCount, parameterA);
}

void
RemoveIndex(name)
	Name	name;
{
	/* delete-index(name); */
	/* am-destroy(name); */

	RelationNameDestroyIndexRelation(name);
}
