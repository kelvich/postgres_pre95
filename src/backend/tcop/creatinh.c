/*
 * creatinh.c --
 *	POSTGRES create/destroy relation with inheritance utility code.
 */

#include "c.h"

#define private

RcsId("$Header$");

#include "anum.h"
#include "attnum.h"
#include "cat.h"
#include "catname.h"
#include "ftup.h"	/* for FormHeapTuple */
#include "heapam.h"
#include "htup.h"
#include "log.h"
#include "name.h"
#include "oid.h"
#include "palloc.h"	/* for pfree */
#include "parse.h"
#include "pg_lisp.h"
#include "rel.h"
#include "syscache.h"
#include "tupdesc.h"

static LispValuePtr	InstalledNameP = &LispNil;

/*
 * LispValuePInstallAsName --
 *	Sets "installed name" to be value.
 */
private
void
LispValuePInstallAsName ARGS((
	LispValue	*valueInP
));

/*
 * CarEqualsInstalledName --
 *	True iff car of list equals "installed name."
 */
private
bool
CarEqualsInstalledName ARGS((
	List	list
));

/*
 * MergeAttributes --
 *	Returns new schema given initial schema and supers.
 *
 * Description:
 *	The schema argument is distructively changed (unless it is LispNil,
 * of course).
 */
private
List
MergeAttributes ARGS((
	List	schema,
	List	supers
));

/*
 * BuildDesc --
 *	Returns new tuple descriptor given schema.
 */
private
TupleDesc
BuildDesc ARGS((
	List	
	List	supers
));

/*
 * StoreCatalogInheritance --
 *	Updates the system catalogs with proper inheritance information.
 */
private
void
StoreCatalogInheritance ARGS((
	ObjectId	objectId,
	List		supers
));

/*
 * CREATE relation_name '(' dom_list ')' [KEY '(' dom_name [USING operator] 
 * 	{ , '(' dom_name [USING operator] } ')']
 * 	[INHERITS '(' relation_name {, relation_name} ')' ] 
 * 	[INDEXABLE dom_name { ',' dom_name } ] 
 * 	[ARCHIVE '=' (HEAVY | LIGHT | NONE)]
 * 
 * 	`(CREATE ,relation_name
 * 		 (
 * 		  (KEY (,domname1 ,operator1) (,domname2 ,operator2) ...)
 * 		  (INHERITS ,relname1 ,relname2 ...)
 * 		  (INDEXABLE ,idomname1 ,idomname2 ...)
 * 		  (ARCHIVE ,archive)
 * 		 )
 * 		 (,attname1 ,typname1) (,attname2 ,typname2) ...)
 * 
 * 	... where	operator?	is either ,operator or 'nil
 * 			archive		is one of 'HEAVY, 'LIGHT, or 'NONE
 * 
 */

void
DefineRelation(relationName, parameters, schema)
	LispValue	relationName;
	LispValue	parameters;
	LispValue	schema;
{
	AttributeNumber	numberOfAttributes;
	AttributeNumber	attributeNumber;
	LispValue	rest;
	ObjectId	relationId;
	List		inheritList = LispNil;

	AssertArg(lispStringp(relationName));
	AssertArg(listp(parameters));
	AssertArg(length(parameters) == 4);
	AssertArg(listp(schema));

	/*
	 * Check each schema element for satisfaction of ...
	 *
	 *	AssertArg(listp(elem));
	 *	AssertArg(listp(CDR(elem)));
	 *	AssertArg(lispStringp(CAR(elem)));
	 *	AssertArg(lispStringp(CADR(elem)));
	 */

	/*
	 * Handle parameters
	 * XXX parameter handling missing below.
	 */
	if (!lispNullp(CAR(parameters))) {
		AssertArg(lispIntegerp(CAR(CAR(parameters))));
		AssertArg(CInteger(CAR(CAR(parameters))) == KEY);

		elog(WARN, "RelationCreate: KEY not yet supported");
	}
	if (!lispNullp(CADR(parameters))) {
		AssertArg(lispIntegerp(CAR(CADR(parameters))));
		AssertArg(CInteger(CAR(CADR(parameters))) == INHERITS);

		inheritList = CDR(CADR(parameters));
	}
	if (!lispNullp(CADDR(parameters))) {
		AssertArg(lispIntegerp(CAR(CADDR(parameters))));
		AssertArg(CInteger(CAR(CADDR(parameters))) == INDEXABLE);

		elog(WARN, "RelationCreate: INDEXABLE not yet supported");
	}
	if (!lispNullp(CADDR(CDR(parameters)))) {
		AssertArg(lispIntegerp(CAR(CADDR(CDR(parameters)))));
		AssertArg(CInteger(CAR(CADDR(CDR(parameters)))) == ARCHIVE);

		elog(WARN, "RelationCreate: ARCHIVE not yet supported");
	}

	schema = MergeAttributes(schema, inheritList);

	numberOfAttributes = length(schema);
	if (numberOfAttributes <= 0) {
		elog(WARN,
"DefineRelation: please inherit from a relation or define an attribute");
	}

	relationId = RelationNameCreateHeapRelation(CString(relationName),
		/*
		 * Default archive mode should be looked up from the VARIABLE
		 * relation the first time.  Note that it is set to none, now.
		 */
		'n',	/* XXX use symbolic constant ... */
		numberOfAttributes, BuildDesc(schema));

	StoreCatalogInheritance(relationId, inheritList);

	/*
	 * No automatic index definition.  (yet?)
	 */
}

void
RemoveRelation(name)
	Name	name;
{
	AssertArg(NameIsValid(name));

	/* (delete-inheritance relation-name) */
	/* (check-indices relation-name) */
	RelationNameDestroyHeapRelation(name);
}

#if 0
(defun index-remove (relation-name)
  (delete-index relation-name)
  (am-destroy relation-name)
  (utility-end "REMOVE"))
#endif

private
void
LispValuePInstallAsName(valueInP)
	LispValue	*valueInP;
{
	InstalledNameP = valueInP;
}

private
bool
CarEqualsInstalledName(list)
	List		list;
{
	return (equal(*InstalledNameP, CAR(list)));
}

private
List
MergeAttributes(schema, supers)
	List	schema;
	List	supers;
{
	List		entry;

	/*
	 * Note:
	 *	Assumes valid argument formats including the fact that
	 * schema contains no duplicate names and that the types all exist.
	 */

	foreach (entry, supers) {
		LispValue	name = CAR(entry);
		Relation	relation;
		List		partialResult = LispNil;
		AttributeNumber	attributeNumber;
		TupleDesc	tupleDesc;

		AssertArg(lispStringp(name));

		/*
		 * grab tuple descriptor for next superclass relation
		 */
		relation = RelationNameOpenHeapRelation(CString(name));
		tupleDesc = RelationGetTupleDescriptor(relation);

		for (attributeNumber = RelationGetNumberOfAttributes(relation);
				AttributeNumberIsValid(attributeNumber);
				attributeNumber -= 1) {

			AttributeTupleForm	attribute =
				tupleDesc->data[attributeNumber - 1];

			LispValue		attributeName;
			LispValue		attributeType;
			LispValue		match;
			HeapTuple		tuple;

			/*
			 * form name and type
			 */
			attributeName = lispString(&attribute->attname);
			tuple = SearchSysCacheTuple(TYPOID,
				attribute->atttypid);
			AssertState(HeapTupleIsValid(tuple));
			attributeType = lispString(
				&((TypeTupleForm)GETSTRUCT(tuple))->typname);

			/*
			 * check validity
			 *
			 * (find attributeName schema :test #'equal :key #'car)
			 */
			LispValuePInstallAsName(&attributeName);
			match = find_if(CarEqualsInstalledName, schema);
			if (!null(match)) {
				if (!equal(attributeType, CADR(match))) {
					elog(WARN, "%s and %s conflict for %s",
						CString(attributeType),
						CString(CADR(match)),
						CString(attributeName));
				}
				/*
				 * this entry already exists
				 */
				continue;
			}
			partialResult = nappend1(partialResult,
				cons(attributeName,
					cons(attributeType, LispNil)));
		}

		/*
		 * iteration cleanup and result collection
		 */
		RelationCloseHeapRelation(relation);

		schema = nconc(schema, partialResult);
	}

	return (schema);
}

private
TupleDesc
BuildDesc(schema)
	List	schema;
{
	AttributeNumber	numberOfAttributes;
	AttributeNumber	attributeNumber;
	List		entry;
	TupleDesc	desc;

	numberOfAttributes = length(schema);

	desc = CreateTemplateTupleDesc(numberOfAttributes);

	attributeNumber = 0;
	foreach(entry, schema) {
		attributeNumber += 1;

		TupleDescInitEntry(desc, attributeNumber,
			CString(CAR(CAR(entry))), CString(CADR(CAR(entry))));
	}

	return (desc);
}

private
void
StoreCatalogInheritance(relationId, supers)
	ObjectId	relationId;
	List		supers;
{
	Relation	relation;
	TupleDesc	desc;
	AttributeNumber	attributeNumber;
	List		entry;
	HeapTuple	tuple;

	/*
	 * Catalog INHERITS information.
	 */
	relation = RelationNameOpenHeapRelation(InheritsRelationName);
	desc = RelationGetTupleDescriptor(relation);

	attributeNumber = 1;
	foreach (entry, supers) {
		Datum		datum[InheritsRelationNumberOfAttributes];
		char		null[InheritsRelationNumberOfAttributes];

		tuple = SearchSysCacheTuple(RELNAME, CString(CAR(entry)));
		AssertArg(HeapTupleIsValid(tuple));

		
		datum[0] = ObjectIdGetDatum(relationId);	/* inhrel */
		datum[1] = ObjectIdGetDatum(tuple->t_oid);	/* inhparent */
		datum[2] = Int16GetDatum(attributeNumber);	/* inhseqno */

		null[0] = ' ';
		null[1] = ' ';
		null[2] = ' ';

		tuple = FormHeapTuple(InheritsRelationNumberOfAttributes, desc,
			datum, null);

		RelationInsertHeapTuple(relation, tuple, NULL);	/* XXX retval */
		pfree(tuple);

		attributeNumber += 1;
	}
	RelationCloseHeapRelation(relation);
	
	/*
	 * Catalog IPL information.
	 */
	relation = RelationNameOpenHeapRelation(
		InheritancePrecidenceListRelationName);
	desc = RelationGetTupleDescriptor(relation);

	attributeNumber = 1;
	/*
	 * XXX need to perform transative closure?
	 */
	foreach (entry, supers) {
		Datum		datum[
			InheritancePrecidenceListRelationNumberOfAttributes];
		char		null[
			InheritancePrecidenceListRelationNumberOfAttributes];

		tuple = SearchSysCacheTuple(RELNAME, CString(CAR(entry)));
		AssertArg(HeapTupleIsValid(tuple));

		datum[0] = ObjectIdGetDatum(relationId);	/* iplrel */
		datum[1] = ObjectIdGetDatum(tuple->t_oid);	/*iplinherits*/
		datum[2] = Int16GetDatum(attributeNumber);	/* iplseqno */

		null[0] = ' ';
		null[1] = ' ';
		null[2] = ' ';

		tuple = FormHeapTuple(
			InheritancePrecidenceListRelationNumberOfAttributes,
			desc, datum, null);

		RelationInsertHeapTuple(relation, tuple, NULL);	/* XXX retval */
		pfree(tuple);

		attributeNumber += 1;
	}
	RelationCloseHeapRelation(relation);
}
