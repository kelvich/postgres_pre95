%{
#define YYDEBUG 1
/**********************************************************************

  $Header$

  POSTGRES YACC rules/actions
  
  Conventions used in coding this YACC file:

  CAPITALS are used to represent terminal symbols.
  non-capitals are used to represent non-terminals.

 **********************************************************************/

#include <strings.h>
#include <ctype.h>

#include "tmp/c.h"
#include "catalog_utils.h"
#include "catalog/catname.h"
#include "catalog/pg_log.h"
#include "catalog/pg_magic.h"
#include "catalog/pg_time.h"
#include "catalog/pg_variable.h"
#include "access/heapam.h"
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
extern List MakeList();
extern List FlattenRelationList();
extern List ParseAgg();
extern LispValue make_array_ref();

#define ELEMENT 	yyval = nappend1( LispNil , yypvt[-0] )

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

int NumLevels = 0;

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

%token  ALL ALWAYS AFTER AND ARCHIVE ARCH_STORE ARG ASCENDING BACKWARD BEFORE
	BINARY BY DEMAND DESCENDING EMPTY FORWARD FROM HEAVY INTERSECT INTO
	IN INDEX INDEXABLE INHERITS INPUTPROC IS KEY LEFTOUTER LIGHT STORE
	MERGE NEVER NEWVERSION NONE NONULLS NOT PNULL ON ONCE OR OUTPUTPROC
        PORTAL PRIORITY QUEL RIGHTOUTER RULE SCONST SETOF SORT TO TRANSACTION
        UNION UNIQUE USING WHERE WITH FUNCTION OPERATOR P_TYPE


/* Special keywords */

%token  IDENT SCONST ICONST Op CCONST FCONST

/* add tokens here */

%token   	INHERITANCE VERSION CURRENT NEW THEN DO INSTEAD VIEW
		REWRITE P_TUPLE TYPECAST P_FUNCTION C_FUNCTION C_FN
		POSTQUEL RELATION RETURNS INTOTEMP LOAD CREATEDB DESTROYDB
		STDIN STDOUT VACUUM PARALLEL AGGREGATE NOTIFY LISTEN 
                IPORTAL ISNULL NOTNULL
                

/* precedence */
%nonassoc Op
%right 	'='
%left	OR
%left	AND
%right	NOT
%nonassoc NOTNULL
%nonassoc ISNULL
%left  	'+' '-'
%left  	'*' '/'
%left	'|'		/* this is the relation union op, not logical or */
%right  ';' ':'		/* Unary Operators      */
%right   UMINUS
%nonassoc  '<' '>'
%left	'.'
%left  	'[' ']' 
%nonassoc TYPECAST
%nonassoc REDUCE
%token PARAM AS
%%

queryblock:
	query 
		{ parser_init(param_type_info, pfunc_num_args); }
	queryblock
		{ parsetree = lispCons ( $1 , parsetree ) ; }
	| query
		{ 
		    parsetree = lispCons($1,LispNil) ; 
		    parser_init(param_type_info, pfunc_num_args);
		}
	;
query:
	stmt
	| PARALLEL ICONST stmt
		{
		    $$ = nappend1($3, $2);
		}
	;

stmt :
	  AttributeAddStmt
	| ATTACH_AS attach_args /* what are the args ? */
	| AggregateStmt
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
        | ListenStmt
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
	| VacuumStmt
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

/* create a relation */
CreateStmt:
	  CREATE relation_name '(' opt_var_defs ')' 
	  OptKeyPhrase OptInherit OptIndexable OptArchiveType
	  OptLocation OptArchiveLocation
		{

		     LispValue temp = LispNil;

		     $11 = lispCons ($11, LispNil);
		     $10 = lispCons ($10, $11);
		     $9 = lispCons ( $9, $10);
		     $8 = lispCons ( $8, $9 );
		     $7 = lispCons ( $7, $8 );
		     $6 = lispCons ( $6, $7 );
		     
		     $2 = lispCons ( $2, LispNil );
		     $$ = lispCons ( KW(create), $2 );
		     
		     $$ = nappend1 ( $$, $6 );
		     temp = $$;

		     while (CDR(temp) != LispNil) 
		       temp = CDR(temp);

		     CDR(temp) = $4;
		}
	| CREATE OptDeltaDirn NEWVERSION relation_name FROM 
          relation_name_version
		{   
		    $$ = MakeList ( $2, $4, $6,-1 );
		}
	;
relation_name_version:
	  relation_name
	| relation_name '[' date ']'	{ $$ = MakeList( $1, $3, -1); }
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

OptLocation:
	/*EMPTY*/				{ NULLTREE }
	| STORE '=' Sconst
		{
		    int which;

		    which = smgrin(LISPVALUE_STRING($3));
		    $$ = lispCons(KW(store), lispInteger(which));
		}
	;

OptArchiveLocation:
	/*EMPTY*/				{ NULLTREE }
	| ARCH_STORE '=' Sconst
		{
		    int which;

		    which = smgrin(LISPVALUE_STRING($3));
		    $$ = lispCons(KW(arch_store), lispInteger(which));
		}
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
	| SETOF Id		{ $$ = lispCons(KW(setof), $2); }
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
	    Using access_method '(' index_params ')' with_clause where_clause
		{
		    /* should check that access_method is valid,
		       etc ... but doesn't */

		    $$ = MakeList ( $6,$4,$8,$10,$12,$13,-1 );
		    $$ = lispCons(KW(index),$$);
		    $$ = lispCons(KW(define),$$);
		}
	;

 /************************************************************
	QUERY:
		merge <rel1> into <rel2>
	TREE:
		( MERGE <rel1> <rel2> )

	XXX - unsupported in version 3 (outside of parser)

  ************************************************************/
MergeStmt:
	MERGE relation_expr INTO relation_name
		{ 
		    $$ = MakeList ( $1, $2, $4, -1 );
		    elog(WARN, "merge is unsupported in version 4");
		}
	;

 /************************************************************
	QUERY:
                define function <fname>
                       (language = <lang>, returntype = <typename> 
                        [, arch_pct = <percentage | pre-defined>]
                        [, disk_pct = <percentage | pre-defined>]
                        [, byte_pct = <percentage | pre-defined>]
                        [, perbyte_cpu = <int | pre-defined>]
                        [, percall_cpu = <int | pre-defined>]
                        [, iscachable])
                        [arg is (<type-1> { , <type-n>})]
                        as <filename or code in language as appropriate>

  ************************************************************/

ProcedureStmt:

      DEFINE FUNCTION def_rest AS SCONST
                {
		    $$ = lispCons($5, LispNil);
		    $$ = lispCons($3, $$);
		    $$ = lispCons(KW(function), $$);
		    $$ = lispCons(KW(define), $$);
		};
AggregateStmt:

	DEFINE AGGREGATE def_rest 
	{
	    $$ = lispCons( KW(aggregate), $3);
	    $$ = lispCons( KW(define), $$);
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
	  Function | Aggregate | Type | Index | RuleType | VIEW ;

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
		  p_target = LispNil;
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
	    ADD_TO_RT ( MakeRangeTableEntry ( (Name)CString(CAR($4)), 
					     LispNil,
					     (Name)"*CURRENT*" ) );
	    ADD_TO_RT ( MakeRangeTableEntry ( (Name)CString(CAR($4)), 
					     LispNil,
					     (Name)"*NEW*" ));
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


/* NOTIFY <relation_name>  can appear both in rule bodies and
  as a query-level command */
NotifyStmt: NOTIFY relation_name 
            {
		LispValue root;
		int x = 0;
		    
		if((x=RangeTablePosn(CString($2),LispNil)) == 0 )
		  ADD_TO_RT (MakeRangeTableEntry ((Name)CString($2),
						  LispNil,
						  (Name)CString($2)));
		    
		if (x==0)
		  x = RangeTablePosn(CString($2),LispNil);
		
		parser_current_rel = heap_openr(VarnoGetRelname(x));
		if (parser_current_rel == NULL)
		  elog(WARN,"notify: relation %s doesn't exist",$2);
		else
		  heap_close(parser_current_rel);

		root = MakeRoot(1,
				KW(notify),
				lispInteger(x), /* result relation */
				p_rtable, /* range table */
				p_priority, /* priority */
				p_ruleinfo, /* rule info */
				LispNil, /* unique flag */
				LispNil, /* sort clause */
				LispNil); /* target list */
		$$ = lispCons ( root , LispNil );
		$$ = nappend1 ( $$ , LispNil ); /*  */
		$$ = nappend1 ( $$ , LispNil ); /*  */
	    }
;

ListenStmt: LISTEN relation_name 
            {
		parser_current_rel = heap_openr($2);
		if (parser_current_rel == NULL)
		  elog(WARN,"listen: relation %s doesn't exist",$2);
		else
		  heap_close(parser_current_rel);

		$$ = MakeList(KW(listen),$2,-1);
	    }
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

 /**************************************************

        Vacuum Stmt
	vacuum

   **************************************************/

VacuumStmt:
	VACUUM
		{ $$ = MakeList(KW(vacuum), -1); }
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
        | NotifyStmt
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
		     ADD_TO_RT( MakeRangeTableEntry((Name)CString($5),
						    LispNil,
						    (Name)CString($5)));
                  if (x==0)
                    x = RangeTablePosn(CString($5),LispNil);
                  parser_current_rel = heap_openr(VarnoGetRelname(x));
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
			heap_close(parser_current_rel);
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
                      ADD_TO_RT (MakeRangeTableEntry ((Name)CString($5),
						      LispNil,
						      (Name)CString($5)));
		    
		    if (x==0)
		      x = RangeTablePosn(CString($5),LispNil);
		    
		    parser_current_rel = heap_openr(VarnoGetRelname(x));
		    if (parser_current_rel == NULL)
		      elog(WARN,"invalid relation name");
		    else
		      heap_close(parser_current_rel);
		    /* ResdomNoIsAttrNo = true;  */
		}
          where_clause
                {
		    LispValue root = LispNil;
		    LispValue command = LispNil;
		    int x = 0;

		    x= RangeTablePosn(CString($5),LispNil);
		    if (x == 0)
		      ADD_TO_RT(MakeRangeTableEntry ((Name)CString ($5),
						     LispNil,
						     (Name)CString($5)));
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
		     ADD_TO_RT( MakeRangeTableEntry ((Name)CString($5),
						     LispNil,
						     (Name)CString($5)));
                  if (x==0)
                    x = RangeTablePosn(CString($5),LispNil);
                  parser_current_rel = heap_openr(VarnoGetRelname(x));
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
		    heap_close(parser_current_rel);
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
	  result opt_unique 
	  '(' res_target_list ')'
 	  where_clause ret_opt2 
  		{
		/* XXX - what to do with opt_unique, from_clause
		 */	
		    LispValue root;
		    LispValue command;

		    command = KW(retrieve);

		    root = MakeRoot(NumLevels,command, $3, p_rtable,
				    p_priority, p_ruleinfo,$4,$9,$6);
		    

		    $$ = lispCons( root , LispNil );
		    $$ = nappend1 ( $$ , $6 );
		    $$ = nappend1 ( $$ , $8 );
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

index_params:
	  index_list				/* $$=$1 */
	| func_index				{ ELEMENT ; }
	;
index_list:
	  index_list ',' index_elem		{ INC_LIST ;  }
	| index_elem				{ ELEMENT ; }
	;

func_index:
	  ind_function opt_class
		{ $$ = nappend1(LispNil,$1); nappend1($$,$2); }
	  ;

ind_function:
	  name '(' name_list ')' 
		{ $$ = nappend1(LispNil,$1); rplacd($$,$3); }
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

/*
 *  jimmy bell-style recursive queries aren't supported in the
 *  current system.
 */

opt_star:
	/*EMPTY*/		{ NULLTREE }
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
	| WHERE a_expr
	    {
		if (CInteger(CAR($2)) != BOOLOID)
		    elog(WARN,
			 "where clause must return type bool, not %s",
			 tname(get_id_type(CInteger(CAR($2)))));
		p_qual = $$ = CDR($2);
	    }
	;

opt_portal:
  	/* common to retrieve, execute */
	/*EMPTY*/
		{ NULLTREE }
        | IPORTAL name 
		{
		    /*
		     *  15 august 1991 -- since 3.0 postgres does locking
		     *  right, we discovered that portals were violating
		     *  locking protocol.  portal locks cannot span xacts.
		     *  as a short-term fix, we installed the check here. 
		     *				-- mao
		     */
		    if (!IsTransactionBlock())
			elog(WARN, "Named portals may only be used in begin/end transaction blocks.");

		    $$ = MakeList ( KW(iportal), $2, -1 ); 
		}
	| PORTAL name 
		{
		    /*
		     *  15 august 1991 -- since 3.0 postgres does locking
		     *  right, we discovered that portals were violating
		     *  locking protocol.  portal locks cannot span xacts.
		     *  as a short-term fix, we installed the check here. 
		     *				-- mao
		     */
		    if (!IsTransactionBlock())
			elog(WARN, "Named portals may only be used in begin/end transaction blocks.");

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
	| relation_name '*'		  %prec '='
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

record_qual:
	/*EMPTY*/
		{ NULLTREE }
	;

/*
 *  Array bounds -- support for arrays in the current system is poor.
 *  We'd like to support multi-dimensional arrays, arrays with lower
 *  bounds at values other than 1, ..., but we don't.  The only thing
 *  that's real in the Array nodes we return below is arrayhigh,
 *  which is the upper bound of the array.
 */

opt_array_bounds:
  	  '[' ']'
		{ 
		    $$ = (LispValue) MakeArray((ObjectId) 0, 0, false, 0,
						-1, /* -1 means var length */
				    0);
		}
	| '[' Iconst ']'
		{
		    $$ = (LispValue) MakeArray((ObjectId) 0, 0, false, 0,
						$2, 0);
		}
	| /* EMPTY */				{ NULLTREE }
	;

Typename:
  	  name opt_array_bounds			{ $$ = lispCons($1,$2); }
	;

var_def: 	
	  Id '=' Typename
		{ 
		    ObjectId typrelid;

		    /* for 4.0, attributes cannot have complex types */
		    typrelid = get_typrelid(type(CString(CAR($3))));
		    if (typrelid != InvalidObjectId) {
			elog(NOTICE, "cannot create attribute of type %s",
				     CString(CAR($3)));
			elog(WARN, "use functions to get complex objects");
		    }

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

agg_where_clause:
	/* empty list */  {NULLTREE; }
	| WHERE a_expr 
	    {
		if (CInteger(CAR($2)) != BOOLOID)
		    elog(WARN,
			 "where clause must return type bool, not %s",
			 tname(get_id_type(CInteger(CAR($2)))));
		$$ = CDR($2);
	    }
	;

 /* 
  * expression grammar, still needs some cleanup
  */

a_expr:
	  attr opt_indirection
           {
		List temp;

		temp = HandleNestedDots($1);
		if ($2 == LispNil)
		    $$ = temp;
		else
		    $$ = make_array_ref(temp, $2);
	   }
	| AexprConst		
	| '-' a_expr %prec UMINUS
		  { $$ = make_op(lispString("-"), LispNil, $2, 'l');
		  Typecast_ok = false; }
	| a_expr '+' a_expr
		{ $$ = make_op (lispString("+"), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| a_expr '-' a_expr
		{ $$ = make_op (lispString("-"), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| a_expr '/' a_expr
		{ $$ = make_op (lispString("/"), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| a_expr '*' a_expr
		{ $$ = make_op (lispString("*"), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| a_expr '<' a_expr
		{ $$ = make_op (lispString("<"), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| a_expr '>' a_expr
		{ $$ = make_op (lispString(">"), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| a_expr '=' a_expr
		{ $$ = make_op (lispString("="), $1, $3, 'b' ) ;
		  Typecast_ok = false; }
	| ':' a_expr
		{ $$ = make_op (lispString(":"), LispNil, $2, 'l' ) ;
		Typecast_ok = false; }
	| ';' a_expr
		{ $$ = make_op (lispString(";"), LispNil, $2, 'l' ) ;
		Typecast_ok = false; }
	| '|' a_expr
		{ $$ = make_op (lispString("|"), LispNil, $2, 'l' ) ;
		Typecast_ok = false; }
	| AexprConst TYPECAST Typename
		{ 
		    extern LispValue parser_typecast();
		    $$ = parser_typecast ( $1, $3 );
		    Typecast_ok = false; /* it's already typecasted */
					 /* don't do it again. */ }
	| '(' a_expr ')'
		{$$ = $2;}
	| a_expr Op a_expr
		{ $$ = make_op ( $2, $1 , $3, 'b' );
		  Typecast_ok = false; }
	| Op a_expr
	  	{ $$ = make_op ( $1, LispNil, $2, 'l');
		  Typecast_ok = false; }
	| a_expr Op
	  	{ $$ = make_op ( $2, $1, LispNil, 'r');
		  Typecast_ok = false; }
	| relation_name
		{ $$ = lispCons ( KW(relation), $1 );
                  INC_NUM_LEVELS(1);
		  Typecast_ok = false; }
	| name '(' ')'
		{
			extern List ParseFunc();
			$$ = ParseFunc ( CString ( $1 ), LispNil );
			Typecast_ok = false;
		}
	| name '(' expr_list ')'
		{
			extern List ParseFunc();
			$$ = ParseFunc ( CString ( $1 ), $3 );
			Typecast_ok = false; }
	 | a_expr ISNULL
			 {
				 extern List ParseFunc();
				 yyval =  nappend1( LispNil, $1);
				 $$ = ParseFunc( "NullValue", yyval );
				 Typecast_ok = false; }
	| a_expr NOTNULL
		{
			extern List ParseFunc();
			 yyval =  nappend1( LispNil, $1);
			 $$ = ParseFunc( "NonNullValue", yyval );
			 Typecast_ok = false;
		}
	| a_expr AND a_expr
	    {
		if (CInteger(CAR($1)) != BOOLOID)
		    elog(WARN,
			 "left-hand side of AND is type '%s', not bool",
			 tname(get_id_type(CInteger(CAR($1)))));
		if (CInteger(CAR($3)) != BOOLOID)
		    elog(WARN,
			 "right-hand side of AND is type '%s', not bool",
			 tname(get_id_type(CInteger(CAR($3)))));

		$$ = lispCons(lispInteger(BOOLOID),
			      MakeList (lispInteger(AND),
					CDR($1), CDR($3), -1));
	    }
	| a_expr OR a_expr
	    {
		if (CInteger(CAR($1)) != BOOLOID)
		    elog(WARN,
			 "left-hand side of OR is type '%s', not bool",
			 tname(get_id_type(CInteger(CAR($1)))));
		if (CInteger(CAR($3)) != BOOLOID)
		    elog(WARN,
			 "right-hand side of OR is type '%s', not bool",
			 tname(get_id_type(CInteger(CAR($3)))));

		$$ = lispCons(lispInteger(BOOLOID),
			      MakeList (lispInteger(OR),
					CDR($1), CDR($3), -1));
	    }
	| NOT a_expr
	    {
		if (CInteger(CAR($2)) != BOOLOID)
		    elog(WARN,
			 "argument to NOT is type '%s', not bool",
			 tname(get_id_type(CInteger(CAR($2)))));

		$$ = lispCons(lispInteger(BOOLOID),
			      MakeList (lispInteger(NOT),
					CDR($2), -1));
	    }
	;

/* we support only single-dim arrays in the current system */
opt_indirection:
	  '[' a_expr ']'		{
		if (CInteger(CAR($2)) != INT4OID)
		    elog(WARN, "array index expressions must be int4's");
		$$ = $2;
	    }
	| /* EMPTY */			{ NULLTREE }
	;

expr_list: a_expr			{ ELEMENT ; }
	|  expr_list ',' a_expr		{ INC_LIST ; }
	;
attr:
          relation_name '.' attrs
                {
		    INC_NUM_LEVELS(1);
		    $$ = lispCons($1,$3);
		}

	| ParamNo '.' attrs
            {
		INC_NUM_LEVELS(1);
		$$ = lispCons($1,$3);
	    }
	;

attrs:
        attr_name                       { $$ = lispCons($1, LispNil); }
      | attrs '.' attr_name             { $$ = nappend1($1, $3);}

agg_res_target_el:
       attr
	  {
	       LispValue varnode, temp;
	       Resdom resnode;
	       int type_id, type_len;

	       temp = HandleNestedDots($1);

	        type_id = CInteger(CAR(temp));

		if (ISCOMPLEX(type_id))
		    elog(WARN,
			 "Cannot use complex type %s as atomic value in target list",
			 tname(get_id_type(type_id)));

	        type_len = tlen(get_id_type(type_id));
	        resnode = MakeResdom ( (AttributeNumber)1,
				     (ObjectId)type_id, ISCOMPLEX(type_id),
				     (Size)type_len,
				     (Name)CString(CAR(last ($1) )),
				     (Index)0 , (OperatorTupleForm)0 ,
				     0 );
	        varnode = CDR(temp);

	        $$ = lispCons((LispValue)resnode, lispCons(varnode,LispNil));
	 }

agg:
	name  '{' agg_res_target_el agg_where_clause '}'
	{
		 LispValue query2;
		 LispValue varnode = $3;
		 $3 = lispCons($3, LispNil);
		 query2 = MakeRoot(1, KW(retrieve), LispNil,
		 p_rtable, p_priority, p_ruleinfo, LispNil,
		 LispNil,$3);
		
		 query2 = lispCons (query2, LispNil);
		 query2 = nappend1 (query2, $3);
		 query2 = nappend1 (query2, $4);
		 /* query2 now retrieves the subquery within the
		  * agg brackets.
		  * this will be the agg node's outer child.
		  */

		  $$ = ParseAgg(CString($1), query2, varnode);

		  /*paramlist = search_quals($4, aggnode);*/
		  /*$$ = nappend1($$, $4);*/
		  /* (rettype, "agg",aggname, query, tlist, -1) */
	}

res_target_list:
	  res_target_list ',' res_target_el	
		{ p_target = nappend1(p_target,$3); $$ = p_target; }
	| res_target_el 			
		{ p_target = lispCons ($1,LispNil); $$ = p_target;}
	| relation_name '.' ALL
		{
			LispValue temp = p_target;
			INC_NUM_LEVELS(1);
			if(temp == LispNil )
			  p_target = ExpandAll((Name)CString($1), &p_last_resno);
			else {
			  while (temp != LispNil && CDR(temp) != LispNil)
			    temp = CDR(temp);
			  CDR(temp) = ExpandAll( (Name)CString($1), &p_last_resno);
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
			  p_target = ExpandAll((Name)CString($3), &p_last_resno);
			else {
			  while(temp != LispNil && CDR(temp) != LispNil )
			    temp = CDR(temp);
			  CDR(temp) = ExpandAll( (Name)CString($3), &p_last_resno);
			}
			$$ = p_target;
		}
	;

res_target_el:
		Id '=' agg
	{
	     int type_id;
	     int resdomno,  type_len;
	     List temp = LispNil;

	     type_id = CInteger(CAR($3));

	     if (ISCOMPLEX(type_id))
		elog(WARN,
		   "Cannot assign complex type to variable %s in target list",
		   CString($1));

	     type_len = tlen(get_id_type(type_id));
	     resdomno = p_last_resno++;
	     temp = lispCons ((LispValue)MakeResdom((AttributeNumber)resdomno,
					 (ObjectId)type_id,
					 ISCOMPLEX(type_id),
					 (Size)type_len,
					 (Name)CString($1),
					 (Index)0, (OperatorTupleForm)0, 0) 
			      , lispCons(CDR($3), LispNil) );
		$$ = temp;
	}

	| Id '=' a_expr
	  {
		if (ISCOMPLEX(CInteger(CAR($3))))
		    elog(WARN, "Cannot assign complex type to variable %s in target list", CString($1));
		$$ = make_targetlist_expr ($1,$3);
	   } 
	| attr opt_indirection
             {
		 LispValue varnode, temp;
		 Resdom resnode;
		 int type_id, type_len;

		 temp = HandleNestedDots($1);

		 if ($2 != LispNil)
		     temp = make_array_ref(temp, $2);

		 type_id = CInteger(CAR(temp));
		 if (ISCOMPLEX(type_id))
		     elog(WARN, "Cannot use complex type %s as atomic value in target list",
			  tname(get_id_type(type_id)));
		 type_len = tlen(get_id_type(type_id));
		 resnode = MakeResdom ( (AttributeNumber)p_last_resno++ ,
					(ObjectId)type_id, ISCOMPLEX(type_id),
				        (Size)type_len, 
					(Name)CString(CAR(last ($1) )),
					(Index)0 , (OperatorTupleForm)0,
					0 );
		 varnode = CDR(temp);
		 $$=lispCons((LispValue)resnode, lispCons(varnode,LispNil));
	    }
	;		 

opt_id:
	/*EMPTY*/		{ NULLTREE }
	| Id
	;

relation_name:
	SpecialRuleRelation
	| Id
	  {
		/* disallow refs to magic system tables */
		if (strncmp(CString($1), Name_pg_log, 16) == 0
		    || strncmp(CString($1), Name_pg_variable, 16) == 0
		    || strncmp(CString($1), Name_pg_time, 16) == 0
		    || strncmp(CString($1), Name_pg_magic, 16) == 0)
		    elog(WARN, "%s cannot be accessed by users", CString($1));
		else
		    $$ = $1;
	  }
	;

database_name:	Id	/*$$=$1*/;
access_method: 		Id 		/*$$=$1*/;
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
AexprConst:
	  Iconst			{ $$ = make_const ( $1 ) ; 
					  Input_is_integer = true; }
	| FCONST			{ $$ = make_const ( $1 ) ; }
	| CCONST			{ $$ = make_const ( $1 ) ; }
	| Sconst 			{ $$ = make_const ( $1 ) ; 
					  Input_is_string = true; }
	| ParamNo			{
		$$ = lispCons(lispInteger(get_paramtype((Param)$1)),
			      $1, -1);
	    }
	| Pnull			 	{ $$ = LispNil; 
					  Typecast_ok = false; }
	;

ParamNo: PARAM				{
	    ObjectId toid;
	    int paramno;
	    
	    paramno = CInteger($1);
	    toid = param_type(paramno);
	    if (!ObjectIdIsValid(toid)) {
		elog(WARN, "Parameter '$%d' is out of range",
		     paramno);
	    }
	    $$ = (LispValue) MakeParam(PARAM_NUM, (AttributeNumber) paramno,
				       (Name)"<unnamed>", (ObjectId)toid,
				       (List) NULL);
	  }
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
		    if (QueryIsRule)
		      $$ = lispString("*CURRENT*");
		    else 
		      elog(WARN,"\"current\" used in non-rule query");
		}
	| NEW
		{ 
		    if (QueryIsRule)
		      $$ = lispString("*NEW*");
		    else 
		      elog(WARN,"NEW used in non-rule query"); 
		    
		}
	;

Function:		FUNCTION	{ $$ = yylval ; } ;
Aggregate:		AGGREGATE	{ $$ = yylval ; } ;
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
parser_init(typev, nargs)
	ObjectId *typev;
	int nargs;
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

	param_type_init(typev, nargs);
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
       expr = lispCons( lispInteger(attrtype), (LispValue)
                MakeConst((ObjectId) attrtype, (Size) attrlen, (Datum) LispNil, (bool)1, (bool) 0 /* ignored */));
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
       return  ( lispCons ((LispValue)MakeResdom ((AttributeNumber)resdomno,
				       (ObjectId) attrtype,
				       ISCOMPLEX(attrtype),
				       (Size)attrlen ,
				       (Name)CString(name),
				       (Index) 0 ,
				       (OperatorTupleForm) 0 , 0 ) ,
                             lispCons((LispValue)CDR(expr),LispNil)) );
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
                val = textout((struct varlena *)get_constvalue((Const)CDR(expr)));
              }else{
                val = get_constvalue((Const)CDR(expr));
              }
              CDR(expr) = (LispValue) MakeConst(attrtype, attrlen,
	  (Datum)fmgr(typeid_get_retinfunc(attrtype),val,get_typelem(attrtype)),
                false, true /*??? Maybe correct-- 80% chance */);
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
    return  ( lispCons ((LispValue)MakeResdom ((AttributeNumber)resdomno,
				    (ObjectId)  attrtype,
				    ISCOMPLEX(attrtype),
				    (Size)  attrlen , 
				    (Name)  CString(name), (Index)0 ,
				    (OperatorTupleForm)0 , 0 ) ,
			      lispCons(CDR(expr),LispNil)) );
    
}
