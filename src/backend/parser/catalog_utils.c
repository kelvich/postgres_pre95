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
#include "fmgr.h"

#include "nodes/pg_lisp.h"
#include "catalog/syscache.h"
#include "catalog/catname.h"

#include "exceptions.h"
#include "catalog_utils.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "catalog/indexing.h"
#include "catalog/catname.h"

#include "access/skey.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "access/htup.h"
#include "access/heapam.h"
#include "access/genam.h"
#include "access/itup.h"

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

/*
 *  This structure holds a list of possible functions or operators that
 *  agree with the known name and argument types of the function/operator.
 */
typedef struct _CandidateList {
    ObjectId *args;
    struct _CandidateList *next;
} *CandidateList;

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

/* return a type name, given a typeid */
Name
get_id_typname(id)
long id;
{
   HeapTuple tup;
   Form_pg_type typetuple;

    if (!(tup = SearchSysCacheTuple(TYPOID, id))) {
	elog ( WARN, "type id lookup of %d failed", id);
	return(NULL);
    }
   typetuple = (Form_pg_type)GETSTRUCT(tup);
    return((Name)&(typetuple->typname));
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


int 
att_attnelems(rd, attid)
Relation rd;
int attid;
{
	return(rd->rd_att.data[attid-1]->attnelems);
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

/* given a type, return its typetype ('c' for 'c'atalog types) */
char
typetypetype(t)
Type t;
{
    TypeTupleForm typ;
    
    typ = (TypeTupleForm) GETSTRUCT(t);
    return(typ->typtype);
}

/* given operator, return the operator OID */
OID
oprid(op)
Operator op;
{
    return(op->t_oid);
}

/*
 *  given opname, leftTypeId and rightTypeId,
 *  find all possible (arg1, arg2) pairs for which an operator named
 *  opname exists, such that leftTypeId can be coerced to arg1 and
 *  rightTypeId can be coerced to arg2
 */
int
binary_oper_get_candidates(opname, leftTypeId, rightTypeId, candidates)
char *opname;
int leftTypeId;
int rightTypeId;
CandidateList *candidates;
{
    CandidateList 	current_candidate;
    Relation            pg_operator_desc;
    HeapScanDesc        pg_operator_scan;
    HeapTuple           tup;
    Form_pg_operator	oper;
    Buffer              buffer;
    int			nkeys;
    int			ncandidates = 0;
    ScanKeyEntryData    opKey[3];

    *candidates = NULL;

    opKey[0].flags = 0;
    opKey[0].attributeNumber = OperatorNameAttributeNumber;
    opKey[0].procedure = NameEqualRegProcedure;
    fmgr_info(NameEqualRegProcedure, &opKey[0].func, &opKey[0].nargs);
    opKey[0].argument = NameGetDatum(opname);

    opKey[1].flags = 0;
    opKey[1].attributeNumber = OperatorKindAttributeNumber;
    opKey[1].procedure = CharacterEqualRegProcedure;
    fmgr_info(CharacterEqualRegProcedure, &opKey[1].func, &opKey[1].nargs);
    opKey[1].argument = CharGetDatum('b');

    opKey[2].flags = 0;
    opKey[2].procedure = ObjectIdEqualRegProcedure;
    fmgr_info(ObjectIdEqualRegProcedure, &opKey[2].func, &opKey[2].nargs);

    if (leftTypeId == UNKNOWNOID) {
        if (rightTypeId == UNKNOWNOID) {
	    nkeys = 2;
        }
	else {
	    nkeys = 3;
	    opKey[2].attributeNumber = OperatorRightAttributeNumber;
	    opKey[2].argument = ObjectIdGetDatum(rightTypeId);
        }
    }
    else if (rightTypeId == UNKNOWNOID) {
	nkeys = 3;
	opKey[2].attributeNumber = OperatorLeftAttributeNumber;
	opKey[2].argument = ObjectIdGetDatum(leftTypeId);
    }
    else {
	/* currently only "unknown" can be coerced */
        return 0;
    }

    pg_operator_desc = heap_openr(OperatorRelationName);
    pg_operator_scan = heap_beginscan(pg_operator_desc,
				      0,
				      SelfTimeQual,
				      nkeys,
				      (ScanKey) opKey);

    do {
        tup = heap_getnext(pg_operator_scan, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
	    current_candidate = (CandidateList)palloc(sizeof(struct _CandidateList));
	    current_candidate->args = (ObjectId *)palloc(2 * sizeof(ObjectId));

	    oper = (Form_pg_operator)GETSTRUCT(tup);
	    current_candidate->args[0] = oper->oprleft;
	    current_candidate->args[1] = oper->oprright;
	    current_candidate->next = *candidates;
	    *candidates = current_candidate;
	    ncandidates++;
	    ReleaseBuffer(buffer);
	}
    } while(HeapTupleIsValid(tup));

    heap_endscan(pg_operator_scan);
    heap_close(pg_operator_desc);

    return ncandidates;
}

/*
 *  given a choice of argument type pairs for a binary operator,
 *  try to choose a default pair
 */
CandidateList
binary_oper_select_candidate(arg1, arg2, candidates)
     int arg1, arg2;
     CandidateList candidates;
{
    /*
     * if both are "unknown", there is no way to select a candidate
     *
     * current wisdom holds that the default operator should be one
     * in which both operands have the same type (there will only
     * be one such operator)
     *
     * 7.27.93 - I have decided not to do this; it's too hard to
     * justify, and it's easy enough to typecast explicitly
     */

/*
    CandidateList result;

    if (arg1 == UNKNOWNOID && arg2 == UNKNOWNOID)
	return (NULL);

    for (result = candidates; result != NULL; result = result->next) {
	if (result->args[0] == result->args[1])
	    return result;
    }
*/
    return (NULL);
}

/* Given operator, types of arg1, and arg2, return oper struct */
Operator
oper(op, arg1, arg2)
     char *op;
     int arg1, arg2;	/* typeids */
{
    HeapTuple tup;
    CandidateList candidates;
    int ncandidates;
    /*
    if (!OpCache) {
	init_op_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(OPRNAME, op, arg1, arg2, (char *) 'b'))) {
	ncandidates = binary_oper_get_candidates(op, arg1, arg2, &candidates);
	if (ncandidates == 0) {
	    op_error(op, arg1, arg2);
	    return(NULL);
	}
	else if (ncandidates == 1) {
	    tup = SearchSysCacheTuple(OPRNAME, op, candidates->args[0],
				      candidates->args[1], (char *) 'b');
	    Assert(HeapTupleIsValid(tup));
	}
	else {
	    candidates = binary_oper_select_candidate(arg1, arg2, candidates);
	    if (candidates != NULL) {
		tup = SearchSysCacheTuple(OPRNAME, op, candidates->args[0],
					  candidates->args[1], (char *) 'b');
		Assert(HeapTupleIsValid(tup));
	    }
	    else {
		char p1[16], p2[16];
		Type tp, get_id_type();

		tp = get_id_type(arg1);
		strncpy(p1, tname(tp), 16);

		tp = get_id_type(arg2);
		strncpy(p2, tname(tp), 16);

		elog(NOTICE, "there is more than one operator %s for types", op);
		elog(NOTICE, "%s and %s. You will have to retype this query", p1, p2);
		elog(WARN, "using an explicit cast");
		
		return(NULL);
	    }
	}
    }
    return((Operator) tup);
}

/*
 *  given opname and typeId, find all possible types for which 
 *  a right/left unary operator named opname exists,
 *  such that typeId can be coerced to it
 */
int
unary_oper_get_candidates(op, typeId, candidates, rightleft)
     char *op;
     int typeId;
     CandidateList *candidates;
     char rightleft;
{
    CandidateList 	current_candidate;
    Relation            pg_operator_desc;
    HeapScanDesc        pg_operator_scan;
    HeapTuple           tup;
    Form_pg_operator	oper;
    Buffer              buffer;
    int			ncandidates = 0;

    static ScanKeyEntryData opKey[2] = {
	{ 0, OperatorNameAttributeNumber, NameEqualRegProcedure },
	{ 0, OperatorKindAttributeNumber, CharacterEqualRegProcedure } };

    *candidates = NULL;

    fmgr_info(NameEqualRegProcedure, &opKey[0].func, &opKey[0].nargs);
    opKey[0].argument = NameGetDatum(op);
    fmgr_info(CharacterEqualRegProcedure, &opKey[1].func, &opKey[1].nargs);
    opKey[1].argument = CharGetDatum(rightleft);

    /* currently, only "unknown" can be coerced */
    if (typeId != UNKNOWNOID) {
        return 0;
    }

    pg_operator_desc = heap_openr(OperatorRelationName);
    pg_operator_scan = heap_beginscan(pg_operator_desc,
				      0,
				      SelfTimeQual,
				      2,
				      (ScanKey) opKey);

    do {
        tup = heap_getnext(pg_operator_scan, 0, &buffer);
	if (HeapTupleIsValid(tup)) {
	    current_candidate = (CandidateList)palloc(sizeof(struct _CandidateList));
	    current_candidate->args = (ObjectId *)palloc(sizeof(ObjectId));

	    oper = (Form_pg_operator)GETSTRUCT(tup);
	    if (rightleft == 'r')
		current_candidate->args[0] = oper->oprleft;
	    else
		current_candidate->args[0] = oper->oprright;
	    current_candidate->next = *candidates;
	    *candidates = current_candidate;
	    ncandidates++;
	    ReleaseBuffer(buffer);
	}
    } while(HeapTupleIsValid(tup));

    heap_endscan(pg_operator_scan);
    heap_close(pg_operator_desc);

    return ncandidates;
}

/* Given unary right-side operator (operator on right), return oper struct */
Operator
right_oper(op, arg)
char *op;
int arg; /* type id */
{
    HeapTuple tup;
    CandidateList candidates;
    int ncandidates;

    /*
    if (!OpCache) {
	init_op_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(OPRNAME, op, arg, 0, (char *) 'r'))) {
	ncandidates = unary_oper_get_candidates(op, arg, &candidates, 'r');
	if (ncandidates == 0) {
	    elog ( WARN ,
		  "Can't find right op: %s for type %d", op, arg );
	    return(NULL);
	}
	else if (ncandidates == 1) {
	    tup = SearchSysCacheTuple(OPRNAME, op, candidates->args[0],
				      0, (char *) 'r');
	    Assert(HeapTupleIsValid(tup));
	}
	else {
	    elog(NOTICE, "there is more than one right operator %s", op);
	    elog(NOTICE, "you will have to retype this query");
	    elog(WARN, "using an explicit cast");
	    return(NULL);
	}
    }
    return((Operator) tup);
}

/* Given unary left-side operator (operator on left), return oper struct */
Operator
left_oper(op, arg)
char *op;
int arg; /* type id */
{
    HeapTuple tup;
    CandidateList candidates;
    int ncandidates;

    /*
    if (!OpCache) {
	init_op_cache();
    }
    */
    if (!(tup = SearchSysCacheTuple(OPRNAME, op, 0, arg, (char *) 'l'))) {
	ncandidates = unary_oper_get_candidates(op, arg, &candidates, 'l');
	if (ncandidates == 0) {
	    elog ( WARN ,
		  "Can't find left op: %s for type %d", op, arg );
	    return(NULL);
	}
	else if (ncandidates == 1) {
	    tup = SearchSysCacheTuple(OPRNAME, op, 0,
				      candidates->args[0], (char *) 'l');
	    Assert(HeapTupleIsValid(tup));
	}
	else {
	    elog(NOTICE, "there is more than one left operator %s", op);
	    elog(NOTICE, "you will have to retype this query");
	    elog(WARN, "using an explicit cast");
	    return(NULL);
	}
    }
    return((Operator) tup);
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

/* Given range variable, return whether attribute of this name
 * is a set.
 * NOTE the ASSUMPTION here that no system attributes are, or ever
 * will be, sets.
 */
bool
varisset( rd, name)
Relation rd;
char *name;
{
     int i;
     
     for (i = 0; i < rd->rd_rel->relnatts; i++) {
	  if (! strcmp(&rd->rd_att.data[i]->attname, name)) {
	       return(rd->rd_att.data[i]->attisset);
	  }
     }
     for (i = 0; i < SPECIALS; i++) {
	  if (! strcmp(special_attr[i].field, name)) {
	       return(false);   /* no sys attr is a set */
	  }
     }

     elog(WARN, "Relation %s does not have attribute %s\n",
	  RelationGetRelationName(rd), name);
     return false;
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

/* Given the attribute type of an array return the arrtribute type of
   an element of the array */

ObjectId
GetArrayElementType(typearray)
ObjectId typearray;
{
	HeapTuple type_tuple;
	TypeTupleForm type_struct_array;

	type_tuple = SearchSysCacheTuple(TYPOID, typearray, NULL, NULL, NULL);

    if (!HeapTupleIsValid(type_tuple))
    elog(WARN, "GetArrayElementType: Cache lookup failed for type %d\n",
         typearray);

    /* get the array type struct from the type tuple */
    type_struct_array = (TypeTupleForm) GETSTRUCT(type_tuple);

    if (type_struct_array->typelem == InvalidObjectId) {
    elog(WARN, "GetArrayElementType: type %s is not an array",
        (Name)&(type_struct_array->typname.data[0]));
    }

	return(type_struct_array->typelem);
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

/*
 * get a list of all argument type vectors for which a function named
 * funcname taking nargs arguments exists
 */
CandidateList
func_get_candidates(funcname, nargs)
     char *funcname;
     int nargs;
{
    Relation heapRelation;
    Relation idesc;
    ScanKeyEntryData skey;
    HeapTuple tuple;
    IndexScanDesc sd;
    GeneralRetrieveIndexResult indexRes;
    Buffer buffer;
    Form_pg_proc pgProcP;
    bool bufferUsed = FALSE;
    CandidateList candidates = NULL;
    CandidateList current_candidate;
    int i;

    heapRelation = heap_openr(ProcedureRelationName);
    ScanKeyEntryInitialize(&skey,
                           (bits16)0x0,
                           (AttributeNumber)1,
                           (RegProcedure)NameEqualRegProcedure,
                           (Datum)funcname);

    idesc = index_openr((Name)ProcedureNameIndex);

    sd = index_beginscan(idesc, false, 1, (ScanKey)&skey);

    do {  
        tuple = (HeapTuple)NULL;
        if (bufferUsed) {
            ReleaseBuffer(buffer);
            bufferUsed = FALSE;
        }

        indexRes = AMgettuple(sd, ForwardScanDirection);
        if (GeneralRetrieveIndexResultIsValid(indexRes)) {
            ItemPointer iptr;

            iptr = GeneralRetrieveIndexResultGetHeapItemPointer(indexRes);
            tuple = heap_fetch(heapRelation, NowTimeQual, iptr, &buffer);
            pfree(indexRes);
            if (HeapTupleIsValid(tuple)) {
                pgProcP = (Form_pg_proc)GETSTRUCT(tuple);
                bufferUsed = TRUE;
		if (pgProcP->pronargs == nargs) {
		    current_candidate = (CandidateList)
			palloc(sizeof(struct _CandidateList));

		    current_candidate->args = (ObjectId *)
			palloc(8 * sizeof(ObjectId));
		    bzero(current_candidate->args, 8 * sizeof(ObjectId));
		    for (i=0; i<nargs; i++) {
			current_candidate->args[i] = 
			    pgProcP->proargtypes.data[i];
		    }

		    current_candidate->next = candidates;
		    candidates = current_candidate;
		}
            }
	}
    } while (GeneralRetrieveIndexResultIsValid(indexRes));

    index_endscan(sd);
    index_close(idesc);
    heap_close(heapRelation);		 

    return candidates;
}

/*
 * can input_typeids be coerced to func_typeids?
 */
bool can_coerce(nargs, input_typeids, func_typeids)
     int nargs;
     ObjectId *input_typeids;
     ObjectId *func_typeids;
{
    int i;
    Type tp;

    /*
     * right now, we only coerce "unknown", and we cannot coerce it to a
     * relation type
     */
    for (i=0; i<nargs; i++) {
	if (input_typeids[i] != func_typeids[i]) {
	    if (input_typeids[i] != UNKNOWNOID || func_typeids[i] == 0)
		return false;
	    
	    tp = get_id_type(input_typeids[i]);
	    if (typetypetype(tp) == 'c' )
		return false;
	}
    }

    return true;
}

/*
 * given a list of possible typeid arrays to a function and an array of
 * input typeids, produce a shortlist of those function typeid arrays
 * that match the input typeids (either exactly or by coercion), and
 * return the number of such arrays
 */
int
match_argtypes(nargs, input_typeids, function_typeids, candidates)
     int nargs;
     ObjectId *input_typeids;
     CandidateList function_typeids;
     CandidateList *candidates;		/* return value */
{
    CandidateList current_candidate;
    CandidateList matching_candidate;
    ObjectId *current_typeids;
    int ncandidates = 0;

    *candidates = NULL;

    for (current_candidate = function_typeids;
	 current_candidate != NULL;
	 current_candidate = current_candidate->next) {
	current_typeids = current_candidate->args;
	if (can_coerce(nargs, input_typeids, current_typeids)) {
	    matching_candidate = (CandidateList)
		palloc(sizeof(struct _CandidateList));
	    matching_candidate->args = current_typeids;
	    matching_candidate->next = *candidates;
	    *candidates = matching_candidate;
	    ncandidates++;
	}
    }

    return ncandidates;
}

/*
 * given the input argtype array and more than one candidate
 * for the function argtype array, attempt to resolve the conflict.
 * returns the selected argtype array if the conflict can be resolved,
 * otherwise returns NULL
 */
ObjectId *
func_select_candidate(nargs, input_typeids, candidates)
     int nargs;
     ObjectId *input_typeids;
     CandidateList candidates;
{
    /* XXX no conflict resolution implemeneted yet */
    return (NULL);
}

bool
func_get_detail(funcname, nargs, oid_array, funcid, rettype,
		retset, true_typeids)
    char *funcname;
    int nargs;
    ObjectId *oid_array;
    ObjectId *funcid;			/* return value */
    ObjectId *rettype;			/* return value */
    bool *retset;			/* return value */
    ObjectId **true_typeids;		/* return value */
{
    ObjectId *fargs;
    ObjectId **input_typeid_vector;
    ObjectId *current_input_typeids;
    CandidateList function_typeids;
    CandidateList current_function_typeids;
    HeapTuple ftup;
    Form_pg_proc pform;

    /*
     * attempt to find named function in the system catalogs
     * with arguments exactly as specified - so that the normal
     * case is just as quick as before
     */
    ftup = SearchSysCacheTuple(PRONAME, funcname, nargs, oid_array, 0);
    *true_typeids = oid_array;

    /*
     * If an exact match isn't found :
     * 1) get a vector of all possible input arg type arrays constructed
     *    from the superclasses of the original input arg types
     * 2) get a list of all possible argument type arrays to the
     *	  function with given name and number of arguments
     * 3) for each input arg type array from vector #1 :
     *	  a) find how many of the function arg type arrays from list #2
     *	     it can be coerced to
     *	  b) - if the answer is one, we have our function
     *	     - if the answer is more than one, attempt to resolve the
     *	       conflict
     *	     - if the answer is zero, try the next array from vector #1
     */
    if (!HeapTupleIsValid(ftup)) {
	function_typeids = func_get_candidates(funcname, nargs);

	if (function_typeids != NULL) {
	    int ncandidates = 0;

	    input_typeid_vector = argtype_inherit(nargs, oid_array);
	    current_input_typeids = oid_array;

	    do {
		ncandidates = match_argtypes(nargs, current_input_typeids,
					     function_typeids,
					     &current_function_typeids);
		if (ncandidates == 1) {
		    *true_typeids = current_function_typeids->args;
		    ftup = SearchSysCacheTuple(PRONAME, funcname, nargs,
					       *true_typeids, 0);
		    Assert(HeapTupleIsValid(ftup));
		}
		else if (ncandidates > 1) {
		    *true_typeids =
			func_select_candidate(nargs,
					      current_input_typeids,
					      current_function_typeids);
		    if (*true_typeids == NULL) {
			elog(NOTICE, "there is more than one function named \"%s\"",
			     funcname);
			elog(NOTICE, "that satisfies the given argument types. you will have to");
			elog(NOTICE, "retype your query using explicit typecasts.");
			func_error("func_get_detail", funcname, nargs, oid_array);
		    }
		    else {
			ftup = SearchSysCacheTuple(PRONAME, funcname, nargs,
						   *true_typeids, 0);
			Assert(HeapTupleIsValid(ftup));
		    }
		}
		current_input_typeids = *input_typeid_vector++;
	    } 
	    while (current_input_typeids !=
		   InvalidObjectId && ncandidates == 0);
	}
    }

    if (!HeapTupleIsValid(ftup)) {
	Type tp = get_id_type(oid_array[0]);

	if (nargs == 1 && typetypetype(tp) == 'c')
	    elog(WARN, "no such attribute or function \"%s\"",
		 funcname);
	else
	    func_error("func_get_detail", funcname, nargs, oid_array);
    }

    else {
	pform = (Form_pg_proc) GETSTRUCT(ftup);
	*funcid = ftup->t_oid;
	*rettype = (ObjectId) pform->prorettype;
	*retset = (ObjectId) pform->proretset;

	return (true);
    }
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
    return (genxprod(arginh, nargs));
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
genxprod(arginh, nargs)
    InhPaths *arginh;
    int nargs;
{
    int nanswers;
    ObjectId **result, **iter;
    ObjectId *oneres;
    int i, j;
    int cur[MAXFARGS];

    nanswers = 1;
    for (i = 0; i < nargs; i++) {
	nanswers *= (arginh[i].nsupers + 2);
	cur[i] = 0;
    }

    iter = result = (ObjectId **) palloc(sizeof(ObjectId *) * nanswers);

    /* compute the cross product from right to left */
    for (;;) {
	oneres = (ObjectId *) palloc(MAXFARGS * sizeof(ObjectId));
	bzero(oneres, MAXFARGS * sizeof(ObjectId));

	for (i = nargs - 1; i >= 0 && cur[i] > arginh[i].nsupers; i--)
	    continue;

	/* if we're done, terminate with NULL pointer */
	if (i < 0) {
	    *iter = NULL;
	    return (result);
	}

	/* no, increment this column and zero the ones after it */
	cur[i] = cur[i] + 1;
	for (j = nargs - 1; j > i; j--)
	    cur[j] = 0;

	for (i = 0; i < nargs; i++) {
	    if (cur[i] == 0)
		oneres[i] = arginh[i].self;
	    else if (cur[i] > arginh[i].nsupers)
		oneres[i] = 0;	/* wild card */
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
		elog (WARN , "type id lookup of %d failed", type_id);
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

/*
 * Error message when function lookup fails that gives details of the
 * argument types
 */
void
func_error(caller, funcname, nargs, argtypes)

char *caller;
char *funcname;
int nargs;
int *argtypes;
{	Type get_id_type();
	char *typname;
	char p[(16+2)*8], *ptr;
	int i;

	ptr = p;
	*ptr = '\0';
	for (i=0; i<nargs; i++) {
	      if (i) {
		    *ptr++ = ',';
		    *ptr++ = ' ';
	      }
	      if (argtypes[i] != 0) {
		    strncpy(ptr, tname(get_id_type(argtypes[i])), 16);
		    *(ptr + 16) = '\0';
	      }
	      else
		    strcpy(ptr, "any");
	      ptr += strlen(ptr);
	}

	elog(WARN, "%s: function %s(%s) does not exist", caller, funcname, p);
}
