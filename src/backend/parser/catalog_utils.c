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

#include "tmp/simplelists.h"
#include "tmp/datum.h"

#include "utils/log.h"
#include "utils/fmgr.h"

#include "nodes/pg_lisp.h"
#include "catalog/syscache.h"

#include "exceptions.h"
#include "catalog_utils.h"
#include "catalog/pg_inherits.h"

#include "access/skey.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "access/htup.h"
#include "access/heapam.h"

#include "storage/buf.h"

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
    "cid",
    "xid",
    "cid",
    "tid",
    "tid",
    "abstime",
    "abstime",
    "char"
  };

struct tuple *SearchSysCache();

#define	MAXFARGS 8		/* max # args to a c or postquel function */

extern	ObjectId	**argtype_inherit();
extern	ObjectId	**genxprod();
extern	OID		typeid_get_relid();

/*
 *  This structure is used to explore the inheritance hierarchy above
 *  nodes in the type tree in order to disambiguate among polymorphic
 *  functions.
 */

typedef struct _InhPaths {
    int		nsupers;	/* number of superclasses */
    ObjectId	self;		/* this class */
    ObjectId	*supervec;	/* vector of superclasses */
} InhPaths;

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
	op_error(op, arg1, arg2);
	return(NULL);
    }
    return((Operator) tup);
}
/* Find default type for right arg of binary operator */
Operator
oper_default(op, arg1)
char *op;
int arg1;       /* typeid */
{
    HeapTuple tup;

    /* see if there is only one type available for right arg. of binary op. */
    tup = (HeapTuple) FindDefaultType(op, arg1);
    if(!tup){  /* this could mean a) there is no operator named op.
                                b) there are more than one possible right arg */
       if (!(tup = SearchSysCacheTuple(OPRNAME, op, arg1, arg1, (char *) 'b')))
       {
          /* there's no reasonable default type for the right argument */
          return(NULL);
       }
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
    if (!(tup = SearchSysCacheTuple(OPRNAME, op, arg, 0, (char *) 'r'))) {
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
	
	if (!(tup = SearchSysCacheTuple(OPRNAME, op, 0, arg, (char *) 'l'))) {
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
/* given range variable, return id of variable */
   
int
nf_varattno ( rd , a)
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
	return InvalidAttributeNumber;
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

    /*
     * Shouldn't get here, but we want lint to be happy...
     */

    return(NULL);
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
	OID typelem;
	
    op = tp->typinput;
	typelem = tp->typelem; /* XXX - used for array_in */
    return((char *) fmgr(op, string, typelem));
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
funcid_get_rettype ( funcid)
     OID funcid;
{
    HeapTuple func_tuple = NULL;
    OID funcrettype = (OID)0;

    func_tuple = SearchSysCacheTuple(PROOID,funcid,0,0,0);

    if ( !HeapTupleIsValid ( func_tuple )) 
	elog (WARN, "function  %d does not exist", funcid);
    
    funcrettype = (OID)
      ((struct proc *)GETSTRUCT(func_tuple))->prorettype ;

    return (funcrettype);
}

ObjectId *
funcname_get_funcargtypes ( function_name )
     char *function_name;
{
    HeapTuple func_tuple = NULL;
    ObjectId *oid_array = NULL;
    struct proc *foo;
    func_tuple = SearchSysCacheTuple(PRONAME,function_name,0,0,0);

    if ( !HeapTupleIsValid ( func_tuple )) 
	elog (WARN, "function named %s does not exist", function_name);
    
    foo = (struct proc *)GETSTRUCT(func_tuple);
    oid_array = foo->proargtypes.data;
    return (oid_array);
}

bool
func_get_detail(funcname, nargs, oid_array, funcid, rettype, retset)
    char *funcname;
    int nargs;
    ObjectId *oid_array;
    ObjectId *funcid;			/* return value */
    ObjectId *rettype;			/* return value */
    bool *retset;			/* return value */
{
    ObjectId *fargs;
    ObjectId **oid_vector;
    HeapTuple ftup;
    Form_pg_proc pform;

    /* find the named function in the system catalogs */
    ftup = SearchSysCacheTuple(PRONAME, funcname, 0, 0, 0);

    if (!HeapTupleIsValid(ftup))
	return (false);

    /*
     *  If the function exists, we need to check the argument types
     *  passed in by the user.  If they don't match, then we need to
     *  check the user's arg types against superclasses of the arguments
     *  to this function.
     */

    pform = (Form_pg_proc)GETSTRUCT(ftup);
    if (pform->pronargs != nargs) {
	elog(NOTICE, "argument count mismatch: %s takes %d, %d supplied",
		     funcname, pform->pronargs, nargs);
	return (false);
    }

    /* typecheck arguments */
    fargs = pform->proargtypes.data;
    if (*fargs != InvalidObjectId && *oid_array != InvalidObjectId) {
	if (bcmp(fargs, oid_array, 8 * sizeof(ObjectId)) != 0) {
	    oid_vector = argtype_inherit(nargs, oid_array);
	    while (**oid_vector != InvalidObjectId) {
		oid_array = *oid_vector++;
		if (bcmp(fargs, oid_array, 8 * sizeof(ObjectId)) == 0)
		    goto okay;
	    }
	    if (**oid_vector == InvalidObjectId)
		elog(NOTICE, "type mismatch in invocation of function %s",
			     funcname);
		return (false);
	}
    }

okay:
    /* by here, we have a match */
    *funcid = ftup->t_oid;
    *rettype = (ObjectId) pform->prorettype;
    *retset = (ObjectId) pform->proretset;

    return (true);
}

/*
 *  argtype_inherit() -- Construct an argtype vector reflecting the
 *			 inheritance properties of the supplied argv.
 *
 *	This function is used to disambiguate among functions with the
 *	same name but different signatures.  It takes an array of eight
 *	type ids.  For each type id in the array that's a complex type
 *	(a class), it walks up the inheritance tree, finding all
 *	superclasses of that type.  A vector of new ObjectId type arrays
 *	is returned to the caller, reflecting the structure of the
 *	inheritance tree above the supplied arguments.
 *
 *	The order of this vector is as follows:  all superclasses of the
 *	rightmost complex class are explored first.  The exploration
 *	continues from right to left.  This policy means that we favor
 *	keeping the leftmost argument type as low in the inheritance tree
 *	as possible.  This is intentional; it is exactly what we need to
 *	do for method dispatch.  The last type array we return is all
 *	zeroes.  This will match any functions for which return types are
 *	not defined.  There are lots of these (mostly builtins) in the
 *	catalogs.
 */

ObjectId **
argtype_inherit(nargs, oid_array)
    int nargs;
    ObjectId *oid_array;
{
    ObjectId relid;
    int i;
    InhPaths arginh[MAXFARGS];

    for (i = 0; i < MAXFARGS; i++) {
	if (i < nargs) {
	    arginh[i].self = oid_array[i];
	    if ((relid = typeid_get_relid(oid_array[i])) != InvalidObjectId) {
		arginh[i].nsupers = findsupers(relid, &(arginh[i].supervec));
	    } else {
		arginh[i].nsupers = 0;
		arginh[i].supervec = (ObjectId *) NULL;
	    }
	} else {
	    arginh[i].self = InvalidObjectId;
	    arginh[i].nsupers = 0;
	    arginh[i].supervec = (ObjectId *) NULL;
	}
    }

    /* return an ordered cross-product of the classes involved */
    return (genxprod(arginh));
}

typedef struct _SuperQE {
    SLNode	sqe_node;
    ObjectId	sqe_relid;
} SuperQE;

int
findsupers(relid, supervec)
    ObjectId relid;
    ObjectId **supervec;
{
    ObjectId *relidvec;
    Relation inhrel;
    HeapScanDesc inhscan;
    ScanKeyData skey;
    HeapTuple inhtup;
    TupleDescriptor inhtupdesc;
    int nvisited;
    SuperQE *qentry, *vnode;
    SLList visited, queue;
    Relation rd;
    Buffer buf;
    Datum d;
    bool newrelid;
    char isNull;

    nvisited = 0;
    SLNewList(&queue, 0);
    SLNewList(&visited, 0);

    inhrel = heap_openr(Name_pg_inherits);
    RelationSetLockForRead(inhrel);
    inhtupdesc = RelationGetTupleDescriptor(inhrel);

    /*
     *  Use queue to do a breadth-first traversal of the inheritance
     *  graph from the relid supplied up to the root.
     */
    do {
	ScanKeyEntryInitialize(&skey.data[0], 0x0, Anum_pg_inherits_inhrel,
			       ObjectIdEqualRegProcedure,
			       ObjectIdGetDatum(relid));

	inhscan = heap_beginscan(inhrel, 0, NowTimeQual, 1, &skey);

	while (HeapTupleIsValid(inhtup = heap_getnext(inhscan, 0, &buf))) {
	    qentry = (SuperQE *) palloc(sizeof(SuperQE));

	    d = (Datum) fastgetattr(inhtup, Anum_pg_inherits_inhparent,
				    inhtupdesc, &isNull);
	    qentry->sqe_relid = DatumGetObjectId(d);

	    /* put this one on the queue */
	    SLNewNode(&(qentry->sqe_node));
	    SLAddTail(&queue, &(qentry->sqe_node));

	    ReleaseBuffer(buf);
	}

	heap_endscan(inhscan);

	/* pull next unvisited relid off the queue */
	do {
	    qentry = (SuperQE *) SLRemHead(&queue);
	    if (qentry == (SuperQE *) NULL)
		break;

	    relid = qentry->sqe_relid;
	    newrelid = true;

	    for (vnode = (SuperQE *) SLGetHead(&visited);
		 vnode != (SuperQE *) NULL;
		 vnode = (SuperQE *) SLGetSucc(&(vnode->sqe_node))) {
		if (qentry->sqe_relid == vnode->sqe_relid) {
		    newrelid = false;
		    break;
		}
	    }
	} while (!newrelid);

	if (qentry != (SuperQE *) NULL) {

	    /* save the type id, rather than the relation id */
	    if ((rd = heap_open(qentry->sqe_relid)) == (Relation) NULL)
		elog(WARN, "relid %d does not exist", qentry->sqe_relid);
	    qentry->sqe_relid = typeid(type(RelationGetRelationName(rd)));
	    heap_close(rd);

	    SLAddTail(&visited, &(qentry->sqe_node));
	    nvisited++;
	}
    } while (qentry != (SuperQE *) NULL);

    RelationUnsetLockForRead(inhrel);
    heap_close(inhrel);

    if (nvisited > 0) {
	relidvec = (ObjectId *) palloc(nvisited * sizeof(ObjectId));
	*supervec = relidvec;
	for (vnode = (SuperQE *) SLGetHead(&visited);
	     vnode != (SuperQE *) NULL;
	     vnode = (SuperQE *) SLGetSucc(&(vnode->sqe_node))) {
	    *relidvec++ = vnode->sqe_relid;
	}
    } else {
	*supervec = (ObjectId *) NULL;
    }

    return (nvisited);
}

ObjectId **
genxprod(arginh)
    InhPaths *arginh;
{
    int nanswers;
    ObjectId **result, **iter;
    ObjectId *oneres;
    int i, j;
    int cur[MAXFARGS];

    nanswers = 1;
    for (i = 0; i < MAXFARGS; i++) {
	nanswers *= (arginh[i].nsupers + 1);
	cur[i] = 0;
    }

    iter = result = (ObjectId **) palloc(sizeof(ObjectId *) * nanswers);

    /* compute the cross product from right to left */
    for (;;) {
	oneres = (ObjectId *) palloc(MAXFARGS * sizeof(ObjectId));

	for (i = MAXFARGS - 1; i >= 0 && cur[i] == arginh[i].nsupers; i--)
	    continue;

	/* if we're done, do wildcard vector and return */
	if (i < 0) {
	    bzero(oneres, MAXFARGS * sizeof(ObjectId));
	    *iter = oneres;
	    return (result);
	}

	/* no, increment this column and zero the ones after it */
	cur[i] = cur[i] + 1;
	for (j = MAXFARGS - 1; j > i; j--)
	    cur[j] = 0;

	for (i = 0; i < MAXFARGS; i++) {
	    if (cur[i] == 0)
		oneres[i] = arginh[i].self;
	    else
		oneres[i] = arginh[i].supervec[cur[i] - 1];
	}

	*iter++ = oneres;
    }
    /* NOTREACHED */
}

ObjectId *
funcid_get_funcargtypes ( funcid )
     ObjectId funcid;
{
    HeapTuple func_tuple = NULL;
    ObjectId *oid_array = NULL;
    struct proc *foo;
    func_tuple = SearchSysCacheTuple(PROOID,funcid,0,0,0);

    if ( !HeapTupleIsValid ( func_tuple )) 
	elog (WARN, "function %d does not exist", funcid);
    
    foo = (struct proc *)GETSTRUCT(func_tuple);
    oid_array = foo->proargtypes.data;
    return (oid_array);
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
	if ( !HeapTupleIsValid ( typeTuple ))
	    elog(WARN,
		 "typeid_get_retinfunc: Invalid type - oid = %d",
		 type_id);

        type = (TypeTupleForm) GETSTRUCT(typeTuple);
        infunc = type->typinput;
        return(infunc);
}

OID
typeid_get_relid(type_id)
        int type_id;
{
        HeapTuple     typeTuple;
        TypeTupleForm   type;
        OID             infunc;
        typeTuple = SearchSysCacheTuple(TYPOID, (char *) type_id,
                  (char *) NULL, (char *) NULL, (char *) NULL);
	if ( !HeapTupleIsValid ( typeTuple ))
	    elog(WARN, "typeid_get_relid: Invalid type - oid = %d ", type_id);

        type = (TypeTupleForm) GETSTRUCT(typeTuple);
        infunc = type->typrelid;
        return(infunc);
}

OID
get_typrelid(typ)
    TypeTupleForm typ;
{
    Form_pg_type typtup;

    typtup = (Form_pg_type) GETSTRUCT(typ);

    return (typtup->typrelid);
}

OID
get_typelem(type_id)

OID type_id;

{
    HeapTuple     typeTuple;
    TypeTupleForm   type;

	if (!(typeTuple = SearchSysCacheTuple(TYPOID, type_id, NULL, NULL, NULL))) {
		elog (WARN , "type name lookup of %d failed", type_id);
	}
    type = (TypeTupleForm) GETSTRUCT(typeTuple);

    return (type->typelem);
}

char
FindDelimiter(typename)
char *typename;
{
    char delim;
    HeapTuple     typeTuple;
    TypeTupleForm   type;


    if (!(typeTuple = SearchSysCacheTuple(TYPNAME, typename))) {
        elog (WARN , "type name lookup of %s failed", typename);
    }
    type = (TypeTupleForm) GETSTRUCT(typeTuple);

    delim = type->typdelim;
    return (delim);
}

/*
 * Give a somewhat useful error message when the operator for two types
 * is not found.
 */

op_error(op, arg1, arg2)

char *op;
int arg1, arg2;

{
	Type tp, get_id_type();
	char p1[16], p2[16];

	tp = get_id_type(arg1);
	strncpy(p1, tname(tp), 16);

	tp = get_id_type(arg2);
	strncpy(p2, tname(tp), 16);

	elog(NOTICE, "there is no operator %s for types %s and %s", op, p1, p2);
	elog(NOTICE, "You will either have to retype this query using an");
	elog(NOTICE, "explicit cast, or you will have to define the operator");
	elog(WARN, "%s for %s and %s using DEFINE OPERATOR", op, p1, p2);
}
