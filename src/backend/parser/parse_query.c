/**********************************************************************

  parse_query.c

  -   All new code for Postquel, Version 2

  take an "optimizable" stmt and make the query tree that
  the planner requires.

  Synthesizes the lisp object via routines in lisplib/lispdep.c
  $Header$
 **********************************************************************/

#include <ctype.h>
#include "rel.h" 		/* Relation stuff */
#include "catalog_utils.h"
#include "datum.h"
#include "pg_lisp.h"
#include "log.h"
#include "parse.h"
#include "parse_query.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "palloc.h"
#include "tim.h"
#include "tqual.h"

extern LispValue parser_ppreserve();
extern int Quiet;

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
    order_op = oper("<",get_vartype(varnode),vartype,vartype);
    order_opid = oprid(order_op);

    return(lispInteger(order_opid));

}

Var
find_tl_elt ( varname ,tlist )
     char *varname;
{
    LispValue i = LispNil;

    foreach ( i, tlist ) {
	Resdom resnode = (Resdom)CAAR(i);
	Var tvarnode = (Var) CADR(CAR(i));
	Name resname = get_resname(resnode );

	if ( strcmp ( resname, varname ) ) {
	    return ( tvarnode );
	} 
    }
    return ( (Var) NULL );
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
	Var  		one_sort_elt = NULL;
	String	 	one_sort_op = NULL;
	int 		sort_elt_type = 0;

	Assert ( consp (one_sort_clause) );

	one_sort_elt = find_tl_elt(one_sort_clause,targetlist );

	Assert(! null (one_sort_elt));

	if ( ! null ( CADR ( one_sort_clause )))
	  one_sort_op = CString(CADR(one_sort_clause));
	else
	  one_sort_op = "<";

	/* Assert ( tlelementP (one_sort_elt) ); */

	sort_clause_elts = nappend1( sort_clause_elts, one_sort_elt );
	sort_elt_type = get_vartype(one_sort_elt);

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
	    if ( ! member ( CADR(tlelt) , sort_clause_elts ) ) {
		sort_clause_elts = nappend1 ( sort_clause_elts, CADR(tlelt)); 
		sort_clause_ops = nappend1 ( sort_clause_ops, 
			any_ordering_op(CADR(tlelt) ));
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
    extern Relation amopenr();
    extern LispValue p_trange;
    int index;
    
    /*printf("relname is : %s\n",(char *)relname); 
      fflush(stdout);*/
    
    index = RangeTablePosn (relname,options);
    
    relation = amopenr( relname );
    if ( relation == NULL ) {
	elog(WARN,"amopenr on %s failed\n",relname);
	/*p_raise (CatalogFailure,
	  form("amopenr on %s failed\n",relname));
	  */
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
    
    entry = lispCons (lispString(relname), 
		      lispCons(RelOID,
			       lispCons(TRange,
					    lispCons(Flags,
						     lispCons(RuleLocks,
							      LispNil)))));

    return ( lispCons ( lispString(refname), entry ));
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
	vnum = RangeTablePosn (relname,0,0);
	if ( vnum == 0 ) {
		p_rtable = nappend1 ( p_rtable,
				     MakeRangeTableEntry (relname, 
							  0 ,  relname));
		physical_relname = VarnoGetRelname(1);
		/* printf("first item in range table"); */
	} else 
		physical_relname = VarnoGetRelname(vnum);


	rdesc = amopenr(physical_relname);
	
	if (rdesc == NULL ) {
		elog(WARN,"Unable to expand all -- amopenr failed ");
		return( LispNil );
	}
	maxattrs = RelationGetNumberOfAttributes(rdesc);

	for ( i = maxattrs-1 ; i > -1 ; --i ) {
		char *attrname = (char *)(&rdesc->rd_att.data[i]->attname);
		/* printf("%s\n",attrname);
		fflush(stdout);*/
		temp = make_var ( relname,
				  attrname );
		varnode = (Var)CDR(temp);
		type_id = CInteger(CAR(temp));
		type_len = tlen(get_id_type(type_id));
		
		resnode = MakeResdom( i + first_resno, type_id, type_len,
					 attrname, 0, 0 );
/*
		tall = lispCons(lispCons(resnode, lispCons(varnode, LispNil)),
				tall);
*/
		tall = lispCons(lispCons(resnode,lispCons(varnode,LispNil)),
				tall);
	}

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
			t1 = abstimein(CString(datestring1));
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
				t1 = abstimein(CString(datestring1));
				if (!AbsoluteTimeIsValid(t1)) {
					elog(WARN,
						"bad range start time: \"%s\"",
						CString(datestring1));
				}
			}
			if (datestring2 == LispNil) {
				t2 = InvalidAbsoluteTime;
			} else {
				t2 = abstimein(CString(datestring2));
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
	return(lispInteger(qual));
}

LispValue 
make_op(op,ltree,rtree)
     LispValue op,ltree,rtree;
{
	Type ltype,rtype;
	Operator temp;
	OperatorTupleForm opform;
	Oper newop;
	LispValue left,right;
	LispValue t1;
	
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
		temp = oper(CString(op),typeid(ltype), typeid ( rtype ));
	}
	opform = (OperatorTupleForm) GETSTRUCT(temp);
	newop = MakeOper ( oprid(temp),    /* operator id */
			    0 ,       	     /* operator relation level */
			    opform->         /* operator result type */
			    oprresult );
	t1 = lispCons ( newop , lispCons (left ,
					     lispCons (right,LispNil)));
	return ( lispCons (lispInteger ( opform->oprresult ) ,
			   t1 ));
			   
} /* make_op */

/**********************************************************************
  make_var

  - takes the attribute and figures out the 
  info necessary for constructing a varnode

  attribute = (relname . attrname)

  - returns a type,varnode pair suitable for use in arith-expr's
  to get just the varnode, strip the type off (use CDR(make_var ..) )
 **********************************************************************/

LispValue
make_var ( relname, attrname )
     Name relname, attrname;
{
    Var varnode;
    int vnum, attid, vartype;
    LispValue vardotfields, vararrayindex ;
	Type rtype;
    Relation rd;
    extern LispValue p_rtable;
    extern int p_last_resno;

    /* if (!Quiet) {
	printf (" now in make_Var\n"); 
	printf ("relation = %s, attr = %s\n",relname,
		attrname); 
	fflush(stdout);
    } */

    vnum = RangeTablePosn ( relname,0,0) ;
    /* if (!Quiet)
      printf("vnum = %d\n",vnum);  */
    if (vnum == 0) {
	p_rtable = nappend1 (p_rtable ,
			     MakeRangeTableEntry ( relname , 0 , relname) );
		vnum = RangeTablePosn (relname,0,0);
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
    vararrayindex = LispNil;                         /* XXX - fix this */
    
    varnode = MakeVar (vnum , attid ,
		       vartype , vardotfields , vararrayindex ,
		       lispCons(lispInteger(vnum),
				lispCons(lispInteger(attid),LispNil)));
    
    return ( lispCons ( lispInteger ( typeid (rtype ) ),
		       varnode ));
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

LispValue
SkipForwardToFromList()
{
        LispValue next_token;
        int i;
        extern char *Ch;
        char *temp = Ch;

        while ((next_token=(LispValue)yylex()) > 0 &&
                next_token != (LispValue)FROM )
          ; /* empty while */

        if (next_token <= 0 ) {
/*
                printf("EOS, no from found\n");
                fflush(stdout);
*/
                Ch = temp;
        }
        if (next_token == (LispValue) FROM ) {
        /*      printf("FROM found\n");*/
                fflush(stdout);
                Ch -= 4;
                from_list_place = Ch;
                target_list_place = temp;
        }

} /* Skip */


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
	if(yychar == (LispValue)WHERE) {
		/*printf("putting the where back\n");*/
		for (i = yyleng; i > -1 ; -- i ) {
			unput (yytext[i]);
			/* fputc (yytext[i],stdout); */
		}
	}

        bcopy(Ch,from_list_place,strlen(Ch) + 1 );
	Ch = target_list_place;
        return(LispNil);

}

LispValue
SkipForwardPastFromList()
{
	
}

StripRangeTable()
{
	LispValue temp;
	LispValue nrtable;
	temp = p_rtable;
	
	while(! lispNullp(temp) ) {
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

/*	printf("in make_const\n");*/
	fflush(stdout);

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
		/* float *dummy_float; */
		tp = type("float4");
		/* 
		dummy_float = (float *)palloc(sizeof(float));
		*dummy_float = (float)(value->val.flonum);
		*/
		val = Float32GetDatum(&value->val.flonum);
	    }
	    break;
	    
	  case PGLISP_STR:
	    if( strlen ( CString (value)) > 16 )
	      tp = type("text");
	    else 
	      tp = type("char16");
	    val = PointerGetDatum(value->val.str);
	    break;
	    
	  default: 
	    elog(NOTICE,"unknown type : %d\n", value->type );
	    fflush (stdout);
	    /* null const */
	    return ( lispCons (LispNil , 
			       MakeConst ( 0 , 0 , 
					  LispNil , 1 )) );
	}

/*	printf("tid = %d , tlen = %d\n",typeid(tp),tlen(tp));*/
	fflush(stdout);

	temp = lispCons (lispInteger ( typeid (tp)) ,
			  MakeConst(typeid( tp ), tlen( tp ),
				    val , 0 ));
/*	lispDisplay( temp , 0 );*/
	return (temp);
	
}
LispValue
parser_ppreserve(temp)
     char *temp;
{
    return((LispValue)temp);
}
