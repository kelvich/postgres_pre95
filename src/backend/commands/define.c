/*
 * define.c --
 *	POSTGRES define (function | type | operator) utility code.
 *
 * NOTES:
 *	These things must be defined and committed in the following order:
 *		input/output, recv/send procedures
 *		type
 *		operators
 */

#include "c.h"

RcsId("$Header$");

#include <strings.h>	/* XXX style */
#include "catalog.h"	/* XXX obsolete file, needed for (struct proc), etc. */

#include "anum.h"
#include "catname.h"
#include "default.h"	/* for FetchDefault */
#include "fmgr.h"	/* for fmgr */
#include "ftup.h"
#include "heapam.h"
#include "htup.h"
#include "log.h"
#include "manip.h"
#include "name.h"
#include "parse.h"	/* for ARG */
#include "pg_lisp.h"
#include "rproc.h"
#include "syscache.h"
#include "tqual.h"

#include "defrem.h"

ObjectId	TypeGet();
static ObjectId	OperatorGet();
static ObjectId	TypeShellMake();
static ObjectId	OperatorShellMake();
static int	OperatorDef();
extern		TypeDefine();
extern		ProcedureDefine();
extern		OperatorDefine();

/*#define USEPARGS	/* XXX */

/*
 * TypeGet
 *
 *	Finds the ObjectId of a type, even if uncommitted; "defined"
 *	is only set if the type has actually been defined, i.e., if
 *	the type tuple is not a shell.
 *
 *	Also called from util/remove.c
 */
ObjectId
TypeGet(typeName, defined)
	Name 	typeName; 	/* name of type to be fetched */
	bool 	*defined; 	/* has the type been defined? */
{
	Relation	relation;
	HeapScanDesc	scan;
	HeapTuple	tup;
	static ScanKeyEntryData	typeKey[1] = {
		{ 0, TypeNameAttributeNumber, NameEqualRegProcedure }
	};
	
	Assert(NameIsValid(typeName));
	Assert(PointerIsValid(defined));

	typeKey[0].argument.name.value = typeName;
	relation = RelationNameOpenHeapRelation(TypeRelationName);
	scan = RelationBeginHeapScan(relation, 0, SelfTimeQual,
				     1, (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	if (!HeapTupleIsValid(tup)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		*defined = false;
		return(InvalidObjectId);
	}
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
	*defined = (bool) ((TypeTupleForm) GETSTRUCT(tup))->typisdefined;
	return(tup->t_oid);
}


/*
 * OperatorGet
 */
static ObjectId
OperatorGet(operatorName, leftTypeName, rightTypeName)
	Name 	operatorName; 	/* name of operator */
	Name 	leftTypeName; 	/* lefthand type */
	Name 	rightTypeName;	/* righthand type */
{
	Relation		relation;
	HeapScanDesc		scan;
	HeapTuple 		tup;
	ObjectId 		leftObjectId = InvalidObjectId;
	ObjectId		rightObjectId = InvalidObjectId;
	ObjectId		operatorObjectId;
	bool			leftDefined = false, rightDefined = false;
	static ScanKeyEntryData	operatorKey[3] = {
		{ 0, OperatorNameAttributeNumber, NameEqualRegProcedure },
		{ 0, OperatorLeftAttributeNumber, ObjectIdEqualRegProcedure },
		{ 0, OperatorRightAttributeNumber, ObjectIdEqualRegProcedure },
	};

	Assert(NameIsValid(operatorName));
	Assert(NameIsValid(leftTypeName) || NameIsValid(rightTypeName));
	
	/* Types must be defined before operators */
	if (NameIsValid(leftTypeName)) {
		leftObjectId = TypeGet(leftTypeName, &leftDefined);
		if (!ObjectIdIsValid(leftObjectId) || !leftDefined)
			elog(WARN, "OperatorGet: left type %s nonexistent",
			     leftTypeName);
	}
	if (NameIsValid(rightTypeName)) {
		rightObjectId = TypeGet(rightTypeName, &rightDefined);
		if (!ObjectIdIsValid(rightObjectId) || !rightDefined)
			elog(WARN, "OperatorGet: right type %s nonexistent",
			     rightTypeName);
	}
	if (!((ObjectIdIsValid(leftObjectId) && leftDefined) ||
	      (ObjectIdIsValid(rightObjectId) && rightDefined)))
		elog(WARN, "OperatorGet: no argument types??");

	operatorKey[0].argument.name.value = operatorName;
	operatorKey[1].argument.objectId.value = leftObjectId;
	operatorKey[2].argument.objectId.value = rightObjectId;

	relation = RelationNameOpenHeapRelation(OperatorRelationName);
	scan = RelationBeginHeapScan(relation, 0, SelfTimeQual,
				     3, (ScanKey) operatorKey);
	tup = HeapScanGetNextTuple(scan, 0, (Buffer *) 0);
	operatorObjectId =
		HeapTupleIsValid(tup) ? tup->t_oid : InvalidObjectId;
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
	return(operatorObjectId);
}


/* 
 * TypeShellMake
 */
static ObjectId
TypeShellMake(typeName)
	Name 	typeName;
{
	register		i;
	Relation 		rdesc;
	HeapTuple 		tup;
	char             	*values[TypeRelationNumberOfAttributes];
	char		 	nulls[TypeRelationNumberOfAttributes];

	Assert(PointerIsValid(typeName));

	for (i = 0; i < TypeRelationNumberOfAttributes; ++i) {
		nulls[i] = ' ';
		values[i] = (char *) NULL;
	}
	i = 0;
	values[i++] = (char *) typeName;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) (int16) 0;
	values[i++] = (char *) (int16) 0;
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	/*
	 * ... and fill typdefault with a bogus value
	 */
	values[i++] = fmgr(TextInRegProcedure, (char *) typeName);

	rdesc = RelationNameOpenHeapRelation(TypeRelationName);
	tup = FormHeapTuple(TypeRelationNumberOfAttributes,
			    &rdesc->rd_att, values, nulls);
	RelationInsertHeapTuple(rdesc, (HeapTuple) tup, (double *) NULL);
	RelationCloseHeapRelation(rdesc);
	return(tup->t_oid);
}


/* 
 * OperatorShellMake
 *
 * Specify operator name and left and right type names, fill
 * an operator struct with this info and NULL's, call RelationInsertHeapTuple
 * and return the ObjectId to the calling process
 */
static ObjectId
OperatorShellMake(operatorName, leftTypeName, rightTypeName)
	Name	operatorName, leftTypeName, rightTypeName;
{
	register		i;
	Relation 		rdesc;
	HeapTuple	 	tup;
	char	 		nulls[OperatorRelationNumberOfAttributes];
	char             	*values[OperatorRelationNumberOfAttributes];
	ObjectId 		leftObjectId = InvalidObjectId;
	ObjectId		rightObjectId = InvalidObjectId;
	bool 			leftDefined = false, rightDefined = false;

	Assert(PointerIsValid(operatorName));
	Assert(NameIsValid(leftTypeName) || NameIsValid(rightTypeName));

	if (NameIsValid(leftTypeName))
		leftObjectId = TypeGet(leftTypeName, &leftDefined);
	if (NameIsValid(rightTypeName))
		rightObjectId = TypeGet(rightTypeName, &rightDefined);

	if (!((ObjectIdIsValid(leftObjectId) && leftDefined) ||
	      (ObjectIdIsValid(rightObjectId) && rightDefined)))
		elog(WARN, "OperatorShellMake: no valid argument types??");

	for (i = 0; i < OperatorRelationNumberOfAttributes; ++i) {
		nulls[i] = ' ';
		values[i] = NULL;
	}
	i = 0;
	values[i++] = (char *) operatorName;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) (uint16) 0;
	/*
	 * ... and fill oprkind with a bogus value ...
	 */
	values[i++] = (char *)'b';
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) leftObjectId;
	values[i++] = (char *) rightObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	
	rdesc = RelationNameOpenHeapRelation(OperatorRelationName);
	tup = FormHeapTuple(OperatorRelationNumberOfAttributes,
			    &rdesc->rd_att, values, nulls);
	RelationInsertHeapTuple(rdesc, (HeapTuple) tup, (double *) NULL);
	RelationCloseHeapRelation(rdesc);
	return(OperatorGet(operatorName, leftTypeName, rightTypeName));
}


/*
 *   TypeDefine
 */
TypeDefine(typeName, internalSize, externalSize,
	   inputProcedure, outputProcedure, sendProcedure, receiveProcedure,
	   defaultTypeValue, passedByValue)
	Name	typeName;
	int16	internalSize;
	int16	externalSize;
	Name	inputProcedure;
	Name	outputProcedure;
	Name	sendProcedure;
	Name	receiveProcedure;
	char	*defaultTypeValue;	/* internal rep */
	Boolean	passedByValue;
{
	register		i, j;
	Relation 		rdesc;
	HeapScanDesc 		sdesc;
	static ScanKeyEntryData	typeKey[1] = {
		{ 0, TypeNameAttributeNumber, NameEqualRegProcedure }
	};
	HeapTuple 		tup;
	char 			nulls[TypeRelationNumberOfAttributes];
	char	 		replaces[TypeRelationNumberOfAttributes];
	char 			*values[TypeRelationNumberOfAttributes];
	ObjectId              	typeObjectId;
	Buffer			buffer;
	char 			procname[sizeof(NameData)+1];
	Name			procs[4];
	bool 			defined;
	ItemPointerData		itemPointerData;

	Assert(NameIsValid(typeName));
	Assert(NameIsValid(inputProcedure) && NameIsValid(outputProcedure));

	typeObjectId = TypeGet(typeName, &defined);
	if (ObjectIdIsValid(typeObjectId) && defined) {
		elog(WARN, "TypeDefine: type %s already defined", typeName);
	}
	if (externalSize == 0) {
		externalSize = -1;	/* variable length */
	}

	for (i = 0; i < TypeRelationNumberOfAttributes; ++i) {
		nulls[i] = ' ';
		replaces[i] = 'r';
		values[i] = (char *) NULL;
	}
	i = 0;
	values[i++] = (char *) typeName;
	values[i++] = (char *) getuid();
	values[i++] = (char *) internalSize;
	values[i++] = (char *) externalSize;
	values[i++] = (char *) passedByValue;
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) (Boolean) 1;
	values[i++] = (char *) InvalidObjectId;
	values[i++] = (char *) InvalidObjectId;
	procname[sizeof(NameData)] = '\0';	/* XXX feh */
	procs[0] = inputProcedure;
	procs[1] = outputProcedure;
	procs[2] = NameIsValid(receiveProcedure)
		? receiveProcedure : inputProcedure;
	procs[3] = NameIsValid(sendProcedure)
		? sendProcedure : outputProcedure;
	for (j = 0; j < 4; ++j) {
		(void) strncpy(procname, (char *) procs[j], sizeof(NameData));
		tup = SearchSysCacheTuple(PRONAME, procname, (char *) NULL,
					  (char *) NULL, (char *) NULL);
		if (!HeapTupleIsValid(tup))
			elog(WARN, "TypeDefine: procedure %s nonexistent",
			     procname);
		values[i++] = (char *) tup->t_oid;
	}
	values[i] = fmgr(TextInRegProcedure,
			 PointerIsValid(defaultTypeValue)
			 ? defaultTypeValue
			 : "-");	/* XXX default typdefault */
	
	rdesc = RelationNameOpenHeapRelation(TypeRelationName);
	typeKey[0].argument.name.value = typeName;
	sdesc = RelationBeginHeapScan(rdesc, 0, SelfTimeQual, 1,
				      (ScanKey) typeKey);
	tup = HeapScanGetNextTuple(sdesc, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
		tup = ModifyHeapTuple(tup, buffer, rdesc,
				      values, nulls, replaces);
		/* XXX may not be necessary */
		ItemPointerCopy(&tup->t_ctid, &itemPointerData);
		setheapoverride(true);
		RelationReplaceHeapTuple(rdesc, &itemPointerData, tup);
		setheapoverride(false);
	} else {
		tup = FormHeapTuple(TypeRelationNumberOfAttributes,
				    &rdesc->rd_att, values, nulls);
		RelationInsertHeapTuple(rdesc, tup, (double *) NULL);
	}
	HeapScanEnd(sdesc);
	RelationCloseHeapRelation(rdesc);
}


/*
 * OperatorDef
 *
 * This routine gets complicated because it allows the user to
 * specify operators that do not exist.  For example, if operator
 * "op" is being defined, the negator operator "negop" and the
 * commutator "commop" can also be defined without specifying
 * any information other than their names.  Since in order to
 * add "op" to the PG_OPERATOR catalog, all the ObjectId's for these
 * operators must be placed in the fields of "op", a forward
 * declaration is done on the commutator and negator operators.
 * This is called creating a shell, and its main effect is to
 * create a tuple in the PG_OPERARTOR catalog with minimal
 * information about the operator (just its name and types).
 * Forward declaration is used only for this purpose, it is
 * not available to the user as it is for type definition.
 *
 * Algorithm:
 * 
 * check if operator already defined 
 *    if so issue error if not definedOk, this is a duplicate
 *    but if definedOk, save the ObjectId -- filling in a shell
 * get the attribute types from relation descriptor for pg_operator
 * assign values to the fields of the operator:
 *   operatorName
 *   owner id (simply the user id of the caller)
 *   precedence
 *   operator "kind" either "b" for binary or "l" for left unary
 *   isLeftAssociative boolean
 *   canHash boolean
 *   leftType ObjectId -- type must already be defined
 *   rightTypeObjectId -- this is optional, enter ObjectId=0 if none specified
 *   resultType -- defer this, since it must be determined from
 *                 the pg_procedure catalog
 *   commutatorObjectId -- if this is NULL, enter ObjectId=0
 *                    else if this already exists, enter it's ObjectId
 *                    else if this does not yet exist, and is not
 *                      the same as the main operatorName, then create
 *                      a shell and enter the new ObjectId
 *                    else if this does not exist but IS the same
 *                      name as the main operator, set the ObjectId=0.
 *                      Later OperatorDefine will make another call
 *                      to OperatorDef which will cause this field
 *                      to be filled in (because even though the names
 *                      will be switched, they are the same name and
 *                      at this point this ObjectId will then be defined)
 *   negatorObjectId   -- same as for commutatorObjectId
 *   leftSortObjectId  -- same as for commutatorObjectId
 *   rightSortObjectId -- same as for commutatorObjectId
 *   operatorProcedure -- must access the pg_procedure catalog to get the
 *		   ObjectId of the procedure that actually does the operator
 *		   actions this is required.  Do an amgetattr to find out the
 *                 return type of the procedure 
 *   restrictionProcedure -- must access the pg_procedure catalog to get
 *                 the ObjectId but this is optional
 *   joinProcedure -- same as restrictionProcedure
 * now either insert or replace the operator into the pg_operator catalog
 * if the operator shell is being filled in
 *   access the catalog in order to get a valid buffer
 *   create a tuple using ModifyHeapTuple
 *   get the t_ctid from the modified tuple and call RelationReplaceHeapTuple
 * else if a new operator is being created
 *   create a tuple using FormHeapTuple
 *   call RelationInsertHeapTuple
 *
 ***************************** NOTE NOTE NOTE ***************************
 * 
 * Currently the system doesn't do anything with negators, commutators, etc.
 * Eventually it will be necessary to add a field to the operator catalog
 * indicating whether the operator is a negator and/or a commutator of a
 * real operator.  Furthermore, depending on how the commutation is implemented,
 * it may be necessary to switch the order of the type fields given to
 * the commutator.  Must be careful when doing this, because don't want
 * to also switch the order of the negator.  Should be considered whether
 * it makes sense to have the same negator for both the operator and
 * its commutator.
 */

static int         /* return status */
OperatorDef(operatorName, definedOK,
	    leftTypeName, rightTypeName, procedureName,
	    precedence, isLeftAssociative,
	    commutatorName, negatorName, restrictionName, joinName,
	    canHash,
	    leftSortName, rightSortName)
	/* "X" indicates an optional argument (i.e. one that can be NULL) */
	Name 	operatorName;	 	/* operator name */
	int 	definedOK; 		/* operator can already have a
					   ObjectId? */
	Name 	leftTypeName;	 	/* X left type name */
	Name 	rightTypeName;		/* X right type name */
	Name 	procedureName;		/* procedure ObjectId for
					   operator code */
	uint16 	precedence; 		/* operator precedence */
	Boolean	isLeftAssociative;	/* operator is left associative? */
	Name 	commutatorName;		/* X commutator operator name */
	Name 	negatorName;	 	/* X negator operator name */
	Name 	restrictionName;	/* X restriction sel. procedure name */
	Name 	joinName;		/* X join sel. procedure name */
	Boolean	canHash; 		/* possible hash operator? */
	Name 	leftSortName;		/* X left sort operator */
	Name 	rightSortName;		/* X right sort operator */
{
	register		i, j;
	Relation 		rdesc;
	static ScanKeyEntryData	operatorKey[3] = {
		{ 0, OperatorNameAttributeNumber, NameEqualRegProcedure },
		{ 0, OperatorLeftAttributeNumber, ObjectIdEqualRegProcedure },
		{ 0, OperatorRightAttributeNumber, ObjectIdEqualRegProcedure },
	};
	HeapScanDesc 		sdesc;
	HeapTuple 		tup;
	Buffer 			buffer;
	ItemPointerData		itemPointerData;
	char	 		nulls[OperatorRelationNumberOfAttributes];
	char	 		replaces[OperatorRelationNumberOfAttributes];
	char 			*values[OperatorRelationNumberOfAttributes];
	ObjectId 		oid, operatorObjectId;
	ObjectId		leftTypeId = InvalidObjectId;
	ObjectId		rightTypeId = InvalidObjectId;
	bool 			leftDefined = false, rightDefined = false;
	Name			name[4];

	Assert(NameIsValid(operatorName));
	Assert(PointerIsValid(leftTypeName) || PointerIsValid(rightTypeName));
	Assert(NameIsValid(procedureName));

	operatorObjectId =
		OperatorGet(operatorName, leftTypeName, rightTypeName);
	if (ObjectIdIsValid(operatorObjectId) && !definedOK)
		elog(WARN, "OperatorDef: operator %s already defined",
		     operatorName); 

	if (NameIsValid(leftTypeName))
		leftTypeId = TypeGet(leftTypeName, &leftDefined);
	if (NameIsValid(rightTypeName))
		rightTypeId = TypeGet(rightTypeName, &rightDefined);
	if (!((ObjectIdIsValid(leftTypeId && leftDefined)) ||
	      (ObjectIdIsValid(rightTypeId && rightDefined))))
		elog(WARN, "OperatorGet: no argument types??");

	for (i = 0; i < OperatorRelationNumberOfAttributes; ++i) {
		values[i] = (char *) NULL;
		replaces[i] = 'r';
		nulls[i] = ' ';
	}

	/*
	 * Look up registered procedures -- find the return type
	 * of procedureName to place in "result" field.
	 * Do this before shells are created so we don't
	 * have to worry about deleting them later.
	 */
	tup = SearchSysCacheTuple(PRONAME, (char *) procedureName,
				  (char *) NULL, (char *) NULL, (char *) NULL);
	if (!PointerIsValid(tup))
		elog(WARN, "OperatorDef: operator procedure %s nonexistent",
		     procedureName);      
	values[OperatorProcedureAttributeNumber-1] =
		(char *) tup->t_oid;
	values[OperatorResultAttributeNumber-1] =
		(char *) ((struct proc *) GETSTRUCT(tup))->prorettype;
	
	if (NameIsValid(restrictionName)) { 	/* optional */
		tup = SearchSysCacheTuple(PRONAME, (char *) restrictionName,
					  (char *) NULL, (char *) NULL,
					  (char *) NULL);
		if (!HeapTupleIsValid(tup))
			elog(WARN,
			     "OperatorDef: restriction proc %s nonexistent",
			     restrictionName);      
		values[OperatorRestrictAttributeNumber-1] =
			(char *) tup->t_oid;
	} else
		values[OperatorRestrictAttributeNumber-1] =
			(char *) InvalidObjectId;
	
	if (NameIsValid(joinName)) { 		/* optional */
		tup = SearchSysCacheTuple(PRONAME, (char *) joinName,
					  (char *) NULL, (char *) NULL,
					  (char *) NULL);
		if (!HeapTupleIsValid(tup))
			elog(WARN,
			     "OperatorDef: join proc %s nonexistent",
			     joinName);      
		values[OperatorJoinAttributeNumber-1] =
			(char *) tup->t_oid;
	} else
		values[OperatorJoinAttributeNumber-1] =
			(char *) InvalidObjectId;

	i = 0;
	values[i++] = (char *) operatorName;
	values[i++] = (char *) (ObjectId) getuid();
	values[i++] = (char *) precedence;
	values[i++] = (char *) (NameIsValid(leftTypeName) ?
		(NameIsValid(rightTypeName) ? 'b' : 'l') : 'r');
	values[i++] = (char *) isLeftAssociative;
	values[i++] = (char *) canHash;
	values[i++] = (char *) leftTypeId;
	values[i++] = (char *) rightTypeId;
	++i; 	/* Skip "prorettype", this was done above */
	
	/*
	 * Set up the other operators.  If they do not currently exist,
	 * set up shells in order to get ObjectId's and call OperatorDef
	 * again later to fill in the shells.
	 */
	name[0] = commutatorName;
	name[1] = negatorName;
	name[2] = leftSortName;
	name[3] = rightSortName;
	for (j = 0; j < 4; ++j) {
		if (NameIsValid(name[j])) {
			oid = OperatorGet(name[j],
					  leftTypeName, rightTypeName);
			if (ObjectIdIsValid(oid))
				/* already in catalogs */
				values[i++] = (char *) oid;
			else if (!NameIsEqual(operatorName, name[j])) {
				/* not in catalogs, different from operator */
			        /* NOTE -- eventually should switch order   */
			        /* for commutator's types                   */
				    oid = OperatorShellMake(name[j],
							leftTypeName,
							rightTypeName);
				if (!ObjectIdIsValid(oid))
					elog(WARN,
					     "OperatorDef: can't create %s",
					     name[j]);     
				values[i++] = (char *) oid;
			} else
				/* not in catalogs, same as operator ??? */
				values[i++] = (char *) InvalidObjectId;
		} else 	/* new operator is optional */
			values[i++] = (char *) InvalidObjectId;
	}
	/* last three fields were filled in first */

	/*
	 * If we are adding to an operator shell, get its t_ctid and a
	 * buffer.
	 */
	rdesc = RelationNameOpenHeapRelation(OperatorRelationName);
	if (operatorObjectId) {
		operatorKey[0].argument.name.value = operatorName;
		operatorKey[1].argument.objectId.value = leftTypeId;
		operatorKey[2].argument.objectId.value = rightTypeId;

		sdesc = RelationBeginHeapScan(rdesc, 0, SelfTimeQual, 3,
					      (ScanKey) operatorKey);
		tup = HeapScanGetNextTuple(sdesc, 0, &buffer);
		if (HeapTupleIsValid(tup)) {
			tup = ModifyHeapTuple(tup, buffer, rdesc,
					      values, nulls, replaces);
			ItemPointerCopy(&tup->t_ctid, &itemPointerData);
			setheapoverride(true);
			RelationReplaceHeapTuple(rdesc, &itemPointerData, tup);
			setheapoverride(false);
		} else
			elog(WARN, "OperatorDef: no operator %d", oid);
		HeapScanEnd(sdesc);
	} else {
		tup = FormHeapTuple(OperatorRelationNumberOfAttributes,
				    &rdesc->rd_att, values, nulls);
		RelationInsertHeapTuple(rdesc, tup, (double *) NULL);
	}
	RelationCloseHeapRelation(rdesc);
}  


/*
 * OperatorDefine
 *
 * Algorithm:
 *
 *  Since the commutator, negator, leftsortoperator, and rightsortoperator
 *  can be defined implicitly through OperatorDefine, must check before
 *  the main operator is added to see if they already exist.  If they
 *  do not already exist, OperatorDef makes a "shell" for each undefined
 *  one, and then OperatorDefine must call OperatorDef again to fill in
 *  each shell.  All this is necessary in order to get the right ObjectId's 
 *  filled into the right fields.
 *
 *  The "definedOk" flag indicates that OperatorDef can be called on
 *  the operator even though it already has an entry in the PG_OPERATOR
 *  relation.  This allows shells to be filled in.  The user cannot
 *  forward declare operators, this is strictly an internal capability.
 *
 *  When the shells are filled in by subsequent calls to OperatorDef,
 *  all the fields are the same as the definition of the original operator
 *  except that the target operator name and the original operatorName
 *  are switched.  In the case of commutator and negator, special flags
 *  are set to indicate their status, telling the executor(?) that
 *  the operands are to be switched, or the outcome of the procedure
 *  negated.
 * 
 * ************************* NOTE NOTE NOTE ******************************
 *  
 *  If the execution of this utility is interrupted, the pg_operator
 *  catalog may be left in an inconsistent state.  Similarly, if
 *  something is removed from the pg_operator, pg_type, or pg_procedure
 *  catalog while this is executing, the results may be inconsistent.
 *
 */
OperatorDefine(operatorName, leftTypeName, rightTypeName, procedureName,
	       precedence, isLeftAssociative, commutatorName, negatorName,
	       restrictionName, joinName, canHash, leftSortName, rightSortName)
	/* "X" indicates an optional argument (i.e. one that can be NULL) */
	Name 	operatorName;	 	/* operator name */
	Name 	leftTypeName;	 	/* X left type name */
	Name 	rightTypeName;	 	/* X right type name */
	Name 	procedureName;	 	/* procedure for operator */
	uint16 	precedence; 		/* operator precedence */
	Boolean	isLeftAssociative; 	/* operator is left associative */
	Name 	commutatorName;	 	/* X commutator operator name */
	Name 	negatorName;	 	/* X negator operator name */
	Name 	restrictionName;	/* X restriction sel. procedure */
	Name 	joinName;	 	/* X join sel. procedure name */
	Boolean	canHash; 		/* operator hashes */
	Name 	leftSortName;	 	/* X left sort operator */
	Name 	rightSortName;	 	/* X right sort operator */
{
	ObjectId 	commObjectId, negObjectId;
	ObjectId	leftSortObjectId, rightSortObjectId;
/*	ObjectId	newOpObjectId;*/
	int	 	definedOK;
	
	Assert(NameIsValid(operatorName));
	Assert(PointerIsValid(leftTypeName) || PointerIsValid(rightTypeName));
	Assert(NameIsValid(procedureName));

	if (NameIsValid(commutatorName))
		commObjectId = OperatorGet(commutatorName,  /* commute type order*/
					   rightTypeName, leftTypeName);
	if (NameIsValid(negatorName))
		negObjectId  = OperatorGet(negatorName,
					   leftTypeName, rightTypeName);
	if (NameIsValid(leftSortName))
		leftSortObjectId = OperatorGet(leftSortName,
					       leftTypeName, rightTypeName);
	if (NameIsValid(rightSortName))
		rightSortObjectId = OperatorGet(rightSortName,
						rightTypeName, leftTypeName);
	
	/* This operator should not be defined yet. */
	definedOK = 0;
	OperatorDef(operatorName, definedOK,
		    leftTypeName, rightTypeName, procedureName,
		    precedence, isLeftAssociative,
		    commutatorName, negatorName, restrictionName, joinName,
		    canHash, leftSortName, rightSortName);

/*	newOpObjectId = OperatorGet(operatorName, leftTypeName, rightTypeName);*/

	/* These operators should be defined or have shells defined. */
	definedOK = 1; 
	if (!ObjectIdIsValid(commObjectId) && NameIsValid(commutatorName))
		OperatorDef(commutatorName, definedOK,   
			    leftTypeName, rightTypeName,  /* should eventually */
			                                  /* commute order */
			    procedureName,
			    precedence, isLeftAssociative,
			    operatorName,	/* commutator */
			    negatorName, restrictionName, joinName,
			    canHash, rightSortName, leftSortName);
	if (!ObjectIdIsValid(negObjectId) && NameIsValid(negatorName))
		OperatorDef(negatorName, definedOK,
			    leftTypeName, rightTypeName, procedureName,
			    precedence, isLeftAssociative, commutatorName,
			    operatorName,	/* negator */
			    restrictionName, joinName,
			    canHash, leftSortName, rightSortName);
	if (!ObjectIdIsValid(leftSortObjectId) && NameIsValid(leftSortName))
		OperatorDef(leftSortName, definedOK,
			    leftTypeName, rightTypeName, procedureName,
			    precedence, isLeftAssociative, commutatorName,
			    negatorName, restrictionName, joinName,
			    canHash,
			    operatorName,	/* left sort */
			    rightSortName);
	if (!ObjectIdIsValid(rightSortObjectId) && NameIsValid(rightSortName))
		OperatorDef(rightSortName, definedOK,
			    leftTypeName, rightTypeName, procedureName,
			    precedence, isLeftAssociative, commutatorName,
			    negatorName, restrictionName, joinName,
			    canHash, leftSortName,
			    operatorName);	/* right sort */
}


/*
 * ProcedureDefine
 */
/*ARGSUSED*/
ProcedureDefine(procedureName, returnTypeName, languageName, fileName,
		canCache, parameterCount, parameters)
	Name 			procedureName;	
	Name 			returnTypeName;	
	Name 			languageName;	
	char 			*fileName;	/* XXX path to binary */
	Boolean			canCache;
	uint16 			parameterCount;
	struct attribute 	*parameters[];	/* XXX for use with PARGS */
{
	register		i;
	Relation 		rdesc;
	HeapTuple 		tup;
	char			nulls[ProcedureRelationNumberOfAttributes];
	char 			*values[ProcedureRelationNumberOfAttributes];
	ObjectId 		languageObjectId, typeObjectId;
#ifdef USEPARGS
	ObjectId		procedureObjectId;
#endif
	bool              	defined;

	Assert(NameIsValid(procedureName));
	Assert(NameIsValid(returnTypeName));
	Assert(NameIsValid(languageName));
	Assert(PointerIsValid(fileName));
	
	tup = SearchSysCacheTuple(PRONAME, (char *) procedureName,
				  (char *) NULL, (char *) NULL, (char *) NULL);
	if (HeapTupleIsValid(tup))
		elog(WARN, "ProcedureDefine: procedure %s already exists",
		     procedureName);
	tup = SearchSysCacheTuple(LANNAME, (char *) languageName,
				  (char *) NULL, (char *) NULL, (char *) NULL);
	if (!HeapTupleIsValid(tup))
		elog(WARN, "ProcedureDefine: no such language %s",
		     languageName);
	languageObjectId = tup->t_oid;
	typeObjectId = TypeGet(returnTypeName, &defined);
	if (!ObjectIdIsValid(typeObjectId)) {
		elog(NOTICE, "ProcedureDefine: return type %s not yet defined",
		     returnTypeName);
		typeObjectId = TypeShellMake(returnTypeName);
		if (!ObjectIdIsValid(typeObjectId))
			elog(WARN, "ProcedureDefine: could not create type %s",
			     returnTypeName);
	}
	
	for (i = 0; i < ProcedureRelationNumberOfAttributes; ++i) {
		nulls[i] = ' ';
		values[i] = (char *) NULL;
	}
	i = 0;
	values[i++] = (char *) procedureName;
	values[i++] = (char *) (ObjectId) getuid();
	values[i++] = (char *) languageObjectId;
	/* XXX isinherited is always false for now */
	values[i++] = (char *) (Boolean) 0;
	/* XXX istrusted is always false for now */
	values[i++] = (char *) (Boolean) 0;
	values[i++] = (char *) canCache;
	values[i++] = (char *) parameterCount;
	values[i++] = (char *) typeObjectId;
	{ 
		/*  XXX Fix this when prosrc is used */
		values[i++] = fmgr(TextInRegProcedure, "-");	/* prosrc */
		values[i++] = fmgr(TextInRegProcedure, fileName); /* probin */
	}
	rdesc = RelationNameOpenHeapRelation(ProcedureRelationName);
	tup = FormHeapTuple(ProcedureRelationNumberOfAttributes,
			    &rdesc->rd_att, values, nulls);
	RelationInsertHeapTuple(rdesc, (HeapTuple) tup, (double *) NULL);
#ifdef USEPARGS
	procedureObjectId = tup->t_oid;
#endif
	RelationCloseHeapRelation(rdesc);

	/* XXX Test to see if tuple inserted ?? */

	/* XXX don't fill in PARG for now (not used for anything) */
#ifdef USEPARGS
	rdesc = RelationNameOpenHeapRelation(ProcedureArgumentRelationName);
	for (i = 0; i < parameterCount; ++i) {
		j = 0;
		values[j++] = (char *) procedureObjectId;
		values[j++] = (char *) parameters[i]->attnum;
		/* XXX ignore array bound for now */
		values[j++] = (char *) '\0';
		values[j++] = (char *) parameters[i]->atttypid;
		tup = FormHeapTuple(ProcedureArgumentRelationNumberOfAttributes,
				    &rdesc->rd_att, values, nulls);
		RelationInsertHeapTuple(rdesc, (HeapTuple) tup,
					(double *) NULL);
	}
	RelationCloseHeapRelation(rdesc);
#endif /* USEPARGS */
}

void
DefineFunction(name, parameters)
	Name		name;
	LispValue	parameters;
{
	String		returnTypeName;
	String		languageName;
	String		fileName;
	bool		canCache;
	Count		argCount;
	TupleDesc	arg;
	LispValue	argList;
	LispValue	entry;

	/*
	 * Note:
	 * XXX	Checking of "name" validity (16 characters?) is needed.
	 */
	AssertArg(NameIsValid(name));
	AssertArg(listp(parameters));

	/*
	 * handle "[ language = X ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "language");
	if (null(entry)) {
		languageName = FetchDefault("language", "C");
	} else {
		languageName = DefineEntryGetString(entry);
	}

	/*
	 * handle "file = X"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters, "file");
	fileName = DefineEntryGetString(entry);

	/*
	 * handle "[ iscachable ]"
	 */
	entry = DefineListRemoveOptionalIndicator(&parameters, "iscachable");
	canCache = (bool)!null(entry);

	/*
	 * handle "returntype = X"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters, "returntype");
	returnTypeName = DefineEntryGetString(entry);

	/*
	 * handle "[ arg is (...) ]"
	 * XXX fix optional arg handling below
	 */
	argList = LispRemoveMatchingSymbol(&parameters, ARG);
	if (null(argList)) {
		argCount = 0;
		arg = NULL;
	} else {
		argCount = length(argList);
		arg = NULL;			
		if (argCount != 0) {
			int		index;
			LispValue	rest;

			arg = CreateTemplateTupleDesc(argCount);
			for (rest = argList; !null(rest); rest = CDR(rest)) {
				if (!lispStringp(CAR(CAR(rest)))) {
	elog(WARN, "DefineFunction: returntype = ?");
				}
			}
		}
		/*
		 * XXX for now, arg is not passed on.
		 */
		argCount = 0;
		arg = NULL;
	}

	DefineListAssertEmpty(parameters);

	ProcedureDefine(name, returnTypeName, languageName, fileName,
		canCache, argCount, arg);
}

void
DefineType(name, parameters)
	Name		name;
	LispValue	parameters;
{
	LispValue	entry;
	int16		internalLength;		/* int2 */
	int16		externalLength;		/* int2 */
	Name		inputName;
	Name		outputName;
	Name		sendName;
	Name		receiveName;
	char*		defaultValue;	/* Datum */
	bool		byValue;	/* Boolean */

	/*
	 * Note:
	 * XXX	Checking of "name" validity (16 characters?) is needed.
	 */
	AssertArg(NameIsValid(name));
	AssertArg(listp(parameters));

	/*
	 * handle "internallength = (number | variable)"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters,
		"internallength");
	internalLength = DefineEntryGetLength(entry);

	/*
	 * handle "[ externallength = (number | variable) ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters,
		"externallength");
	externalLength = 0;		/* FetchDefault? */
	if (!null(entry)) {
		externalLength = DefineEntryGetLength(entry);
	}

	/*
	 * handle "input = procedure"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters, "input");
	inputName = DefineEntryGetName(entry);

	/*
	 * handle "output = procedure"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters, "output");
	outputName = DefineEntryGetName(entry);

	/*
	 * handle "[ send = procedure ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "send");
	sendName = NULL;
	if (!null(entry)) {
		sendName = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ receive = procedure ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "receive");
	receiveName = NULL;
	if (!null(entry)) {
		receiveName = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ default = `...' ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "default");
	defaultValue = NULL;
	if (!null(entry)) {
		defaultValue = DefineEntryGetString(entry);
	}

	/*
	 * handle "[ passedbyvalue ]"
	 */
	entry = DefineListRemoveOptionalIndicator(&parameters, "passedbyvalue");
	byValue = (bool)!null(entry);

	DefineListAssertEmpty(parameters);

	TypeDefine(name, internalLength, externalLength, inputName, outputName,
		sendName, receiveName, defaultValue, byValue);
}

void
DefineOperator(name, parameters)
	Name		name;
	LispValue	parameters;
{
	LispValue	entry;
	Name 		functionName;	 	/* function for operator */
	Name 		typeName1;	 	/* first type name */
	Name 		typeName2;	 	/* optional second type name */
	uint16 		precedence; 		/* operator precedence */
	bool		canHash; 		/* operator hashes */
	bool	isLeftAssociative; 	/* operator is left associative */
	Name 	commutatorName;	 	/* optional commutator operator name */
	Name 	negatorName;	 	/* optional negator operator name */
	Name 	restrictionName;	/* optional restrict. sel. procedure */
	Name 	joinName;	 	/* optional join sel. procedure name */
	Name 	sortName1;	 	/* optional first sort operator */
	Name 	sortName2;	 	/* optional second sort operator */

	/*
	 * Note:
	 * XXX	Checking of "name" validity (16 characters?) is needed.
	 */
	AssertArg(NameIsValid(name));
	AssertArg(listp(parameters));

	/*
	 * XXX ( ... arg1 = typname [ , arg2 = typname ] ... )
	 * XXX is undocumented in the reference manual source as of 89/8/22.
	 */
	/*
	 * handle "arg1 = typname"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters, "arg1");
	typeName1 = DefineEntryGetName(entry);

	/*
	 * handle "[ arg2 = typname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "arg2");
	typeName2 = NULL;
	if (!null(entry)) {
		typeName2 = DefineEntryGetName(entry);
	}

	/*
	 * handle "procedure = proname"
	 */
	entry = DefineListRemoveRequiredAssignment(&parameters, "procedure");
	functionName = DefineEntryGetName(entry);

	/*
	 * handle "[ precedence = number ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "precedence");
	if (null(entry)) {
		precedence = 0;		/* FetchDefault? */
	} else {
		precedence = DefineEntryGetInteger(entry);
	}

	/*
	 * handle "[ associativity = (left|right|none|any) ]"
	 */
	/*
	 * XXX Associativity code below must be fixed when the catalogs and
	 * XXX the planner/executor support proper associativity semantics.
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "precedence");
	if (null(entry)) {
		isLeftAssociative = true;	/* XXX FetchDefault */
	} else {
		String	string;

		string = DefineEntryGetString(entry);
		if (StringEquals(string, "right")) {
			isLeftAssociative = false;
		} else if (!StringEquals(string, "left") &&
				!StringEquals(string, "none") &&
				!StringEquals(string, "any")) {
			elog(WARN, "Define: precedence = what?");
		} else {
			isLeftAssociative = true;
		}
	}

	/*
	 * handle "[ commutator = oprname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "commutator");
	commutatorName = NULL;
	if (!null(entry)) {
		commutatorName = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ negator = oprname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "negator");
	negatorName = NULL;
	if (!null(entry)) {
		negatorName = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ restrict = proname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "restrict");
	restrictionName = NULL;
	if (!null(entry)) {
		restrictionName = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ join = proname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "join");
	joinName = NULL;
	if (!null(entry)) {
		joinName = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ hashes ]"
	 */
	entry = DefineListRemoveOptionalIndicator(&parameters, "hashes");
	canHash = (bool)!null(entry);

	/*
	 * XXX ( ... [ , sort1 = oprname ] [ , sort2 = oprname ] ... )
	 * XXX is undocumented in the reference manual source as of 89/8/22.
	 */
	/*
	 * handle "[ sort1 = oprname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "sort1");
	sortName1 = NULL;
	if (!null(entry)) {
		sortName1 = DefineEntryGetName(entry);
	}

	/*
	 * handle "[ sort2 = oprname ]"
	 */
	entry = DefineListRemoveOptionalAssignment(&parameters, "sort2");
	sortName2 = NULL;
	if (!null(entry)) {
		sortName2 = DefineEntryGetName(entry);
	}

	DefineListAssertEmpty(parameters);

	OperatorDefine(name, typeName1, typeName2, functionName, precedence,
		isLeftAssociative, commutatorName, negatorName,
		restrictionName, joinName, canHash, sortName1, sortName2);
}
