/*
 * defind.c --
 *	POSTGRES define, extend and remove index code.
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/attnum.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/funcindex.h"
#include "catalog/syscache.h"
#include "catalog/pg_index.h"
#include "catalog/pg_proc.h"
#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "utils/log.h"
#include "utils/palloc.h"

#include "commands/defrem.h"
#include "parse.h"
#include "parser/parsetree.h"
#include "planner/prepqual.h"
#include "planner/clause.h"
#include "planner/clauses.h"
#include "lib/copyfuncs.h"

#define IsFuncIndex(ATTR_LIST) (listp(CAAR(ATTR_LIST)))

void
DefineIndex(heapRelationName, indexRelationName, accessMethodName,
		attributeList, parameterList, predicate)

	Name		heapRelationName;
	Name		indexRelationName;
	Name		accessMethodName;
	LispValue	attributeList;
	LispValue	parameterList;
	LispValue	predicate;
{
	ObjectId	*classObjectId;
	ObjectId	accessMethodId;
	ObjectId	relationId;
	AttributeNumber	numberOfAttributes;
	AttributeNumber	*attributeNumberA;
	HeapTuple	tuple;
	uint16		parameterCount;
	Datum		*parameterA = NULL;
	Datum		*nextP;
	FuncIndexInfo	fInfo;
	LispValue	cnfPred = LispNil;

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
			&(heapRelationName->data[0]));
	}
	relationId = tuple->t_oid;

	/*
	 * compute access method id
	 */
	tuple = SearchSysCacheTuple(AMNAME, accessMethodName);
	if (!HeapTupleIsValid(tuple)) {
		elog(WARN, "DefineIndex: %s access method not found",
			&(accessMethodName->data[0]));
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
		parameterA = LintCast(Datum *,
			palloc(2 * parameterCount * sizeof *parameterA));

		nextP = &parameterA[0];
		while (!lispNullp(parameterList)) {
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CAR(parameterList)));
#endif
			*nextP = (Datum)CString(CAR(parameterList));
			parameterList = CDR(parameterList);
			nextP += 1;
		}
	}

	/*
	 * Convert the partial-index predicate from parsetree form to plan
	 * form, so it can be readily evaluated during index creation.
	 * Note: "predicate" comes in as a list containing (1) the predicate
	 * itself (a where_clause), and (2) a corresponding range table.
	 */
	if (predicate != LispNil && CAR(predicate) != LispNil) {
	    cnfPred = cnfify(lispCopy(CAR(predicate)), true);
	    fix_opids(cnfPred);
	    CheckPredicate(cnfPred, CADR(predicate), relationId);
	}

	if (IsFuncIndex(attributeList))
	{
		int nargs;
		
		nargs = length(CDR(CAAR(attributeList)));
		if (nargs > INDEX_MAX_KEYS)
		{
			elog(WARN, 
			     "Too many args to function, limit of %d",
			     INDEX_MAX_KEYS);
		}

		FIgetnArgs(&fInfo) = nargs;
		strncpy(FIgetname(&fInfo), 
			CString(CAAR(CAR(attributeList))), 
			sizeof(NameData));

		attributeNumberA = LintCast(AttributeNumber *,
			palloc(nargs * sizeof attributeNumberA[0]));
		classObjectId = LintCast(ObjectId *,
			palloc(sizeof classObjectId[0]));

		FuncIndexArgs(attributeList, attributeNumberA, 
			      &(FIgetArg(&fInfo, 0)),
			      classObjectId, relationId);

		index_create(heapRelationName, indexRelationName,
			&fInfo, accessMethodId, 
			numberOfAttributes, attributeNumberA,
			classObjectId, parameterCount, parameterA, cnfPred);
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
			classObjectId, parameterCount, parameterA, cnfPred);
	}
}


void
ExtendIndex(indexRelationName, predicate)
	Name		indexRelationName;
	LispValue	predicate;
{
	ObjectId	*classObjectId;
	ObjectId	accessMethodId;
	ObjectId	indexId, relationId;
	ObjectId	indproc;
	AttributeNumber	numberOfAttributes;
	AttributeNumber	*attributeNumberA;
	HeapTuple	tuple;
	FuncIndexInfo	fInfo;
	FuncIndexInfo	*funcInfo = NULL;
	IndexTupleForm	index;
	LispValue	oldPred = LispNil;
	LispValue	cnfPred = LispNil;
	LispValue	predInfo;
	Relation	heapRelation;
	Relation	indexRelation;
	int		i;

	AssertArg(NameIsValid(indexRelationName));

	/*
	 * compute index relation id and access method id
	 */
	tuple = SearchSysCacheTuple(RELNAME, indexRelationName);
	if (!HeapTupleIsValid(tuple)) {
		elog(WARN, "ExtendIndex: %s index not found",
			&(indexRelationName->data[0]));
	}
	indexId = tuple->t_oid;
	accessMethodId = ((Form_pg_relation) GETSTRUCT(tuple))->relam;

	/*
	 * find pg_index tuple
	 */
	tuple = SearchSysCacheTuple(INDEXRELID, indexId);
	if (!HeapTupleIsValid(tuple)) {
		elog(WARN, "ExtendIndex: %s is not an index",
			&(indexRelationName->data[0]));
	}

	/*
	 * Extract info from the pg_index tuple
	 */
	index = (IndexTupleForm)GETSTRUCT(tuple);
	Assert(index->indexrelid == indexId);
	relationId = index->indrelid;
	indproc = index->indproc;

	for (i=0; i<INDEX_MAX_KEYS; i++)
		if (index->indkey[i] == 0) break;
	numberOfAttributes = i;

	if (VARSIZE(&index->indpred) != 0) {
	    char *predString;
	    LispValue lispReadString();
	    predString = fmgr(F_TEXTOUT, &index->indpred);
	    oldPred = lispReadString(predString);
	    pfree(predString);
	}
	if (oldPred == LispNil)
	    elog(WARN, "ExtendIndex: %s is not a partial index",
		    &(indexRelationName->data[0]));

	/*
	 * Convert the extension predicate from parsetree form to plan
	 * form, so it can be readily evaluated during index creation.
	 * Note: "predicate" comes in as a list containing (1) the predicate
	 * itself (a where_clause), and (2) a corresponding range table.
	 */
	if (CAR(predicate) != LispNil) {
	    cnfPred = cnfify(lispCopy(CAR(predicate)), true);
	    fix_opids(cnfPred);
	    CheckPredicate(cnfPred, CADR(predicate), relationId);
	}

	/* make predInfo list to pass to index_build */
	predInfo = lispCons(cnfPred, lispCons(oldPred, LispNil));

	attributeNumberA = LintCast(AttributeNumber *,
		palloc(numberOfAttributes*sizeof attributeNumberA[0]));
	classObjectId = LintCast(ObjectId *,
		palloc(numberOfAttributes * sizeof classObjectId[0]));

	for (i=0; i<numberOfAttributes; i++) {
		attributeNumberA[i] = index->indkey[i];
		classObjectId[i] = index->indclass[i];
	}

	if (indproc != InvalidObjectId)
	{
		funcInfo = &fInfo;
		FIgetnArgs(funcInfo) = numberOfAttributes;

		tuple = SearchSysCacheTuple(PROOID, indproc);
		if (!HeapTupleIsValid(tuple))
			elog(WARN, "ExtendIndex: index procedure not found");
		strncpy(FIgetname(funcInfo), 
			((Form_pg_proc) GETSTRUCT(tuple))->proname,
			sizeof(NameData));
		FIgetProcOid(funcInfo) = tuple->t_oid;
	}

	heapRelation = heap_open(relationId);
	indexRelation = index_open(indexId);

	RelationSetLockForWrite(heapRelation);

	InitIndexStrategy(numberOfAttributes, indexRelation, accessMethodId);

	index_build(heapRelation, indexRelation, numberOfAttributes,
		    attributeNumberA, 0, NULL, funcInfo, predInfo);
}


/*
 * CheckPredicate
 *	Checks that the given list of partial-index predicates refer
 *	(via the given range table) only to the given base relation oid,
 *	and that they're in a form the planner can handle, i.e.,
 *	boolean combinations of "ATTR OP CONST" (yes, for now, the ATTR
 *	has to be on the left).
 */

CheckPredicate(predList, rangeTable, baseRelOid)
    LispValue	predList;
    LispValue	rangeTable;
    ObjectId	baseRelOid;
{
    LispValue	item;

    foreach (item, predList) {
	CheckPredExpr(CAR(item), rangeTable, baseRelOid);
    }
}

CheckPredExpr(predicate, rangeTable, baseRelOid)
    LispValue	predicate;
    LispValue	rangeTable;
    ObjectId	baseRelOid;
{
    LispValue clauses = LispNil, clause;

    if (fast_is_clause(predicate)) {	/* CAR is Oper node */
	CheckPredClause(predicate, rangeTable, baseRelOid);
	return;
    }
    else if (fast_or_clause(predicate))
	clauses = (LispValue) get_orclauseargs(predicate);
    else if (fast_and_clause(predicate))
	clauses = (LispValue) get_andclauseargs(predicate);
    else
	elog(WARN, "Unsupported partial-index predicate expression type");

    foreach (clause, clauses) {
	CheckPredExpr(CAR(clause), rangeTable, baseRelOid);
    }
}

CheckPredClause(predicate, rangeTable, baseRelOid)
    LispValue	predicate;
    LispValue	rangeTable;
    ObjectId	baseRelOid;
{
    Var		pred_var;
    Const	pred_const;

    pred_var = (Var)get_leftop(predicate);
    pred_const = (Const)get_rightop(predicate);
 
    if (!IsA(CAR(predicate),Oper) ||
	!IsA(pred_var,Var) ||
	!IsA(pred_const,Const)) {
	elog(WARN, "Unsupported partial-index predicate clause type");
    }
    if (CInteger(getrelid(get_varno(pred_var), rangeTable)) != baseRelOid)
	elog(WARN,
	     "Partial-index predicates may refer only to the base relation");
}


FuncIndexArgs(attList, attNumP, argTypes, opOidP, relId)
	LispValue	attList;
	AttributeNumber *attNumP;
        ObjectId	*argTypes;
	ObjectId	*opOidP;
	ObjectId	relId;
{
	LispValue	rest;
	HeapTuple	tuple;
	Attribute	att;

	tuple = SearchSysCacheTuple(CLANAME, CString(CADR(CAR(attList))));

	if (!HeapTupleIsValid(tuple)) 
	{
		elog(WARN, "DefineIndex: %s class not found",
			CString(CADR(CAR(attList))));
	}
	*opOidP = tuple->t_oid;

	bzero(argTypes, 8 * sizeof(ObjectId));
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
		att = (Attribute)GETSTRUCT(tuple);
		*attNumP++ = att->attnum;
		*argTypes++ = att->atttypid;
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
		elog(WARN, "index \"%s\" nonexistant", &(name->data[0]));
	}

	if (((RelationTupleForm)GETSTRUCT(tuple))->relkind != 'i') {
		elog(WARN, "relation \"%s\" is of type \"%c\"",
			&(name->data[0]),
			((RelationTupleForm)GETSTRUCT(tuple))->relkind);
	}

	index_destroy(tuple->t_oid);
}
