/**********************************************************************

  parse_query.c

  -   All new code for Postquel, Version 2

  take an "optimizable" stmt and make the query tree that
  the planner requires.

  Synthesizes the lisp object via routines in lisplib/lispdep.c

 **********************************************************************/

#include "rel.h" 		/* Relation stuff */
#include "catalog_utils.h"
#include "lispdep.h"
#include "log.h"
#include "parse.h"
#include "tim.h"
#include "trange.h"

int MAKE_RESDOM,MAKE_VAR,MAKE_ADT,MAKE_CONST,MAKE_PARAM,MAKE_OP;

LispValue
lispMakeResdom(resno,restype,reslen,attrname,reskey,reskeyop)
     LispValue attrname;
     int resno,restype,reslen,reskey,reskeyop;
{
#ifdef ALLEGRO
	return ( lisp_call (MAKE_RESDOM, resno,a,b,attrname,c,d ));
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
	else
	  m = lispCons ( LispNil , m);
	if (vardotfields != 0 )
	  m = lispCons ( lispInteger (vardotfields) , m ) ;
	else
	  m = lispCons ( LispNil , m);
	m = lispCons ( lispInteger (vartype) , m );
	m = lispCons ( lispInteger (attid) , m );
	m = lispCons ( lispInteger (vnum) , m );
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

LispValue 
lispMakeOp (opid,oprel,opresult)
     int opid,oprel,opresult;
{
#ifdef ALLEGRO
	return(lisp_call (MAKE_OP,opid,oprel,opresult));
#endif
#ifdef FRANZ43
	LispValue l = lispCons ( lispInteger (oprel) , LispNil );
	l = lispCons (lispInteger (oprel) , l );
	l = lispCons (lispInteger (opid) , l );
	l = lispCons (lispAtom ("make_op") , l );
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
	return(LispNil);
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

	printf("relname is : %s\n",(char *)relname);
	fflush(stdout);

	index = RangeTablePosn (CString(relname)); 
	
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
	  Flags = lispCons( lispString ("inherit") , Flags );

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
	extern LispValue p_target;
	int i;

	rdesc = amopenr(CString(relname));
	if (rdesc == NULL ) {
		elog(WARN,"Unable to expand all -- amopenr failed ");
		return( LispNil );
	}
#ifndef PGLISP
	for ( i = 0 ; i < rdesc->rd_rel->relnatts; i++ ) {
		printf("%s\n",&rdesc->rd_att.data[i]->attname);
		fflush(stdout);
		nappend1(p_target,
			 lispCons(
				  lispMakeResdom( this_resno , 0, 0,
				      lispString(&rdesc->rd_att.data[i]->
						   attname), 0, 0 ),
			   LispNil)); /* XXX should be expr node */
		*this_resno++;
	}
#endif
	return(p_target);
}

LispValue
MakeTimeRange( datestring1 , datestring2 , timecode )
     LispValue datestring1, datestring2;
     int timecode; /* 0 = snapshot , 1 = timerange */
{
	TimeRange  trange;
	Time t1,t2;

	if ( datestring1 == LispNil )
	  t1 = InvalidTime;
	else
	  t1 = abstimein ( CString (datestring1));
	if ( datestring2 == LispNil )
	  t2 = InvalidTime;
	else
	  t2 = abstimein (CString (datestring2));

	if(timecode == 0) 
	  trange = TimeFormSnapshotTimeRange( t1 );
	else  /* timecode == 1 */
	  trange = TimeFormRangedTimeRange(t1,t2);

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

	printf (" now in make_Var\n");
	fflush(stdout);

	vnum = RangeTablePosn ( CString (relname)) ;
	if (vnum == 0) {
		nappend1 (p_rtable ,
			  MakeRangeTableEntry ( relname , 0 , relname) );
		vnum = RangeTablePosn (CString (relname));
		relname = VarnoGetRelname(vnum);
	} else {
	  	relname = VarnoGetRelname( vnum );
		printf("relname to open is %s",CString(relname));
	}

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

static char tlist_buf[1024];
static int end_tlist_buf = 0;

LispValue
SkipForwardToFromList()
{
	char c;
	int i;
	end_tlist_buf = 0;
	for(i=0;i<1024;i++) {
		c = input();
		fputc(c,stdout);
		fflush(stdout);
		switch (c) {
	        case 0:
			/* eos, so backup */
			while (end_tlist_buf != -1) {
				unput(tlist_buf[--end_tlist_buf]);
			}
			return(LispNil);
		case 'f':
			/* possibly a from ? */
			unput(c);
			return(LispNil);
		default:
			/* gobble up text */
			if(end_tlist_buf == 1023 )
			  elog(NOTICE,"overflowed tlist buffer ");
			else {
				tlist_buf[end_tlist_buf] = c;
				end_tlist_buf++;
		        }
		}
	}
}

LispValue
SkipBackToTlist()
{
	extern int yyleng,yymorfg;
	extern char yytext[];
	char *temp;
	int i;

	printf("length = %d, text = %s\n", yyleng,yytext);
	fflush(stdout);
	
	if(!strcmp (yytext,"where")) {
		temp = yytext;
		for ( i = yyleng; i > -1 ; i -- ) {
			unput(yytext[i] );
			fputc (yytext[i], stdout );
			fflush (stdout);
		}
	}

	while(end_tlist_buf != -1 ) {
	        fputc(tlist_buf[end_tlist_buf],stdout);
		fflush(stdout);
		unput(tlist_buf[end_tlist_buf]);
		--end_tlist_buf;
	}
	return(LispNil);
}

LispValue
SkipForwardPastFromList()
{
	static LispValue yychar;
	char c;
	int i;
	extern int yyleng;
	extern char yytext[];

	for(;;) {
		c = input();
		fputc(c,stdout);
		fflush(stdout);
		switch (c) {
	        case 0:
			/* eos, so done */
		        end_tlist_buf = 0;
			return(LispNil);
		case 'w':
			/* possibly a where ? */
			unput(c);
			(LispValue)yychar = (LispValue)yylex();
			if (yychar == (LispValue)WHERE )
			  for ( i=yyleng; i-- ; i > -1 )
			    unput(yytext[i]);
			end_tlist_buf = 0;
			return(LispNil);
		default:
			/* gobble up text */
			if(end_tlist_buf == 1023 )
			  elog(NOTICE,"overflowed tlist buffer ");
			else {
				tlist_buf[end_tlist_buf] = c;
				end_tlist_buf++;
		        }
		}
	}
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

	printf("in make_const\n");
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
		printf("unknown type : %d\n", TYPE(value) );
		fflush (stdout);
		/* null const */
		return ( lispCons (LispNil , 
				   lispMakeConst ( 0 , 0 , 
					       LispNil , 1 )) );
	}
#endif
	printf("tid = %d , tlen = %d\n",typeid(tp),tlen(tp));
	fflush(stdout);

	temp = lispCons (lispInteger ( typeid (tp)) ,
			  lispMakeConst(typeid( tp ), tlen( tp ),
				    value , 0 ));
/*	lispDisplay( temp , 0 );*/
	return (temp);
	
}

