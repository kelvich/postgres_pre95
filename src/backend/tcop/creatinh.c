/* ----------------------------------------------------------------
 *   FILE
 *	creatinh.c
 *	
 *   DESCRIPTION
 *	POSTGRES create/destroy relation with inheritance utility code.
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) tcopdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "tcop/tcopdebug.h"

#include "access/ftup.h"
#include "utils/log.h"
#include "parser/parse.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"

#include "catalog/syscache.h"
#include "catalog/catname.h"
#include "catalog/pg_type.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_ipl.h"

#include "tcop/creatinh.h"

/* ----------------
 *	external functions
 * ----------------
 */
/*
 * BuildDescForRelation --
 *	Returns new tuple descriptor given schema.
 */
extern
TupleDesc
BuildDescForRelation ARGS((
	List	schema,
	Name	relationName
));

/* ----------------
 *	local stuff
 * ----------------
 */

#define private

LispValuePtr	InstalledNameP = &LispNil;

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
 * StoreCatalogInheritance --
 *	Updates the system catalogs with proper inheritance information.
 */
private
void
StoreCatalogInheritance ARGS((
	ObjectId	objectId,
	List		supers
));

/* ----------------------------------------------------------------
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
 * ----------------------------------------------------------------
 */
/* ----------------------------------------------------------------
 *	DefineRelation
 *
 * ----------------------------------------------------------------
 */
void
DefineRelation(relname, parameters, schema)
    char	*relname;
    LispValue	parameters;
    LispValue	schema;
{
    AttributeNumber	numberOfAttributes;
    AttributeNumber	attributeNumber;
    LispValue		rest 		= NULL;
    ObjectId		relationId;
    List		arch 		= NULL ;
    char		archChar;
    List		inheritList 	= NULL;
    Name		archiveName 	= NULL;
    Name		relationName 	= NULL;
    TupleDesc		descriptor;

    /* ----------------
     *	minimal argument checking follows
     * ----------------
     */
    AssertArg(NameIsValid(relname));
    AssertArg(listp(parameters));
    AssertArg(listp(schema));

    if ( strlen ( relname ) > sizeof ( NameData ))
      elog(WARN, "the relation name %s is > %d characters long", relname,
	   sizeof(NameData));

    relationName = (Name)
	strcpy (palloc(sizeof (NameData)+1), relname);

    /* ----------------
     * 	Handle parameters
     * 	XXX parameter handling missing below.
     * ----------------
     */
    if (!lispNullp(CAR(parameters))) {
	elog(WARN, "RelationCreate: KEY not yet supported");
    }
    if (!lispNullp(CADR(parameters))) {
	inheritList = CDR(CADR(parameters));
    }
    if (!lispNullp(CADDR(parameters))) {
	elog(WARN, "RelationCreate: INDEXABLE not yet supported");
    }

    /* ----------------
     *	determine archive mode
     * 	XXX use symbolic constants...
     * ----------------
     */
    archChar = 'n';

    if (!lispNullp(arch = CADDR(CDR(parameters)))) {
	switch (LISPVALUE_INTEGER(CDR(arch))) {
	case NONE:
	    archChar = 'n';
	    break;

	case LIGHT:
	    archChar = 'l';
	    break;

	case HEAVY:
	    archChar = 'h';
	    break;

	default:
	    elog(NOTICE, "Botched archive mode %d, ignoring",
		 LISPVALUE_INTEGER(CDR(arch)));
	    archChar = 'n';
	    break;
	}
    }

    /* ----------------
     *	generate relation schema, including inherited attributes.
     * ----------------
     */
    schema = MergeAttributes(schema, inheritList);

    numberOfAttributes = length(schema);
    if (numberOfAttributes <= 0) {
	elog(WARN, "DefineRelation: %s",
	     "please inherit from a relation or define an attribute");
    }

    /* ----------------
     *	create a relation descriptor from the relation schema
     *  and create the relation.  
     * ----------------
     */
    descriptor = BuildDescForRelation(schema, relationName);
    relationId = RelationNameCreateHeapRelation(relationName,
						archChar,
						numberOfAttributes,
						descriptor);

    StoreCatalogInheritance(relationId, inheritList);

    /* ----------------
     *	create an archive relation if necessary
     * ----------------
     */
    if (archChar != 'n') {
	/*
	 *  Need to create an archive relation for this heap relation.
	 *  We cobble up the command by hand, and increment the command
	 *  counter ourselves.
	 */

	CommandCounterIncrement();
	archiveName = (Name) palloc(sizeof(NameData));
	sprintf(&archiveName->data[0], "a,%ld", relationId);
	
	relationId =
	    RelationNameCreateHeapRelation(archiveName,
					   'n', /* archive isn't archived */
					   numberOfAttributes,
					   descriptor);
	pfree(archiveName);
    }
}

void
RemoveRelation(name)
	Name	name;
{
	AssertArg(NameIsValid(name));

	RelationNameDestroyHeapRelation(name);
}


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
	List	entry;

	/*
	 * Validates that there are no duplications.
	 * Validity checking of types occurs later.
	 */
	foreach (entry, schema) {
		List	rest;

		foreach (rest, CDR(entry)) {
			/*
			 * check for duplicated relation names
			 */
			if (equal(CAR(CAR(entry)), CAR(CAR(rest)))) {
				elog(WARN, "attribute \"%s\" duplicated",
					CString(CAR(CAR(entry))));
			}
		}
	}
	foreach (entry, supers) {
		List	rest;

		foreach (rest, CDR(entry)) {
			if (equal(CAR(entry), CAR(rest))) {
				elog(WARN, "relation \"%s\" duplicated",
					CString(CAR(CAR(entry))));
			}
		}
	}

	/*
	 * merge the inherited attributes into the schema
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

/*
 * CL copy-list
 *	Returns #'equal list by copying top level of list structure.
 */
List
lispCopyList(list)
	List	list;
{
	List	result;
	List	rest;
	List	last;

	if (null(list)) {
		return (LispNil);
	}
	AssertArg(listp(list));

	result = lispCons(CAR(list), LispNil);

	last = result;
	foreach (rest, CDR(list)) {
		CDR(last) = lispCons(CAR(rest), LispNil);
		last = CDR(last);
	}

	return (result);
}

private
void
StoreCatalogInheritance(relationId, supers)
	ObjectId	relationId;
	List		supers;
{
	Relation	relation;
	TupleDesc	desc;
	int16		seqNumber;
	List		entry;
	List		idList;
	HeapTuple	tuple;

	AssertArg(ObjectIdIsValid(relationId));

	if (null(supers)) {
		return;
	}
	AssertArg(listp(supers));

	/*
	 * Catalog INHERITS information.
	 */
	relation = RelationNameOpenHeapRelation(InheritsRelationName);
	desc = RelationGetTupleDescriptor(relation);

	seqNumber = 1;
	idList = LispNil;
	foreach (entry, supers) {
		Datum		datum[InheritsRelationNumberOfAttributes];
		char		null[InheritsRelationNumberOfAttributes];

		tuple = SearchSysCacheTuple(RELNAME, CString(CAR(entry)));
		AssertArg(HeapTupleIsValid(tuple));

		/*
		 * build idList for use below
		 */
		idList = nappend1(idList, lispInteger(tuple->t_oid));

		datum[0] = ObjectIdGetDatum(relationId);	/* inhrel */
		datum[1] = ObjectIdGetDatum(tuple->t_oid);	/* inhparent */
		datum[2] = Int16GetDatum(seqNumber);		/* inhseqno */

		null[0] = ' ';
		null[1] = ' ';
		null[2] = ' ';

		tuple = FormHeapTuple(InheritsRelationNumberOfAttributes, desc,
			datum, null);

		RelationInsertHeapTuple(relation, tuple, NULL);	/* XXX retval */
		pfree(tuple);

		seqNumber += 1;
	}
	RelationCloseHeapRelation(relation);
	
	/*
	 * Catalog IPL information.
	 *
	 * Algorithm:
	 *	0. list superclasses (by ObjectId) in order given (see idList).
	 *	1. append after each relationId, its superclasses, recursively.
	 *	3. remove all but last of duplicates.
	 *	4. store result.
	 */

	/*
	 * 1.
	 */
	foreach (entry, idList) {
		HeapTuple		tuple;
		ObjectId		id;
		int16			number;
		List			next;
		List			current;
		InheritsTupleForm	form;

		id = CInteger(CAR(entry));
		current = entry;
		next = CDR(entry);

		for (number = 1; ; number += 1) {
			tuple = SearchSysCacheTuple(INHRELID, id, number);

			if (!HeapTupleIsValid(tuple)) {
				break;
			}
			CDR(current) = lispCons(lispInteger(
				((InheritsTupleForm)
					GETSTRUCT(tuple))->inhparent),
				LispNil);

			current = CDR(current);
		}
		CDR(current) = next;
	}

	/*
	 * 2.
	 */
	foreach (entry, idList) {
		LispValue	name;
		List		rest;
		bool		found = false;

	again:
		name = CAR(entry);
		foreach (rest, CDR(entry)) {
			if (equal(name, CAR(rest))) {
				found = true;
				break;
			}
		}
		if (found) {
			/*
			 * entry list must be of length >= 2 or else no match
			 *
			 * so, remove this entry.
			 */
			CAR(entry) = CADR(entry);
			CDR(entry) = CDR(CDR(entry));

			found = false;
			goto again;
		}
	}

	/*
	 * 3.
	 */
	relation = RelationNameOpenHeapRelation(
		InheritancePrecidenceListRelationName);
	desc = RelationGetTupleDescriptor(relation);

	seqNumber = 1;

	foreach (entry, idList) {
		Datum		datum[
			InheritancePrecidenceListRelationNumberOfAttributes];
		char		null[
			InheritancePrecidenceListRelationNumberOfAttributes];

		datum[0] = ObjectIdGetDatum(relationId);	/* iplrel */
		datum[1] = ObjectIdGetDatum(CInteger(CAR(entry)));
								/*iplinherits*/
		datum[2] = Int16GetDatum(seqNumber);		/* iplseqno */

		null[0] = ' ';
		null[1] = ' ';
		null[2] = ' ';

		tuple = FormHeapTuple(
			InheritancePrecidenceListRelationNumberOfAttributes,
			desc, datum, null);

		RelationInsertHeapTuple(relation, tuple, NULL);	/* XXX retval */
		pfree(tuple);

		seqNumber += 1;
	}
	RelationCloseHeapRelation(relation);
}
