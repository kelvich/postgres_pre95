
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
 *    		get_regproc
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

#include "c.h"

#include "pg_lisp.h"
#include "syscache.h"
#include "att.h"
#include "rel.h"
#include "attnum.h"

#include "catalog/pg_amop.h"
#include "catalog/pg_type.h"

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
 *    	Return t iff operator 'opid' is in operator class 'opclass'.
 *    
 */

/*  .. in-line-lambda%598040608, match-index-orclause
 */
bool
op_class (opid,opclass)
     ObjectId opid;
     int32 opclass ;
{
    AccessMethodOperatorTupleForm amoptup = 
      new(AccessMethodOperatorTupleFormD);

    if (SearchSysCacheStruct (AMOPOPID,amoptup,opclass,opid,0,0))
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
    Attribute att_tup = new(AttributeTupleFormD);
    Name retval = (Name)NULL;

    if( SearchSysCacheStruct (ATTNUM,att_tup,relid,attnum,0,0)) {
	retval = (Name)palloc(sizeof(att_tup->attname));
	bcopy(&(att_tup->attname),retval,sizeof(att_tup->attname));
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
	Attribute  att_tup = new(AttributeTupleFormD);

	if(SearchSysCacheStruct (ATTNAME,att_tup,relid,attname,0,0) ) 
	  return(att_tup->attnum);
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

    if( SearchSysCacheStruct (ATTNUM,att_tup,relid,attnum,0,0) ) 
      return(att_tup->atttypid);
    else
      return((ObjectId)NULL);
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
get_opcode (opid)
     ObjectId opid;
{
    OperatorTupleForm optup = new(OperatorTupleFormD);

    if( SearchSysCacheStruct (OPROID,optup,opid,0,0,0) ) 
      return(optup->oprcode);
    else
      return((RegProcedure)NULL);
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
op_mergesortable (opid,ltype,rtype)
     ObjectId opid;
     ObjectId ltype,rtype ;
{
    OperatorTupleForm optup = new(OperatorTupleFormD);

    if(SearchSysCacheStruct (OPROID,optup,opid,0,0,0) &&
       optup->oprlsortop &&
       optup->oprrsortop && 
       optup->oprleft == ltype &&
       optup->oprright == rtype) 
      return (lispCons (optup->oprlsortop,
	               lispCons (optup->oprrsortop,LispNil)));
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
op_hashjoinable (opid,ltype,rtype)
     ObjectId opid,ltype,rtype ;
{
    OperatorTupleForm optup = new(OperatorTupleFormD);

    if (SearchSysCacheStruct (OPROID,optup,opid,0,0,0) && 
	optup->oprcanhash  &&
	optup->oprleft == ltype &&
	optup->oprright == rtype) 
      return(opid);
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
get_commutator (opid)
     ObjectId opid ;
{
    OperatorTupleForm optup = (OperatorTupleForm)palloc(sizeof(* optup));

    if(SearchSysCacheStruct (OPROID,optup,opid,0,0,0))
      return(optup->oprcom);
    else
      return((ObjectId)NULL);
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
get_negator (opid)
     ObjectId opid ;
{
    OperatorTupleForm optup = new(OperatorTupleFormD);

    if(SearchSysCacheStruct (OPROID,optup,opid,0,0,0))
      return(optup->oprnegate);
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
get_oprrest (opid)
     ObjectId opid ;
{
    OperatorTupleForm optup = new(OperatorTupleFormD);

    if(SearchSysCacheStruct (OPROID,optup,opid,0,0,0))
      return(optup->oprrest );
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
get_oprjoin (opid)
     ObjectId opid ;
{
    OperatorTupleForm optup = new(OperatorTupleFormD);

    if(SearchSysCacheStruct (OPROID,optup,opid,0,0,0))
      return(optup->oprjoin);
    else
      return((RegProcedure)NULL);
}

/*    		---------- PROCEDURE CACHE ----------
 */

/*    
 *         get_regproc
 *    
 *         Given the function name
 *         return the "oid" field from the FUNCTION relation.
 *    
 */

ObjectId
get_regproc (funname)
     Name funname ;
{
    return((ObjectId)SearchSysCacheGetAttribute( PRONAME,-3,funname,0,0,0));
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
    RelationTupleForm reltup = new(RelationTupleFormD);

    if(SearchSysCacheStruct (RELOID,reltup,relid,0,0,0))
      return(reltup->relnatts);
    else
      return((AttributeNumber)NULL);
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
    RelationTupleForm reltup = new(RelationTupleFormD);
    Name retval = (Name)NULL;

    if((SearchSysCacheStruct (RELOID,reltup,relid,0,0,0))) {
	retval = (Name)palloc(sizeof(reltup->relname));
	bcopy(&(reltup->relname),retval,sizeof(reltup->relname));
	return(retval);
    } else
      return((Name)NULL);
}

/*
 * get_relstub
 * return the stubs associated with a relation.
 * NOTE: we return a pointer to a *copy* of the stubs.
 */
struct varlena *
get_relstub (relid)
     ObjectId relid ;
{
    HeapTuple tuple;
    RelationTupleForm relp;
    struct varlena * retval;
    struct varlena * relstub;

    tuple = SearchSysCacheTuple(RELOID,relid,0,0,0);
    if (HeapTupleIsValid(tuple)) {
	relp = (RelationTupleForm) HeapTupleGetForm(tuple);
	/*
	 * skip the 4 first bytes to get the actual
	 * varlena struct
	 */
	relstub = (struct varlena *) PSIZESKIP( & (relp->relstub) );
	retval = (struct varlena *)palloc(VARSIZE( relstub ));
	bcopy(relstub, retval, VARSIZE( relstub ));
	return(retval);
    } else {
      return((struct varlena *)NULL);
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
    TypeTupleForm typtup = new(TypeTupleFormD);

    if (SearchSysCacheStruct (TYPOID,typtup,typid,0,0,0))
      return(typtup->typlen);
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
    TypeTupleForm typtup = new(TypeTupleFormD);
    if(SearchSysCacheStruct (TYPOID,typtup,typid,0,0,0))
      return((bool)typtup->typbyval);
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
    TypeTupleForm typtup = new(TypeTupleFormD);
    if(SearchSysCacheStruct (TYPOID,typtup,typid,0,0,0)) {
      return(typtup->typtype);
    } else {
      return('\0');
    }
}

