/**********************************************************************

  parse_query.c

  -   All new code for Postquel, Version 2

  take an "optimizable" stmt and make the query tree that
  the planner requires.

  Synthesizes the lisp object via routines in lisplib/lispdep.c

 **********************************************************************/

#include <ctype.h>
#include "rel.h" 		/* Relation stuff */
#include "catalog_utils.h"
#include "lispdep.h"
#include "log.h"
#include "parse.h"
#include "tim.h"
#include "trange.h"
#include "parse_query.h"

int MAKE_RESDOM,MAKE_VAR,MAKE_ADT,MAKE_CONST,MAKE_PARAM,MAKE_OP;

LispValue
lispMakeResdom(resno,restype,reslen,attrname,reskey,reskeyop)
     LispValue attrname;
     int resno,restype,reslen,reskey,reskeyop;
{
#ifdef ALLEGRO
	return ( lisp_call (MAKE_RESDOM, resno,restype,
		 	    reslen,attrname,reskey,reskeyop ));
#endif
#ifdef FRANZ43
	LispValue l = lispCons( lispInteger(reskeyop), LispNil);
	l = lispCons ( lispInteger(reskey) , l);
	l = lispCons ( attrname , l);
	l = lispCons ( lispInteger (reslen) , l );
	l = lispCons ( lispInteger (restype) , l );
	l = lispCons ( lispInteger (resno) , l );
	l = lispCons ( lispAtom ("make_resdom") , l);
	return (evalList(l));
#endif
}

LispValue
lispMakeVar(vnum,attid,vartype,vardotfields,vararrayindex)
     int vnum,attid,vartype,vardotfields,vararrayindex ;
{
#ifdef ALLEGRO
	return ( lisp_call (MAKE_VAR,vnum,attid,vartype,vardotfields,
			    vararrayindex ));
#endif
#ifdef FRANZ43
	LispValue l = LispNil, m = LispNil;
	
	if (vararrayindex != 0 )
	  m = lispCons ( lispInteger (vararrayindex) , m ) ;
	/* else
	  m = lispCons ( LispNil , m); */
	if (vardotfields != 0 )
	  m = lispCons ( lispInteger (vardotfields) , m ) ; /* bogus, since
							       no multi-level
							       in Ver 1 */
	/* else
	  m = lispCons ( LispNil , m); */
	/* m = lispCons ( lispInteger (vartype) , m );    /* vartype */
	m = lispCons ( lispInteger (attid) , m );      /* varattno */
	m = lispCons ( lispInteger (vnum) , m );       /* varno */
	m = lispCons ( lispAtom ("list") , m );

	l = lispCons ( m , LispNil );
	if (vararrayindex != 0 )
	  l = lispCons ( lispInteger (vararrayindex) , l ) ;
	else
	  l = lispCons ( LispNil , l );
	if (vardotfields != 0 )
	  l = lispCons ( lispInteger (vardotfields) , l ) ;
	else
	  l = lispCons ( LispNil , l );
	l = lispCons ( lispInteger (vartype) , l );
	l = lispCons ( lispInteger (attid) , l );
	l = lispCons ( lispInteger (vnum) , l );

	l = lispCons ( lispAtom ("make_var") , l );
	return (evalList (l));
#endif       
}

/**********************************************************************

  lispMakeOp
  returns
  #S(oper :opno <opid> :oprelationlevel <oprel> :opresulttype <opresult> )

 **********************************************************************/

LispValue 
lispMakeOp (opid,oprel,opresult)
     int opid,oprel,opresult;
{
#ifdef ALLEGRO
	return(lisp_call (MAKE_OP,opid,oprel,opresult));
#endif
#ifdef FRANZ43
	LispValue l = lispCons ( lispInteger (opresult) , LispNil );
	if ( oprel != 0 )
	  l = lispCons (lispInteger (oprel) , l );
	else
	  l = lispCons (LispNil , l );
	  
	l = lispCons (lispInteger (opid) , l );
	l = lispCons (lispAtom ("make_oper") , l );
	return (evalList(l));
#endif
#ifdef PG_LISP
	return(LispNil);
#endif
}

/**********************************************************************

  lispMakeParam
  
  - takes the param from yacc
  and constructs a param struct as per nodedefs.cl

  XXX - why isn't the foo in foo[ $param ] used ?

 **********************************************************************/


LispValue 
lispMakeParam (paramval )
     LispValue paramval;
{
#ifdef ALLEGRO
	return(lisp_call (MAKE_PARAM,paramval));
#endif
#ifdef FRANZ43
	LispValue l = lispCons ( lispInteger(paramval), LispNil );
	
	l = lispCons ( lispAtom ("make_param") , LispNil );
#endif
#ifdef PG_LISP
	return(LispNil);
#endif
}
/**********************************************************************

  lispMakeConst
  - takes the stuff and makes it into a 
  #S(const :consttype  <typid> 
           :constlen   <typlen>
	   :constvalue <lvalue>
	   :constisnull <nil | constisnull>

 **********************************************************************/

LispValue 
lispMakeConst (typid,typlen,lvalue,constisnull)
     int typid,typlen,constisnull;
     LispValue lvalue;
{
#ifdef ALLEGRO
	return(lisp_call (MAKE_CONST,typid,typlen,lvalue,constisnull));
#endif
#ifdef FRANZ43
	LispValue l = LispNil;
	if ( constisnull != 0 )
	  l = lispCons ( lispInteger (constisnull) , LispNil );
	else
	  l = lispCons ( LispNil, LispNil );

	l = lispCons ( lvalue , l );
	l = lispCons ( lispInteger (typlen) , l );
	l = lispCons ( lispInteger (typid) , l );
	l = lispCons ( lispAtom ("make_const") , l );
	return (evalList( l ));
#endif
#ifdef PG_LISP
	return(LispNil);
#endif
}

LispValue
ModifyQueryTree(query,priority,ruletag)
     LispValue query,priority,ruletag;
{
  return(lispString("rule query")); /* XXX temporary */
}

LispValue
VarnoGetRelname( vnum )
     int vnum;
{
	int i;
	extern LispValue p_rtable;
	LispValue temp = p_rtable;
	for( i = 1; i < vnum ; i++) 
		temp = CDR(temp);
	return(lispString(CString(CAR(CDR(CAR(temp))))));
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
MakeRoot(NumLevels,query_name,result,rtable,priority,ruleinfo)
     int NumLevels;
     LispValue query_name,result;
     LispValue rtable,priority,ruleinfo;
{
	LispValue newroot = LispNil;

	newroot = lispCons( ruleinfo , LispNil );
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

 **************************************************/

LispValue
MakeRangeTableEntry( relname , options , refname)
     LispValue relname;
     int options;
     LispValue refname;
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

	index = RangeTablePosn (CString(relname),(options & 0x01),
				(CInteger(p_trange) != 0 )); 
	
	relation = amopenr( CString( relname ) );
	if ( relation == NULL ) {
		elog(WARN,"amopenr on %s failed\n",relname);
	    /*p_raise (CatalogFailure,
		     form("amopenr on %s failed\n",relname));
	    */
	}
	/* RuleLocks - for now, always empty, since preplanner fixes */

	/* Flags - zero or more from archive,inheritance,union,version */

	if(options & 0x01 ) /* XXX - fix this */
	  Flags = lispCons( lispAtom ("inheritance") , Flags );

	/* TimeRange */

	  TRange = p_trange; /* default is lispInteger(0) */

	/* RelOID */
	RelOID = lispInteger ( RelationGetRelationId (relation ));

	entry = lispCons (relname, 
			  lispCons(RelOID,
				   lispCons(TRange,
					    lispCons(Flags,
						     lispCons(RuleLocks,
							      LispNil)))));

	return ( lispCons ( refname, entry ));
}

LispValue
MakeTargetList()
{
	
}


/**************************************************

  ExpandAll

  	- makes a list of attributes
	- assumes reldesc caching works
  **************************************************/

LispValue
ExpandAll(relname,this_resno)
     LispValue relname;
     int *this_resno;
{
	Relation rdesc;
	LispValue tall = LispNil;
	LispValue resnode = LispNil;
	LispValue varnode = LispNil;
	LispValue temp = LispNil;
	int i,maxattrs,first_resno;
	int type_id,type_len,vnum;
	LispValue physical_relname;

	first_resno = *this_resno;
	
	/* printf("\nExpanding %s.all\n",CString(relname)); */
	vnum = RangeTablePosn (CString(relname),0,0);
	if ( vnum == 0 ) {
		p_rtable = nappend1 ( p_rtable,
				     MakeRangeTableEntry (relname, 
							  0 ,  relname));
		physical_relname = VarnoGetRelname(1);
		/* printf("first item in range table"); */
	} else 
		physical_relname = VarnoGetRelname(vnum);


	rdesc = amopenr(CString(physical_relname));
	
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
				  lispString(attrname) );
		varnode = CDR(temp);
		type_id = CInteger(CAR(temp));
		type_len = tlen(get_id_type(type_id));
		
		resnode = lispMakeResdom( i + first_resno, type_id, type_len,
					 lispString(attrname), 0, 0 );

		tall = lispCons(lispCons(resnode, lispCons(varnode, LispNil)),
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
	TimeRange  trange;
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
			trange = TimeFormSnapshotTimeRange(t1);
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
			trange = TimeFormRangedTimeRange(t1,t2);
			break;
		default:
			elog(WARN, "MakeTimeRange: internal parser error");
	}
	return(ppreserve(trange));
}

notify_parser (a,b,c,d,e,f)
	int a,b,c,d,e,f;
{
	MAKE_RESDOM = a;
	MAKE_VAR = b;
	MAKE_ADT = c;
	MAKE_CONST = d;
	MAKE_PARAM = e;
	MAKE_OP = f;
}

LispValue 
make_op(op,ltree,rtree)
     LispValue op,ltree,rtree;
{
	Type ltype,rtype;
	Operator temp;
	OperatorTupleForm opform;
	LispValue newop;
	LispValue left,right;

	left = CDR(ltree);
	right = CDR(rtree);
	ltype = get_id_type ( CInteger ( CAR(ltree) ));
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
	newop = lispMakeOp ( oprid(temp),    /* operator id */
			    0 ,       	     /* operator relation level */
			    opform->         /* operator result type */
			    oprresult );
	newop = lispCons ( newop , lispCons (left ,
					     lispCons (right,LispNil)));
	return ( lispCons (lispInteger ( opform->oprresult ) ,
			   newop ));
			   
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
     LispValue relname, attrname;
{
	LispValue varnode;
	int vnum, attid , vartype, vardotfields, vararrayindex ;
	Type rtype;
	Relation rd;
	extern LispValue p_rtable;
	extern int p_last_resno;

	/* 
	printf (" now in make_Var\n"); 
	printf ("relation = %s, attr = %s\n",CString(relname),
		CString(attrname)); 
		*/
	fflush(stdout);

	vnum = RangeTablePosn ( CString (relname),0,0) ;
	/* printf("vnum = %d\n",vnum); */
	if (vnum == 0) {
		p_rtable = nappend1 (p_rtable ,
			  MakeRangeTableEntry ( relname , 0 , relname) );
		vnum = RangeTablePosn (CString (relname),0,0);
		/* printf("vnum = %d\n",vnum); */
		relname = VarnoGetRelname(vnum);
	} else {
	  	relname = VarnoGetRelname( vnum );
	}

	/* printf("relname to open is %s",CString(relname));*/

	rd = amopenr ( CString (relname ));
	attid = varattno (rd , CString(attrname) );
	vartype = att_typeid ( rd , attid );
	rtype = get_id_type(vartype);
	vardotfields = 0;                          /* XXX - fix this */
	vararrayindex = 0;                         /* XXX - fix this */

	varnode = lispMakeVar (vnum , attid ,
			      vartype , vardotfields , vararrayindex );

	return ( lispCons ( lispInteger ( typeid (rtype ) ),
			    varnode ));
}

/**********************************************************************
  SkipToFromList

  - employ lookahead to see if there is indeed a from_list
  ahead of us.  If so, skip to it and continue processing

 **********************************************************************/

static char *tlist_buf = 0;
static int end_tlist_buf = 0;

LispValue
SkipForwardToFromList()
{
	extern LispValue yylval;
	extern char yytext[];
	extern int yyleng;
	LispValue next_token;
	int i;
	char *temp;

	if (tlist_buf == 0 )
	  tlist_buf = (char*) malloc(1024);
	
	for(i=0;i<1024;i++)
	  tlist_buf[i]=0;
	
	end_tlist_buf = 0;
	while ((next_token=(LispValue)yylex()) > 0 && 
	        next_token != (LispValue)FROM ) {
		tlist_buf[end_tlist_buf++] = ' ';
		/* fputc(' ',stdout); */
		switch( (int) next_token ) {
		      case SCONST:
			temp = (char *)CString(yylval);
			tlist_buf[end_tlist_buf++] = '\"';
			for (i = 0; i < strlen(temp) ; i ++) {
			  if (temp[i] == '\"' )
			    tlist_buf[end_tlist_buf++] =
			      '\\';
			  tlist_buf[end_tlist_buf++] = 
			    temp[i];
			}
			tlist_buf[end_tlist_buf++] = '\"';
			break;
		      case CCONST:
			temp = (char *)CString(yylval);
			tlist_buf[end_tlist_buf++] = '\'';
			tlist_buf[end_tlist_buf++] = 
			  temp[0];
			tlist_buf[end_tlist_buf++] = '\'';
			break;
		      default:
			for (i = 0; i < yyleng; i ++) {
				tlist_buf[end_tlist_buf++] =
				  yytext[i];
				/* fputc(yytext[i],stdout); */
			}
			break;
		}

	} /* while */
	fflush(stdout);
	if (next_token <= 0 ) {
/*		printf("%d\n",(LispValue)0);
		printf("EOS, no from found\n");
		fflush(stdout);*/
		for (i = end_tlist_buf; i > -1 ;--i ) {
			unput(tlist_buf[i] );
			/* fputc(tlist_buf[i],stdout); */
		}
		end_tlist_buf = 0;
		fflush(stdout);
	}
	if (next_token == (LispValue) FROM ) {
/*		printf("FROM found\n");
		fflush(stdout);
*/
		for (i = yyleng ; i > -1; --i ) {
			unput(yytext[i] );
			/* fputc(yytext[i],stdout); */
		}
		fflush(stdout);
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
	fflush(stdout);
	
	if(end_tlist_buf == 0 )
	  return(LispNil);
	/* printf("Moving back to target list\n");*/
	while( end_tlist_buf > 0 ) {
		unput(tlist_buf[--end_tlist_buf] );
		/* fputc(tlist_buf[end_tlist_buf],stdout); */
	}
	fflush(stdout);
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

/*	printf("in make_const\n");*/
	fflush(stdout);
#ifdef ALLEGRO
	switch( GetType( value )) {

	      case FixnumType:
		tp = type("int4");
		break;
		
	      case CharType:
		tp = type("char");
		break;

	      case S_FloatType:
	      case D_FloatType:
		tp = type("float4");
		break;

	      case V_charType:
		if( strlen ( CString (value)) > 16 )
		  tp = type("text");
		else 
		  tp = type("char16");
		break;

	      case NilType: 
	      default: 
		/* null const */
		return ( lispCons (LispNil , 
				   lispMakeConst ( 0 , 0 , 
					       LispNil , 1 )) );
	}
#endif
#ifdef FRANZ43
	switch( TYPE( value )) {

	      case INT:
		tp = type("int4");
		break;
		
/*	      case CharType:
		tp = type("char");
		break;
*/
	      case DOUB:
		tp = type("float4");
		break;

	      case STRNG:
		if( strlen ( CString (value)) > 16 )
		  tp = type("text");
		else 
		  tp = type("char16");
		break;

	      default: 
		elog(NOTICE,"unknown type : %d\n", TYPE(value) );
		fflush (stdout);
		/* null const */
		return ( lispCons (LispNil , 
				   lispMakeConst ( 0 , 0 , 
					       LispNil , 1 )) );
	}
#endif
/*	printf("tid = %d , tlen = %d\n",typeid(tp),tlen(tp));*/
	fflush(stdout);

	temp = lispCons (lispInteger ( typeid (tp)) ,
			  lispMakeConst(typeid( tp ), tlen( tp ),
				    value , 0 ));
/*	lispDisplay( temp , 0 );*/
	return (temp);
	
}

