/* ----------------------------------------------------------------
 *   FILE
 *	pg_operator.c
 *	
 *   DESCRIPTION
 *	routines to support manipulation of the pg_operator relation
 *
 *   INTERFACE ROUTINES
 * 	OperatorGetWithOpenRelation
 * 	OperatorGet
 * 	OperatorShellMakeWithOpenRelation
 * 	OperatorShellMake
 *	
 *   NOTES
 *	these routines moved here from commands/define.c and
 *	somewhat cleaned up.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

 RcsId("$Header$");

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/htup.h"
#include "utils/rel.h"
#include "utils/log.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"

/* ----------------
 *	support functions in pg_type.c
 * ----------------
 */
extern ObjectId	TypeGet();
extern ObjectId	TypeShellMake();

/* ----------------------------------------------------------------
 * 	OperatorGetWithOpenRelation
 *
 *	preforms a scan on pg_operator for an operator tuple
 *	with given name and left/right type oids.
 * ----------------------------------------------------------------
 */
ObjectId
OperatorGetWithOpenRelation(pg_operator_desc, operatorName,
			    leftObjectId, rightObjectId)
    Relation	pg_operator_desc;	/* reldesc for pg_operator */
    Name 	operatorName;		/* name of operator to fetch */
    ObjectId 	leftObjectId;		/* left oid of operator to fetch */
    ObjectId	rightObjectId;		/* right oid of operator to fetch */
{
    HeapScanDesc	pg_operator_scan;
    ObjectId		operatorObjectId;
    HeapTuple 		tup;

    static ScanKeyEntryData	opKey[3] = {
	{ 0, OperatorNameAttributeNumber,  NameEqualRegProcedure },
	{ 0, OperatorLeftAttributeNumber,  ObjectIdEqualRegProcedure },
	{ 0, OperatorRightAttributeNumber, ObjectIdEqualRegProcedure },
    };

	fmgr_info(NameEqualRegProcedure,     &opKey[0].func, &opKey[0].nargs);
	fmgr_info(ObjectIdEqualRegProcedure, &opKey[1].func, &opKey[1].nargs);
	fmgr_info(ObjectIdEqualRegProcedure, &opKey[2].func, &opKey[2].nargs);

    /* ----------------
     *	form scan key
     * ----------------
     */
    opKey[0].argument = NameGetDatum(operatorName);
    opKey[1].argument = ObjectIdGetDatum(leftObjectId);
    opKey[2].argument = ObjectIdGetDatum(rightObjectId);

    /* ----------------
     *	begin the scan
     * ----------------
     */
    pg_operator_scan = RelationBeginHeapScan(pg_operator_desc,
					     0,
					     SelfTimeQual,
					     3,
					     (ScanKey) opKey);

    /* ----------------
     *	fetch the operator tuple, if it exists, and determine
     *  the proper return oid value.
     * ----------------
     */
    tup = HeapScanGetNextTuple(pg_operator_scan, 0, (Buffer *) 0);
    operatorObjectId = HeapTupleIsValid(tup) ? tup->t_oid : InvalidObjectId;

    /* ----------------
     *	close the scan and return the oid.
     * ----------------
     */
    HeapScanEnd(pg_operator_scan);

    return
	operatorObjectId;
}

/* ----------------------------------------------------------------
 * 	OperatorGet
 *
 *	finds the operator associated with the specified name
 *	and left and right type names.
 * ----------------------------------------------------------------
 */
ObjectId
OperatorGet(operatorName, leftTypeName, rightTypeName)
    Name 	operatorName; 	/* name of operator */
    Name 	leftTypeName; 	/* lefthand type */
    Name 	rightTypeName;	/* righthand type */
{
    Relation		pg_operator_desc;
    HeapScanDesc	pg_operator_scan;
    HeapTuple 		tup;
    
    ObjectId		operatorObjectId;
    ObjectId 		leftObjectId = InvalidObjectId;
    ObjectId		rightObjectId = InvalidObjectId;
    bool		leftDefined = false;
    bool		rightDefined = false;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(operatorName));
    Assert(NameIsValid(leftTypeName) || NameIsValid(rightTypeName));
	
    /* ----------------
     *	look up the operator types.
     *
     *  Note: types must be defined before operators
     * ----------------
     */
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

    /* ----------------
     *	open the pg_operator relation
     * ----------------
     */
    pg_operator_desc = RelationNameOpenHeapRelation(OperatorRelationName);

    /* ----------------
     *	get the oid for the operator with the appropriate name
     *  and left/right types.
     * ----------------
     */
    operatorObjectId = OperatorGetWithOpenRelation(pg_operator_desc,
						   operatorName,
						   leftObjectId,
						   rightObjectId);

    /* ----------------
     *	close the relation and return the operator oid.
     * ----------------
     */
    RelationCloseHeapRelation(pg_operator_desc);

    return
	operatorObjectId;
}

/* ----------------------------------------------------------------
 * 	OperatorShellMakeWithOpenRelation
 *
 * ----------------------------------------------------------------
 */
ObjectId
OperatorShellMakeWithOpenRelation(pg_operator_desc, operatorName,
				  leftObjectId, rightObjectId)
    Relation 	pg_operator_desc;
    Name	operatorName;
    ObjectId 	leftObjectId;
    ObjectId	rightObjectId;
{
    register int 	i;
    HeapTuple 		tup;
    char        	*values[ OperatorRelationNumberOfAttributes ];
    char		nulls[ OperatorRelationNumberOfAttributes ];
    ObjectId		operatorObjectId;
    
    /* ----------------
     *	initialize our nulls[] and values[] arrays
     * ----------------
     */
    for (i = 0; i < OperatorRelationNumberOfAttributes; ++i) {
	nulls[i] = ' ';
	values[i] = NULL;	/* redundant, but safe */
    }
    
    /* ----------------
     *	initialize values[] with the type name and 
     * ----------------
     */
    i = 0;
    values[i++] = (char *) operatorName;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) (uint16) 0;

    values[i++] = (char *)'b';	/* fill oprkind with a bogus value */

    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) (Boolean) 0;
    values[i++] = (char *) leftObjectId; 	/* <-- left oid */
    values[i++] = (char *) rightObjectId; 	/* <-- right oid */
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;
    values[i++] = (char *) InvalidObjectId;

    /* ----------------
     *	create a new operator tuple
     * ----------------
     */
    tup = FormHeapTuple(OperatorRelationNumberOfAttributes,
			&pg_operator_desc->rd_att,
			values,
			nulls);

    /* ----------------
     *	insert our "shell" operator tuple and
     *  close the relation
     * ----------------
     */
    RelationInsertHeapTuple(pg_operator_desc, tup, (double *) NULL);
    operatorObjectId = tup->t_oid;

    /* ----------------
     *	free the tuple and return the operator oid
     * ----------------
     */
    pfree(tup);
    
    return
	operatorObjectId;   
}

/* ----------------------------------------------------------------
 * 	OperatorShellMake
 *
 * 	Specify operator name and left and right type names,
 *	fill an operator struct with this info and NULL's,
 *	call RelationInsertHeapTuple and return the ObjectId
 *	to the caller.
 * ----------------------------------------------------------------
 */
ObjectId
OperatorShellMake(operatorName, leftTypeName, rightTypeName)
    Name	operatorName, leftTypeName, rightTypeName;
{    
    Relation 		pg_operator_desc;
    ObjectId		operatorObjectId;
    
    ObjectId 		leftObjectId = InvalidObjectId;
    ObjectId		rightObjectId = InvalidObjectId;
    bool 		leftDefined = false;
    bool		rightDefined = false;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(PointerIsValid(operatorName));
    Assert(NameIsValid(leftTypeName) || NameIsValid(rightTypeName));

    /* ----------------
     *	get the left and right type oid's for this operator
     * ----------------
     */
    if (NameIsValid(leftTypeName))
	leftObjectId = TypeGet(leftTypeName, &leftDefined);

    if (NameIsValid(rightTypeName))
	rightObjectId = TypeGet(rightTypeName, &rightDefined);

    if (!((ObjectIdIsValid(leftObjectId) && leftDefined) ||
	  (ObjectIdIsValid(rightObjectId) && rightDefined)))
	elog(WARN, "OperatorShellMake: no valid argument types??");
    
    /* ----------------
     *	open pg_operator
     * ----------------
     */
    pg_operator_desc = RelationNameOpenHeapRelation(OperatorRelationName);

    /* ----------------
     *	add a "shell" operator tuple to the operator relation
     *  and recover the shell tuple's oid.
     * ----------------
     */
    operatorObjectId =
	OperatorShellMakeWithOpenRelation(pg_operator_desc,
					  operatorName,
					  leftObjectId,
					  rightObjectId);
    /* ----------------
     *	close the operator relation and return the oid.
     * ----------------
     */
    RelationCloseHeapRelation(pg_operator_desc);
    
    return
	operatorObjectId;
}

/* --------------------------------
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
 * --------------------------------
 */

int  /* return status */
OperatorDef(operatorName, definedOK, leftTypeName, rightTypeName,
	    procedureName, precedence, isLeftAssociative,
	    commutatorName, negatorName, restrictionName, joinName,
	    canHash, leftSortName, rightSortName)
    
    /* "X" indicates an optional argument (i.e. one that can be NULL) */
    Name 	operatorName;	 	/* operator name */
    int 	definedOK; 		/* operator can already have an oid? */
    Name 	leftTypeName;	 	/* X left type name */
    Name 	rightTypeName;		/* X right type name */
    Name 	procedureName;		/* procedure oid for operator code */
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
    Relation 		pg_operator_desc;

    HeapScanDesc 	pg_operator_scan;
    HeapTuple 		tup;
    Buffer 		buffer;
    ItemPointerData	itemPointerData;
    char	 	nulls[ OperatorRelationNumberOfAttributes ];
    char	 	replaces[ OperatorRelationNumberOfAttributes ];
    char 		*values[ OperatorRelationNumberOfAttributes ];
    ObjectId 		other_oid;
    ObjectId		operatorObjectId;
    ObjectId		leftTypeId = InvalidObjectId;
    ObjectId		rightTypeId = InvalidObjectId;
    ObjectId		commutatorId = InvalidObjectId;
    ObjectId		negatorId = InvalidObjectId;
    bool 		leftDefined = false;
    bool		rightDefined = false;
    Name		name[4];
    
    static ScanKeyEntryData	opKey[3] = {
	{ 0, OperatorNameAttributeNumber, NameEqualRegProcedure },
	{ 0, OperatorLeftAttributeNumber, ObjectIdEqualRegProcedure },
	{ 0, OperatorRightAttributeNumber, ObjectIdEqualRegProcedure },
    };

	fmgr_info(NameEqualRegProcedure,     &opKey[0].func, &opKey[0].nargs);
	fmgr_info(ObjectIdEqualRegProcedure, &opKey[1].func, &opKey[1].nargs);
	fmgr_info(ObjectIdEqualRegProcedure, &opKey[2].func, &opKey[2].nargs);

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(operatorName));
    Assert(PointerIsValid(leftTypeName) || PointerIsValid(rightTypeName));
    Assert(NameIsValid(procedureName));

    operatorObjectId = 	OperatorGet(operatorName,
				    leftTypeName,
				    rightTypeName);
    
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

    /* ----------------
     * Look up registered procedures -- find the return type
     * of procedureName to place in "result" field.
     * Do this before shells are created so we don't
     * have to worry about deleting them later.
     * ----------------
     */
    tup = SearchSysCacheTuple(PRONAME,
			      (char *) procedureName,
			      (char *) NULL,
			      (char *) NULL,
			      (char *) NULL);
    
    if (!PointerIsValid(tup))
	elog(WARN, "OperatorDef: operator procedure %s nonexistent",
	     procedureName);      

    values[ OperatorProcedureAttributeNumber-1 ] =
	(char *) tup->t_oid;
    values[ OperatorResultAttributeNumber-1 ] =
	(char *) ((struct proc *) GETSTRUCT(tup))->prorettype;

    /* ----------------
     *	find restriction
     * ----------------
     */
    if (NameIsValid(restrictionName)) { 	/* optional */
	tup = SearchSysCacheTuple(PRONAME,
				  (char *) restrictionName,
				  (char *) NULL,
				  (char *) NULL,
				  (char *) NULL);
	if (!HeapTupleIsValid(tup))
	    elog(WARN,
		 "OperatorDef: restriction proc %s nonexistent",
		 restrictionName);      
	
	values[ OperatorRestrictAttributeNumber-1 ] = (char *) tup->t_oid;
    } else
	values[ OperatorRestrictAttributeNumber-1 ] = (char *) InvalidObjectId;

    /* ----------------
     *	find join
     * ----------------
     */
    if (NameIsValid(joinName)) { 		/* optional */
	tup = SearchSysCacheTuple(PRONAME,
				  (char *) joinName,
				  (char *) NULL,
				  (char *) NULL,
				  (char *) NULL);
	if (!HeapTupleIsValid(tup))
	    elog(WARN,
		 "OperatorDef: join proc %s nonexistent",
		 joinName);      
	values[OperatorJoinAttributeNumber-1] =	(char *) tup->t_oid;
    } else
	values[OperatorJoinAttributeNumber-1] =	(char *) InvalidObjectId;

    /* ----------------
     * set up values in the operator tuple
     * ----------------
     */
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

	    /* for the commutator, switch order of arguments */
	    if (j == 0) {
	        other_oid = OperatorGet(name[j],
			          rightTypeName,
			          leftTypeName);
		commutatorId = other_oid;
	    } else {
	        other_oid = OperatorGet(name[j],
			          leftTypeName,
			          rightTypeName);
		if (j == 1)
		    negatorId = other_oid;
	    }

	    if (ObjectIdIsValid(other_oid)) /* already in catalogs */
		values[i++] = (char *) other_oid;
	    else if (!NameIsEqual(operatorName, name[j])) {
		/* not in catalogs, different from operator */

		/* for the commutator, switch order of arguments */
		if (j == 0) {
		    other_oid = OperatorShellMake(name[j],
					    rightTypeName,
					    leftTypeName);
		} else {
		    other_oid = OperatorShellMake(name[j],
					    leftTypeName,
					    rightTypeName);
		}

		if (!ObjectIdIsValid(other_oid))
		    elog(WARN,
			 "OperatorDef: can't create %s",
			 name[j]);     
		values[i++] = (char *) other_oid;
		
	    } else /* not in catalogs, same as operator ??? */
		values[i++] = (char *) InvalidObjectId;
	    
	} else 	/* new operator is optional */
	    values[i++] = (char *) InvalidObjectId;
    }

    /* last three fields were filled in first */

    /*
     * If we are adding to an operator shell, get its t_ctid and a
     * buffer.
     */
    pg_operator_desc = RelationNameOpenHeapRelation(OperatorRelationName);

    if (operatorObjectId) {
	opKey[0].argument = NameGetDatum(operatorName);
	opKey[1].argument = ObjectIdGetDatum(leftTypeId);
	opKey[2].argument = ObjectIdGetDatum(rightTypeId);

	pg_operator_scan = RelationBeginHeapScan(pg_operator_desc,
						 0,
						 SelfTimeQual,
						 3,
						 (ScanKey) opKey);
	
	tup = HeapScanGetNextTuple(pg_operator_scan, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
	    tup = ModifyHeapTuple(tup,
				  buffer,
				  pg_operator_desc,
				  values,
				  nulls,
				  replaces);
	    
	    ItemPointerCopy(&tup->t_ctid, &itemPointerData);
	    setheapoverride(true);
	    RelationReplaceHeapTuple(pg_operator_desc, &itemPointerData, tup);
	    setheapoverride(false);
	} else
	    elog(WARN, "OperatorDef: no operator %d", other_oid);
	
	HeapScanEnd(pg_operator_scan);
	
    } else {
	tup = FormHeapTuple(OperatorRelationNumberOfAttributes,
			    &pg_operator_desc->rd_att,
			    values,
			    nulls);

	RelationInsertHeapTuple(pg_operator_desc, tup, (double *) NULL);
	operatorObjectId = tup->t_oid;
    }

    RelationCloseHeapRelation(pg_operator_desc);

    /*
     *  It's possible that we're creating a skeleton operator here for
     *  the commute or negate attributes of a real operator.  If we are,
     *  then we're done.  If not, we may need to update the negator and
     *  commutator for this attribute.  The reason for this is that the
     *  user may want to create two operators (say < and >=).  When he
     *  defines <, if he uses >= as the negator or commutator, he won't
     *  be able to insert it later, since (for some reason) define operator
     *  defines it for him.  So what he does is to define > without a
     *  negator or commutator.  Then he defines >= with < as the negator
     *  and commutator.  As a side effect, this will update the > tuple
     *  if it has no commutator or negator defined.
     *
     *  Alstublieft, Tom Vijlbrief.
     */
    if (!definedOK)
	OperatorUpd(operatorObjectId, commutatorId, negatorId);
}

/* ----------------------------------------------------------------
 * OperatorUpd
 *
 *  For a given operator, look up its negator and commutator operators.
 *  If they are defined, but their negator and commutator operators
 *  (respectively) are not, then use the new operator for neg and comm.
 *  This solves a problem for users who need to insert two new operators
 *  which are the negator or commutator of each other.
 * ---------------------------------------------------------------- 
 */
OperatorUpd(baseId, commId, negId)
    ObjectId baseId;
    ObjectId commId;
    ObjectId negId;
{
    register		i, j;
    Relation 		pg_operator_desc;

    HeapScanDesc 	pg_operator_scan;
    HeapTuple 		tup;
    Buffer 		buffer;
    ItemPointerData	itemPointerData;
    char	 	nulls[ Natts_pg_operator ];
    char	 	replaces[ Natts_pg_operator ];
    char 		*values[ Natts_pg_operator ];
    
    static ScanKeyEntryData	opKey[1] = {
	{ 0, ObjectIdAttributeNumber, ObjectIdEqualRegProcedure },
    };

	fmgr_info(ObjectIdEqualRegProcedure, &opKey[0].func, &opKey[0].nargs);

    for (i = 0; i < Natts_pg_operator; ++i) {
	values[i] = (char *) NULL;
	replaces[i] = ' ';
	nulls[i] = ' ';
    }

    pg_operator_desc = RelationNameOpenHeapRelation(OperatorRelationName);

    /* check and update the commutator, if necessary */
    opKey[0].argument = ObjectIdGetDatum(commId);

    pg_operator_scan = RelationBeginHeapScan(pg_operator_desc,
					     0,
					     SelfTimeQual,
					     1,
					     (ScanKey) opKey);
    
    tup = HeapScanGetNextTuple(pg_operator_scan, 0, &buffer);

    /* if the commutator and negator are the same operator, do one update */
    if (commId == negId) {
	if (HeapTupleIsValid(tup)) {
	   struct operator *t;

	   t = (struct operator *) GETSTRUCT(tup);
	   if (!ObjectIdIsValid(t->oprcom)
	       || !ObjectIdIsValid(t->oprnegate)) {

		if (!ObjectIdIsValid(t->oprnegate)) {
		    values[Anum_pg_operator_oprnegate] =
				    (char *)ObjectIdGetDatum(baseId);
		    replaces[ Anum_pg_operator_oprnegate ] = 'r';
		}

		if (!ObjectIdIsValid(t->oprcom)) {
		    values[Anum_pg_operator_oprcom] =
				    (char *)ObjectIdGetDatum(baseId);
		    replaces[ Anum_pg_operator_oprcom ] = 'r';
		}

		tup = ModifyHeapTuple(tup,
				      buffer,
				      pg_operator_desc,
				      values,
				      nulls,
				      replaces);

		ItemPointerCopy(&tup->t_ctid, &itemPointerData);
		setheapoverride(true);
		RelationReplaceHeapTuple(pg_operator_desc,
					 &itemPointerData,
					 tup);
		setheapoverride(false);
	    }
	}
	HeapScanEnd(pg_operator_scan);

	RelationCloseHeapRelation(pg_operator_desc);

	return;
    }

    /* if commutator and negator are different, do two updates */
    if (HeapTupleIsValid(tup) &&
       !(ObjectIdIsValid(((struct operator *) GETSTRUCT(tup))->oprcom))) {
	values[ Anum_pg_operator_oprcom ] = (char *) ObjectIdGetDatum(baseId);
	replaces[ Anum_pg_operator_oprcom ] = 'r';
	tup = ModifyHeapTuple(tup,
			      buffer,
			      pg_operator_desc,
			      values,
			      nulls,
			      replaces);

	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
	setheapoverride(true);
	RelationReplaceHeapTuple(pg_operator_desc, &itemPointerData, tup);
	setheapoverride(false);

	values[ Anum_pg_operator_oprcom ] = (char *) NULL;
	replaces[ Anum_pg_operator_oprcom ] = ' ';
    }

    /* check and update the negator, if necessary */
    opKey[0].argument = ObjectIdGetDatum(negId);

    pg_operator_scan = RelationBeginHeapScan(pg_operator_desc,
					     0,
					     SelfTimeQual,
					     1,
					     (ScanKey) opKey);
    
    tup = HeapScanGetNextTuple(pg_operator_scan, 0, &buffer);
    if (HeapTupleIsValid(tup) &&
       !(ObjectIdIsValid(((struct operator *) GETSTRUCT(tup))->oprnegate))) {
	values[Anum_pg_operator_oprnegate] = (char *) ObjectIdGetDatum(baseId);
	replaces[ Anum_pg_operator_oprnegate ] = 'r';
	tup = ModifyHeapTuple(tup,
			      buffer,
			      pg_operator_desc,
			      values,
			      nulls,
			      replaces);

	ItemPointerCopy(&tup->t_ctid, &itemPointerData);
	setheapoverride(true);
	RelationReplaceHeapTuple(pg_operator_desc, &itemPointerData, tup);
	setheapoverride(false);
    }
    HeapScanEnd(pg_operator_scan);

    RelationCloseHeapRelation(pg_operator_desc);
}


/* ----------------------------------------------------------------
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
 * ----------------------------------------------------------------
 */
void
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
/*  ObjectId	newOpObjectId; */
    int	 	definedOK;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(NameIsValid(operatorName));
    Assert(PointerIsValid(leftTypeName) || PointerIsValid(rightTypeName));
    Assert(NameIsValid(procedureName));

    /* ----------------
     *	get the oid's of the operator's associated operators, if possible.
     * ----------------
     */
    if (NameIsValid(commutatorName))
	commObjectId = OperatorGet(commutatorName,  /* commute type order */
				   rightTypeName,
				   leftTypeName);

    if (NameIsValid(negatorName))
	negObjectId  = OperatorGet(negatorName,
				   leftTypeName,
				   rightTypeName);

    if (NameIsValid(leftSortName))
	leftSortObjectId = OperatorGet(leftSortName,
				       leftTypeName,
				       rightTypeName);

    if (NameIsValid(rightSortName))
	rightSortObjectId = OperatorGet(rightSortName,
					rightTypeName,
					leftTypeName);

    /* ----------------
     *  Use OperatorDef() to define the specified operator and
     *  also create shells for the operator's associated operators
     *  if they don't already exist.
     *
     *	This operator should not be defined yet.
     * ----------------
     */
    definedOK = 0;
    
    OperatorDef(operatorName, definedOK,
		leftTypeName, rightTypeName, procedureName,
		precedence, isLeftAssociative,
		commutatorName, negatorName, restrictionName, joinName,
		canHash, leftSortName, rightSortName);

    /*
    newOpObjectId = OperatorGet(operatorName, leftTypeName, rightTypeName);
    */
    
    /* ----------------
     *	Now fill in information in the operator's associated
     *  operators.
     *
     *  These operators should be defined or have shells defined.
     * ----------------
     */
    definedOK = 1; 

    if (!ObjectIdIsValid(commObjectId) && NameIsValid(commutatorName))
	OperatorDef(commutatorName,
		    definedOK,   
		    leftTypeName,	/* should eventually */
		    rightTypeName,      /* commute order */  
		    procedureName,
		    precedence,
		    isLeftAssociative,
		    operatorName,	/* commutator */
		    negatorName,
		    restrictionName,
		    joinName,
		    canHash,
		    rightSortName,
		    leftSortName);
    
    if (!ObjectIdIsValid(negObjectId) && NameIsValid(negatorName))
	OperatorDef(negatorName,
		    definedOK,
		    leftTypeName,
		    rightTypeName,
		    procedureName,
		    precedence,
		    isLeftAssociative,
		    commutatorName,
		    operatorName,	/* negator */
		    restrictionName,
		    joinName,
		    canHash,
		    leftSortName,
		    rightSortName);

    if (!ObjectIdIsValid(leftSortObjectId) && NameIsValid(leftSortName))
	OperatorDef(leftSortName,
		    definedOK,
		    leftTypeName,
		    rightTypeName,
		    procedureName,
		    precedence,
		    isLeftAssociative,
		    commutatorName,
		    negatorName,
		    restrictionName,
		    joinName,
		    canHash,
		    operatorName,	/* left sort */
		    rightSortName);

    if (!ObjectIdIsValid(rightSortObjectId) && NameIsValid(rightSortName))
	OperatorDef(rightSortName,
		    definedOK,
		    leftTypeName,
		    rightTypeName,
		    procedureName,
		    precedence,
		    isLeftAssociative,
		    commutatorName,
		    negatorName,
		    restrictionName,
		    joinName,
		    canHash,
		    leftSortName,
		    operatorName);	/* right sort */
}

/* find the default type for the right arg of a binary operator */
HeapTuple
FindDefaultType(operatorName, leftTypeId)
char *operatorName;
int leftTypeId;
{
    Relation            pg_operator_desc;
    HeapScanDesc        pg_operator_scan;
    HeapTuple           tup1, tup2;  /* returned tuples */
    Buffer              buffer;

    static ScanKeyEntryData     opKey[3] = {
        { 0, OperatorNameAttributeNumber, NameEqualRegProcedure },
        { 0, OperatorLeftAttributeNumber, ObjectIdEqualRegProcedure },
        { 0, OperatorRightAttributeNumber, ObjectIdEqualRegProcedure },
    };

	fmgr_info(NameEqualRegProcedure,     &opKey[0].func, &opKey[0].nargs);
	fmgr_info(ObjectIdEqualRegProcedure, &opKey[1].func, &opKey[1].nargs);
	fmgr_info(ObjectIdEqualRegProcedure, &opKey[2].func, &opKey[2].nargs);

    pg_operator_desc = RelationNameOpenHeapRelation(OperatorRelationName);

        opKey[0].argument = NameGetDatum(operatorName);
        opKey[1].argument = ObjectIdGetDatum(leftTypeId);

        pg_operator_scan = RelationBeginHeapScan(pg_operator_desc,
                                                 0,
                                                 SelfTimeQual,
                                                 2,
                                                 (ScanKey) opKey);

        tup1 = HeapScanGetNextTuple(pg_operator_scan, 0, &buffer);
        if (!HeapTupleIsValid(tup1)) {
            elog(WARN, "OperatorDef: no operator %s", operatorName);
            return ((HeapTuple)NULL);
        }
        tup2 = HeapScanGetNextTuple(pg_operator_scan, 0, &buffer);

        HeapScanEnd(pg_operator_scan);

        RelationCloseHeapRelation(pg_operator_desc);

        if (HeapTupleIsValid(tup2)) {
            return ((HeapTuple)NULL); /* failed to find default type, since 
 				 right arg can take on two or more types */
        }

        return (tup1);

}
