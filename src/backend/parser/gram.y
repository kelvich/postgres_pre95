%{
#define YYDEBUG 1
/**********************************************************************

  $Header$

  POSTGRES YACC rules/actions
  the result returned by parser is the undecorated parsetree.
  
  Conventions used in coding this YACC file
  (apart from C code):
  CAPITALS are used to represent terminal symbols.
  non-capitals are used to represent non-terminals.

  the format of a YACC statement is :
  <non-terminal>:
	  <rule 1>
		<action 1>
	| <rule 2>
		<action 2>
		...
	| <rule n>
		<action n>
	;

  except when the action is < 30 chars long in which case it is
  	  <rule x>			<action x>

 **********************************************************************/

#include <strings.h>

#include "tmp/c.h"
#include "catalog_utils.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "nodes/pg_lisp.h"
/* XXX ORDER DEPENDENCY */
#include "parse_query.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "rules/params.h"
#include "utils/lsyscache.h"

extern LispValue new_filestr();
extern LispValue parser_typecast();
extern LispValue make_targetlist_expr();
extern Relation amopenr();
extern List MakeList();
extern List FlattenRelationList();

bool CurrentWasUsed = false;
bool NewWasUsed = false;

#define ELEMENT 	yyval = nappend1( LispNil , yypvt[-0] )
			/* yypvt [-1] = $1 */
#define INC_LIST 	yyval = nappend1( yypvt[-2] , yypvt[-0] ) /* $1,$3 */

#define NULLTREE	yyval = LispNil ; 
#define LAST(lv)        lispCons ( lv , LispNil )

#define INC_NUM_LEVELS(x)	{ if (NumLevels < x ) NumLevels = x; }
#define KW(keyword)		lispAtom(CppAsString(keyword))

#define ADD_TO_RT(rt_entry)     p_rtable = nappend1(p_rtable,rt_entry) 

#define YYSTYPE LispValue
extern YYSTYPE parsetree;

LispValue NewOrCurrentIsReally = (LispValue)NULL;
bool ResdomNoIsAttrNo = false;
extern YYSTYPE parser_ppreserve();

static YYSTYPE temp;
static int NumLevels = 0;
YYSTYPE yylval;

YYSTYPE p_target,
        p_qual,
        p_root,
        p_priority,
        p_ruleinfo;
YYSTYPE	       p_rtable,
               p_target_resnos;

static int p_numlevels,p_last_resno;
Relation parser_current_rel = NULL;
static bool QueryIsRule = false;

bool Input_is_string = false;
bool Input_is_integer = false;
bool Typecast_ok = true;

%}

/* Commands */

%token  ABORT_TRANS ADD_ATTR APPEND ATTACH_AS BEGIN_TRANS CLOSE
        CLUSTER COPY CREATE DEFINE DELETE DESTROY END_TRANS EXECUTE
        FETCH MERGE MOVE PURGE REMOVE RENAME REPLACE RETRIEVE

/* Keywords */

%token  ALL ALWAYS AFTER AND ARCHIVE ARG ASCENDING BACKWARD BEFORE BINARY BY
        DEMAND DESCENDING EMPTY FORWARD FROM HEAVY INTERSECT INTO
        IN INDEX INDEXABLE INHERITS INPUTPROC IS KEY LEFTOUTER LIGHT MERGE
        NEVER NEWVERSION NONE NONULLS NOT PNULL ON ONCE OR OUTPUTPROC
        PORTAL PRIORITY QUEL RIGHTOUTER RULE SCONST SORT TO TRANSACTION
        UNION UNIQUE USING WHERE WITH FUNCTION OPERATOR P_TYPE 


/* Special keywords */

%token  IDENT SCONST ICONST Op CCONST FCONST

/* add tokens here */

%token   	INHERITANCE VERSION CURRENT NEW THEN DO INSTEAD VIEW
		REWRITE P_TUPLE TYPECAST P_FUNCTION C_FUNCTION C_FN
		POSTQUEL RELATION RETURNS INTOTEMP LOAD CREATEDB DESTROYDB
		STDIN STDOUT ARRAY

/* precedence */
%nonassoc Op
%right 	'='
%left	OR
%left	AND
%right	NOT
%left  	'+' '-'
%left  	'*' '/'
%left	'|'		/* this is the relation union op, not logical or */
%right   UMINUS
%nonassoc  '<' '>'
%left	'.'
%left  	'[' ']' 
%nonassoc TYPECAST
%nonassoc REDUCE

%%

queryblock:
	query 
		{ parser_init(); }
	queryblock
		{ parsetree = lispCons ( $1 , parsetree ) ; }
	| query
		{ 
		    parsetree = lispCons($1,LispNil) ; 
		    parser_init();
		}
	;
query:
	stmt 
	;

stmt :
	  AttributeAddStmt
	| ATTACH_AS attach_args /* what are the args ? */
	| ClosePortalStmt
	| ClusterStmt
	| CopyStmt
	| CreateStmt
	| DefineStmt
	| DestroyStmt
	| FetchStmt
	| IndexStmt
	| MergeStmt
	| MoveStmt
	| ProcedureStmt
	| PurgeStmt			
	| RemoveOperatorStmt
	| RemoveStmt
	| RenameStmt
	| OptimizableStmt		
	| RuleStmt			
	| TransactionStmt
  	| ViewStmt
	| LoadStmt
	| CreatedbStmt
	| DestroydbStmt
	;


 /**********************************************************************

	QUERY :
		addattr ( attr1 = type1 .. attrn = typen ) to <relname>
	TREE :
		(ADD_ATTR <relname> (attr1 type1) . . (attrn typen) )

  **********************************************************************/
AttributeAddStmt:
	  ADD_ATTR '(' var_defs ')' TO relation_name
		{
		     $$ = lispCons( $6 , $3);
		     $$ = lispCons( KW(addattr) , $$);
		}
	;

 /**********************************************************************
	
	QUERY :
		close <optname>
	TREE :
		(CLOSE <portalname>)

   **********************************************************************/
ClosePortalStmt:
	  CLOSE opt_id	
		{  $$ = MakeList ( KW(close), $2, -1 ); }

	;

 /**********************************************************************
	
	QUERY :
		cluster <relation> on <indexname> [using <operator>]
	TREE:
		XXX ??? unimplemented as of 1/8/89

  **********************************************************************/
ClusterStmt:
	  CLUSTER relation_name ON index_name OptUseOp
  		{ elog(WARN,"cluster statement unimplemented in version 2"); }
	;

 /**********************************************************************

	QUERY :
		COPY [BINARY] [NONULLS] <relname> FROM/TO 
		<filename> [USING <maprelname>]

	TREE:
		( COPY ("relname" [BINARY] [NONULLS] )
		       ( FROM/TO "filename")
		       ( USING "maprelname")

  **********************************************************************/

CopyStmt:
	  COPY copy_type copy_null 
	  relation_name copy_dirn copy_file_name copy_map
	
		{
		    LispValue temp;
		    $$ = lispCons( KW(copy), LispNil);
		    $4 = lispCons($4,LispNil);
		    $4 = nappend1($4,$2);
		    $4 = nappend1($4,$3);
		    $$ = nappend1($$,$4);
		    $$ = nappend1($$,lispCons ($5,lispCons($6,LispNil)));
		    if(! lispNullp($7))
		      $7 = lispCons($7,LispNil);
		    $$ = nappend1($$, lispCons(KW(using), $7));
		    temp = $$;
		    while(temp != LispNil && CDR(temp) != LispNil )
		      temp = CDR(temp);
		    CDR(temp) = LispNil;
		}
	;

copy_dirn:	  
  	TO 
		{ $$ = KW(to); }
	| FROM
		{ $$ = KW(from); }
	;

copy_map:
	/*EMPTY*/				{ NULLTREE }
	| Using map_rel_name			{ $$ = $2 ; }
	;

copy_null:
	/*EMPTY*/				{ NULLTREE }
	| Nonulls
	;

copy_type:
	/*EMPTY*/				{ NULLTREE }
	| BINARY				{ $$ = KW(binary); }
	;

 /************************************************************
	Create a relation:

	QUERY :
		create version <rel1> from <rel2>
	TREE :
		( CREATENEW <rel1> <rel2> )
	QUERY :
		create <relname> ( 
  ************************************************************/

CreateStmt:
	  CREATE relation_name '(' opt_var_defs ')' 
	  OptKeyPhrase OptInherit OptIndexable OptArchiveType
		{

		     LispValue temp = LispNil;

		     $9 = lispCons ( $9, LispNil);
		     $8 = lispCons ( $8, $9 );
		     $7 = lispCons ( $7, $8 );
		     $6 = lispCons ( $6, $7 );
		     
		     $2 = lispCons ( $2, LispNil );
		     $$ = lispCons ( KW(create), $2 );
		     
		     $$ = nappend1 ( $$, $6 );
		     temp = $$;
		     while (temp != LispNil && CDR(temp) != LispNil) 
		       temp = CDR(temp);
		     CDR(temp) = $4;
		     /* $$ = nappend1 ( $$, $4 ); */
		}
	| CREATE OptDeltaDirn NEWVERSION relation_name FROM 
          relation_name_version
		{   
		    $$ = MakeList ( $2, $4, $6,-1 );
		}
	;
relation_name_version:
	  relation_name
	| relation_name '[' date ']'
	  {
             $$ = MakeList( $1, $3, -1);
	  }
	;

OptDeltaDirn:
	  /*EMPTY*/		{ $$ = KW(forward);}
	| FORWARD		{ $$ = KW(forward); }
	| BACKWARD		{ $$ = KW(backward); }
	;

OptIndexable:
	/*EMPTY*/
		{ NULLTREE }
	| Indexable dom_name_list
		{ $$ = lispCons ( $1 , $2 ); }
	;

dom_name_list:		name_list;

OptArchiveType:
	/*EMPTY*/				{ NULLTREE }
	| ARCHIVE '=' archive_type		
		{ $$ = lispCons (KW(archive) , $3); }
	;

archive_type:	  
	HEAVY	{ $$ = KW(heavy); }
	| LIGHT { $$ = KW(light); }
	| NONE  { $$ = KW(none); }
	;

OptInherit:
	  /*EMPTY */				{ NULLTREE }
	| INHERITS '(' relation_name_list ')'	
		{ 
		    $$ = lispCons ( KW(inherits), $3 ) ; 
		}
	;


OptKeyPhrase :
	  /* NULL */				{ NULLTREE }
	| Key '(' key_list ')'			
		{ 
		    $$ = MakeList ( $1, $3, -1 );
		}
	;

key_list:
	  key					{ ELEMENT ; }
	| key_list ',' key			{ INC_LIST ;  }
	;

key:
	  attr_name OptUseOp
		{
		  $$ = lispCons($1,lispCons($2,LispNil)) ;
		}
	;

 /**********************************************************************

   	QUERY :
		define (type,operator)
	TREE :

  **********************************************************************/
DefineStmt:
	  DEFINE def_type def_rest
		{
		   $2 = lispCons ($2 , $3 ); 
		   $$ = lispCons (KW(define) , $2 ); 
		}
	;

def_rest:
	  def_name definition opt_def_args
		{
		   if ( $3 != LispNil ) 
			$2 = nappend1 ($2, $3 );
		   $$ = lispCons ($1 , $2 );
		}
	;

def_type:   Operator | Type	;

def_name:  Id | Op
      | '+'   { $$ = lispString("+"); }
      | '-'   { $$ = lispString("-"); }
      | '*'   { $$ = lispString("*"); }
      | '/'   { $$ = lispString("/"); }
      | '<'   { $$ = lispString("<"); }
      | '>'   { $$ = lispString(">"); }
      | '='   { $$ = lispString("="); }
      ;

opt_def_args:	/* Because "define procedure .." */
	  ARG IS '(' def_name_list ')'
		{ $$ = lispCons (KW(arg), $4); }
	| /*EMPTY*/
		{ NULLTREE }
	;

def_name_list:		name_list;	

def_arg:
	  Id
	| Op
	| NumConst
      	| Sconst
	;

definition:
	  '(' def_list ')'
		{ $$ = $2; }
	;

def_elem:
	  def_name '=' def_arg
		{ $$ = lispCons($1, lispCons($3, LispNil)); }
	| def_name
		{ $$ = lispCons($1, LispNil); }
	;

def_list:
	  def_elem			{ ELEMENT ; }
	| def_list ',' def_elem		{ INC_LIST ;  }
	;

 /**********************************************************************
	
	destroy <relname1> [, <relname2> .. <relnameN> ]
		( DESTROY "relname1" ["relname2" .. "relnameN"] )

  **********************************************************************/

DestroyStmt:
	  DESTROY relation_name_list		
		{ $$ = lispCons( KW(destroy) ,$2) ; }
	;

 /**********************************************************************

	QUERY:
		fetch [forward | backward] [number | all ] [ in <portalname> ]
	TREE:
		(FETCH <portalname> <dirn> <number> )

  **********************************************************************/
FetchStmt:
	  FETCH opt_direction fetch_how_many opt_portal_name
		{
		    $$ = MakeList ( KW(fetch), $4, $2, $3, -1 );
	        }
	;

fetch_how_many:
	/*EMPTY, default is all*/
		{ $$ = KW(all) ; }
	| NumConst
	| ALL
		{ $$ = KW(all) ; }
	;

 /**********************************************************************
	QUERY:
		move [<dirn>] [<whereto>] [<portalname>]
	TREE:
		( MOVE ["portalname"] <dirn> (TO <whereto> |
  **********************************************************************/

MoveStmt:
	  MOVE opt_direction opt_move_where opt_portal_name
		{ 
		    $$ = MakeList ( KW(move), $4, $2, $3, -1 );
		}
	;

opt_direction:
	/*EMPTY, by default forward */		{ $$ = KW(forward); }
	| FORWARD
		{ $$ = KW(forward); }
	| BACKWARD
		{ $$ = KW(backward); }
	;

opt_move_where: 
	/*EMPTY*/				{ NULLTREE }
	| NumConst				/* $$ = $1 */
	| TO NumConst				
		{ $$ = MakeList ( KW(to), $2, -1 ); }
	| TO record_qual			
		{ $$ = lispString("record quals unimplemented") ; }
	;

opt_portal_name:
	/*EMPTY*/				{ NULLTREE }
	| IN name				{ $$ = $2;}
	;



 /************************************************************

	define index:
	
	define [archive] index <indexname> on <relname>
	  using <access> "(" (<col> with <op>)+ ")" with
	  <target_list>

  ************************************************************/


IndexStmt:
	  DEFINE opt_archive Index index_name ON relation_name
	    Using access_method '(' index_list ')' with_clause
		{
		    /* should check that access_method is valid,
		       etc ... but doesn't */

		    $$ = lispCons($12,LispNil);
		    $$  = lispCons($10,$$);
		    $$  = lispCons($8,$$);
		    $$  = lispCons($4,$$);
		    $$  = lispCons($6,$$);
		    $$  = lispCons(KW(index),$$);
		    $$  = lispCons(KW(define),$$);
		}
	;

 /************************************************************
	QUERY:
		merge <rel1> into <rel2>
	TREE:
		( MERGE <rel1> <rel2> )

	XXX - unsupported in version 1 (outside of parser)

  ************************************************************/
MergeStmt:
	MERGE relation_expr INTO relation_name
		{ 
		    elog(NOTICE, "merge is unsupported in version 1");
		    $$ = MakeList ( $1, $2, $4, -1 );
		}
	;

 /************************************************************
	QUERY:
		define c function ...
		define postquel function <func-name> '(' rel-name ')'
		returns <ret-type> is <postquel-optimizable-stmt>
	TREE:
		(define cfunction) ... XXX - document this

		postquel function:

		(define pfunction "func-name" "rel-name" <ret-type-oid>
		 "postquel-optimizable-stmt")

  ************************************************************/
 
ProcedureStmt:

	DEFINE C_FN FUNCTION def_rest
		{
		   $$ = lispCons (KW(cfunction) , $4 ); 
		   $$ = lispCons (KW(define) , $$ ); 
		}
	| DEFINE POSTQUEL FUNCTION name '(' relation_name ')' 
    	  RETURNS relation_name IS
		{
		   extern char *Ch;
		   char *bogus = NULL;
		   int x = strlen(Ch);

		   QueryIsRule = true;
		   NewOrCurrentIsReally = $6;
		   /*
		    * NOTE: 'CURRENT' must always have a varno
		    * equal to 1 and 'NEW' equal to 2.
		    */
		   ADD_TO_RT ( MakeRangeTableEntry ( CString($6), 
						    LispNil,
						    "*CURRENT*" ) );
		   ADD_TO_RT ( MakeRangeTableEntry ( CString($6), 
						    LispNil,
						    "*NEW*" ));
		   p_last_resno = 4;
		   bogus = (char *) palloc(x+1);
		   bogus = strcpy ( bogus, Ch );
		   $10 = lispString(bogus);
		}
	  OptimizableStmt
		{
		   $$ = lispCons ( $10, LispNil );
		   $$ = lispCons ( $6 , $$ );
		   $$ = lispCons ( $4, $$ );
		   $$ = lispCons ( KW(pfunction), $$ );
		   $$ = lispCons ($1, $$ );
		   QueryIsRule = false;
		}
	;

 /**********************************************************************

	Purge:

	purge <relname> [before <date>] [after <date>]
	  or
	purge <relname>  [after<date>][before <date>] 
	
		(PURGE "relname" ((BEFORE date)(AFTER date)))

  **********************************************************************/

PurgeStmt:
	  PURGE relation_name purge_quals
		{ 
		  $$ = lispCons ( $3, LispNil );
		  $$ = lispCons ( $2, $$ );
		  $$ = lispCons ( KW(purge), $$ );
		}
	;

purge_quals:
	/*EMPTY*/
		{$$=lispList();}
	| before_clause
		{ $$ = lispCons ( $1, LispNil ); }
	| after_clause
		{ $$ = lispCons ( $1, LispNil ); }
	| before_clause after_clause
		{ $$ = MakeList ( $1, $2 , -1 ); }
	| after_clause before_clause
		{ $$ = MakeList ( $2, $1, -1 ); }
	;

before_clause:	BEFORE date		{ $$ = MakeList ( KW(before),$2,-1); }
after_clause:	AFTER date		{ $$ = MakeList ( KW(after),$2,-1 ); }

 /**********************************************************************

	Remove:

	remove function <funcname>
		(REMOVE FUNCTION "funcname")
	remove operator <opname>
		(REMOVE OPERATOR ("opname" leftoperand_typ rightoperand_typ))
	remove type <typename>
		(REMOVE TYPE "typename")
	remove rewrite rule <rulename>
		(REMOVE REWRITE RULE "rulename")
	remove tuple rule <rulename>
		(REMOVE RULE "rulename")

  **********************************************************************/

RemoveStmt:
	REMOVE remove_type name
		{
		    $$ = MakeList ( KW(remove), $2, $3 , -1 ); 
		}
	;

remove_type:
	  Function | Type | Index | RuleType | VIEW;

RuleType:
	 newruleTag RULE
		{ 
			/*
			 * the rule type can be either
			 * P_TYPE or REWRITE
			 */
			$$ = CAR($1);
		}
	;  
RemoveOperatorStmt:
	  REMOVE Operator Op '(' remove_operator ')'
		{
		    $$  = lispCons($3, $5);
		    $$  = lispCons($2, lispCons($$, LispNil));
		    $$  = lispCons( KW(remove) , $$);
		}
        | REMOVE Operator MathOp '(' remove_operator ')'
                {
                    $$  = lispCons($3, $5);
                    $$  = lispCons($2, lispCons($$, LispNil));
                    $$  = lispCons( KW(remove) , $$);
                }
        ;
MathOp:
	| '+'   { $$ = lispString("+"); }
	| '-'   { $$ = lispString("-"); }
	| '*'   { $$ = lispString("*"); }
	| '/'   { $$ = lispString("/"); }
	| '<'   { $$ = lispString("<"); }
	| '>'   { $$ = lispString(">"); }
	| '='   { $$ = lispString("="); }
	;

remove_operator:
	  name
		{ $$ = lispCons($1, LispNil); }
	| name ',' name
		{ $$ = MakeList ( $1, $3, -1 ); }
	;

 /**********************************************************************
	 
	rename <attrname1> in <relname> to <attrname2>
		( RENAME "ATTRIBUTE" "relname" "attname1" "attname2" )
	rename <relname1> to <relname2>
		( RENAME "RELATION" "relname1" "relname2" )
	
  **********************************************************************/

RenameStmt :
	  RENAME attr_name IN relation_name TO attr_name
		{ 
		    $$ = MakeList ( KW(rename), lispString("ATTRIBUTE"),
				    $4, $2, $6, -1 );
		}
	| RENAME relation_name TO relation_name
                {
		    $$ = MakeList ( KW(rename), lispString("RELATION"),
				    $2, $4, -1 );
		}
	;


 /* ADD : Define Rewrite Rule , Define Tuple Rule 
          Define Rule <old rules >
  */

opt_support:
  	'[' NumConst ',' NumConst ']'
		{
		    $$ = lispCons ( $2, $4 );
		}
	| /* EMPTY */
		{
		    $$ = lispCons ( lispFloat(1.0), lispFloat(0.0) );
		}
	 ;

RuleStmt:
	DEFINE newruleTag RULE name opt_support IS
		{
		    p_ruleinfo = lispCons(lispInteger(0),LispNil);
		    p_priority = lispInteger(0) ;
		}
	RuleBody
		{
		    if ( CAtom(CAR($2))==P_TUPLE) {
			/* XXX - do the bogus fix to get param nodes
			         to replace varnodes that have current 
				 in them */
			/*==== XXX
			 * LET THE TUPLE-LEVEL RULE MANAGER DO
			 * THE SUBSTITUTION....
			SubstituteParamForNewOrCurrent ( $8 );
			 *====
			 */
			$$ = lispCons ( $8, lispCons ( p_rtable, NULL ));
			$$ = lispCons ( $4, $$ );
			if (null(CDR($2))) {
			    List t;
			    t = lispCons(LispNil, LispNil);
			    t = lispCons(KW(rule), t);
			    $$ = lispCons(t, $$);
			} else {
			    List t;
			    t = lispCons(CADR($2), LispNil);
			    t = lispCons(KW(rule), t);
			    $$ = lispCons(t, $$);
			}
			$$ = lispCons ( CAR($2), $$ );	
			$$ = lispCons ( KW(define), $$ );	
		    } else {
			$$ = nappend1 ( $8, $5 ); 
			$$ = lispCons ( $4, $$ );
			$$ = lispCons ( KW(rule), $$ );
			$$ = lispCons ( CAR($2), $$ );	
			$$ = lispCons ( KW(define), $$ );	
		    }
		}
	;

/*-------------------------
 * We currently support the following rule tags:
 *
 * 'tuple' i.e. a rule implemented with the tuple level rule system
 * 'rewrite' i.e. a rule implemented with the rewrite system
 * 'relation tuple' a rule implemented with the tuple level rule
 *		system, but which is forced to use a relation
 *		level lock (even if we could have used the
 *		more efficient tuple-level rule lock).
 *		This option is used for debugging...
 * 'tuple tuple' i.e. it must be implemented with a tuple level
 *		lock.
 *
 *  25 feb 1991:  stonebraker is objectifying our terminology.  as a
 *		  result, 'tuples' no longer exist.  they've been replaced
 *		  by 'instances'.  we still use the old terminology inside
 *		  the system, but the visible interface has been changed.
 */
newruleTag: P_TUPLE 
		{ $$ = lispCons(KW(instance), LispNil); }
	| REWRITE 
		{ $$ = lispCons(KW(rewrite), LispNil); }
	| RELATION P_TUPLE
		{
		    $$ = lispCons(KW(relation), LispNil);
		    $$ = lispCons(KW(instance), $$);
		}
	| P_TUPLE P_TUPLE
		{
		    $$ = lispCons(KW(instance), LispNil);
		    $$ = lispCons(KW(instance), $$);
		}
	| /* EMPTY */
		{ $$ = lispCons(KW(instance), LispNil); }
	;

OptStmtList:
	  Id
		{ 
		  if ( ! strcmp(CString($1),"nothing") ) {
			$$ = LispNil;
		  } else
			elog(WARN,"bad rule action %s", CString($1));
		}
	| OptimizableStmt
		{ $$ = lispCons ( $1, LispNil ); }
	| '[' OptStmtBlock ']'
		{ $$ = $2; }
        ;

OptStmtBlock:
	  OptimizableStmt 
		{ ELEMENT ;}
       | OptStmtBlock
               {
                  p_last_resno = 1;
                  p_target_resnos = LispNil;
               }
         OptimizableStmt
               { $$ = nappend1($1, $3); }

	;

RuleBody: 
	ON event TO 
	event_object 
	{ 
	    /* XXX - figure out how to add a from_clause into this
	             rule thing properly.  Spyros/Greg's bogus
		     fix really breaks things, so I had to axe it. */

	    NewOrCurrentIsReally = $4;
	   /*
	    * NOTE: 'CURRENT' must always have a varno
	    * equal to 1 and 'NEW' equal to 2.
	    */
	    ADD_TO_RT ( MakeRangeTableEntry ( CString(CAR($4)), 
					     LispNil,
					     "*CURRENT*" ) );
	    ADD_TO_RT ( MakeRangeTableEntry ( CString(CAR($4)), 
					     LispNil,
					     "*NEW*" ));
	    QueryIsRule = true;
	} 
	where_clause
	DO opt_instead 
	OptStmtList
	{
	    $$ = MakeList ( $2, $4, $6, $8, $9, -1 );
	    /* modify the rangetable entry for "current" and "new" */
	}
	;

event_object: 
	relation_name '.' attr_name
		{ $$ = MakeList ( $1, $3, -1 ); }
	| relation_name
		{ $$ = lispCons ( $1, LispNil ); }
  	;
event:
  	RETRIEVE | REPLACE | DELETE | APPEND ;

opt_instead:
  	INSTEAD				{ $$ = lispInteger((int)true); }
	| /* EMPTY */			{ $$ = lispInteger((int)false); }
	;




 /**************************************************

	Transactions:

	abort transaction
		(ABORT)
	begin transaction
		(BEGIN)
	end transaction
		(END)
	
  **************************************************/
TransactionStmt:
	  ABORT_TRANS TRANSACTION
		{ $$ = lispCons ( KW(abort), LispNil ) ; } 
	| BEGIN_TRANS TRANSACTION
		{ $$ = lispCons ( KW(begin), LispNil ) ; } 
	| END_TRANS TRANSACTION
		{ $$ = lispCons ( KW(end), LispNil ) ; } 
	| ABORT_TRANS
		{ $$ = lispCons ( KW(abort), LispNil ) ; } 
	| BEGIN_TRANS
		{ $$ = lispCons ( KW(begin), LispNil ) ; } 
	| END_TRANS
		{ $$ = lispCons ( KW(end), LispNil ) ; } 
	;

 /**************************************************

	View Stmt  
	define view <viewname> '('target-list ')' [where <quals> ]

   **************************************************/

ViewStmt:
	DEFINE VIEW name 
	RetrieveSubStmt
		{ 
		    $3 = lispCons ( $3 , $4 );
		    $2 = lispCons ( KW(view) , $3 );
		    $$ = lispCons ( KW(define) , $2 );
		}
	;

 /**************************************************

        Load Stmt
        load "filename"

   **************************************************/

LoadStmt:
        LOAD file_name
                {  $$ = MakeList ( KW(load), $2, -1 ); }
        ;

 /**************************************************

        Createdb Stmt
        createdb dbname

   **************************************************/

CreatedbStmt:
        CREATEDB database_name
                {  $$ = MakeList ( KW(createdb), $2, -1 ); }
        ;

 /**************************************************

        Destroydb Stmt
        destroydb dbname

   **************************************************/

DestroydbStmt:
        DESTROYDB database_name
                {  $$ = MakeList ( KW(destroydb), $2, -1 ); }
        ;

 /**********************************************************************
  **********************************************************************

	Optimizable Stmts:

	one of the five queries processed by the planner

	[ultimately] produces query-trees as specified
	in the query-spec document in ~postgres/ref

  **********************************************************************
  **********************************************************************/

OptimizableStmt:	
	  RetrieveStmt
	| ExecuteStmt
	| ReplaceStmt
	| AppendStmt
	| DeleteStmt			/* by default all are $$=$1 */
	;

 /**************************************************

   AppendStmt
   - an optimizable statement, produces a querytree
   as per query spec

  **************************************************/

AppendStmt:
  	  APPEND
                { SkipForwardToFromList(); }
          from_clause
          opt_star relation_name
                {
                   int x = 0;
                   if((x=RangeTablePosn(CString($5),LispNil)) == 0)
		     ADD_TO_RT( MakeRangeTableEntry(CString($5),
						    LispNil,
						    CString($5)));
                  if (x==0)
                    x = RangeTablePosn(CString($5),LispNil);
                  parser_current_rel = amopenr(VarnoGetRelname(x));
                  if (parser_current_rel == NULL)
                        elog(WARN,"invalid relation name");
                  ResdomNoIsAttrNo = true;
                }
          '(' res_target_list ')'
          where_clause
                {
                        LispValue root;
			LispValue command;
                        int x = RangeTablePosn(CString($5),LispNil);

			if ( $4 == LispNil )
			  command = KW(append);
			else
			  command = lispCons( lispInteger('*'),KW(append));

                        root = MakeRoot(1, command , lispInteger(x),
                                        p_rtable,
                                        p_priority, p_ruleinfo,
					LispNil,LispNil,$8);
                        $$ = lispCons ( root , LispNil );
                        $$ = nappend1 ( $$ , $8 ); /* (eq p_target $8) */
                        $$ = nappend1 ( $$ , $10 ); /* (eq p_qual $10 */
                        ResdomNoIsAttrNo = false;
                }
        ;



 /**************************************************

   DeleteStmt
   - produces a querytree that looks like
   that in the query spec

  **************************************************/
   
DeleteStmt:
          DELETE
                { SkipForwardToFromList(); }
          from_clause
          opt_star var_name
                {
		    int x = 0;
		    
		    if((x=RangeTablePosn(CString($5),LispNil)) == 0 )
                      ADD_TO_RT (MakeRangeTableEntry (CString($5),
						      LispNil,
						      CString($5)));
		    
		    if (x==0)
		      x = RangeTablePosn(CString($5),LispNil);
		    
		    parser_current_rel = amopenr(VarnoGetRelname(x));
		    if (parser_current_rel == NULL)
		      elog(WARN,"invalid relation name");
		    /* ResdomNoIsAttrNo = true;  */
		}
          where_clause
                {
		    LispValue root = LispNil;
		    LispValue command = LispNil;
		    int x = 0;

		    x= RangeTablePosn(CString($5),LispNil);
		    if (x == 0)
		      ADD_TO_RT(MakeRangeTableEntry (CString ($5),
						     LispNil,
						     CString($5)));
		    if ( $4 == LispNil )
		      command = KW(delete);
		    else
		      command = lispCons( lispInteger('*'),KW(delete));
		    
		    root = MakeRoot(1,command,
				    lispInteger ( x ) ,
				    p_rtable, p_priority, p_ruleinfo,
				    LispNil,LispNil,LispNil);
		    $$ = lispCons ( root , LispNil );

		    /* check that var_name is in the relation */

		    $$ = nappend1 ( $$ , LispNil ); 	/* no target list */
		    $$ = nappend1 ( $$ , $7 ); 		/* (eq p_qual $7 */
                }
        ;

 /**************************************************
   
   ExecuteStmt
   - produces a querytree that looks like that
   in the queryspec

  **************************************************/

ExecuteStmt:
	  EXECUTE opt_star opt_portal '(' res_target_list ')'
            from_clause with_clause where_clause
		{ 
			$$ = KW(execute);
		  	elog(WARN, "execute does not work in Version 1");
		}
	;

 /**********************************************************************

	Replace:

	- as per qtree specs

  **********************************************************************/

ReplaceStmt:
 	REPLACE 
                { SkipForwardToFromList(); }
        from_clause
        opt_star relation_name
                {
                   int x = 0;
                   if((x=RangeTablePosn(CString($5),LispNil) ) == 0 )
		     ADD_TO_RT( MakeRangeTableEntry (CString($5),
						     LispNil,
						     CString($5)));
                  if (x==0)
                    x = RangeTablePosn(CString($5),LispNil);
                  parser_current_rel = amopenr(VarnoGetRelname(x));
                  if (parser_current_rel == NULL)
                        elog(WARN,"invalid relation name");
                  ResdomNoIsAttrNo = true; }
        '(' res_target_list ')' where_clause
                {
                    LispValue root;
		    LispValue command = LispNil;
                    int result = RangeTablePosn(CString($5),LispNil);
                    if (result < 1)
                      elog(WARN,"parser internal error , bogus relation");
		    if ( $4 == LispNil )
		      command = KW(replace);
		    else
		      command = lispCons( lispInteger('*'),KW(replace));

                    root = MakeRoot(1, command , lispInteger(result),
                                    p_rtable,
                                    p_priority, p_ruleinfo,
				    LispNil,LispNil,$8);

                    $$ = lispCons( root , LispNil );
                    $$ = nappend1 ( $$ , $8 );          /* (eq p_target $6) */
                    $$ = nappend1 ( $$ , $10 );         /* (eq p_qual $9) */
                    ResdomNoIsAttrNo = false;
                }
        ;


		 
 /************************************************************

	Retrieve:

	( ( <NumLevels> RETRIEVE <result> <rtable> <priority>
	    <RuleInfo> )
	  (targetlist)
	  (qualification) )

  ************************************************************/

RetrieveStmt:
	RETRIEVE 
	RetrieveSubStmt
		{ $$ = $2 ; }
	;

RetrieveSubStmt:
	  { SkipForwardToFromList(); }
	  from_clause 
	  opt_star result opt_unique 
	  '(' res_target_list ')'
 	  where_clause ret_opt2 
  		{
		/* XXX - what to do with opt_unique, from_clause
		 */	
		    LispValue root;
		    LispValue command;

		    if ( $3 == LispNil )
		      command = KW(retrieve);
		    else
		      command = lispCons( lispInteger('*'),KW(retrieve));

		    root = MakeRoot(NumLevels,command, $4, p_rtable,
				    p_priority, p_ruleinfo,$5,$10,$7);
		    

		    $$ = lispCons( root , LispNil );
		    $$ = nappend1 ( $$ , $7 );	/* (eq p_target $8) */
		    $$ = nappend1 ( $$ , $9 );	        /* (eq p_qual $10) */
		}

 /**************************************************

	Retrieve result:
	into <relname>
		(INTO "relname");
	portal <portname>
		(PORTAL "portname")
	NULL
		nil

  **************************************************/
result:
	  INTO relation_name
		{
			$2=lispCons($2 , LispNil );
			/* should check for archive level */
			$$=lispCons(KW(into), $2);
		}
	| opt_portal
		/* takes care of the null case too */
	;

opt_unique:
	/*EMPTY*/
		{ NULLTREE }
	| UNIQUE
		{ $$ = KW(unique); }
	;

ret_opt2: 
	/*EMPTY*/
		{ NULLTREE }
	| Sort BY sortby_list
		{ $$ = $3 ; }
	;

sortby_list:
	  sortby				{ ELEMENT ; }
	| sortby_list ',' sortby		{ INC_LIST ;  }
	;

sortby:
	  Id OptUseOp
		{ $$ = lispCons ( $1, lispCons ($2, LispNil )) ; }
	| attr OptUseOp
                { /* i had to do this since a bug in yacc wouldn't catch */
                  /* this syntax error - ron choi 4/11/91 */
                  yyerror("syntax error: use \"sort by attribute_name\"");
                }

	;



opt_archive:
		{ NULLTREE }
	| ARCHIVE 
		{ $$ = KW(archive); }
	;

index_list:
	  index_list ',' index_elem		{ INC_LIST ;  }
	| index_elem				{ ELEMENT ; }
	;

index_elem:
	  attr_name opt_class
		{ $$ = nappend1(LispNil,$1); nappend1($$,$2);}
	;
opt_class:
	  /* empty */				{ NULLTREE; }
	| class
	| With class
		{$$=$2;}
	;

star:
	  '*'			
	;
		
  
opt_star:
	/*EMPTY*/		{ NULLTREE }
	| star
  	;

relation_name_list:	name_list ;

name_list: 
	  name 				{ ELEMENT ; }
	| name_list ',' name		{ INC_LIST ;  }
	;	

 /**********************************************************************
   
   clauses common to all Optimizable Stmts:
   	from_clause 	-
       	where_clause 	-
	
  **********************************************************************/
with_clause:
	/* NULL */				{ NULLTREE }
	| With '(' param_list ')'		{ $$ = MakeList ( $1,$3,-1 ); }
	;

param_list: 		
	  param_list ',' param			{ INC_LIST ;  }
	| param					{ ELEMENT ; }
	;

param: 			
	  Id '=' string 
		{ $$ = MakeList ( $1, $3, -1 ); }
	;

from_clause:
	 FROM from_list 			
		{ 
			$$ = $2 ; 
			SkipBackToTlist();
			yychar = -1;
			/* goto yynewstate; */
		}
	| /*empty*/				{ NULLTREE ; }
	;

from_list:
	  from_list ',' from_val 		{ INC_LIST ;  }
	| from_val				{ ELEMENT ; }
	;

from_val:
	  var_list IN relation_expr
		{
		    extern List MakeFromClause ();
		    $$ = MakeFromClause ( $1, $3 );
		}	
	;
var_list:
	  var_list ',' var_name			{ INC_LIST ;  }
	| var_name				{ ELEMENT ; }
	;

where_clause:
	/* empty - no qualifiers */
	        { NULLTREE } 
	| WHERE boolexpr
		{ $$ = $2; p_qual = $$; }
	;
opt_portal:
  	/* common to retrieve, execute */
	/*EMPTY*/
		{ NULLTREE }
	| PORTAL name 
		{
		    $$ = MakeList ( KW(portal), $2, -1 ); 
		}
	;

OptUseOp:
	  /*EMPTY*/			{ NULLTREE }
	| USING Op			{ $$ = $2; }
	| USING '<'			{ $$ = lispString("<"); }
	| USING '>'			{ $$ = lispString(">"); }
	;

relation_expr:
	  relation_name
  		{ 
		    /* normal relations */
		    $$ = lispCons ( MakeList ( $1, LispNil , LispNil , -1 ),
				   LispNil );
		}
	| relation_expr '|' relation_expr %prec '='
		{ 
		    /* union'ed relations */
		    elog ( DEBUG, "now consing the union'ed relations");
		    $$ = append ( $1, $3 );
		}
	| relation_name '*'
		{ 
		    /* inheiritance query */
		    $$ = lispCons ( MakeList ( $1, LispNil, 
					      lispString("*"), -1) ,
				   LispNil );
		}
	| relation_name time_range 
		{ 
		    /* time-qualified query */
		    $$ = lispCons ( MakeList ( $1, $2, LispNil , -1 ) ,
				    LispNil );
		}
	| '(' relation_expr ')'
		{ 
		    elog( DEBUG, "now reducing by parentheses");
		    $$ = $2; 
		}
	;

	  
time_range:
	  '[' opt_range_start ',' opt_range_end ']'
        	{ 
		    $$ = MakeList ( $2, $4, lispInteger(1), -1 );
		}
	| '[' date ']'
		{ 
		    $$ = MakeList ( $2 , LispNil, lispInteger(0), -1 );
		}
        ;

opt_range_start:
	  /* no date, default to nil */
		{ $$ = lispString("epoch"); }
	| date
	;

opt_range_end:
	  /* no date, default to nil */
		{ $$ = lispString("now"); }
	| date
	;
 /**********************************************************************

   Boolean Expressions - qualification clause

	XXX - for each left, right side of boolean op,
	need to check that qualification is indeed boolean

  **********************************************************************/

boolexpr:
	  a_expr
		{ $$ = CDR($1); }
	| boolexpr AND boolexpr
		{ $$ = MakeList ( lispInteger(AND) , $1 , $3 , -1 ); }
	| boolexpr OR boolexpr
		{ $$ = MakeList ( lispInteger(OR) , $1, $3, -1 ); }
	| NOT boolexpr
		{ $$ = MakeList ( lispInteger(NOT) , $2, -1 ); }
	| '(' boolexpr ')'
		{ $$ = $2; }
	;

record_qual:
	/*EMPTY*/
		{ NULLTREE }
	;


 /* 
  * (tuple) variable definition(s)
  * normal tuple vars are defined as '(name type)
  * array tuple vars are defined as '(name type size)
  * if size = VARLENGTH = -1, then the array is of variable length
  * otherwise, the size is an integer from 1 to MAXINT 
  */

opt_array_bounds:
  	  '[' ']'
		{ 
#ifdef REAL_ARRAYS		    
		    $$ = lispCons (lispInteger(-1),LispNil);
#else
		    $$ = (LispValue)"[]";
#endif
		}
	| '[' Iconst ']'
		{
		    /* someday, multidimensional arrays will work */
#ifdef REAL_ARRAYS
		    $$ = lispCons ($2, LispNil);
#else
		    char *bogus = (char *) palloc(20);
		    int retval;
		    retval = (int) sprintf (bogus, "[%d]", CInteger($2));
		    $$ = (LispValue)bogus;
#endif
		}
	| /* EMPTY */
		{ NULLTREE }
	;

Typename:
  	  name opt_array_bounds
		{ 
#ifdef REAL_ARRAYS
		    $$ = lispCons($1,$2);
#else
		    char *bogus = (char *) palloc(40);
		    int retval;
		    if ($2 != (LispValue)NULL) {
		      retval = (int) sprintf(bogus,"%s%s", CString($1), $2);
		      $$ = lispString(bogus);
		    } else
		      $$ = $1;
#endif
		}
	;

var_def: 	
	  Id '=' Typename 
		{ 
		    $$ = MakeList($1,$3,-1);
		}
	;

var_defs:
	  var_defs ',' var_def		{ INC_LIST ;  }
	| var_def			{ ELEMENT ; }
	;

opt_var_defs: 
	  var_defs
	| {NULLTREE} ;


 /* 
  * expression grammar, still needs some cleanup
  */

a_expr:
	  attr
		{
		    Var temp = (Var)NULL;
		    temp =  (Var)CDR ( make_var ( CString(CAR ($1)) , 
					    CString(CADR($1)) ));

		    $$ = (LispValue)temp;

		    if (CurrentWasUsed) {
			$$ = (LispValue)MakeParam ( PARAM_OLD , 
						   get_varattno((Var)temp), 
						   CString(CDR($1)),
						   get_vartype((Var)temp));
			CurrentWasUsed = false;
		    }

                    if (NewWasUsed) {
			$$ = (LispValue)MakeParam ( PARAM_NEW , 
						   get_varattno((Var)temp), 
						   CString(CDR($1)),
						   get_vartype((Var)temp));
			NewWasUsed = false;
		    }
		    if (IsA(temp,Var)) {
			set_vardotfields ( temp , CDR(CDR($1)));
			$$ = lispCons ( lispInteger (get_vartype((Var)temp)),
				       $$ );
		    } else if (IsA(temp,LispList) ) {
			$$ = lispCons ( lispInteger 
				          (get_vartype((Var)CADR((List)temp))),
				       $$ );
		    } else {
			elog(WARN, "Var or union expected, not found");
		    }
			
		}
	| AexprConst		
	| attr optional_indirection
		{ 
		     $$ = (List)make_array_ref_var ( CString(CAR($1)),
						    CString(CADR($1)),
						    $2);
		}
	| spec  { Typecast_ok = false; }
	| '-' a_expr %prec UMINUS
  		{ $$ = make_op(lispString("-"),$2, LispNil);
		  Typecast_ok = false; }
	| a_expr '+' a_expr
		{ $$ = make_op (lispString("+"), $1, $3 ) ;
		  Typecast_ok = false; }
	| a_expr '-' a_expr
		{ $$ = make_op (lispString("-"), $1, $3 ) ;
		  Typecast_ok = false; }
	| a_expr '/' a_expr
		{ $$ = make_op (lispString("/"), $1, $3 ) ;
		  Typecast_ok = false; }
	| a_expr '*' a_expr
		{ $$ = make_op (lispString("*"), $1, $3 ) ;
		  Typecast_ok = false; }
	| a_expr '<' a_expr
		{ $$ = make_op (lispString("<"), $1, $3 ) ;
		  Typecast_ok = false; }
	| a_expr '>' a_expr
		{ $$ = make_op (lispString(">"), $1, $3 ) ;
		  Typecast_ok = false; }
	| a_expr '=' a_expr
		{ $$ = make_op (lispString("="), $1, $3 ) ;
		  Typecast_ok = false; }
	| AexprConst TYPECAST Typename
		{ 
		    extern LispValue parser_typecast();
		    $$ = parser_typecast ( $1, $3 );
		    Typecast_ok = false; /* it's already typecasted */
					 /* don't do it again. */ }
	| '(' a_expr ')'
		{$$ = $2;}
	/* XXX Or other stuff.. */
	| a_expr Op a_expr
		{ $$ = make_op ( $2, $1 , $3 );
		  Typecast_ok = false; }
	| relation_name
		{ $$ = lispCons ( KW(relation), $1 );
		  Typecast_ok = false; }
	| name '(' expr_list ')'
		{
			extern List ParseFunc();
			$$ = ParseFunc ( CString ( $1 ), $3 ); 
			Typecast_ok = false; }
	|  ARRAY TYPECAST Typename
               {
                        extern List ParseArrayList();
                        $$ = ParseArrayList ( $1, $3 );  
			Typecast_ok = false; }
	;

optional_indirection:
	  '[' Iconst ']'
		{ $$ = lispCons($2, LispNil);}
	|  optional_indirection '[' Iconst ']'
		{ $$ = nappend1($1, $3);}

expr_list:
	|  a_expr				{ ELEMENT ; }
	|  expr_list ',' a_expr		{ INC_LIST ; }
	;
attr:
	  relation_name '.' attr_name
		{    
		    INC_NUM_LEVELS(1);		
		    if( RangeTablePosn ( CString ($1),LispNil ) == 0 )
		      ADD_TO_RT( MakeRangeTableEntry (CString($1) ,
						      LispNil, 
						      CString($1)));
		    $$ = MakeList ( $1 , $3 , -1 );
		}
	| attr '.' attr_name
		{
		    $$ = nappend1 ( $1, $3 ); 
		}
	;


res_target_list:
	  res_target_list ',' res_target_el	
		{ p_target = nappend1(p_target,$3); $$ = p_target;}
	| res_target_el 			
		{ p_target = lispCons ($1,LispNil); $$ = p_target;}
	| relation_name '.' ALL
		{
			LispValue temp = p_target;
			INC_NUM_LEVELS(1);
			if(temp == LispNil )
			  p_target = ExpandAll(CString($1), &p_last_resno);
			else {
			  while (temp != LispNil && CDR(temp) != LispNil)
			    temp = CDR(temp);
			  CDR(temp) = ExpandAll( CString($1), &p_last_resno);
			}
			$$ = p_target;
		}
	| res_target_list ',' relation_name '.' ALL
		{
			LispValue temp = p_target;
			if (ResdomNoIsAttrNo == true) {
			  elog(WARN,"all doesn't make any sense here");
			  return(1);
			}
			INC_NUM_LEVELS(1);
			if(temp == LispNil )
			  p_target = ExpandAll(CString($3), &p_last_resno);
			else {
			  while(temp != LispNil && CDR(temp) != LispNil )
			    temp = CDR(temp);
			  CDR(temp) = ExpandAll( CString($3), &p_last_resno);
			}
			$$ = p_target;
		}
	;

res_target_el:
	  Id '=' a_expr
		{
		    $$ = make_targetlist_expr ($1,$3);
		}

	| attr
		{
		      LispValue varnode, temp;
		      Resdom resnode;
		      int type_id, type_len;

		      temp = make_var ( CString(CAR($1)) , CString(CADR($1)));
		      type_id = CInteger(CAR(temp));
		      type_len = tlen(get_id_type(type_id));
		      resnode = MakeResdom ( p_last_resno++ ,
						type_id, type_len, 
						CString(CAR(last ($1) ))
					    , 0 , 0 , 0 );
		      varnode = CDR(temp);
		      if ( IsA(varnode,Var))
			set_vardotfields ( varnode , CDR(CDR($1)));
		      else if ( CDR(CDR($1)) != LispNil )
			elog(WARN,"cannot mix procedures with unions");

		      $$ = lispCons(resnode,lispCons(varnode,LispNil));
		}
	| attr optional_indirection
		{
		      LispValue varnode, temp;
		      Resdom resnode;
		      int type_id, type_len;

		      temp = (LispValue) make_array_ref_var ( CString(CAR($1)),
							      CString(CADR($1)),
							      $2);
		      type_id = CInteger(CAR(temp));
		      type_len = tlen(get_id_type(type_id));
		      resnode = MakeResdom ( p_last_resno++ ,
						type_id, type_len, 
						CString(CAR(last ($1) ))
					    , 0 , 0 , 0 );
		      varnode = CDR(temp);
		      if ( IsA(varnode,Var))
			set_vardotfields ( varnode , CDR(CDR($1)));
		      else if ( CDR(CDR($1)) != LispNil )
			elog(WARN,"cannot mix procedures with unions");

		      $$ = lispCons(resnode,lispCons(varnode,LispNil));
		}
	;		 

opt_id:
	/*EMPTY*/		{ NULLTREE }
	| Id
	;

relation_name:
	SpecialRuleRelation
	| Id		/* $$ = $1 */;
	;

database_name:	Id	/*$$=$1*/;
access_method: 		Id 		/*$$=$1*/;
adt_name:		Id		/*$$=$1*/;
attr_name: 		Id		/*$$=$1*/;
class: 			Id		/*$$=$1*/;
index_name: 		Id		/*$$=$1*/;
map_rel_name:		Id		/*$$=$1*/;
var_name:		Id		/*$$=$1*/;
name:			Id		/*$$-$1*/;
string: 		Id		/*$$=$1 Sconst ?*/;

date:			Sconst		/*$$=$1*/;
file_name:		SCONST		{$$ = new_filestr($1); };
copy_file_name:
          SCONST		{$$ = new_filestr($1); };
	| STDIN				{ $$ = lispString(CppAsString(stdin)); }
	| STDOUT			{ $$ = lispString(CppAsString(stdout)); }
 /* adt_const:		Sconst		/*$$=$1*/;
attach_args:		Sconst		/*$$=$1*/;
			/* XXX should be converted by fmgr? */
spec:
	  adt_name '[' '$' spec_tail ']'
		{ $$ = (LispValue)MakeParam( $4 ) ; }
	;

spec_tail:
	  Iconst			/*$$ = $1*/
	| '.' Id			{ $$ = $2 ; }
	;

AexprConst:
	  Iconst			{ $$ = make_const ( $1 ) ; 
					  Input_is_integer = true; }
	| FCONST			{ $$ = make_const ( $1 ) ; }
	| CCONST			{ $$ = make_const ( $1 ) ; }
	| Sconst 			{ $$ = make_const ( $1 ) ; 
					  Input_is_string = true; }
	| Pnull			 	{ $$ = LispNil; 
					  Typecast_ok = false; }
	;

NumConst:
	  Iconst			/*$$ = $1 */
	| FCONST 			{ $$ = yylval; }
	;

Iconst: 	ICONST			{ $$ = yylval; }
Sconst:		SCONST			{ $$ = yylval; }

Id:
	IDENT           { $$ = yylval; }
	;

SpecialRuleRelation:
	CURRENT
		{ 
		    if (QueryIsRule) {
		      $$ = lispString("*CURRENT*");
		      /* CurrentWasUsed = true; */
		    } else 
		      yyerror("\"current\" used in non-rule query");
		}
	| NEW
		{ 
		    if (QueryIsRule) {
		      $$ = lispString("*NEW*");
		      /* NewWasUsed = true; */
		    } else 
		      elog(WARN,"NEW used in non-rule query"); 
		    
		}
	;

Function:		FUNCTION	{ $$ = yylval ; } ;
Index:			INDEX		{ $$ = yylval ; } ;
Indexable:		INDEXABLE	{ $$ = yylval ; } ;
Key:			KEY		{ $$ = yylval ; } ;
Nonulls:		NONULLS		{ $$ = yylval ; } ;
Operator:		OPERATOR	{ $$ = yylval ; } ;
Sort:			SORT		{ $$ = yylval ; } ;
Type:			P_TYPE		{ $$ = yylval ; } ;
Using:			USING		{ $$ = yylval ; } ;
With:			WITH		{ $$ = yylval ; } ;
Pnull:			PNULL		{ $$ = yylval ; } ;

%%
parser_init()
{
	NumLevels = 0;
	p_target = LispNil;
	p_qual = LispNil;
	p_root = LispNil;
	p_priority = lispInteger(0);
	p_ruleinfo = LispNil;
	p_rtable = LispNil;
	p_last_resno = 1;
	p_numlevels = 0;
	p_target_resnos = LispNil;
	ResdomNoIsAttrNo = false;
	QueryIsRule = false;
	Input_is_string = false;
	Input_is_integer = false;
	Typecast_ok = true;
}


LispValue
make_targetlist_expr ( name , expr )
     LispValue name;
     LispValue expr;
{
    extern bool ResdomNoIsAttrNo;
    extern Relation parser_current_rel;
    int type_id,type_len, attrtype, attrlen;
    int resdomno;
    Relation rd;

    if(ResdomNoIsAttrNo && (expr == LispNil) && !Typecast_ok){
       /* expr is NULL, and the query is append or replace */
       /* append, replace work only on one relation,
          so multiple occurence of same resdomno is bogus */
       rd = parser_current_rel;
       Assert(rd != NULL);
       resdomno = varattno(rd,CString(name));
       attrtype = att_typeid(rd,resdomno);
       attrlen = tlen(get_id_type(attrtype));
       expr = lispCons( lispInteger(attrtype),
                MakeConst(attrtype, attrlen, LispNil, 1));
       Input_is_string = false;
       Input_is_integer = false;
       Typecast_ok = true;
       if( lispAssoc( lispInteger(resdomno),p_target_resnos)
          != -1 ) {
           elog(WARN,"two or more occurence of same attr");
       } else {
           p_target_resnos = lispCons( lispInteger(resdomno),
                                     p_target_resnos);
       }
       return  ( lispCons (MakeResdom (resdomno,
                                         attrtype,
                                        attrlen ,
                                         CString(name), 0 , 0 , 0 ) ,
                             lispCons((Var)CDR(expr),LispNil)) );
     }
 
    type_id = CInteger(CAR(expr));
    type_len = tlen(get_id_type(type_id));
    if (ResdomNoIsAttrNo) { /* append or replace query */
	/* append, replace work only on one relation,
	   so multiple occurence of same resdomno is bogus */
	rd = parser_current_rel;
	Assert(rd != NULL);
	resdomno = varattno(rd,CString(name));
	attrtype = att_typeid(rd,resdomno);
	attrlen = tlen(get_id_type(attrtype)); 

	if(Input_is_string && Typecast_ok){
              Datum val;
              if (CInteger(CAR(expr)) == typeid(type("unknown"))){
                val = textout(get_constvalue(CDR(expr)));
              }else{
                val = get_constvalue(CDR(expr));
              }
              CDR(expr) = (LispValue) MakeConst(attrtype, attrlen,
                fmgr(typeid_get_retinfunc(attrtype),val,get_typelem(attrtype)),
                0);
	} else if((Typecast_ok) && (attrtype != type_id)){
              CDR(expr) = (LispValue) 
			parser_typecast2 ( expr, get_id_type((long)attrtype));
	} else 
	     if (attrtype != type_id)
		elog(WARN, "unequal type in tlist : %s \n", CString(name));
	Input_is_string = false;
	Input_is_integer = false;
        Typecast_ok = true;

	if( lispAssoc( lispInteger(resdomno),p_target_resnos) 
	   != -1 ) {
	    elog(WARN,"two or more occurence of same attr");
	} else {
	    p_target_resnos = lispCons( lispInteger(resdomno),
				       p_target_resnos);
	}
    } else {
	resdomno = p_last_resno++;
	attrtype = type_id;
	attrlen = type_len;
    }
    return  ( lispCons (MakeResdom (resdomno,
					  attrtype,
					  attrlen , 
					  CString(name), 0 , 0 , 0 ) ,
			      lispCons((Var)CDR(expr),LispNil)) );
    
}

