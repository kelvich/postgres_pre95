/* ----------------------------------------------------------------
 *   FILE
 *	ylib.c
 *
 *   DESCRIPTION
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <strings.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/param.h>		/* for MAXPATHLEN */

#include "utils/log.h"
#include "parser/catalog_utils.h"
#include "nodes/pg_lisp.h"
#include "utils/exc.h"
#include "utils/excid.h"
#include "parser/io.h"
#include "utils/palloc.h"
#include "catalog/pg_aggregate.h"
#include "catalog/pg_type.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "parse.h"
#include "parser/parsetree.h"
#include "parser/parse_query.h"
#include "utils/builtins.h"
#include "lib/lisplist.h"
 
RcsId("$Header$");

LispValue parsetree;

extern bool Input_is_string;
extern bool Typecast_ok;

#define DB_PARSE(foo) 

void fixupsets();
void define_sets();

parser(str, l, typev, nargs)
     char *str;
     LispValue l;
     ObjectId *typev;
     int nargs;
{
    LispValue parse, savetree;
    int yyresult;

    init_io();

    /* Set things up to read from the string, if there is one */
    if (strlen(str) != 0) {
	StringInput = 1;
	TheString = (char *) palloc(strlen(str) + 1);
	bcopy(str,TheString,strlen(str)+1);
    }

    parser_init(typev, nargs);
    yyresult = yyparse();

    clearerr(stdin);
      
    if (yyresult) {	/* error */
	CAR(l) = LispNil;
	CDR(l) = LispNil;
	return(-1);
    }

    /* Fixing up sets calls the parser, so it reassigns the global
     * variable parsetree. So save the real parsetree.
     */
    savetree = parsetree;
    foreach (parse, savetree) {  /* savetree is really a list of parses */
	 fixupsets(CAR(parse));  /* find set definitions embedded in query */
    }

    CAR(l) = CAR(savetree);
    CDR(l) = CDR(savetree);

    if (savetree == NULL) {
	return(-1);
    } else {
	return(0);
    }
}

void
fixupsets(parse)
LispValue parse;
{
     if (parse == NULL)
	  return;
     if (atom(CAR(parse)))  /* utility */
	  return;
     if (LISPVALUE_INTEGER(CAR(CDR(CAR(parse)))) != APPEND)
	  return;
     define_sets (parse);
}

/* Recursively find all of the Consts in the parsetree.  Some of
 * these may represent a set.  The value of the Const will be the
 * query (a string) which defines the set.  Call SetDefine to define
 * the set, and store the OID of the new set in the Const instead.
 */
void
define_sets (clause)
     LispValue clause ;
{
     ObjectId setoid;
     Type t = type("oid");
     ObjectId typeoid = typeid(t);
     Size oidsize = tlen(t);
     bool oidbyval = tbyval(t);

	if (null (clause) ) {
	  return;
        } else if (IsA(clause,LispList)) {
	     define_sets(CAR(clause));
	     define_sets(CDR(clause));
	} else if (IsA(clause,Const)) {
	     if (get_constisnull((Const)clause) || 
                !get_constisset((Const)clause)) {
		  return;
	     }
	     setoid = SetDefine(get_constvalue((Const)clause),
			    get_id_typname(get_consttype((Const)clause)));
	     set_constvalue((Const)clause, setoid);
	     set_consttype((Const)clause,typeoid);
	     set_constlen((Const)clause,oidsize);
	     set_constbyval((Const)clause,oidbyval);
	} else if ( IsA(clause,Iter) ) {
	      define_sets(get_iterexpr((Iter)clause));
	} else if (single_node (clause)) {
	     return;
	} else if (or_clause (clause)) {
	     LispValue temp;
	     /* mapcan */
	     foreach (temp,get_orclauseargs(clause) ) {
		  define_sets(CAR(temp));
	     }
	} else if (is_funcclause (clause)) {
	     LispValue temp;
	     /* mapcan */
	     foreach(temp,get_funcargs(clause)) {
		  define_sets(CAR(temp));
	     }
	} else if (IsA(clause,ArrayRef)) {
	    define_sets(get_refassgnexpr((ArrayRef)clause));
	} else if (not_clause (clause)) {
	    define_sets (get_notclausearg (clause));
	} else if (is_clause (clause)) {
	    define_sets ((LispValue)get_leftop (clause));
	    define_sets ((LispValue)get_rightop (clause));
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
    char *type_string, type_string_2[16];
    int32 len;
    char *cp = NULL;
    char *const_string; 
    bool string_palloced = false;

    type_string = CString(CAR(typename));

    if (!Input_is_string && !Typecast_ok)
	elog(WARN,
	     "parser_typecast: cannot cast this expression to type \"%s\"",
	     type_string);
    
    if (CDR(typename) != LispNil) {
	    sprintf(type_string_2,"_%s", type_string);
	    tp = (Type) type(type_string_2);
    } else {
	    tp = (Type) type(type_string);
    }
    
    len = tlen(tp);

    switch ( CInteger(CAR(expr)) ) {
      case 23: /* int4 */
	const_string = (char *) palloc(256);
	string_palloced = true;
	sprintf(const_string,"%d", get_constvalue((Const)CDR(expr)));
	break;

      case 19: /* char16 */
	const_string = (char *) palloc(256);
	string_palloced = true;
	sprintf(const_string,"%s", get_constvalue((Const)CDR(expr)));
	break;

      case 18: /* char */
	const_string = (char *) palloc(256);
	string_palloced = true;
	sprintf(const_string,"%c", get_constvalue((Const)CDR(expr)));
	break;

      case 701:/* float8 */
	const_string = (char *) palloc(256);
	string_palloced = true;
	sprintf(const_string,"%f", get_constvalue((Const)CDR(expr)));
	break;

      case 25: /* text */
	const_string = DatumGetPointer(get_constvalue((Const)CDR(expr)));
	const_string = (char *) textout((struct varlena *)const_string);
	break;

      case 705: /* unknown */
        const_string = DatumGetPointer(get_constvalue((Const)CDR(expr)));
        const_string = (char *) textout((struct varlena *)const_string);
        break;

      default:
	elog(WARN,"unknown type %d", CInteger(CAR(expr)));
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
    
    adt = MakeConst ( typeid(tp), len, (Datum)lcp , 0, tbyvalue(tp), 
                      0 /* not a set */);

    if (string_palloced)
	pfree(const_string);

    return (lispCons(lispInteger(typeid(tp)), (LispValue)adt));
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
    
    adt = MakeConst ( (ObjectId)typeid(tp), (Size)len, (Datum)lcp , 0 , 
                      0/*was omitted*/, 0 /* not a set */);
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

#define ADD_TO_RT(rt_entry)     p_rtable = nappend1(p_rtable,rt_entry) 
List
ParseFunc ( funcname , fargs, curr_resno )
     char *funcname;
     List fargs;
     int *curr_resno;
{
    OID rettype = (OID)0;
    OID funcid = (OID)0;
    OID argrelid;
    LispValue i = LispNil;
    List first_arg_type = NULL;
    Name relname, oldname;
    Relation rd;
    ObjectId relid;
    int attnum;
    int nargs;
    Iter iter;
    Func funcnode;
    ObjectId oid_array[8];
    ObjectId *true_oid_array;
    OID argtype;
    Param f;
    int vnum;
    LispValue retval;
    LispValue setup_base_tlist();
    bool retset;
    bool exists;
    extern LispValue p_rtable;
    extern void make_arguments();
    bool attisset = false;
    ObjectId toid;

    if (fargs)
     {
	 if (CAR(fargs) == LispNil)
	   elog (WARN,"function %s does not allow NULL input",funcname);
	 first_arg_type = CAR(CAR(fargs));
     }

    /*
    ** check for projection methods: if function takes one argument, and 
    ** that argument is a relation, param, or PQ function returning a complex 
    ** type, then the function could be a projection.
    */
    if (length(fargs) == 1)
     {
	 if (lispAtomp(first_arg_type) && CAtom(first_arg_type) == RELATION)
	  {
	      /* this is could be a projection */
	      relname = (Name) CString(CDR(CAR(fargs)));
	      if( RangeTablePosn ( relname ,LispNil ) == 0 )
		ADD_TO_RT( MakeRangeTableEntry ((Name)relname,
						LispNil, 
						(Name)relname ));
	      oldname = relname;
	      relname = VarnoGetRelname(RangeTablePosn(oldname,0));
	      rd = heap_openr(relname);
	      relid = RelationGetRelationId(rd);
	      heap_close(rd);
	      if ((attnum = get_attnum(relid, (Name) funcname)) 
		  != InvalidAttributeNumber) {
		   /* If the attr isn't a set, just make a var for it.  If
		    * it is a set, treat it like a function and drop through.
		    */
/*		   if (!(attisset = get_attisset(relid, funcname))) {
*/
			return((LispValue)(make_var(oldname, funcname)));
/*		   }
*/
	      } else		/* drop through - attr is a set */;
	  }
	 else if (ISCOMPLEX(CInteger(first_arg_type)) &&
		  IsA(CDR(CAR(fargs)),Iter) && 
		  (argrelid = typeid_get_relid
		   ((int)(argtype=funcid_get_rettype
			  (get_funcid((Func)(CAR(get_iterexpr((Iter)CDR(CAR(fargs)))))))))))
	  {
	      /* the argument is a function returning a tuple, so funcname
		 may be a projection */
	      if ((attnum = get_attnum(argrelid, (Name) funcname)) 
		  != InvalidAttributeNumber)
	       {
		   /* add a tlist to the func node and return the Iter */
		   rd = heap_openr(tname(get_id_type(argtype)));
		   if (RelationIsValid(rd))
		    {
			relid = RelationGetRelationId(rd);
			relname = RelationGetRelationName(rd);
			heap_close(rd);
		    }
		   if (RelationIsValid(rd))
		    {
			iter = (Iter)CDR(CAR(fargs));
			set_func_tlist((Func)CAR(get_iterexpr(iter)), 
				       setup_tlist(funcname, argrelid));
			return(lispCons(lispInteger(att_typeid(rd,attnum)),iter));
		    }
		   else elog(WARN, 
			     "Function %s has bad returntype %d", 
			     funcname, argtype);
	       }
	      else		/* drop through */;
	  }
	 else if (ISCOMPLEX(CInteger(first_arg_type)) &&
		  IsA(CDR(CAR(fargs)),Var))
	 {
	      /* The argument is a set, so this is either a projection
	       * or a function call on this set.
	       */
	      attisset = true;
	      toid = (ObjectId)CInteger(first_arg_type);
	      rd = heap_openr(tname(get_id_type(toid)));
	      if (RelationIsValid(rd)) {
		   relname = RelationGetRelationName(rd);
		   heap_close(rd);
	      } else
		   elog(WARN,
			"Type %s is not a relation type",
			tname(get_id_type(toid)));
	      argrelid = typeid_get_relid(toid);
	      /* A projection contains either an attribute name or the
	       * word "all".
	       */
	      if (((attnum = get_attnum(argrelid, (Name) funcname)) 
		  == InvalidAttributeNumber) && strcmp(funcname, "all"))
	      {
		   elog(WARN, "Functions on sets are not yet supported");
	      }
	      else		/* drop through */;
	  }
	 else if (ISCOMPLEX(CInteger(first_arg_type)) &&
		  IsA(CADR(CAR(fargs)),Func) && 
		  (argrelid = typeid_get_relid
		   ((int)(argtype=funcid_get_rettype
			  (get_funcid((Func)CADR(CAR(fargs))))))))
	  {
	      /* the argument is a function returning a tuple, so funcname
		 may be a projection */
	      if ((attnum = get_attnum(argrelid, (Name) funcname)) 
		  != InvalidAttributeNumber)
	       {
		   /* add a tlist to the func node */
		   rd = heap_openr(tname(get_id_type(argtype)));
		   if (RelationIsValid(rd))
		    {
			relid = RelationGetRelationId(rd);
			relname = RelationGetRelationName(rd);
			heap_close(rd);
		    }
		   if (RelationIsValid(rd))
		    {
			funcnode = (Func)CADR(CAR(fargs));
			set_func_tlist(funcnode,
				       setup_tlist(funcname, argrelid));
			return(lispCons(lispInteger(att_typeid(rd,attnum)),
					CDR(CAR(fargs))));
		    }
		   else elog(WARN,
			     "Function %s has bad returntype %d", 
			     funcname, argtype);
	       }
	      else		/* drop through */;
	  }
	 else if (ISCOMPLEX(CInteger(first_arg_type)) &&
		  IsA(CADR(CAR(fargs)),Param))
	  {
	      /* If the Param is a complex type, this could be a projection */
	      f = (Param)CADR(CAR(fargs));
	      rd = heap_openr(tname(get_id_type(get_paramtype(f))));
	      if (RelationIsValid(rd)) 
	       {
		   relid = RelationGetRelationId(rd);
		   relname = RelationGetRelationName(rd);
		   heap_close(rd);
	       }
	      if (RelationIsValid(rd) && 
		  (attnum = get_attnum(relid, (Name) funcname))
	          != InvalidAttributeNumber)
	       {
		   set_param_tlist(f, setup_tlist(funcname, relid));
		   return(lispCons(lispInteger(att_typeid(rd, attnum)), f));
	       }
	      else		/* drop through */;
	  }
     }


    /*
    ** If we dropped through to here it's really a function (or a set, which
    ** is implemented as a function.)
    ** extract arg type info and transform relation name arguments into
    ** varnodes of the appropriate form.
    */

    bzero(&oid_array[0], 8 * sizeof(ObjectId));
    nargs=0;

    foreach ( i , fargs ) 
     {
	 List pair = CAR(i);
	 
	 if (lispAtomp(CAR(pair)) && CAtom(CAR(pair)) == RELATION)
	  {
	      relname = (Name)CString(CDR(pair));

	      /* get the range table entry for the var node */
	      vnum = RangeTablePosn(relname, 0);
	      if (vnum == 0) {
		  p_rtable = nappend1(p_rtable ,
				      MakeRangeTableEntry(relname, 0, relname));
		  vnum = RangeTablePosn (relname, 0);
	      }

	      /*
	       *  We have to do this because the relname in the pair
	       *  may have been a range table variable name, rather
	       *  than a real relation name.
	       */

	      relname = (Name) VarnoGetRelname(vnum);

	      rd = heap_openr(relname);
	      relid = RelationGetRelationId(rd);
	      heap_close(rd);
	      
	      /*
	       *  for func(relname), the param to the function
	       *  is the tuple under consideration.  we build a special
	       *  VarNode to reflect this -- it has varno set to the 
	       *  correct range table entry, but has varattno == 0 to 
	       *  signal that the whole tuple is the argument.
	       */
	      toid = typeid(type(relname));		   
	      rplacd(pair, (LispValue)MakeVar(vnum, 0, toid,
					      lispCons(lispInteger(vnum),
						       lispCons(lispInteger(0),
								LispNil)),
					      0 /* varslot */));
	  }
	 else if (!attisset)  /* set functions don't have parameters */
	  {
	      toid = CInteger(CAR(pair));
	  }

	 oid_array[nargs++] = toid;
     }

    /*
     *  func_get_detail looks up the function in the catalogs, does
     *  disambiguation for polymorphic functions, handles inheritance,
     *  and returns the funcid and type and set or singleton status of
     *  the function's return value.  it also returns the true argument
     *  types to the function.  if func_get_detail returns true,
     *  the function exists.  otherwise, there was an error.
     */
    if (attisset) { /* we know all of these fields already */
	 /* We create a funcnode with a placeholder function SetEval.
	  * SetEval() never actually gets executed.  When the function
	  * evaluation routines see it, they use the funcid projected
	  * out from the relation as the actual function to call.
	  * Example:  retrieve (emp.mgr.name)
	  * The plan for this will scan the emp relation, projecting
	  * out the mgr attribute, which is a funcid.  This function
	  * is then called (instead of SetEval) and "name" is projected
	  * from its result.
	  */
	 funcid = SetEvalRegProcedure;
	 rettype = toid;
	 retset = true;
	 true_oid_array = oid_array;
	 exists = true;
    } else {
	 exists = func_get_detail(funcname, nargs, oid_array, &funcid,
				  &rettype, &retset, &true_oid_array);
    }

    if (!exists)
	elog(WARN, "no such attribute or function %s", funcname);

    /* got it */
    funcnode = MakeFunc(funcid, rettype, false, 0, NULL, LispNil, LispNil);

    /* perform the necessary typecasting */
    make_arguments(nargs, fargs, oid_array, true_oid_array);

    /* remove the argtypes from fargs */
    foreach(i, fargs) {
	rplaca(i, CDR(CAR(i)));
    }

    /*
     *  for functions returning base types, we want to project out the
     *  return value.  set up a target list to do that.  the executor
     *  will ignore these for c functions, and do the right thing for
     *  postquel functions.
     */

    if (typeid_get_relid(rettype) == InvalidObjectId)
	set_func_tlist(funcnode, (LispValue)setup_base_tlist(rettype));

    /* For sets, we want to make a targetlist to project out this
     * attribute of the set tuples.
     */
    if (attisset) {
	 if (!strcmp(funcname, "all")) {
	      set_func_tlist(funcnode, ExpandAll(relname, curr_resno));
	 } else {
	      set_func_tlist(funcnode, (LispValue)setup_tlist(funcname,argrelid));
	      rettype = find_atttype(argrelid, funcname);
	 }
    }

    retval = lispCons((LispValue) funcnode, fargs);

    /*
     *  if the function returns a set of values, then we need to iterate
     *  over all the returned values in the executor, so we stick an
     *  iter node here.  if it returns a singleton, then we don't need
     *  the iter node.
     */

    if (retset)
	retval = (LispValue) MakeIter(retval);

    /* store type info in the return value for use by the type checker */
    retval = lispCons (lispInteger(rettype), retval);

    return(retval);
}

List
ParseAgg(aggname, query, tlist)
    char *aggname;
    List query;
    List tlist;
{
    ObjectId fintype;
    ObjectId basetype;
    ObjectId vartype;
    ObjectId xfn1;
    Form_pg_aggregate aggform;
    List list = LispNil;
    char *keyword = "agg";
    HeapTuple theAggTuple;
    tlist = CADR(tlist);

    if (!query) {
	elog(WARN,"aggregate %s called without arguments", aggname);
    }
    theAggTuple = SearchSysCacheTuple(AGGNAME, aggname, 0, 0, 0); 
    if (!HeapTupleIsValid(theAggTuple)) {
        elog(WARN, "aggregate %s does not exist", aggname);
    }

    aggform = (Form_pg_aggregate) GETSTRUCT(theAggTuple);
    fintype = aggform->aggfinaltype;
    xfn1 = aggform->aggtransfn1;

    /* only aggregates with transfn1 need a base type */
    if (ObjectIdIsValid(xfn1)) {	
	basetype = aggform->aggbasetype;
	vartype = get_vartype((Var)tlist);
	if (basetype != vartype) {
	    Type tp, get_id_type();
	    char p1[16], p2[16];

	    tp = get_id_type(basetype);
	    strncpy(p1, tname(tp), 16);
	    tp = get_id_type(vartype);
	    strncpy(p1, tname(tp), 16);
	    elog(NOTICE, "Aggregate type mismatch:");
	    elog(WARN, "%s works on %s, not %s", aggname, p1, p2);
        }
    }

    list = (LispValue)MakeList(lispInteger(fintype), lispName(keyword),
			       lispName(aggname), query, tlist, -1);
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
    extern LispValue p_rtable;

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
	    
	    flags = lispCons(lispInteger((long) NULL),LispNil); /* time */
	    
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

/*
** HandleNestedDots --
**    Given a nested dot expression (i.e. (relation func ... attr), build up
** a tree with of Iter and Func nodes.
*/
LispValue HandleNestedDots(dots, curr_resno)
     List dots;
     int *curr_resno;
{
    List mutator_iter;
    LispValue retval = LispNil;

    if (IsA(CAR(dots),Param))
      retval = 
	ParseFunc(CString(CADR(dots)),
		  lispCons(MakeList
			   (lispInteger(get_paramtype((Param)CAR(dots))),
			    CAR(dots), -1),
			   LispNil),
		  curr_resno);
    else
      retval = ParseFunc(CString(CADR(dots)), 
			 lispCons(lispCons(lispAtom("relation"), 
					   CAR(dots)),
				  LispNil),
			 curr_resno);

    foreach (mutator_iter, CDR(CDR(dots))) {
/*	 if (lispAtomp(CAR(mutator_iter)) && 
               (LISPVALUE_INTEGER(CAR(mutator_iter)) == ALL))
	      retval = ParseFunc("all", lispCons(retval, LispNil));
	 else
*/	      retval = ParseFunc(CString(CAR(mutator_iter)), 
                                         lispCons(retval, LispNil),
				 curr_resno);
    }

    return(retval);
}

/*
** setup_tlist --
**     Build a tlist that says which attribute to project to.
**     This routine is called by ParseFunc() to set up a target list
**     on a tuple parameter or return value.  Due to a bug in 4.0,
**     it's not possible to refer to system attributes in this case.
*/
LispValue setup_tlist(attname, relid)
     Name attname;
     ObjectId relid;
{
    TLE tle;
    LispValue tlist;
    Resdom resnode;
    Var varnode;
    ObjectId typeid;
    int attno;
    bool attisset;

    attno = get_attnum(relid, attname);
    if (attno < 0)
	elog(WARN, "cannot reference attribute %s of tuple params/return values for functions", attname);

    typeid = find_atttype(relid, attname);
    attisset = get_attisset(relid, attname);
    resnode = MakeResdom(1, typeid, 
			 attisset ? false : ISCOMPLEX(typeid),
			 tlen(get_id_type(typeid)),
			 get_attname(relid, attno),
			 NULL	/* reskey   */,
			 NULL	/* reskeyop */,
			 0	/* resjunk  */);
    varnode = MakeVar(-1, attno, typeid,
		      lispCons(lispInteger(-1),
			       lispCons(lispInteger(attno),LispNil)),
		      0		/* varslot */);

    tle = (LispValue)MakeList(resnode, varnode, -1);
    return(MakeList(tle, -1));
}

/*
** setup_base_tlist --
**	Build a tlist that extracts a base type from the tuple
**	returned by the executor.
*/
LispValue
setup_base_tlist(typeid)
    ObjectId typeid;
{
    TLE tle;
    LispValue tlist;
    Resdom resnode;
    Var varnode;

    resnode = MakeResdom(1, typeid, ISCOMPLEX(typeid), 
			 tlen(get_id_type(typeid)),
			 (Name) "<noname>", NULL, NULL, 0);
    varnode = MakeVar(-1, 1, typeid,
			lispCons(lispInteger(-1),
				 lispCons(lispInteger(1), LispNil)),
			0);

    tle = (LispValue)MakeList(resnode, varnode, -1);
    return (MakeList(tle, -1));
}

/*
** make_arguments --
**   Given the number and types of arguments to a function, and the 
**   actual arguments and argument types, do the necessary typecasting.
*/
void
make_arguments(nargs, fargs, input_typeids, function_typeids)
     int nargs;
     List fargs;
     ObjectId *input_typeids;
     ObjectId *function_typeids;
{
    /*
     * there are two ways an input typeid can differ from a function typeid :
     * either the input type inherits the function type, so no typecasting is
     * necessary, or the input type can be typecast into the function type.
     * right now, we only typecast unknowns, and that is all we check for.
     */

    List current_fargs;
    LispValue current_arg;
    int i;

    for (i=0, current_fargs = fargs;
	 i<nargs;
	 i++, current_fargs = CDR(current_fargs)) {
	if (input_typeids[i] == UNKNOWNOID && function_typeids[i] != 0) {
	    rplaca(current_fargs,
		   lispCons(lispInteger(function_typeids[i]),
			    parser_typecast2(CAR(current_fargs),
					 get_id_type(function_typeids[i]))));
	}
    }
}
