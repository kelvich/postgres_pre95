/**********************************************************************

  parse_query.c

  -   All new code for Postquel, Version 2

  take an "optimizable" stmt and make the query tree that
  the planner requires.

  Synthesizes the lisp object via routines in lisplib/lispdep.c
  $Header$
 **********************************************************************/

#include <ctype.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/tqual.h"
#include "parser/parse.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "utils/rel.h" 		/* Relation stuff */

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

#include "catalog/syscache.h"
#include "catalog_utils.h"
#include "parse_query.h"
#include "utils/lsyscache.h"

extern LispValue parser_ppreserve();
extern int Quiet;

static ObjectId *param_type_info;
static int pfunc_num_args;

LispValue
ModifyQueryTree(query,priority,ruletag)
     LispValue query,priority,ruletag;
{
    return(lispString("rule query")); /* XXX temporary */
}

Name
VarnoGetRelname( vnum )
     int vnum;
{
    int i;
    extern LispValue p_rtable;
    LispValue temp = p_rtable;
    for( i = 1; i < vnum ; i++) 
		temp = CDR(temp);
    return((Name)CString(CAR(CDR(CAR(temp)))));
}

LispValue
any_ordering_op( varnode )
	Var varnode;
{
    int vartype;
    Operator order_op;
    OID order_opid;

    Assert(! null(varnode));

    vartype = get_vartype(varnode);
    order_op = oper("<",vartype,vartype);
    order_opid = (OID)oprid(order_op);

    return(lispInteger(order_opid));

}

Resdom
find_tl_elt ( varname ,tlist )
     char *varname;
     List tlist;
{
    LispValue i = LispNil;

    foreach ( i, tlist ) {
	Resdom resnode = (Resdom)CAAR(i);
	/* Var tvarnode = (Var) CADR(CAR(i));*/
	Name resname = get_resname(resnode );

	if ( ! strcmp ( resname, varname ) ) {
	    return ( resnode );
	} 
    }
    return ( (Resdom) NULL );
}

/**************************************************
  MakeRoot
  
  lispval :
      ( NumLevels NIL )
      |   ( NumLevels RETRIEVE ( PORTAL "result" ) )
      |   ( NumLevels RETRIEVE ( RELATION "result" ) )
      |   ( NumLevels OtherOptStmt rt_index )
      
  where rt_index = index into range_table
  
  **************************************************/

LispValue
MakeRoot(NumLevels,query_name,result,rtable,priority,ruleinfo,unique_flag,
	 sort_clause,targetlist)
     int NumLevels;
     LispValue query_name,result;
     LispValue rtable,priority,ruleinfo;
     LispValue unique_flag,sort_clause;
     LispValue targetlist;
{
    LispValue newroot = LispNil;
    LispValue new_sort_clause = LispNil;
    LispValue sort_clause_elts = LispNil;
    LispValue sort_clause_ops = LispNil;
    LispValue i = LispNil;

    foreach (i, sort_clause) {

	LispValue 	one_sort_clause = CAR(i);
	Resdom 		one_sort_elt = NULL;
	String	 	one_sort_op = NULL;
	int 		sort_elt_type = 0;

	Assert ( consp (one_sort_clause) );

	one_sort_elt = find_tl_elt(CString(CAR(one_sort_clause)),targetlist );

	if ( null (one_sort_elt))
	  elog(WARN,"The field being sorted by must appear in the target list");

	if ( ! null ( CADR ( one_sort_clause )))
	  one_sort_op = CString(CADR(one_sort_clause));
	else
	  one_sort_op = "<";

	/* Assert ( tlelementP (one_sort_elt) ); */

	sort_clause_elts = nappend1(sort_clause_elts, (LispValue)one_sort_elt);
	sort_elt_type = get_restype(one_sort_elt);

	sort_clause_ops = nappend1( sort_clause_ops,
				   lispInteger(oprid( oper(one_sort_op,
							   sort_elt_type,
							   sort_elt_type ) )));
    }
        
    if ( unique_flag != LispNil ) {
	/* concatenate all elements from target list
	   that are not already in the sortby list */
        foreach (i,targetlist) {
	    LispValue tlelt = CAR(i);
	    if ( ! member ( CAR(tlelt) , sort_clause_elts ) ) {
		sort_clause_elts = nappend1 ( sort_clause_elts, CAR(tlelt)); 
		sort_clause_ops = nappend1 ( sort_clause_ops, 
			any_ordering_op((Var)CADR(tlelt) ));
	    }
	}    
    }

    if ( sort_clause_elts != LispNil && sort_clause_ops != LispNil ) {
        new_sort_clause = lispCons ( lispAtom("sort"),
			         lispCons (sort_clause_elts, 
			                   lispCons (sort_clause_ops, 
						     LispNil ))); 
    }

    newroot = lispCons (new_sort_clause,LispNil);
    newroot = lispCons (unique_flag,newroot );
    newroot = lispCons( ruleinfo , newroot );
    newroot = lispCons( priority , newroot );
    newroot = lispCons( rtable , newroot );
    newroot = lispCons( result , newroot );
    newroot = lispCons( query_name, newroot );
    newroot = lispCons( lispInteger( NumLevels ) , newroot );

    return (newroot) ;
}


/**************************************************
  
  MakeRangeTableEntry :
  INPUT:
  <relname>
  OUTPUT:
  ( <relname> <relnum> <time> <flags> <rulelocks> )

  NOTES:
    (eq CAR(options) , Trange)
    (eq CDR(optioons) , flags )

 **************************************************/

LispValue
MakeRangeTableEntry( relname , options , refname)
     Name relname;
     List options;
     Name refname;
{
    LispValue entry 	= LispNil,
    RuleLocks 	= LispNil,
    Flags 	= LispNil,
    TRange	= LispNil,
    RelOID	= LispNil;
    Relation relation;
    
    relation = heap_openr(relname);
    if (relation == NULL) {
	elog(WARN,"heap_openr on %s failed\n",relname);
    }
    
    /* RuleLocks - for now, always empty, since preplanner fixes */
    
    /* Flags - zero or more from archive,inheritance,union,version
               or recursive (transitive closure) */

    if (consp(options)) { 
      Flags = CDR(options);
      TRange = CAR(options);
    } else {
      Flags = LispNil;
      TRange = lispInteger(0);
    }

    /* RelOID */
    RelOID = lispInteger ( RelationGetRelationId (relation ));
    
    entry = lispCons (lispString(&relname->data[0]), 
		      lispCons(RelOID,
			       lispCons(TRange,
					    lispCons(Flags,
						     lispCons(RuleLocks,
							      LispNil)))));
    /*
     * close the relation we're done with it for now.
     */
    heap_close(relation);
    return ( lispCons ( lispString(&refname->data[0]), entry ));
}

/**************************************************

  ExpandAll

  	- makes a list of attributes
	- assumes reldesc caching works
  **************************************************/

LispValue
ExpandAll(relname,this_resno)
     Name relname;
     int *this_resno;
{
	Relation rdesc;
	LispValue tall = LispNil;
	Resdom resnode;
	Var varnode;
	LispValue temp = LispNil;
	int i,maxattrs,first_resno;
	int type_id,type_len,vnum;
	Name physical_relname;

	first_resno = *this_resno;
	
	/* printf("\nExpanding %s.all\n",relname); */
	vnum = RangeTablePosn (relname,0);
	if ( vnum == 0 ) {
		p_rtable = nappend1 ( p_rtable,
				     MakeRangeTableEntry (relname, 
							  0 ,  relname));
		vnum = RangeTablePosn (relname,0);
	}

	physical_relname = VarnoGetRelname(vnum);


	rdesc = heap_openr(physical_relname);
	
	if (rdesc == NULL ) {
	    elog(WARN,"Unable to expand all -- heap_openr failed ");
	    return( LispNil );
	}
	maxattrs = RelationGetNumberOfAttributes(rdesc);

	for ( i = maxattrs-1 ; i > -1 ; --i ) {
		char *attrname;

		attrname = (char *) palloc (sizeof(NameData)+1);
		strcpy(attrname, (char *)(&rdesc->rd_att.data[i]->attname));
		temp = make_var ( relname, (Name) attrname );
		varnode = (Var)CDR(temp);
		type_id = CInteger(CAR(temp));
		type_len = (int)tlen(get_id_type(type_id));
		
		resnode = MakeResdom((AttributeNumber) i + first_resno, (ObjectId)type_id, (Size)type_len,(Name)
					 attrname, (Index)0, (OperatorTupleForm)0, 0 );
/*
		tall = lispCons(lispCons(resnode, lispCons(varnode, LispNil)),
				tall);
*/
		tall = lispCons(lispCons((LispValue)resnode,
					 lispCons((LispValue)varnode,LispNil)),
				tall);
	}

	/*
	 * Close the reldesc - we're done with it now
	 */
	heap_close(rdesc);
	*this_resno = first_resno + maxattrs;
	return(tall);
}

LispValue
MakeTimeRange( datestring1 , datestring2 , timecode )
     LispValue datestring1, datestring2;
     int timecode; /* 0 = snapshot , 1 = timerange */
{
	TimeQual	qual;
	Time t1,t2;

	switch (timecode) {
		case 0:
			if (datestring1 == LispNil) {
				elog(WARN, "MakeTimeRange: bad snapshot arg");
			}
			t1 = nabstimein(CString(datestring1));
			if (!AbsoluteTimeIsValid(t1)) {
				elog(WARN, "bad snapshot time: \"%s\"",
					CString(datestring1));
			}
			qual = TimeFormSnapshotTimeQual(t1);
			break;
		case 1:
			if (datestring1 == LispNil) {
				t1 = InvalidAbsoluteTime;
			} else {
				t1 = nabstimein(CString(datestring1));
				if (!AbsoluteTimeIsValid(t1)) {
					elog(WARN,
						"bad range start time: \"%s\"",
						CString(datestring1));
				}
			}
			if (datestring2 == LispNil) {
				t2 = InvalidAbsoluteTime;
			} else {
				t2 = nabstimein(CString(datestring2));
				if (!AbsoluteTimeIsValid(t2)) {
					elog(WARN,
						"bad range end time: \"%s\"",
						CString(datestring2));
				}
			}
			qual = TimeFormRangedTimeQual(t1,t2);
			break;
		default:
			elog(WARN, "MakeTimeRange: internal parser error");
	}
	return(lispInteger((int)qual));
}

LispValue 
make_op(op,ltree,rtree)
     LispValue op,ltree,rtree;
{
	Type ltype,rtype;
	Operator temp;
	OperatorTupleForm opform, optemp;
	Oper newop;
	LispValue left,right;
	LispValue t1;
	ObjectId fid;
	
	left = CDR(ltree);
	right = CDR(rtree);
	if (! lispNullp(left)) 
	  ltype = get_id_type ( CInteger ( CAR(ltree) ));
	if (! lispNullp(right))
	  rtype = get_id_type ( CInteger ( CAR(rtree) ) );
	/* XXX - do we want auto type casting ?
	   if ( !strcmp(CString(op),"=") &&
	     (ltype != rtype) )
	  */
	  
	if ( lispNullp(left) ) {
		/* right operator */
	 	temp = right_oper( CString( op ), typeid(rtype));
	} else if ( lispNullp(right)) {
		/* left operator */
		temp = left_oper( CString( op ), typeid(ltype) );
	} else {
		/* binary operator */
                if (typeid(rtype) == typeid(type("unknown")))
                {
                    /* trying to find default type for the right arg... */
                    temp = (Operator) oper_default(CString(op),typeid(ltype));
                    /* now, we have the default type, typecast */
                    if(temp){
                        Datum val;
                        val = textout((struct varlena *)get_constvalue((Const)right));
                        optemp = (OperatorTupleForm) GETSTRUCT(temp);
                        right = (LispValue) MakeConst(optemp->oprright,
					tlen(get_id_type(optemp->oprright)),
			     (Datum)fmgr(typeid_get_retinfunc(optemp->oprright),
					     val),
					false, true /*XXX was omitted */);
                     } else 
					 op_error(CString(op), typeid(ltype), typeid(rtype));
                }
                else
                   temp = oper(CString(op),typeid(ltype), typeid ( rtype ));
	}
	opform = (OperatorTupleForm) GETSTRUCT(temp);

	newop = MakeOper ( oprid(temp),    /* opno */
			    InvalidObjectId,	/* opid */
			    0 ,       	     /* operator relation level */
			    opform->oprresult, /* operator result type */
			    NULL, NULL);
	t1 = lispCons ( (LispValue)newop , lispCons (left ,
					     lispCons (right,LispNil)));
	return ( lispCons (lispInteger ( opform->oprresult ) ,
			   t1 ));
			   
} /* make_op */

/*
 * make_concat_var
 * 
 * for the relational "concatenation" operator
 * a real relational union is too expensive since 
 * a union b = a + b - ( a intersect b )
 */

List 
make_concat_var ( list_of_varnos , attid , vartype, vardotfields, 
		  vararraylist )
     List list_of_varnos;
     int attid;
     int vartype;
     List vardotfields;
     List vararraylist;
{
    List retval = NULL;
    List temp = NULL;
    Var varnode;

    foreach ( temp , list_of_varnos ) {
	LispValue each_varno = CAR(temp);
	int vnum;

	vnum = CInteger(each_varno);
	varnode = MakeVar (vnum , attid ,
			   vartype , vardotfields , vararraylist ,
			   lispCons(lispInteger(vnum),
				    lispCons(lispInteger(attid),LispNil)),
			   0 );
	retval = lispCons ( (LispValue)varnode , retval );
    }
    retval = lispCons ( lispAtom ( "union" ), retval );
    return(retval);
}

    

/**********************************************************************
  make_var

  - takes the attribute and figures out the 
  info necessary for constructing a varnode

  attribute = (relname . attrname)

  - returns a type,varnode pair suitable for use in arith-expr's
  to get just the varnode, strip the type off (use CDR(make_var ..) )
 **********************************************************************/

LispValue
make_var ( relname, attrname)
     Name relname, attrname;
{
    Var varnode;
    int vnum, attid, vartype;
    LispValue vardotfields;
    Type rtype;
    Relation rd;
    extern LispValue p_rtable;
    extern int p_last_resno;
    extern List RangeTablePositions();
    List vararraylist = LispNil;
    List multi_varnos = RangeTablePositions ( relname , 0 );

    vnum = RangeTablePosn ( relname,0) ;
    /* if (!Quiet)
      printf("vnum = %d\n",vnum);  */
    if (vnum == 0) {
	p_rtable = nappend1 (p_rtable ,
			     MakeRangeTableEntry ( relname , 0 , relname) );
		vnum = RangeTablePosn (relname,0);
	/* printf("vnum = %d\n",vnum); */
	relname = VarnoGetRelname(vnum);
    } else {
	relname = VarnoGetRelname( vnum );
    }
    
    /* if (!Quiet)
      printf("relname to open is %s",relname); */
    
    rd = amopenr ( relname );
    attid = varattno (rd , attrname );
    vartype = att_typeid ( rd , attid );
    rtype = get_id_type(vartype);
    vardotfields = LispNil;                          /* XXX - fix this */

    varnode = MakeVar (vnum , attid ,
                       vartype , vardotfields , vararraylist ,
                       lispCons(lispInteger(vnum),
                                lispCons(lispInteger(attid),LispNil)), 0);

    /*
     * close relation we're done with it now
     */
    amclose(rd);
    /*
     * for now at least, attributes of concatenated relations will not have
     * procedural fields or arrays. This can be changed later.
     * We also assume that they have identical schemas
     */

    if ( length ( multi_varnos ) > 1 )
      return ( lispCons ( lispInteger ( typeid (rtype)),
			 make_concat_var ( multi_varnos , attid , vartype,
					   LispNil, LispNil )));

    return ( lispCons ( lispInteger ( typeid (rtype ) ),
		       (LispValue)varnode ));
}
/**********************************************************************
 make_array_ref_var

 - makes an varnode cooresponding to an array de-reference.  Makes
 sure to find out what type the array reference is by looking at the
 "element" field of the relation descriptor.  Otherwise it is similar
 to make_var above.

 **********************************************************************/

LispValue
make_array_ref_var( relname, attrname, indirect_list)
     Name relname, attrname;
     List indirect_list;  /* has indirection - e.g. arrayname[i][j][k] */ 
{
    Var varnode;
    int vnum, attid, typearray;
    LispValue vardotfields;
    Type rtype;
    Relation rd;
    extern LispValue p_rtable;
    extern int p_last_resno;
    HeapTuple type_tuple;
    TypeTupleForm type_struct_array, type_struct_element;
    List vararraylist = LispNil;
    bool done = false;
    bool entering = true;
    int indirect_num;
    
    vnum = RangeTablePosn ( relname,0) ;
    if (vnum == 0) {
	p_rtable = nappend1 (p_rtable ,
			     MakeRangeTableEntry ( relname , 0 , relname) );
		vnum = RangeTablePosn (relname,0);
	relname = VarnoGetRelname(vnum);
    } else {
	relname = VarnoGetRelname( vnum );
    }
    
    rd = amopenr ( relname );
    attid = varattno (rd , attrname );
    typearray = att_typeid ( rd , attid );
    vardotfields = LispNil;                          /* XXX - fix this */

    type_tuple = SearchSysCacheTuple(TYPOID,
				     typearray,
				     NULL,
				     NULL,
				     NULL);
    
    if (!HeapTupleIsValid(type_tuple)) {
	elog(WARN, "make_array_ref_var: Cache lookup failed for type %d\n",
	     typearray);
	return LispNil;
    }


    /* ----------------
     *   get the array type struct from the type tuple
     * ----------------
     */
    type_struct_array = (TypeTupleForm) GETSTRUCT(type_tuple);

    if (type_struct_array->typelem == InvalidObjectId)
    {
	elog(WARN, "make_array_ref_var: %s is not an array", attrname);
    }
   
    while (!done) {
    	type_tuple = SearchSysCacheTuple(TYPOID,
					 (type_struct_array)->typelem,
					 NULL,
					 NULL,
					 NULL);
    
    	if (!HeapTupleIsValid(type_tuple)) {
		elog(WARN, 
		"make_array_ref_var: Cache lookup failed for type %d\n",
		     typearray);
		return LispNil;
    	}
    	type_struct_element = (TypeTupleForm) GETSTRUCT(type_tuple);

	indirect_num = CInteger(CAR(indirect_list));
	indirect_list = CDR(indirect_list);

	vararraylist = nappend1(vararraylist, (LispValue)
				MakeArray((type_struct_array)->typelem,
					  (type_struct_element)->typlen,
					  (type_struct_element)->typbyval,
					  indirect_num,
					  0,
					  (type_struct_array)->typlen));

	if (indirect_list != NULL &&
	    type_struct_element->typelem == InvalidObjectId)
	{
		elog(WARN, "make_array_ref_var: too many levels of []'s for %s",
		     attrname);
	}
	else if (indirect_list == NULL ||
		 type_struct_element->typelem == InvalidObjectId)
	{
		done = true;
	}
	else
	{
		type_struct_array = type_struct_element;
	}
    }

    varnode = MakeVar (vnum , attid ,
		       type_struct_array->typelem , vardotfields ,
		       vararraylist ,
		       lispCons(lispInteger(vnum),
				lispCons(lispInteger(attid),LispNil)), 0);
    
    /*
     * close relation - we're done with it now
     */
    amclose(rd);
    rtype = get_id_type((type_struct_array)->typelem);
    return ( lispCons ( lispInteger ( typeid (rtype ) ), (LispValue)varnode ));
}



/**********************************************************************
  SkipToFromList

  - employ lookahead to see if there is indeed a from_list
  ahead of us.  If so, skip to it and continue processing

 **********************************************************************/

static char tlist_buf[1024];
static int end_tlist_buf = 0;
static char *target_list_place;
static char *from_list_place;

SkipForwardToFromList()
{
        LispValue next_token;
        extern char *Ch;
        char *temp = Ch;

        while ((int)(next_token=(LispValue)yylex()) > 0 &&
                next_token != (LispValue)FROM )
          ; /* empty while */

        if ((int)next_token <= 0 )
                Ch = temp;

        if (next_token == (LispValue) FROM ) {
                Ch -= 4;
                from_list_place = Ch;
                target_list_place = temp;
        }
}

LispValue
SkipBackToTlist()
{
	extern char yytext[];
	extern LispValue yychar;
        char *temp = yytext;
	extern int yyleng;
	int i;

	/* need to put the token after the target_list back first */
	temp = yytext;
	if((yychar == (LispValue)WHERE) || (yychar == (LispValue)SORT)){ 
		for (i = yyleng; i > -1 ; -- i ) {
			unput (yytext[i]);
		}
	}

	bcopy(Ch,from_list_place,strlen(Ch) + 1 );
	Ch = target_list_place;
	return(LispNil);
}

LispValue
SkipForwardPastFromList()
{
return(LispNil);	
}

StripRangeTable()
{
	LispValue temp;
	temp = p_rtable;
	
	while(! lispNullp(temp) ) {
	    char *from_name = CString(CAR(CAR(temp)));

	    /*
	     * for current or new, we need
	     * a marker to be present
	     */

	    if ((!strcmp ( from_name, "*CURRENT*")) ||
		(!strcmp ( from_name, "*NEW*")) )
	      CAR(CDR(CAR(temp))) = CAR(CAR(temp));
	    
	    CAR(temp) = CDR (CAR (temp));
	    temp = CDR(temp);
	}
}

/**********************************************************************

  make_const

  - takes a lispvalue, (as returned to the yacc routine by the lexer)
  extracts the type, and makes the appropriate type constant
  by invoking the (c-callable) lisp routine c-make-const
  via the lisp_call() mechanism

  eventually, produces a "const" lisp-struct as per nodedefs.cl
  
 **********************************************************************/
LispValue
make_const( value )
     LispValue value;
{
	Type tp;
	LispValue temp;
	Datum val;

	switch( value->type ) {
	  case PGLISP_INT:
	    tp = type("int4");
	    val = Int32GetDatum(value->val.fixnum);
	    break;
		
	  case PGLISP_ATOM:
	    tp = type("char");
	    val = PointerGetDatum(value->val.name);
	    break;
	    
	  case PGLISP_FLOAT:
	    {
		float64 dummy;
		tp = type("float8");
		 
		dummy = (float64)palloc(sizeof(float64data));
		*dummy = value->val.flonum;
		
		val = Float64GetDatum(dummy);
	    }
	    break;
	    
	  case PGLISP_STR:
              tp = type("unknown"); /* unknown for now, will be type coerced */
              val = PointerGetDatum(textin(value->val.str));
            break;
	    
	  default: 
	    elog(NOTICE,"unknown type : %d\n", value->type );
	    /* null const */
	    return ( lispCons (LispNil , 
			       (LispValue)MakeConst ( (ObjectId)0 , (Size)0 , 
					  (Datum)LispNil , 1, 0/*ignored*/ )) );
	}

	temp = lispCons (lispInteger ( typeid (tp)) ,
			  (LispValue)MakeConst(typeid( tp ), tlen( tp ),
				    val , false, tbyval(tp) ));
/*	lispDisplay( temp , 0 );*/
	return (temp);
	
}
LispValue
parser_ppreserve(temp)
     char *temp;
{
    return((LispValue)temp);
}

/*-------------------------------------------------------------------
 *
 * make_param
 * Creates a Param node. This routine is called when a 'define rule'
 * statement contains references to the 'current' and/or 'new' tuple.
 * 
 * paramKind:
 *	One of the possible param kinds (PARAM_NEW, PARAM_OLD, etc)
 * 	(see lib/H/primnodes.h)
 * relationName:
 * 	the name of the relation where 'attrName' belongs.
 *	this is not stored anywhere in the Param node, but we
 * 	need it in order to infer the type of the parameter
 * 	and to do some checking....
 * attrName:
 *	the name of the attribute whose value will be substituted
 *	(at rule activation time) in the place of this param
 * 	node.
 */
LispValue
make_param(paramKind, relationName, attrName)
int paramKind;
char *relationName;
char *attrName;
{
    LispValue result;
    Relation relation;
    int attrNo;
    ObjectId attrType;
    Name name;
    Param paramNode;

    /*
     * open the relation
     */
    relation = heap_openr(relationName);
    if (relation == NULL) {
	elog(WARN,"make_param: can not open relation '%s'",relationName);
    }
    
    /*
     * find the attribute number and type
     */
    attrNo = varattno(relation, attrName);
    attrType = att_typeid(relation, attrNo);

    /*
     * create the Param node
     */
    name = (Name) palloc(sizeof(NameData));
    if (name == NULL) {
	elog(WARN, "make_param: out of memory!\n");
    }
    strcpy(&(name->data[0]), attrName);

    paramNode = MakeParam(paramKind,	/* paramkind */
			attrNo,		/* paramid - i.e. attrno */
			name,		/* paramname */
			attrType);	/* paramtype */
    
    /*
     * close the relation - we're done with it now
     */
    heap_close(relation);

    /*
     * form the final result (a list of the parameter type and
     * the Param node)
     */
    result = lispCons(lispInteger(attrType), (LispValue)paramNode);
    return(result);
		    
}
/*
 * param_type_init()
 *
 * keep enough information around fill out the type of param nodes
 * used in postquel functions
 * 
 * NOTE: the rule system uses param nodes for something totally different.
 *       hopefully we won't collide
 */

void param_type_init(def_list)
    List def_list;
{
    int nargs,y=0;
    List i,x,args = LispNil;

    nargs = length(def_list);
    def_list = (List) lispCopy(def_list);

    if (nargs)
	param_type_info = (ObjectId *) palloc(sizeof(ObjectId)*nargs);

    else {
	    pfunc_num_args = 0;
	    param_type_info = NULL;
	    return;
	}
    foreach (i, def_list) {
	List t = CAR(CAR(i));

	if (!lispStringp(t))
	    elog(WARN, "DefinePFunction: arg type = ?");
	args = nappend1(args, t);
    }
    foreach (x, args) {
	List t = CAR(x);
	ObjectId toid;
	int defined;
	toid = TypeGet(CString(t), &defined);
	if (!ObjectIdIsValid(toid)) {
	    elog(WARN, "ProcedureDefine: arg type '%s' is not defined",
		 CString(t));
	}
	param_type_info[y++]= toid;
    }
    pfunc_num_args = nargs;
}
ObjectId param_type(t)
     int t;
{
    if ((t >pfunc_num_args) ||(t ==0)) return InvalidObjectId;
    return param_type_info[t-1];
}


/*--------------------------------------------------------------------
 * FindVarNodes
 *
 * Find all the Var nodes of a parse tree
 *
 * NOTE #1: duplicates are not removed
 * NOTE #2: we do not make copies of the Var nodes.
 *
 *--------------------------------------------------------------------
 */
LispValue
FindVarNodes(list)
LispValue list;
{
    List i, t;
    List result;
    List tempResult;

    result = LispNil;

    foreach (i , list) {
	List temp = CAR(i);
	if ( temp && IsA (temp,Var) ) {
	    result = lispCons(temp, result);
	} 
	if (  temp && temp->type == PGLISP_DTPR ) {
	   tempResult = FindVarNodes(temp);
	   /*
	    * I shoud have used 'append1' but it has a bug!
	    */
	   foreach(t, tempResult)
	       result = lispCons(CAR(t), result); 
	}
    }

    return(result);
}

/*--------------------------------------------------------------------
 *
 * IsPlanUsingNewOrCurrent
 *
 * used to find if a plan references the 'NEW' or 'CURRENT'
 * relations.
 *--------------------------------------------------------------------
 */

void
IsPlanUsingNewOrCurrent(parsetree, currentUsed, newUsed)
List parsetree;
bool *currentUsed;
bool *newUsed;
{
    List varnodes;
    List i;
    Var v;

    /*
     * first find the varnodes referenced in this parsetree
     */
    varnodes = FindVarNodes(parsetree);

    *currentUsed = false;
    *newUsed = false;

    foreach(i, varnodes) {
	v = (Var) CAR(i);
	if (get_varno(v) == 1)
	    *currentUsed = true;
	if (get_varno(v) == 2)
	    *newUsed = true;
    }
}


/*--------------------------------------------------------------------
 *
 * SubstituteParamForNewOrCurrent
 *
 * Change all the "Var" nodes corresponding to the NEW & CURRENT relation
 * to the equivalent "Param" nodes.
 *
 * NOTE: this routine used to be called from the parser, but now it is
 * called from the tuple-level rule system (when defining a rule),
 * and it takes an extra argument (the relation oid for the
 * NEW/CURRENT relation.
 * 
 *--------------------------------------------------------------------
 */
SubstituteParamForNewOrCurrent ( parsetree, relid )
     List parsetree;
     ObjectId relid;
{
    List i = NULL;
    Name getAttrName();

    /*
     * sanity check...
     */
    if (relid==NULL) {
	elog(WARN, "SubstituteParamForNewOrCurrent: wrong argument rel");
    }

    foreach ( i , parsetree ) {
	List temp = CAR(i);
	if ( temp && IsA (temp,Var) ) {
	    Name attrname = NULL;
	    AttributeNumber attrno;


	    if ( get_varno((Var)temp) == 1) {
		/* replace with make_param(old) */
		attrname = get_attname(relid, get_varattno((Var)temp));
		attrno = get_varattno((Var)temp);
		CAR(i) = (List)MakeParam (PARAM_OLD,
				    attrno,
				    attrname,
				    get_vartype((Var)temp));
	    } 
	    if ( get_varno((Var)temp) == 2) {
		/* replace with make_param(new) */
		attrname = get_attname(relid, get_varattno((Var)temp));
		attrno = get_varattno((Var)temp);
		CAR(i) = (List)MakeParam(PARAM_NEW,
				   attrno,
				   attrname,
				   get_vartype((Var)temp) );
	    }
	} 
	if (  temp && temp->type == PGLISP_DTPR ) 
	  SubstituteParamForNewOrCurrent ( temp , relid );
    }
}

