/* ----------------------------------------------------------------
 *   FILE
 *	catalog_utils.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/log.h"
#include "utils/fmgr.h"

#include "nodes/pg_lisp.h"
#include "catalog/syscache.h"

#include "exceptions.h"
#include "catalog_utils.h"

struct {
    char *field;
    int code;
} special_attr[] =
	       {
		   { "ctid", SelfItemPointerAttributeNumber },
		   { "lock", RuleLockAttributeNumber },
		   { "oid", ObjectIdAttributeNumber },
		   { "xmin", MinTransactionIdAttributeNumber },
		   { "cmin", MinCommandIdAttributeNumber },
		   { "xmax", MaxTransactionIdAttributeNumber },
		   { "cmax", MaxCommandIdAttributeNumber },
		   { "chain", ChainItemPointerAttributeNumber },
		   { "anchor", AnchorItemPointerAttributeNumber },
		   { "tmin", MinAbsoluteTimeAttributeNumber },
		   { "tmax", MaxAbsoluteTimeAttributeNumber },
		   { "vtype", VersionTypeAttributeNumber }
	       };

#define SPECIALS (sizeof(special_attr)/sizeof(*special_attr))
  
static String attnum_type[SPECIALS] = {
    "tid",
    "lock",
    "oid",
    "xid",
    "iid",
    "xid",
    "iid",
    "tid",
    "tid",
    "abstime",
    "abstime",
    "char"
  };

struct tuple *SearchSysCache();

/* return a Type structure, given an typid */
Type
get_id_type(id)
long id;
{
    HeapTuple tup;

    if (!(tup = SearchSysCacheTuple(TYPOID, id))) {
	elog ( WARN, "type id lookup of %d failed", id);
	return(NULL);
    }
    return((Type) tup);
}

/* return a Type structure, given type name */
Type
type(s)
char *s;
{
    HeapTuple tup;

    if (s == NULL) {
	elog ( WARN , "type(): Null type" );
    }

    if (!(tup = SearchSysCacheTuple(TYPNAME, s))) {
	elog (WARN , "type name lookup of %s failed", s);
    }
    return((Type) tup);
}

/* given attribute id, return type of that attribute */
/* XXX Special case for pseudo-attributes is a hack */
OID
att_typeid(rd, attid)
Relation rd;
int attid;
{

    if (attid < 0) {
	return(typeid(type(attnum_type[-attid-1])));
    }
    /* -1 because varattno (where attid comes from) returns one
       more than index */
    return(rd->rd_att.data[attid-1]->atttypid);
}

/* given type, return the type OID */
OID
typeid(tp)
Type tp;
{
    if (tp == NULL) {
	elog ( WARN , "typeid() called with NULL type struct");
    }
    return(tp->t_oid);
}

/* given type (as type struct), return the length of type */
int16
tlen(t)
Type t;
{
    TypeTupleForm    typ;

    typ = (TypeTupleForm)GETSTRUCT(t);
    return(typ->typlen);
}

/* given type (as type struct), return the value of its 'byval' attribute.*/
bool
tbyval(t)
Type t;
{
    TypeTupleForm    typ;

    typ = (TypeTupleForm)GETSTRUCT(t);
    return(typ->typbyval);
}

/* given type (as type struct), return the name of type */
Name
tname(t)
Type t;
{
    TypeTupleForm    typ;

    typ = (TypeTupleForm)GETSTRUCT(t);
    return((Name)&(typ->typname));
}

/* given type (as type struct), return wether type is passed by value */
int
tbyvalue(t)
Type t;
{
    TypeTupleForm typ;

    typ = (TypeTupleForm) GETSTRUCT(t);
    return(typ->typbyval);
}

/* given operator, return the operator OID */
OID
oprid(op)
Operator op;
{
    return(op->t_oid);
}

/* Given operator, types of arg1, and arg2, return oper struct */
Operator
oper(op, arg1, arg2)
char *op;
int arg1, arg2;	/* typeids */
{
    HeapTuple tup;

    /*
    if (!OpCache) {
	init_op_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(OPRNAME, op, arg1, arg2, (char *) 'b'))) {
	elog ( WARN , "Can't find binary op: %s for types %d and %d",
	      op, arg1, arg2);
	return(NULL);
    }
    return((Operator) tup);
}

/* Given unary right-side operator (argument on right), return oper struct */
Operator
right_oper(op, arg)
char *op;
int arg; /* type id */
{
    HeapTuple tup;

    /*
    if (!OpCache) {
	init_op_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(OPRNAME, op, 0, arg, (char *) 'r'))) {
	elog ( WARN ,
	      "Can't find right op: %s for type %d", op, arg );
	return(NULL);
    }
    return((Operator) tup);
}

/* Given unary left-side operator (argument on left), return oper struct */
Operator
left_oper(op, arg)
     char *op;
     int arg; /* type id */
{
	HeapTuple tup;
	
	if (!(tup = SearchSysCacheTuple(OPRNAME, op, arg, 0, (char *) 'l'))) {
		elog (WARN ,
			"Can't find left op: %s for type %d", op, arg);
		return(NULL);
	}
	return ((Operator) tup);
}

/* given range variable, return id of variable */
   
int
varattno ( rd , a)
     Relation rd;
     char *a;
{
	int i;
	
	for (i = 0; i < rd->rd_rel->relnatts; i++) {
		if (!strcmp(&rd->rd_att.data[i]->attname, a)) {
			return(i+1);
		}
	}
	for (i = 0; i < SPECIALS; i++) {
		if (!strcmp(special_attr[i].field, a)) {
			return(special_attr[i].code);
		}
	}

	elog(WARN,"Relation %s does not have attribute %s\n", 
	     RelationGetRelationName(rd), a );
	return(-1);
}
/*-------------
 * given an attribute number and a relation, return its relation name
 */
Name
getAttrName(rd, attrno)
Relation rd;
int attrno;
{
    Name name;
    int i;

    if (attrno<0) {
	for (i = 0; i < SPECIALS; i++) {
		if (special_attr[i].code == attrno) {
		    name = (Name) special_attr[i].field;
		    return(name);
		}
	}
	elog(WARN, "Illegal attr no %d for relation %s\n",
	    attrno, RelationGetRelationName(rd));
    } else if (attrno >=1 && attrno<= RelationGetNumberOfAttributes(rd)) {
	name = &(rd->rd_att.data[attrno-1]->attname);
	return(name);
    } else {
	elog(WARN, "Illegal attr no %d for relation %s\n",
	    attrno, RelationGetRelationName(rd));
    }
    
}

/* given range variable, return id of variable */
RangeTablePosn ( rangevar , options )
     char *rangevar;
     List options;
{
	int index = 1;
	extern LispValue p_rtable;
	LispValue temp = p_rtable;
	int inherit = 0;
	int timerange = 0;

	/*printf("Looking for relation : %s\n",rangevar);
	fflush(stdout);*/
	index = 1;
	temp = p_rtable;
	while ( ! lispNullp (temp )) {
	    LispValue refvalue = ( CAR ( CAR (temp )));
	    if ( IsA (refvalue,LispStr)) {
		if ( (! strcmp ( CString ( refvalue ),
				rangevar ) ) &&
		    (inherit == inherit) &&
		    (timerange == timerange))
		  return (index );
	    } else {
		List i;
		foreach ( i , refvalue ) {
		    Name actual_ref = (Name)CString(CAR(i));
		    if ( !strcmp ( actual_ref , rangevar ) &&
			 inherit == inherit &&
			 timerange == timerange )
		      return ( index );
		}
	    }
		temp = CDR(temp);
		index++;
	}
		return(0);
}

/* given range variable, return id of variable */
List
RangeTablePositions ( rangevar , options )
     char *rangevar;
     List options;
{
    int index = 1;
    extern LispValue p_rtable;
    LispValue temp = p_rtable;
    List list_of_positions = NULL;
    int inherit = 0;
    int timerange = 0;
    
    index = 1;
    temp = p_rtable;

    while ( ! lispNullp (temp )) {
	LispValue refvalue = CAR ( CAR ( temp ));
	if ( IsA (refvalue,LispStr)) {
	    if ( (! strcmp ( CString ( refvalue ),
			    rangevar ) ) &&
		(inherit == inherit) &&
		(timerange == timerange)) {
		
		list_of_positions = lispCons ( lispInteger ( index ),
					      list_of_positions );
	    } 
	} else {
	    List i;
	    foreach ( i , refvalue ) {
		Name actual_ref = (Name)CString(CAR(i));
		if ( !strcmp ( actual_ref , rangevar ) &&
		    inherit == inherit &&
		    timerange == timerange ) {
		    
		list_of_positions = lispCons ( lispInteger ( index ),
					      list_of_positions );
		}
	    }
	}
	temp = CDR(temp);
	index++;
    }

    return(list_of_positions);
}

/* Given a typename and value, returns the ascii form of the value */

char *
outstr(typename, value)
char *typename;	/* Name of type of value */
char *value;	/* Could be of any type */
{
    TypeTupleForm tp;
    OID op;

    tp = (TypeTupleForm ) GETSTRUCT(type(typename));
    op = tp->typoutput;
    return((char *) fmgr(op, value));
}

/* Given a Type and a string, return the internal form of that string */
char *
instr2(tp, string)
Type tp;
char *string;
{
    return(instr1((TypeTupleForm ) GETSTRUCT(tp), string));
}

/* Given a type structure and a string, returns the internal form of
   that string */
char *
instr1(tp, string)
TypeTupleForm tp;
char *string;
{
    OID op;
	
    op = tp->typinput;
    return((char *) fmgr(op, string));
}

/* Given a typename and string, returns the internal form of that string */
char *
instr(typename, string)
char *typename;	/* Name of type to turn string into */
char *string;
{
    TypeTupleForm tp;

    tp = (TypeTupleForm) GETSTRUCT(type(typename));
    return(instr1(tp, string));
}

OID
funcname_get_rettype ( function_name )
     char *function_name;
{
    HeapTuple func_tuple = NULL;
    OID funcrettype = (OID)0;

    func_tuple = SearchSysCacheTuple(PRONAME,function_name,0,0,0);

    if ( !HeapTupleIsValid ( func_tuple )) 
	elog (WARN, "function named %s does not exist", function_name);
    
    funcrettype = (OID)
      ((struct proc *)GETSTRUCT(func_tuple))->prorettype ;

    return (funcrettype);
}

OID
funcname_get_funcid ( function_name )
     char *function_name;
{
    HeapTuple func_tuple = NULL;

    func_tuple = SearchSysCacheTuple(PRONAME,function_name,0,0,0);

    if ( func_tuple != NULL )
      return ( func_tuple->t_oid );
    else
      return ( (OID) 0 );
}

/* Given a type id, returns the in-conversion function of the type */
OID
typeid_get_retinfunc(type_id)
        int type_id;
{
        HeapTuple     typeTuple;
        TypeTupleForm   type;
        OID             infunc;
        typeTuple = SearchSysCacheTuple(TYPOID, (char *) type_id,
                  (char *) NULL, (char *) NULL, (char *) NULL);
        type = (TypeTupleForm) GETSTRUCT(typeTuple);
        infunc = type->typinput;
        return(infunc);
}
