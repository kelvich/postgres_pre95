/*    
 *    	lsyscache
 *    
 *    	Routines to access information within system caches	
 *    $Header$
 */


/*    
 *    		retrieve-cache-attribute
 *    		op_class
 *    		get_attname
 *    		get_attnum
 *    		get_atttype
 *    		get_opcode
 *    		op_mergesortable
 *    		op_hashjoinable
 *    		get_commutator
 *    		get_negator
 *    		get_oprrest
 *    		get_oprjoin
 *    		get_relnatts
 *    		get_rel_name
 *    		get_typlen
 *    		get_typbyval
 *    		get_typdefault
 *    
 *     NOTES:
 *    	Eventually, the index information should go through here, too.
 *    
 *    	Most of these routines call SearchSysCacheStruct() and thus simply
 *    	(1) allocate some space for the return struct and (2) call it.
 *    
 */

#include "tmp/c.h"

#include "nodes/pg_lisp.h"
#include "catalog/syscache.h"
#include "access/att.h"
#include "access/tupmacs.h"
#include "utils/rel.h"
#include "utils/palloc.h"
#include "utils/log.h"
#include "access/attnum.h"

#include "catalog/pg_amop.h"
#include "catalog/pg_type.h"
#include "catalog/pg_prs2rule.h"
#include "catalog/pg_prs2stub.h"

/* 
require ("cstructs");
require ("cdefs");
require ("cacheids");
*/

/*    
 *    	AttributeGetAttName
 *    
 *    	Returns a string.
 *    
 */

/*  .. get_attname
 */
Name
AttributeGetAttName (attribute)
     Attribute attribute ;
{
/*
  for (loop = 0;loop + 1(null) = LispNil;(null) = LispNil;) { 
  if ( loop == 16 == val == 0) {
  string (implode (newlist));
  }
  val = attribute_>attname (attribute,loop);
  ;
  if(!val == 0) {
  newlist = append1 (newlist,val);
  }
  }; /*XXX Do List */
  
}

/*    		---------- AMOP CACHES ----------
 */

/*    
 *    	op_class
 *    
 *    	Return t iff operator 'opno' is in operator class 'opclass'.
 *    
 */

/*  .. in-line-lambda%598040608, match-index-orclause
 */
bool
op_class (opno,opclass)
     ObjectId opno;
     int32 opclass ;
{
    AccessMethodOperatorTupleFormD amoptup;

    if (SearchSysCacheStruct(AMOPOPID, (char *) &amoptup,
			     (char *) ObjectIdGetDatum(opclass),
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL))
      return(true);
    else
      return(false);
}

/*    		---------- ATTRIBUTE CACHES ----------
 */

/*    
 *    	get_attname
 *    
 *    	Given the relation id and the attribute number,
 *    	return the "attname" field from the attribute relation.
 *    
 */

/*  .. fix-parsetree-attnums, parameterize, print_var
 *  .. resno-targetlist-entry
 */

Name
get_attname (relid,attnum)
     ObjectId relid;
     AttributeNumber attnum;
{
    AttributeTupleFormD att_tup;
    Name retval = (Name)NULL;

    if (SearchSysCacheStruct(ATTNUM, (char *) &att_tup,
			     (char *) ObjectIdGetDatum(relid),
			     (char *) UInt16GetDatum(attnum),
			     (char *) NULL, (char *) NULL)) {
	retval = (Name)palloc(sizeof(att_tup.attname));
	bcopy(&(att_tup.attname),retval,sizeof(att_tup.attname));
	return(retval);
    }
    else
      return((Name)NULL);
}

/*    
 *    	get_attnum
 *    
 *    	Given the relation id and the attribute name,
 *    	return the "attnum" field from the attribute relation.
 *    
 */

/*  .. fix-parsetree-attnums
 */

AttributeNumber
get_attnum (relid,attname)
     ObjectId relid;
     Name attname ;
{
	AttributeTupleFormD att_tup;

	if (SearchSysCacheStruct(ATTNAME, (char *) &att_tup,
				 (char *) ObjectIdGetDatum(relid),
				 (char *) attname,
				 (char *) NULL, (char *) NULL))
	  return(att_tup.attnum);
	else
	  return(InvalidAttributeNumber);
}

/*    
 *    	get_atttype
 *    
 *    	Given the relation OID and the attribute number with the relation,
 *    	return the attribute type OID.
 *    
 */

/*  .. resno-targetlist-entry
 */

ObjectId
get_atttype (relid,attnum)
     ObjectId relid;
     AttributeNumber attnum ;
{
    Attribute att_tup = (Attribute)palloc(sizeof(*att_tup));

    if (SearchSysCacheStruct(ATTNUM, (char *) att_tup,
			     (char *) ObjectIdGetDatum(relid),
			     (char *) UInt16GetDatum(attnum),
			     (char *) NULL, (char *) NULL))
      return(att_tup->atttypid);
    else
      return((ObjectId)NULL);
}

/* This routine uses the attname instead of the attnum because it
 * replaces the routine find_atttype, which is called sometimes when
 * only the attname, not the attno, is available.
 */
bool
get_attisset (relid, attname)
ObjectId relid;
Name attname;
{
     HeapTuple htup;
     AttributeNumber attno;
     Form_pg_attribute att_tup;

     attno = get_attnum(relid, attname);
     
     htup = SearchSysCacheTuple(ATTNAME, (char *) ObjectIdGetDatum(relid),
				(char *) attname, (char *) NULL,
				(char *) NULL);
     if (!HeapTupleIsValid(htup))
	  elog(WARN, "get_attisset: no attribute %.16s in relation %d",
	       attname->data, relid);
     if (heap_attisnull(htup, attno))
	 return(false);
     else {
	  att_tup = (Form_pg_attribute)GETSTRUCT(htup);
	  return(att_tup->attisset);
     }
}

/*    		---------- INDEX CACHE ----------
 */

/*    	watch this space...
 */

/*    		---------- OPERATOR CACHE ----------
 */

/*    
 *    	get_opcode
 *    
 *    	Returns the regproc id of the routine used to implement an
 *    	operator given the operator uid.
 *    
 */

/*  .. create_hashjoin_node, create_mergejoin_node, relation-sortkeys
 *  .. replace_opid, set-temp-tlist-operators
 */


RegProcedure
get_opcode (opno)
     ObjectId opno;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return(optup.oprcode);
    else
      return((RegProcedure)NULL);
}

NameData
get_opname(opno)
ObjectId opno;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL))
       return(optup.oprname);
    else
       elog(WARN, "can't look up operator %d\n", opno);
}

/*    
 *    	op_mergesortable
 *    
 *    	Returns the left and right sort operators and types corresponding to a
 *    	mergesortable operator, or nil if the operator is not mergesortable.
 *    
 */

/*  .. mergesortop
 */

LispValue
op_mergesortable (opno,ltype,rtype)
     ObjectId opno;
     ObjectId ltype,rtype ;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL) &&
       optup.oprlsortop &&
       optup.oprrsortop && 
       optup.oprleft == ltype &&
       optup.oprright == rtype) 
       return(lispCons((LispValue) ObjectIdGetDatum(optup.oprlsortop),
	               lispCons((LispValue) ObjectIdGetDatum(optup.oprrsortop),
				LispNil)));
    else
      return(LispNil);
}

/*    
 *    	op_hashjoinable
 *    
 *    	Returns the hash operator corresponding to a hashjoinable operator, 
 *    	or nil if the operator is not hashjoinable.
 *    
 */

/*  .. hashjoinop
 */
ObjectId
op_hashjoinable (opno,ltype,rtype)
     ObjectId opno,ltype,rtype ;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL) &&
	optup.oprcanhash  &&
	optup.oprleft == ltype &&
	optup.oprright == rtype) 
      return(opno);
    else
      return((ObjectId)NULL);
}

/*    
 *    	get_commutator
 *    
 *    	Returns the corresponding commutator of an operator.
 *    
 */

/*  .. in-line-lambda%598040608
 */
ObjectId
get_commutator (opno)
     ObjectId opno ;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return(optup.oprcom);
    else
      return((ObjectId)NULL);
}

/*  .. CommuteClause()
 */
HeapTuple
get_operator_tuple (opno)
     ObjectId opno ;
{
    HeapTuple optup;

    if ((optup = SearchSysCacheTuple(OPROID, (char *) ObjectIdGetDatum(opno),
				     (char *) NULL, (char *) NULL,
				     (char *) NULL)))
      return(optup);
    else
      return((HeapTuple)NULL);
}

/*    
 *    	get_negator
 *    
 *    	Returns the corresponding negator of an operator.
 *    
 */

/*  .. push-nots
 */

ObjectId
get_negator (opno)
     ObjectId opno ;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return(optup.oprnegate);
    else
      return((ObjectId)NULL);
}

/*    
 *    	get_oprrest
 *    
 *    	Returns procedure id for computing selectivity of an operator.
 *    
 */

/*  .. compute_selec
 */
RegProcedure
get_oprrest (opno)
     ObjectId opno ;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return(optup.oprrest );
    else
      return((RegProcedure) NULL);
}

/*    
 *    	get_oprjoin
 *    
 *    	Returns procedure id for computing selectivity of a join.
 *    
 */

/*  .. compute_selec
 */

RegProcedure
get_oprjoin (opno)
     ObjectId opno ;
{
    OperatorTupleFormD optup;

    if (SearchSysCacheStruct(OPROID, (char *) &optup,
			     (char *) ObjectIdGetDatum(opno),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return(optup.oprjoin);
    else
      return((RegProcedure)NULL);
}

/*    		---------- RELATION CACHE ----------
 */

/*    
 *    	get_relnatts
 *    
 *    	Returns the number of attributes for a given relation.
 *    
 */

/*  .. expand-targetlist, write-decorate
 */

AttributeNumber
get_relnatts (relid)
     ObjectId relid ;
{
    RelationTupleFormD reltup;

    if (SearchSysCacheStruct(RELOID, (char *) &reltup,
			     (char *) ObjectIdGetDatum(relid),
			     (char *) NULL, (char *) NULL, (char *) NULL))
	return(reltup.relnatts);
    else
    	return(InvalidAttributeNumber);
}

/*    
 *    	get_rel_name
 *    
 *    	Returns the name of a given relation.
 *    
 */

/*  .. ExecOpenR, new-rangetable-entry
 */

Name
get_rel_name (relid)
     ObjectId relid ;
{
    RelationTupleFormD reltup;
    Name retval = (Name)NULL;

    if ((SearchSysCacheStruct(RELOID, (char *) &reltup,
			      (char *) ObjectIdGetDatum(relid),
			      (char *) NULL, (char *) NULL, (char *) NULL))) {
	retval = (Name)palloc(sizeof(reltup.relname));
	bcopy(&(reltup.relname),retval,sizeof(reltup.relname));
	return (retval);
    } else
      return((Name)NULL);
}

/*
 * get_relstub
 * return the stubs associated with a relation.
 * NOTE: we return a pointer to a *copy* of the stubs.
 * NOTE2: I have added an extra return argument: 'islastP'
 */
struct varlena *
get_relstub (relid, no, islastP)
     ObjectId relid ;
     int no;
     bool *islastP;
{
    HeapTuple tuple;
    Form_pg_prs2stub prs2stubStruct;
    struct varlena * retval;
    struct varlena * relstub;

    tuple = SearchSysCacheTuple(PRS2STUB,
				(char *) ObjectIdGetDatum(relid),
				(char *) Int32GetDatum(no),
				(char *) NULL, (char *) NULL);

    if (HeapTupleIsValid(tuple)) {
	prs2stubStruct = (Form_pg_prs2stub) GETSTRUCT(tuple);
	/*
	 * NOTE:
	 * skip the 4 first bytes to get the actual
	 * varlena struct
	 *
	 * This was only need because we used to store the size twice
	 * which I have now fixed -mer 2 April 1992
	 *
	 * relstub = (struct varlena *) PSIZESKIP( & (prs2stubStruct->prs2stub) );
	 */
	relstub = (struct varlena *) &(prs2stubStruct->prs2stub);
	retval = (struct varlena *)palloc(VARSIZE( relstub ));
	bcopy(relstub, retval, VARSIZE( relstub ));
	if (prs2stubStruct->prs2islast)
	    *islastP = true;
	else
	    *islastP = false;
	return(retval);
    } else {
      return((struct varlena *)NULL);
    }
}

/*
 * get_ruleid
 * given a tuple level rule name, return its oid.
 */
ObjectId
get_ruleid(ruleName)
Name ruleName;
{
    HeapTuple tuple;

    tuple = SearchSysCacheTuple(PRS2RULEID, (char *) ruleName,
				(char *) NULL, (char *) NULL, (char *) NULL);
    if (HeapTupleIsValid(tuple)) {
	return(tuple->t_oid);
    } else {
	return(InvalidObjectId);
    }
}

/*
 * get_eventrelid
 * given a (tuple system) rule oid, return the oid of the relation
 * where this rule is defined on.
 */
ObjectId
get_eventrelid(ruleId)
ObjectId ruleId;
{
    HeapTuple tuple;
    Form_pg_prs2rule prs2ruleStruct;

    tuple = SearchSysCacheTuple(PRS2EVENTREL,
				(char *) ObjectIdGetDatum(ruleId),
				(char *) NULL, (char *) NULL, (char *) NULL);
    if (HeapTupleIsValid(tuple)) {
	prs2ruleStruct = (Form_pg_prs2rule) GETSTRUCT(tuple);
	return(prs2ruleStruct->prs2eventrel);
    } else {
	return(InvalidObjectId);
    }
}


/*    		---------- TYPE CACHE ----------
 */

/*    
 *    	get_typlen
 *    
 *    	Given the type OID, return the length of the type.
 *    
 */

/*  .. compute-attribute-width, create_tl_element, flatten-tlist
 *  .. resno-targetlist-entry, substitute-parameters
 */

int16
get_typlen (typid)
     ObjectId typid;
{
    TypeTupleFormD typtup;

    if (SearchSysCacheStruct(TYPOID, (char *) &typtup,
			     (char *) ObjectIdGetDatum(typid),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return(typtup.typlen);
    else
      return((int16)NULL);
}

/*    
 *    	get_typbyval
 *    
 *    	Given the type OID, determine whether the type is returned by value or
 *    	not.  Returns 1 if by value, 0 if by reference.
 *    
 */

/*  .. ExecTypeFromTL
 */

bool
get_typbyval (typid)
     ObjectId typid ;
{
    TypeTupleFormD typtup;

    if (SearchSysCacheStruct(TYPOID, (char *) &typtup,
			     (char *) ObjectIdGetDatum(typid),
			     (char *) NULL, (char *) NULL, (char *) NULL))
      return((bool)typtup.typbyval);
    else
      return(false);
}

/*    
 *    	get_typdefault
 *    
 *    	Given the type OID, return the default value of the ADT.
 *    
 */

/*  .. resno-targetlist-entry
 */

struct varlena *
get_typdefault (typid)
     ObjectId typid ;
{
    struct varlena *typdefault = 
      (struct varlena *)TypeDefaultRetrieve (typid);
    return(typdefault);
}

/*    
 *    	get_typtype
 *    
 *    	Given the type OID, find if it is a basic type, a named relation
 *	or the generic type 'relation'.
 *	It returns the null char if the cache lookup fails...
 *    
 */

char
get_typtype (typid)
     ObjectId typid ;
{
    TypeTupleFormD typtup;

    if (SearchSysCacheStruct(TYPOID, (char *) &typtup,
			     (char *) ObjectIdGetDatum(typid),
			     (char *) NULL, (char *) NULL, (char *) NULL)) {
      return(typtup.typtype);
    } else {
      return('\0');
    }
}

