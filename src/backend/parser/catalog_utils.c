static char *catalog_utils_c = "$Header$";


#include "catalog_utils.h"
#include "pg_lisp.h"
#include "log.h"
#include "fmgr.h"
#include "syscache.h"
#include "exceptions.h"


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
    "tid",
    "oid",
    "xid",
    "iid",
    "xid",
    "iid",
    "tid",
    "tid",
    "dt",
    "dt",
    "char"
  };

struct tuple *SearchSysCache();

/* return a Type structure, given an typid */
Type
get_id_type(id)
long id;
{
    HeapTuple tup;

    /*
    if (!TypeIdCache) {
	init_type_id_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(TYPOID, id))) {
	p_raise(CatalogFailure,
		form("type id lookup of %d failed", id));
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
	p_raise(InternalError, "type(): Null type");
    }
    /*
    if (!TypeNameCache) {
	init_type_name_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(TYPNAME, s))) {
	p_raise(CatalogFailure,
		form("type name lookup of %s failed", s));
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
	p_raise(InternalError, "typeid() called with NULL type struct");
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
	p_raise(CatalogFailure, 
		form("Can't find binary op: %s for types %d and %d",
		     op, arg1, arg2));
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
	p_raise(CatalogFailure,
		form("Can't find right op: %s for type %d", op, arg));
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
		p_raise(CatalogFailure,
			form("Can't find left op: %s for type %d", op, arg));
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
	p_raise(CatalogFailure,
		form("No such attribute: %s\n", a));
	return(-1);
}

/* given range variable, return id of variable */
RangeTablePosn ( rangevar , inherit , timerange)
     char *rangevar;
     int inherit,timerange;
{
	int index = 1;
	extern LispValue p_rtable;
	LispValue temp = p_rtable;
	
	/*printf("Looking for relation : %s\n",rangevar);
	fflush(stdout);*/
	/* search thru aliases
	  while ( ! lispNullp (temp )) {
		if ( ! strcmp ( CString ( CAR( CAR (temp ))),
			        rangevar ))
		  return (index );
		temp = CDR(temp);
		index++;
	} */
	index = 1;
	temp = p_rtable;
	while ( ! lispNullp (temp )) {
		/* car(cdr(car(temp)) = actual relname 
		  printf("%s\n",CString ( CAR(CDR(CAR (temp )))));
		fflush (stdout );*/
		if ( (! strcmp ( CString ( CAR( CAR (temp ))),
				rangevar ) ) &&
		    (inherit == inherit) &&
		    (timerange == timerange))
		  return (index );
		temp = CDR(temp);
		index++;
	}
		return(0);
}

/* Given a range variable name, reutrn associated reldesc pointer 
Relation
get_rgdesc(rg)
char *rg;
{
    struct r_table *crt = RangeTable;
    int index;

    while (crt != NULL) {
	if (!strcmp(rg, crt->var_name)) {
	    return(crt->rdesc);
	}
	crt = crt->next;
    }
    index = add_rel_rt(rg);

    return(get_rgdesc(rg));
}
*/
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
