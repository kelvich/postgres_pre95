#define MAXPATHLEN 1024

#include <strings.h>
#include <stdio.h>
#include <pwd.h>

#include "utils/log.h"
#include "catalog_utils.h"
#include "nodes/pg_lisp.h"
#include "utils/exc.h"
#include "utils/excid.h"
#include "io.h"
#include "utils/palloc.h"
#include "parse_query.h"
#include "catalog/pg_aggregate.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "parser/parse.h"
#include "parser/parsetree.h"
#include "utils/builtins.h"
#include "lib/lisplist.h"
 
RcsId("$Header$");

LispValue parsetree;

#define DB_PARSE(foo) 

parser(str, l, typev, nargs)
     char *str;
     LispValue l;
     ObjectId *typev;
     int nargs;
{
    int yyresult;

    init_io();
/*
 * Parser must now return a parse tree in C space.  Thus, it cannot
 * start/end at this high a granularity.
 *
    startmmgr(0);
 */

    /* Set things up to read from the string, if there is one */
    if (strlen(str) != 0) {
	StringInput = 1;
	TheString = (char *) palloc(strlen(str) + 1);
	bcopy(str,TheString,strlen(str)+1);
    }

    {
      parser_init(typev, nargs);
      yyresult = yyparse();
      
      clearerr(stdin);
      
      if (yyresult) {	/* error */
	CAR(l) = LispNil;
	CDR(l) = LispNil;
	return(-1);
      }
      
      CAR(l) = CAR(parsetree);
      CDR(l) = CDR(parsetree);
    }

    if (parsetree == NULL) {
	return(-1);
    } else {
	return(0);
    }
}

LispValue
new_filestr ( filename )
     LispValue filename;
{
  return (lispString (filename_in (CString(filename))));
}

int
lispAssoc ( element, list)
     LispValue element, list;
{
    LispValue temp = list;
    int i = 0;
    if (list == LispNil) 
      return -1; 
    /* printf("Looking for %d", CInteger(element));*/

    while (temp != LispNil ) {
	if(CInteger(CAR(temp)) == CInteger(element))
	  return i;
	temp = CDR(temp);
	i ++;
    }
	   
    return -1;
}

LispValue
parser_typecast ( expr, typename )
     LispValue expr;
     LispValue typename;
{
    /* check for passing non-ints */
    Const adt;
    Datum lcp;
    Type tp;
	char *type_string, *p, type_string_2[16];
    int32 len;
    char *cp = NULL;

    char *const_string; 
	bool string_palloced = false;

	type_string = CString(typename);
	if ((p = (char *) index(type_string, '[')) != NULL)
	{
		*p = 0;
		sprintf(type_string_2,"_%s", type_string);
		tp = (Type) type(type_string_2);
	}
	else
	{
		tp = (Type) type(type_string);
	}

	len = tlen(tp);

    switch ( CInteger(CAR(expr)) ) {
      case 23: /* int4 */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%d",
		get_constvalue((Const)CDR(expr)));
	break;
      case 19: /* char16 */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%s",
		get_constvalue((Const)CDR(expr)));
	break;
      case 18: /* char */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%c",
		get_constvalue((Const)CDR(expr)));
	break;
      case 701:/* float8 */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%f",
		get_constvalue((Const)CDR(expr)));
	break;
      case 25: /* text */
	const_string = 
	  DatumGetPointer(
			  get_constvalue((Const)CDR(expr)) );
	  const_string = (char *) textout((struct varlena *)const_string);
	break;
      case 705: /* unknown */
        const_string =
          DatumGetPointer(
                          get_constvalue((Const)CDR(expr)) );
          const_string = (char *) textout((struct varlena *)const_string);
        break;
      default:
	elog(WARN,"unknown type%d ",
	     CInteger(CAR(expr)) );
    }
    
    cp = instr2 (tp, const_string);
    
    
    if (!tbyvalue(tp)) {
	if (len >= 0 && len != PSIZE(cp)) {
	    char *pp;
	    pp = (char *) palloc(len);
	    bcopy(cp, pp, len);
	    cp = pp;
	}
	lcp = PointerGetDatum(cp);
    } else {
	switch(len) {
	  case 1:
	    lcp = Int8GetDatum(cp);
	    break;
	  case 2:
	    lcp = Int16GetDatum(cp);
	    break;
	  case 4:
	    lcp = Int32GetDatum(cp);
	    break;
	  default:
	    lcp = PointerGetDatum(cp);
	    break;
	}
    }
    
    adt = MakeConst ( typeid(tp), len, (Datum)lcp , 0, 0/*was omitted*/ );
    /*
      printf("adt %s : %d %d %d\n",CString(expr),typeid(tp) ,
      len,cp);
      */
	if (string_palloced) pfree(const_string);
    return (lispCons  ( lispInteger (typeid(tp)) , (LispValue)adt ));
}

LispValue
parser_typecast2 ( expr, tp)
     LispValue expr;
     Type tp;
{
    /* check for passing non-ints */
    Const adt;
    Datum lcp;
    int32 len = tlen(tp);
    char *cp = NULL;

    char *const_string; 
	bool string_palloced = false;
    
    switch ( CInteger(CAR(expr)) ) {
      case 23: /* int4 */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%d",
		get_constvalue((Const)CDR(expr)));
	break;
      case 19: /* char16 */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%s",
		get_constvalue((Const)CDR(expr)));
	break;
      case 18: /* char */
	  const_string = (char *) palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%c",
		get_constvalue((Const)CDR(expr)));
	break;
      case 701:/* float8 */
	{
	    float64 floatVal = 
		DatumGetFloat64(get_constvalue((Const)CDR(expr)));
	    const_string = (char *) palloc(256);
	    string_palloced = true;
	    sprintf(const_string,"%f", *floatVal);
	    break;
	}
      case 25: /* text */
	const_string = 
	  DatumGetPointer(
			  get_constvalue((Const)CDR(expr)) );
	  const_string = (char *) textout((struct varlena *)const_string);
	break;
      case 705: /* unknown */
        const_string =
          DatumGetPointer(
                          get_constvalue((Const)CDR(expr)) );
          const_string = (char *) textout((struct varlena *)const_string);
        break;
      default:
	elog(WARN,"unknown type%d ",
	     CInteger(CAR(expr)) );
    }
    
    cp = instr2 (tp, const_string);
    
    
    if (!tbyvalue(tp)) {
	if (len >= 0 && len != PSIZE(cp)) {
	    char *pp;
	    pp = (char *) palloc(len);
	    bcopy(cp, pp, len);
	    cp = pp;
	}
	lcp = PointerGetDatum(cp);
    } else {
	switch(len) {
	  case 1:
	    lcp = Int8GetDatum(cp);
	    break;
	  case 2:
	    lcp = Int16GetDatum(cp);
	    break;
	  case 4:
	    lcp = Int32GetDatum(cp);
	    break;
	  default:
	    lcp = PointerGetDatum(cp);
	    break;
	}
    }
    
    adt = MakeConst ( (ObjectId)typeid(tp), (Size)len, (Datum)lcp , 0 , 0/*was omitted*/);
    /*
      printf("adt %s : %d %d %d\n",CString(expr),typeid(tp) ,
      len,cp);
      */
	if (string_palloced) pfree(const_string);
    return ((LispValue) adt);
}

char *
after_first_white_space( input )
     char *input;
{
    char *temp = input;
    while ( *temp != ' ' && *temp != 0 ) 
      temp = temp + 1;

    return (temp+1);
}

int 
*int4varin( input_string )
     char *input_string;
{
    int *foo = (int *)malloc(1024*sizeof(int));
    register int i = 0;

    char *temp = input_string;
    
    while ( sscanf(temp,"%ld", &foo[i+1]) == 1 
	   && i < 1022 ) {
	i = i+1;
	temp = after_first_white_space(temp);
    }
    foo[0] = i;
    return(foo);
}

char *
int4varout ( an_array )
     int *an_array;
{
    int temp = an_array[0];
    char *output_string = NULL;
    extern int itoa();
    int i;

    if ( temp > 0 ) {
	char *walk;
	output_string = (char *)malloc(16*temp); /* assume 15 digits + sign */
	walk = output_string;
	for ( i = 0 ; i < temp ; i++ ) {
	    itoa(an_array[i+1],walk);
	    printf ( "%s\n", walk );
	    while (*++walk != '\0')
	      ;
	    *walk++ = ' ';
	}
	*--walk = '\0';
    }
    return(output_string);
}

 
#define ADD_TO_RT(rt_entry)     p_rtable = nappend1(p_rtable,rt_entry) 
List
ParseFunc ( funcname , fargs )
     char *funcname;
     List fargs;
{
    extern OID funcname_get_rettype();
    extern OID funcname_get_funcid();
    extern ObjectId *funcname_get_funcargtypes();
    OID rettype = (OID)0;
    OID funcid = (OID)0;
    OID argrelid;
    Func funcnode = (Func)NULL;
    LispValue i = LispNil;
    List first_arg_type = NULL;
    Name relname;
    extern List p_rtable;
    extern Var make_relation_var();
    Relation rd;
    ObjectId relid;
    int attnum;
    int nargs,x;
    ObjectId *oid_array;
    OID argtype;
    Iter iter;
    Param f;
    int vnum;

    if (fargs)
     {
	 if (CAR(fargs) == LispNil)
	   elog (WARN,"function %s does not allow NULL input",funcname);
	 first_arg_type = CAR(CAR(fargs));
     }

    /*
    ** check for methods: if function takes one argument, and that argument
    ** is either a relation or a PQ function returning a complex type,
    ** then the function could be a projection.
    */

    if (length(fargs) == 1)
      if (lispAtomp(first_arg_type) && CAtom(first_arg_type) == RELATION)
       {
	   /* this is could be a method */
	   relname = (Name) CString(CDR(CAR(fargs)));
	   if( RangeTablePosn ( relname ,LispNil ) == 0 )
	     ADD_TO_RT( MakeRangeTableEntry ((Name)relname,
					     LispNil, 
					     (Name)relname ));
	   rd = heap_openr(relname);
	   relid = RelationGetRelationId(rd);
	   heap_close(rd);
	   if ((attnum = get_attnum(relid, (Name) funcname)) 
	       != InvalidAttributeNumber)
	     return(make_var(relname, funcname));
	   else	/* drop through */;
       }
      else if (IsA(CADR(CAR(fargs)),Func) && 
	      (argrelid = typeid_get_relid
	       ((int)(argtype = funcid_get_rettype
		(get_funcid((Func)CADR(CAR(fargs))))))))
       {
	   /* the argument is a function returning a tuple, so funcname
	      may be a projection */
	   if ((attnum = get_attnum(argrelid, (Name) funcname)) 
	       != InvalidAttributeNumber)
	    {
		/* 
		** build an Iter containing this func node, add a tlist to the 
		** func node, and return the Iter.
		*/
		iter = (Iter)MakeIter((LispValue)CDR(CAR(fargs)));
		setup_func_tlist((Func)CAR(get_iterexpr(iter)), 
				 funcname, argrelid);
		return(lispCons(lispInteger(argtype),iter));
	    }
	   else /* drop through */;
       }

    /* If we dropped through to here it's really a function */
    funcid = funcname_get_funcid ( funcname );
    rettype = funcname_get_rettype ( funcname );
	
    if ( funcid != (OID)0 && rettype != (OID)0 ) 
     {
	 funcnode = MakeFunc ( funcid , rettype , false, 0,LispNil,0, NULL );
     } 
    else elog (WARN,"function %s does not exist",funcname);
    nargs = funcname_get_funcnargs(funcname);
    if (nargs != length(fargs))
      elog(WARN, "function '%s' takes %d arguments not %d",
	   funcname, nargs, length(fargs));
    oid_array = funcname_get_funcargtypes(funcname);

    /* 
    ** type checking and resolution -- if an argument is a relation we turn it 
    ** into a Var.  if it's a Param we resolve the type of the parameter.
    */
    x=0;
    foreach ( i , fargs ) 
     {
	 List pair = CAR(i);
	 ObjectId toid;
	 
	 if (IsA(CDR(pair),Param)) 
	  {
	      f = (Param)CDR(pair);
	      rd = heap_open(get_paramtype(f));
	      if (!RelationIsValid(rd)) {
		  elog(WARN,
		       "$%d is not a complex type; illegal expression",
		       get_paramid(f));
	      }
	      relname = RelationGetRelationName(rd);
	      relid = RelationGetRelationId(rd);
	     
	      /* set up the function args list to point to the parameter */
	      CAR(i) = (LispValue)MakeList
		(MakeParam(get_paramkind(f),get_paramid(f),
			   get_paramname(f),get_paramtype(f)),
		 -1);
	  }

	 else if (lispAtomp(CAR(pair)) && CAtom(CAR(pair)) == RELATION)
	  {
	      toid = typeid(type(CString(CDR(pair))));

	      relname = (Name)CString(CDR(pair));
	      rd = heap_openr(relname);
	      relid = RelationGetRelationId(rd);

	      /* get the range table entry for the var node */
	      vnum = RangeTablePosn(relname, 0);
	      if (vnum == 0) {
		  p_rtable = nappend1(p_rtable ,
				      MakeRangeTableEntry(relname, 0, relname));
		  vnum = RangeTablePosn (relname, 0);
	      }

	      /*
	       *  for func(func..(relname)..), the param to the first function
	       *  is the tuple under consideration.  we build a special
	       *  VarNode to reflect this -- it has varno set to the correct
	       *  range table entry, but has varattno == 0 to signal that the
	       *  whole tuple is the argument.
	       */

	      CAR(i) = (LispValue)
		MakeList(MakeVar(vnum, 0, relid,
				 LispNil /* vardotfields */,
				 LispNil /* vararraylist */,
				 lispCons(lispInteger(vnum),
					  lispCons(lispInteger(0),LispNil)),
				 0 /* varslot */),
			 -1);
	  }

	 else
	  {
	      toid = CInteger(CAR(pair));
	      CAR(i) = CDR(pair);
	  }

	 if (oid_array[x] != 0 && toid != oid_array[x]) 
	   elog(WARN, "Argument type mismatch in function '%s': arg %d is not of type %s",
		funcname, x+1, tname(get_id_type(oid_array[x])));

	 x++;
     }
	
    return ( lispCons (lispInteger(rettype) ,
		       lispCons ( (LispValue)funcnode , fargs )));
	    
}

List
ParseAgg(aggname, query, tlist)
    char *aggname;
    List query;
    List tlist;
{
    int fintype;
    OID AggId = (OID)0;
    List list = LispNil;
    char *keyword = "agg";
    HeapTuple theAggTuple;
    tlist = CADR(tlist);
    if(!query)
	elog(WARN,"aggregate %s called without arguments", aggname);
    theAggTuple = SearchSysCacheTuple(AGGNAME, aggname, 0, 0, 0); 
    AggId = (theAggTuple ? theAggTuple->t_oid : (OID)0);

    if(AggId == (OID)0) {
       elog(WARN, "aggregate %s does not exist", aggname);
    }
    fintype = CInteger((LispValue)SearchSysCacheGetAttribute(AGGNAME,
		AggregateFinalTypeAttributeNumber, aggname, 0, 0, 0));

    if(fintype != 0 ) {
       list = (LispValue)
	 MakeList(lispInteger(fintype),lispName(keyword),lispName(aggname),
		  query,tlist,-1);
    } else
	elog(WARN, "aggregate %s does not exist", aggname);

    return (list);
}


/*
 * RemoveUnusedRangeTableEntries
 * - removes relations from the rangetable that are no longer
 *   useful.  This helps in preventing the planner from generating
 *   extra joins that are not needed.
 */

/* XXX - not used yet

List
RemoveUnusedRangeTableEntries ( parsetree )
     List parsetree;
{

}
*/

/* 
 * FlattenRelationList
 *
 * at this point, time-qualified relation/relation-lists
 * have a lispInteger in the front,
 * inheritance relations have a lispString("*")
 * in front
 */ 


#ifdef NOTYET
 XXX - not used yet 

List
FlattenRelationList ( rel_list )
     List rel_list;
{
    List temp = NULL;
    List possible_rtentry = NULL;

    foreach ( temp, rel_list ) {
	possible_rtentry = CAR(temp);
	
	if ( consp ( possible_rtentry ) ) {

	    /* can be any one of union, inheritance or timerange queries */
	    
	} else {
	    /* normal entry */
	    
	}
    }
}

#endif

List
MakeFromClause ( from_list, base_rel )
     List from_list;
     LispValue base_rel;
{
    List i = NULL;
    List j = NULL;
    List k = NULL;
    List flags = NULL;
    List entry = NULL;
    bool IsConcatenation = false;
    List existing_varnos = NULL;
    extern List RangeTablePositions();

    /* from_list will be a list of strings */
    /* base_rel will be a wierd assortment of things, 
       including timerange, inheritance and union relations */

    if ( length ( base_rel ) > 1 ) {
	IsConcatenation = true;
    }

    foreach ( i , from_list ) {
        List temp = CAR(i);
	
	foreach ( j , base_rel ) {
	    List x = CAR(j);

	    /* now reinitialize "flags" so that we don't 
	     * get inheritance or time range queries for
	     * relations that we didn't want them on 
	     */
	    
	    flags = lispCons(lispInteger(0),LispNil);
	    
	    if ( IsConcatenation == true ) {
		flags = nappend1(flags, lispAtom("union"));
	    }
	    if ( ! null ( CADDR(x))) {
		flags = nappend1(flags, lispAtom("inherits"));
	    }
	    
	    if ( ! null ( CADR(x))) {
		/* set time range, if one exists */
		CAR(flags) = CADR(x);
	    }
	    
	    existing_varnos = 
	      RangeTablePositions ( CString( CAR(x)), LispNil );
	    /* XXX - 2nd argument is currently ignored */
	    
	    if ( existing_varnos == NULL ) {
		/* if the base relation does not exist in the rangetable
		 * as a virtual name, make a new rangetable entry
		 */
		entry = (List)MakeRangeTableEntry ( (Name)CString ( CAR(x)),
						   flags , 
						   (Name)CString(temp));
	        ADD_TO_RT(entry);
	    } else {
		/* XXX - temporary, should append the existing flags
		 * to the new flags or not ?
		 */
		foreach(k,existing_varnos) {
		    int existing_varno = CInteger(CAR(k));
		    List rt_entry = nth ( existing_varno-1 , p_rtable );
		    if ( IsA(CAR(rt_entry),LispStr)) {
			/* first time consing it */
			CAR(rt_entry) = lispCons ( CAR(rt_entry) , 
						  lispCons (temp,LispNil ));
		    } else {
			/* subsequent consing */
			CAR(rt_entry) = nappend1 ( CAR(rt_entry) , temp);
		    }
		    /* in either case, if no union flag exists, 
		     * cons one in
		     */
		    rt_flags (rt_entry) = nappend1 ( rt_flags(rt_entry),
						    lispAtom("union"));
		} /* foreach k */

	    } /* if existing_varnos == NULL */

	    /* NOTE : we dont' pfree old "flags" because they are
	     * really still present in the parsetree
	     */

	  } /* foreach j */
      } /* foreach i */

    return ( entry );
}

Var make_relation_var(relname)

char *relname;

{
    Var varnode;
    int vnum, attid = 1, vartype;
    LispValue vardotfields;
    extern LispValue p_rtable;
    extern int p_last_resno;
    Index vararrayindex = 0;
    
    vnum = RangeTablePosn ( relname,0) ;
    if (vnum == 0) {
	p_rtable = nappend1 (p_rtable ,
			     MakeRangeTableEntry ( (Name)relname , 0 , (Name)relname) );
		vnum = RangeTablePosn (relname,0);
    }

    vartype = RELATION;
    vardotfields = LispNil;                          /* XXX - fix this */
    
    varnode = MakeVar ((Index)vnum , (AttributeNumber)attid ,
		       (ObjectId)vartype , (List)vardotfields , (List)vararrayindex ,
		       (List)lispCons(lispInteger(vnum),
				lispCons(lispInteger(attid),LispNil)),
		       (Pointer)0 );
    
    return ( varnode );
}

int is_postquel_func(parameters)
     List parameters;
{
    List assoc_list;
    List rest;
    assoc_list = CDR(parameters);
    foreach (rest, assoc_list) {
	List item = CAR(CAR(rest));
	List value;
	
	/*
	 * if this parameter does not have an associated value.
	 */
	if ( !CDR(CAR(rest)) )
	    continue;
	else
	    value = CAR(CDR(CAR(rest)));

 	if (!stringp(item)) continue;
	if (!strcmp(CString(item), "language")) {
	    char *name;
	    char *c;

	    name = CString(value);
	    for (c = name; *c != '\0'; c++)
		*c = (islower(*c) ? toupper(*c) : *c);
	    if (!strcmp(name, "POSTQUEL")) 
		return true;
	    else
		return false;
	}
    }
    return false;
}
char *postquel_func_arg(parameters)
     List parameters;
{
    List assoc_list;
    List rest;
    assoc_list = CDR(parameters);
    foreach (rest, assoc_list) {
	List item = CAR(CAR(rest));
	List value = CAR(CDR(CAR(rest)));

	if (atom(item) && (CAtom(item) == ARG)) {
	    if (stringp(value)) {
		char *name = CString(value);
		return name;
	    }
	}

    }
    elog(WARN, "no zero argument postquel functions");
}
List func_arg_list(parameters)
     List parameters;
{
    List assoc_list;
    List rest;
    assoc_list = CDR(parameters);
    foreach (rest, assoc_list) {
	List item = CAR(CAR(rest));
	List value = CAR(CDR(CAR(rest)));

	if (atom(item) && (CAtom(item) == ARG)) return CDR(CAR(rest));
    }
    return LispNil;
}


/*
** HandleNestedDots --
**    Given a nested dot expression (i.e. (relation func ... attr), build up
** a tree with of Iter and Func nodes.
*/
LispValue HandleNestedDots(dots)
     List dots;
{
    int vnum;
    ObjectId relid;
    extern LispValue p_rtable;
    Name attrname; 
    ObjectId producer_relid, producer_type, first_func;
    List mutator_lisp,mutator_iter,nest,newfunc;
    int attnum;
    Name relname;
    Relation rd;
    char *mutator;
    LispValue retval = LispNil, oldval = LispNil;
    ObjectId funcid;
    ObjectId functype;
    OID funcrettype;
    LispValue first;
    Param f;
    List fargs;
    ObjectId *oid_array,input_type;
    int nargs;


    /*
     *  If this nested dot expression is hanging off a param node,
     *  then we need to resolve the type of the parameter before
     *  we go any further.  Otherwise, it must be hanging off a
     *  relation name.
     */

    first = CAR(dots);
    if (IsA(first,Param)) {
	f = (Param)first;
	rd = heap_open(get_paramtype(f));
	if (!RelationIsValid(rd)) {
	    elog(WARN,
		 "$%d is not a complex type; illegal nested dot expression",
		 get_paramid(f));
	}
	relname = RelationGetRelationName(rd);
	relid = RelationGetRelationId(rd);

	/* set up the function args list to point to the parameter */
	retval = (LispValue)MakeList(MakeParam(get_paramkind(f),get_paramid(f),
					       get_paramname(f),get_paramtype(f)),
			  -1);
    } else {
	relname = (Name)CString(first);
	rd = heap_openr(relname);
	relid = RelationGetRelationId(rd);

	/* get the range table entry for the var node */
	vnum = RangeTablePosn(relname, 0);
	if (vnum == 0) {
	     p_rtable = nappend1(p_rtable ,
				  MakeRangeTableEntry(relname, 0, relname));
	     vnum = RangeTablePosn (relname, 0);
	}

	/*
	 *  for relname.func.func...., the param to the first function
	 *  is the tuple under consideration.  we build a special
	 *  VarNode to reflect this -- it has varno set to the correct
	 *  range table entry, but has varattno == 0 to signal that the
	 *  whole tuple is the argument.
	 */

	retval = (LispValue)
	  MakeList(MakeVar(vnum, 0, relid,
			   LispNil /* vardotfields */,
			   LispNil /* vararraylist */,
			   lispCons(lispInteger(vnum),
				    lispCons(lispInteger(0),LispNil)),
			   0 /* varslot */),
		   -1);
    }

    /* 
     * the producer is the relation associated with the last nesting we've 
     * examined. At this point it's the relation at the head of the list
     */

    producer_relid = relid;
    producer_type = typeid(type(relname));
    nest = LispNil;
    first_func = 0;

    /* 
     * walk through the dots after the relation, 
     * and build the tree bottom up.
     */

    foreach (mutator_iter, CDR(dots)) {

	mutator_lisp = CAR(mutator_iter);
	mutator = CString(mutator_lisp);

	attnum = get_attnum(producer_relid, (Name) mutator);
	if (attnum) {
	    /* this attribute is actually a field of the previous Rel */
	    if (CDR(mutator_iter) != LispNil)
		elog(WARN,
		     "%s is a projection, cannot put a dot after it", mutator);
	    setup_func_tlist((Func)CAR(get_iterexpr((Iter)retval)), 
			     mutator, producer_relid);
	    producer_relid = 0;

	} else {
	
	    /* this attribute is a PQfunction */
	    funcid = funcname_get_funcid(mutator);

	    if (!funcid)
		elog(WARN,
		     "Component of nested dot is not a function or final projection");
	    /* do type checking */
	    funcrettype = funcname_get_rettype(mutator);
	    functype = funcname_get_rettype(mutator);

	    if (first_func == 0)
		first_func = funcid;

	    input_type = producer_type;
	    producer_type = functype;
	    producer_relid = typeid_get_relid(functype);
	    nargs = funcname_get_funcnargs(mutator);

	    if (nargs != 1)
		elog(WARN,
		  "Functions in nested dot expressions take single arguments");

	    oid_array = funcname_get_funcargtypes(mutator);
	    if (oid_array[0] != input_type)
		elog(WARN, "Type incompatibility in nested dot");

	    /* put func and an iter onto the retval structure */
	    oldval = retval;
	    retval = (LispValue) MakeIter(lispDottedPair());
	    CAR(get_iterexpr((Iter)retval)) = (LispValue)
	      MakeFunc(funcid, functype, 0, 
		       tlen(get_id_type(funcrettype)),
		       LispNil /* func_fcache */,
		       LispNil /* func_tlist */),
	      CDR(get_iterexpr((Iter)retval)) = oldval;
	
	}
    }

    if (producer_relid != 0)
      elog(WARN, "nested dot must return a base type [temporarily]");

    /* be tidy */
    heap_close(rd);

    /*
     *  Last thing to do is to declare the return type of the nested
     *  dot expression for use by the typechecking code.
     */

    retval = lispCons(lispInteger(producer_type), retval);

    return(retval);
}


/*
** setup_func_tlist --
**     Add a tlist to a Func node that says which attribute to project to.
*/
void setup_func_tlist(func, attname, relid)
     Func func;
     Name attname;
     ObjectId relid;
{
    TLE tle;
    LispValue tlist;
    Resdom resnode;
    Var varnode;
    ObjectId typeid;
    int attno;

    attno = get_attnum(relid, attname);
    typeid = find_atttype(relid, attname);
    resnode = MakeResdom(1, typeid,
			 tlen(get_id_type(typeid)),
			 get_attname(relid, attno),
			 NULL	/* reskey   */,
			 NULL	/* reskeyop */,
			 0	/* resjunk  */);
    varnode = MakeVar(-1, attno, typeid,
		      LispNil	/* vardotfields */,
		      LispNil	/* vararraylist */,
		      lispCons(lispInteger(-1),
			       lispCons(lispInteger(attno),LispNil)),
		      0		/* varslot */);

    tle = (LispValue)MakeList(resnode, varnode, -1);
    tlist = MakeList(tle, -1);
    set_func_tlist(func, tlist);
}
