%{
#define YYDEBUG 1
/**********************************************************************
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

#include "c.h"

RcsId("$Header$");

#include "catalog_utils.h"
#include "log.h"
#include "pg_lisp.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "parse_query.h"
#include <pwd.h>

extern LispValue new_filestr();
extern Relation amopenr();

#define MAXPATHLEN 1024
#define ELEMENT 	yyval = nappend1( LispNil , yypvt[-0] )
			/* yypvt [-1] = $1 */
#define INC_LIST 	yyval = nappend1( yypvt[-2] , yypvt[-0] ) /* $1,$3 */

#define NULLTREE	yyval = LispNil ; 
#define LAST(lv)        lispCons ( lv , LispNil )

#define INC_NUM_LEVELS(x)	if (NumLevels < x ) NumLevels = x;
#define KW(keyword)		lispAtom(CppAsString(keyword))

#define YYSTYPE LispValue
extern YYSTYPE parsetree;
static ResdomNoIsAttrNo = 0;
extern YYSTYPE parser_ppreserve();

static YYSTYPE temp;
static int NumLevels = 0;
YYSTYPE yylval;

YYSTYPE p_target,
        p_qual,
        p_root,
        p_priority,
        p_ruleinfo;
YYSTYPE	       p_trange,
	       p_rtable,
               p_target_resnos;

static int p_numlevels,p_last_resno;
static Relation CurrentRelationPtr = NULL;

%}

/* Commands */

%token 	ABORT_TRANS ADD_ATTR APPEND ATTACH_AS BEGIN_TRANS CLOSE 
	CLUSTER COPY CREATE DEFINE DELETE DESTROY END_TRANS EXECUTE
	FETCH MERGE MOVE PURGE REMOVE RENAME REPLACE RETRIEVE

/* Keywords */

%token  ALL ALWAYS AFTER AND ARCHIVE ARG ASCENDING BACKWARD BEFORE BINARY BY 
	DEMAND DESCENDING EMPTY FORWARD FROM HEAVY INTERSECT INTO
	IN INDEX INDEXABLE INHERITS INPUTPROC IS KEY LEFTOUTER LIGHT MERGE
	NEVER NEWVERSION NONE NONULLS NOT PNULL	ON ONCE OR OUTPUTPROC 
	PORTAL PRIORITY QUEL RIGHTOUTER RULE SCONST SORT TO TRANSACTION
	UNION UNIQUE USING WHERE WITH FUNCTION OPERATOR P_TYPE

/* Special keywords */

%token  IDENT SCONST ICONST Op CCONST FCONST

/* precedence */
%right '='
%left  '+' '-'
%left  '*' '/'
%left  '(' ')' '[' ']' 
%left	'.'

%%

query:
	stmt 
		{ $$ = $1;  parsetree=$$; }
	| experiment
	;

experiment: 
	a_expr
	{ parsetree = $1; };

stmt :
	  AttributeAddStmt
	| Attachas attach_args /* what are the args ? */
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
	| PurgeStmt			
	| RemoveOperatorStmt
	| RemoveStmt
	| RenameStmt
	| OptimizableStmt		
	| RuleStmt			
	| TransactionStmt		
	;

 /**********************************************************************

	QUERY :
		addattr ( attr1 = type1 .. attrn = typen ) to <relname>
	TREE :
		(ADD_ATTR <relname> (attr1 type1) . . (attrn typen) )

  **********************************************************************/
AttributeAddStmt:
	  Addattr '(' dom_list ')' To relation_name
		{
		     $6 = lispCons( $6 , $3);
		     $$ = lispCons( $1 , $6);
		}
	;

 /**********************************************************************
	
	QUERY :
		close <optname>
	TREE :
		(CLOSE <portalname>)

   **********************************************************************/
ClosePortalStmt:
	  Close opt_id			
		{ $$ = lispCons ( $1 , lispCons ( $2 , LispNil )) ; }
	;

 /**********************************************************************
	
	QUERY :
		cluster <relation> on <indexname> [using <operator>]
	TREE:
		XXX ??? unimplemented as of 1/8/89

  **********************************************************************/
ClusterStmt:
	  Cluster Relation On index_name OptUseOp
	;

 /**********************************************************************

	QUERY :
		COPY [BINARY] [NONULLS] <relname> 
		'(' [col_name = id]* ')'  FROM/TO 
		<filename> [USING <maprelname>

	TREE:
		( COPY ("relname" [BINARY] [NONULLS] )
		       ( FROM/TO "filename")
		       ( USING "maprelname")
		       ( "col_name" "id")* )

  **********************************************************************/

CopyStmt:
	  Copy copy_type copy_null 
	  relation_name '(' copy_list ')' copy_dirn file_name copy_map
	
		{
		  LispValue temp;
			$$ = lispCons($1,LispNil);
			$4 = lispCons($4,LispNil);
			$4 = nappend1($4,$2);
			$4 = nappend1($4,$3);
			$$ = nappend1($$,$4);
			$$ = nappend1($$,lispCons ($8,lispCons($9,LispNil)));
			if(! lispNullp($10))
			  $10 = lispCons($10,LispNil);
			$$ = nappend1($$, lispCons(KW(using), $10));
			temp = $$;
			while(temp != LispNil && CDR(temp) != LispNil )
			  temp = CDR(temp);
		        CDR(temp) = $6;
			/* $$ = nappend1 ($$ , $6 ); /* copy_list */
		}
	;

copy_dirn:	  To	| From	;

copy_map:
	/*EMPTY*/				{ NULLTREE }
	| Using map_rel_name			
		{ $$ = lispCons ($2 , LispNil ); }
	;

copy_null:
	/*EMPTY*/				{ NULLTREE }
	| Nonulls
	;

copy_type:
	/*EMPTY*/				{ NULLTREE }
	| Binary
	;

copy_list:
	/*EMPTY*/				{ NULLTREE }
	| copy_spec				{ ELEMENT ; }
	| copy_list ',' copy_spec		{ INC_LIST ;  }
	;

copy_spec:
	  col_name copy_eq copy_char
		{ if ($3 != LispNil )
		    $3 = lispCons ($3,LispNil);
		  $$ = lispCons ( $1, lispCons ($2, $3 ) ); 

		}
	| Sconst				/*$$=$1*/
	;

copy_eq:
	  /*EMPTY*/				{ NULLTREE }
	| equals Id				{ $$ = $2 ; }
	;
 
copy_char:
	  /*EMPTY*/				{ NULLTREE }
	| CCONST				
		{ char *cconst = (char *)CString($1); 
		  $$ = lispInteger(cconst[0]) ;
		}
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
	  Create relation_name '(' opt_dom_list ')' 
	  OptKeyPhrase OptInherit OptIndexable OptArchiveType
		{
			LispValue temp = LispNil;

			$9 = lispCons ( $9, LispNil);
			$8 = lispCons ( $8, $9 );
			$7 = lispCons ( $7, $8 );
			$6 = lispCons ( $6, $7 );

			$2 = lispCons ( $2, LispNil );
			$$ = lispCons ( $1, $2 );

			$$ = nappend1 ( $$, $6 );
			temp = $$;
			while (temp != LispNil && CDR(temp) != LispNil) 
			  temp = CDR(temp);
			CDR(temp) = $4;
			/* $$ = nappend1 ( $$, $4 ); */
		}
	| Create Version relation_name FROM Relation
		{
		    $3 = lispCons( $3, LispNil);
		    $3 = nappend1( $3, $5 );
		    $$ = lispCons( $2, $3 );
		}
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
	| Archive equals archive_type		{$$ = lispCons ($1 , $3); }
	;

archive_type:	  Heavy	| Light	| None	;

OptInherit:
	  /*EMPTY */				{ NULLTREE }
	| Inherits '(' relation_name_list ')'	{ $$ = lispCons ( $1, $3 ) ; }
	;


OptKeyPhrase :
	  /* NULL */				{ NULLTREE }
	| Key '(' key_list ')'			{ $1 = lispCons ( $1, LispNil);
						  $$ = nappend1 ($1, $3 ); }
	;

key_list:
	  key					{ ELEMENT ; }
	| key_list ',' key			{ INC_LIST ;  }
	;

key:
	  dom_name OptUseOp
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
	  Define def_type def_name definition opt_def_args
		{
		   if ( $5 != LispNil ) 
			$4 = nappend1 ($4, $5 );
		   $3 = lispCons ($3 , $4 );
		   $2 = lispCons ($2 , $3 ); 
		   $$ = lispCons ($1 , $2 ); 
		}
	;

def_type:	
	  Function 
		{ $$ = KW(function); }
	| Operator 
		{ $$ = KW(operator); }
	| Type 
		{ $$ = KW(type); }
	;
def_name:
	  Id 
	| Op	
	;

opt_def_args:	/* Because "define procedure .." */
	  Arg Is '(' def_name_list ')'
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
	  def_name equals def_arg

/*		{ $$ = lispCons ( lispAtom(CString($1)) , */
/*			lispCons ($3, LispNil)); } */
		{ $$ = lispCons($1, lispCons($3, LispNil)); }
	| def_name
/*		{ $$ = lispCons(lispAtom(CString($1)), LispNil); } */
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
	  Destroy relation_name_list		{ $$ = lispCons($1,$2) ; }
	;

 /**********************************************************************

	QUERY:
		fetch [forward | backward] [number | all ] [ in <portalname> ]
	TREE:
		(FETCH <portalname> <dirn> <number> )

  **********************************************************************/
FetchStmt:
	  Fetch OptFetchDirn OptFetchNum OptFetchPname
		{
		    $3 = lispCons ( $3 , LispNil );
		    $2 = lispCons ( $2 , $3 );
		    $4 = lispCons ( $4 , $2 );
		    $$ = lispCons ( $1 , $4 );
	        }
	;

OptFetchDirn:
	  /*EMPTY, default is forward*/
		{ $$ = KW(forward); }
	| Backward
	| Forward
	;

OptFetchNum:
	/*EMPTY, default is all*/
		{ $$ = KW(all) ; }
	| NumConst
	| All
	;

OptFetchPname:
	/*EMPTY*/				{ NULLTREE }
	| In portal_name			{ $$ = $2; }
	;

 /************************************************************
	QUERY:
		merge <rel1> into <rel2>
	TREE:
		( MERGE <rel1> <rel2> )

	XXX - unsupported in version 1 (outside of parser)

  ************************************************************/
MergeStmt:
	Merge Relation Into relation_name
		{ 
		    elog(NOTICE, "merge is unsupported in version 1");
		    $$ = lispCons($1,
		                  lispCons($2,lispCons($4,LispNil))) ;
		}
	;

 /**********************************************************************
	QUERY:
		move [<dirn>] [<whereto>] [<portalname>]
	TREE:
		( MOVE ["portalname"] <dirn> (TO <whereto> |
  **********************************************************************/
MoveStmt:
	  Move opt_move_dirn opt_move_where opt_move_pname
		{ 
		    $3 = lispCons ( $3 , LispNil );
		    $2 = lispCons ( $2 , $3 );
		    $4 = lispCons ( $4 , $2 );
		    $$ = lispCons ( $1 , $4 );
		}
	;

opt_move_dirn:
	/*EMPTY, by default forward */		{ $$ = KW(forward); }
	| Forward 				/* $$ = $1 */
	| Backward 				/* $$ = $1 */
	;

opt_move_where: 
	/*EMPTY*/				{ NULLTREE }
	| NumConst				/* $$ = $1 */
	| To NumConst				
		{ $$ = lispCons ( $1 , lispCons( $2, LispNil )); }
	| To record_qual			
		{ $$ = lispString("record quals unimplemented") ; }
	;

opt_move_pname:
	/*EMPTY*/				{ NULLTREE }
	| In portal_name			{ $$ = $2;}
	;


 /**********************************************************************

	Purge:

	purge <relname> [before <date>] [after <date>]
	  or
	purge <relname>  [after<date>][before <date>] 
	
		(PURGE "relname" ((BEFORE . date)(AFTER . date)))

  **********************************************************************/

PurgeStmt:
	  Purge relation_name purge_quals
		{ 
		  $$=nappend1(LispNil,$1); 
		  nappend1($$,$2);
		  nappend1($$,$3);
		}
	;

purge_quals:
	/*EMPTY*/
		{$$=lispList();}
	| before_clause
		{$$=nappend1(LispNil,$1);}
	| after_clause
		{$$=nappend1(LispNil,$1);}
	| before_clause after_clause
		{$$=nappend1(LispNil,$1);$$=nappend1($$,$2);}
	| after_clause before_clause
		{$$ = nappend1(LispNil,$2);$$=nappend1($$,$1);}
	;

before_clause:	BEFORE date		{ $$ = lispCons($1,$2); } ;
after_clause:	AFTER date		{ $$ = lispCons($1,$2); } ;

 /**********************************************************************

	Remove:

	remove function <funcname>
		(REMOVE FUNCTION "funcname")
	remove operator <opname>
		(REMOVE OPERATOR "opname")
	remove type <typename>
		(REMOVE TYPE "typename")
	remove rule <rulename>
		(REMOVE RULE "rulename")

  **********************************************************************/

RemoveStmt:
	Remove remove_type name
		{
		    $3=lispCons($3,LispNil);
		    $2=lispCons($2,$3);
		    $$=lispCons($1,$2);
		}
	;

remove_type:
	  Function | Rule | Type | Index ;

RemoveOperatorStmt:
	Remove Operator Op '(' remove_operator ')'
		{
		    $3  = lispCons($3, $5);
		    $2  = lispCons($2, lispCons($3, LispNil));
		    $$  = lispCons($1, $2);
		}
	;

remove_operator:
	  name
		{ $$ = lispCons($1, LispNil); }
	| name ',' name
		{ $$ = lispCons($1, lispCons($3, LispNil)); }
	;

 /**********************************************************************
	 
	rename <attrname1> in <relname> to <attrname2>
		( RENAME "ATTRIBUTE" "relname" "attname1" "attname2" )
	rename <relname1> to <relname2>
		( RENAME "RELATION" "relname1" "relname2" )
	
  **********************************************************************/

RenameStmt :
	  RENAME att_name In relation_name To att_name
		{ 
		    $2 = lispCons ($2 , lispCons ($6, LispNil ));
		    $4 = lispCons ($4 , $2);
		    $$ = lispCons ($1 , 
				   lispCons(lispString("ATTRIBUTE") ,$4 ));
		}
	| RENAME relation_name To relation_name
                {
		    $2 = lispCons ($2, lispCons ($4, LispNil));
		    $$ = lispCons ($1,
				   lispCons (lispString ("RELATION") ,$2 ));
		}
	;



 /**************************************************
	
	Rules:
	define rule <rulename> is <ruletag> <rulequery> 
	  <priority>
		(DEFINE RULE "rulename" (rulequery))

	- <ruletag> is one of always,never,once
	- <priority> is between 0 and 7
	- <rulequery> is one of 5 query stmts
	  (retrieve,replace,append,execute)

  **************************************************/

RuleStmt: 
	  Define Rule name opt_priority		
	  Is rule_tag
	  OptimizableStmt 
		{
		  $3 = lispCons($3,LispNil);	/* name */
		  $2 = lispCons($2,$3);		/* rule */
		  $$ = lispCons($1,$2); 	/* define */
		  $$ = nappend1($$,$7 );	/* rule query */
		}
	;

rule_tag:	  Always | Once	| Never	;

opt_priority:
	  /*EMPTY*/				{ $$ = lispInteger(0); }
	| PRIORITY Iconst			{ $$ = $2 ; }
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
	  Abort Transaction
		{ $$ = lispCons ( $1, LispNil ) ; } 
	| Begin Transaction
		{ $$ = lispCons ( $1, LispNil ) ; } 
	| End Transaction
		{ $$ = lispCons ( $1, LispNil ) ; } 
	| Abort
		{ $$ = lispCons ( $1, LispNil ) ; } 
	| Begin
		{ $$ = lispCons ( $1, LispNil ) ; } 
	| End
		{ $$ = lispCons ( $1, LispNil ) ; } 
	;
  
 /************************************************************

	define index:
	
	define [archive] index <indexname> on <relname>
	  using <access> "(" (<col> with <op>)+ ")" with
	  <target_list>

  ************************************************************/


IndexStmt:
	  Define opt_archive Index index_name On relation_name
	    Using access_method '(' index_list ')' with_clause
		{
		    $12 = lispCons($12,LispNil);
		    $10 = lispCons($10,$12);
		    $8  = lispCons($8,$10);
		    $4  = lispCons($4,$8);
		    $6  = lispCons($6,$4);
		    $3  = lispCons($3,$6);
		    $$  = lispCons($1,$3);
		}
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
	  Append opt_star relation_name 
		{ CurrentRelationPtr = amopenr(CString($3)); 
		  ResdomNoIsAttrNo = 1; SkipForwardToFromList(); 
		  if(RangeTablePosn(CString($3),0,0) == 0)
		    p_rtable = nappend1 (p_rtable, MakeRangeTableEntry
					       (CString($3),0,CString($3)));
		}
	  from_clause
	  '(' res_target_list ')'
	  where_clause
		{
			LispValue root;

			StripRangeTable();
		    	root = MakeRoot(1, KW(append), lispInteger(1), 
					p_rtable,
					p_priority, p_ruleinfo);
			$$ = lispCons ( root , LispNil );
			$$ = nappend1 ( $$ , $7 ); /* (eq p_target $5) */
			$$ = nappend1 ( $$ , $9 ); /* (eq p_qual $8 */
			ResdomNoIsAttrNo = 0;
		}
	;


 /**************************************************

   DeleteStmt
   - produces a querytree that looks like
   that in the query spec

  **************************************************/
   
DeleteStmt:
	  Delete 
		{ SkipForwardToFromList(); }
	  from_clause
	  opt_star var_name  where_clause
		{ 
			LispValue root;
			if(RangeTablePosn(CString($5),0,0) == 0)
			  p_rtable = nappend1 (p_rtable, MakeRangeTableEntry
					       (CString($5),0,CString($5)));
			StripRangeTable();
			root = MakeRoot(NumLevels,KW(delete),
					lispInteger ( 1 ) ,
					p_rtable, p_priority, p_ruleinfo);
			$$ = lispCons ( root , LispNil );
			/* check that var_name is in the relation */
			$$ = nappend1 ( $$ , LispNil ); /* (eq p_target $5) */
			$$ = nappend1 ( $$ , $6 ); /* (eq p_qual $5 */
		}
	;

 /**************************************************
   
   ExecuteStmt
   - produces a querytree that looks like that
   in the queryspec

  **************************************************/

ExecuteStmt:
	  Execute opt_star opt_portal '(' res_target_list ')'
            from_clause with_clause where_clause
		{ 
			$$ = $1; 
		  	elog(WARN, "execute does not work in Version 1");
		}
	;

 /**********************************************************************

	Replace:

	- as per qtree specs

  **********************************************************************/

ReplaceStmt:
 	Replace 
		{ SkipForwardToFromList(); }
	from_clause  
	opt_star relation_name
		{  
		   int x = 0;
		   if((x=RangeTablePosn(CString($5),0,0)) == 0)
		      p_rtable = nappend1 (p_rtable, MakeRangeTableEntry
					   (CString($5),0,CString($5)));
		  if (x==0) x = 1;
		  CurrentRelationPtr = amopenr(VarnoGetRelname(x));
		  if (CurrentRelationPtr == NULL) 
			elog(WARN,"invalid relation name");
		  ResdomNoIsAttrNo = 1; }
	'(' res_target_list ')' where_clause
  		{
		    LispValue root;

		    StripRangeTable();
		    root = MakeRoot(1, KW(replace), lispInteger(1),
				    p_rtable,
				    p_priority, p_ruleinfo);

		    $$ = lispCons( root , LispNil );
		    $$ = nappend1 ( $$ , $8 );		/* (eq p_target $6) */
		    $$ = nappend1 ( $$ , $10 );	        /* (eq p_qual $9) */
		    ResdomNoIsAttrNo = 0;
		}
	;
		 
opt_rep_duration: /*EMPTY*/ {NULLTREE} 	| Demand | Always ;


 /************************************************************

	Retrieve:

	( ( <NumLevels> RETRIEVE <result> <rtable> <priority>
	    <RuleInfo> )
	  (targetlist)
	  (qualification) )

  ************************************************************/

RetrieveStmt:
	  Retrieve 
		{ SkipForwardToFromList(); }
	  from_clause 
	  opt_star result opt_unique 
	  '(' res_target_list ')'
 	  where_clause ret_opt2 
  		{
		/* XXX - what to do with opt_unique, from_clause
		 */	
		    LispValue root;
	
		    StripRangeTable();
		    root = MakeRoot(NumLevels,KW(retrieve), $5, p_rtable,
				    p_priority, p_ruleinfo);

		    $$ = lispCons( root , LispNil );
		    $$ = nappend1 ( $$ , p_target );	/* (eq p_target $8) */
		    $$ = nappend1 ( $$ , $10 );	        /* (eq p_qual $10) */
		}

 /**************************************************

	Retrieve result:
	into <relname>
		(RELATION "relname");
	portal <portname>
		(PORTAL "portname")
	NULL
		nil

  **************************************************/
result:
	  Into relation_name
		{
			$2=lispCons($2 , LispNil );
			/* should check for archive level */
			$$=lispCons(KW(relation), $2);
		}
	| opt_portal
		/* takes care of the null case too */
	;

opt_unique:
	/*EMPTY*/
		{ NULLTREE }
	| Unique 
	;

ret_opt2: 
	/*EMPTY*/
		{ NULLTREE }
	| Sort By sortby_list
		{ $$ = $3 ; }
	;

sortby_list:
	  sortby				{ ELEMENT ; }
	| sortby_list ',' sortby		{ INC_LIST ;  }
	;

sortby:
	  Id OptUseOp
		{ $$ = lispCons ( $1, lispCons ($2, LispNil )) ; }
	;



opt_archive:
		{$$=LispNil;}
	| Archive
	;

index_list:
	  index_list ',' index_elem		{ INC_LIST ;  }
	| index_elem				{ ELEMENT ; }
	;

index_elem:
	  dom_name opt_class
		{ $$ = nappend1(LispNil,$1); nappend1($$,$2);}
	;
opt_class:
	| class
	| With class
		{$$=$2;}
	;

star:
	  Op			
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
	| With '(' param_list ')'		
		{ 
			$$ = lispCons( $1 , LispNil ); 
			$$ = nappend1( $$ , $3 );
		}
	;

param_list: 		
	  param_list ',' param			{ INC_LIST ;  }
	| param					{ ELEMENT ; }
	;

param: 			
	  Id equals string 
		{ $$ = lispCons ( $1 , lispCons ($3 , LispNil ));  }
	;

from_clause:
	 From from_list 			{ $$ = $2 ; 
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
	  var_list In from_rel_name
		{
			/* Convert "p, p1 in proc" to 
			   "p in proc, p1 in proc" */
			LispValue temp,temp2,temp3;
			LispValue frelname;

			temp = p_rtable;
			while(! lispNullp(CDR (temp))) {
				temp = CDR(temp); /* move to last elt */
			}
			frelname = CAR(CDR(CAR(temp)));
			/*printf("from relname = %s\n",CString(frelname));
			fflush(stdout);*/
			CAR(CAR(temp)) = CAR($1); 

			temp2 = CDR($1);
			while(! lispNullp(temp2)) {
				temp3 = lispCons(CAR(temp2),CDR(CAR(temp)));
				p_rtable = nappend1(p_rtable,temp3);
				temp2 = CDR(temp2);
			}

		}
	;
var_list:
	  var_list ',' var_name			{ INC_LIST ;  }
	| var_name				{ ELEMENT ; }
	;

where_clause:
	/* empty - no qualifiers */
	        { NULLTREE } 
	| Where boolexpr
		{ 
		  /*printf("now processing where_clause\n");*/
		  fflush(stdout);
		  $$ = $2; p_qual = $$; }
	;
opt_portal:
  	/* common to retrieve, execute */
	/*EMPTY*/
		{ NULLTREE }
	| Portal portal_name 
		{
		    $2=lispCons($2,LispNil);
		    $$=lispCons($1,$2);
		}
	;

OptUseOp:
	  /*EMPTY*/			{ NULLTREE }
	| Using Op			{ $$ = $2; }
	| Using Id			{ $$ = $2; }
	;

from_rel_name:
	  Relation	
	| le_op Iconst gt_op		{ $$ = $2; }
	;

Relation:
	  relation_name opt_time_range opt_star
		{
			int inherit_flag = 0;
			int trange_flag = (int)$2;
			 
			printf("timerange flag = %d",(int)$2);

			if ($3 != LispNil )	/* inheritance query*/
				inherit_flag = 1;
			if( !RangeTablePosn(CString($1),inherit_flag,p_trange))
				p_rtable = nappend1 (p_rtable, 
					MakeRangeTableEntry (CString($1),
						inherit_flag | trange_flag,
						CString($1) ));
		}
	;

opt_time_range:
	  '[' opt_date ',' opt_date ']'
		{ printf ("time range\n");fflush(stdout);
		  p_trange = MakeTimeRange($2,$4,1); $$ = (LispValue)2; }
	| '[' date ']'
		{ printf("snapshot\n"); fflush(stdout);
		p_trange = MakeTimeRange($2,LispNil,0); $$ = (LispValue)2; }
	| /*EMPTY*/
		{ p_trange = lispInteger(0); $$ = lispInteger(0); }
	;
opt_date:
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
	  b_expr And boolexpr
		{ $$ = lispCons ( lispInteger(AND) , lispCons($1 , 
		  lispCons($3 ,LispNil ))) ; }
	| b_expr Or boolexpr
		{ $$ = lispCons ( lispInteger(OR) , lispCons($1 , 
		  lispCons ( $3 , LispNil))); }
	| b_expr
		{ $$ = $1;}
	| Not b_expr
		{ $$ = lispCons ( lispInteger(NOT) , 
		       lispCons ($2, LispNil  )); }
	;

record_qual:
	/*EMPTY*/
		{ NULLTREE }
	;

opt_dom_list: dom_list | {NULLTREE} ;

dom_list:
	  dom_list ',' dom		{ INC_LIST ;  }
	| dom				{ ELEMENT ; }
	;

dom: 	
	  dom_name equals adt
		{ 
		    $3 = lispCons($3,LispNil); 
		    $$ = lispCons($1,$3);
		}
	;
b_expr:	a_expr { $$ = CDR($1) ; } /* necessary to strip the addnl type info 
				     XXX - check that it is boolean */
a_expr:
	  attr
		{
		$$ = make_var ( CString(CAR ($1)) , CString(CDR($1)));
		}
	| AexprConst		
	| adt_name '[' adt_const ']'
		{
			/* check for passing non-ints */
		        Const adt;
			LispValue lcp;
			Type tp = type(CString($1));
			int32 len = tlen(tp);
			char *cp = instr2 (tp, CString($3));

			if (!tbyvalue(tp)) {
			  if (len >= 0 && len != PSIZE(cp)) {
			    char *pp;
			    pp = palloc(len);
			    bcopy(cp, pp, len);
			    cp = pp;
			  }
			  lcp = parser_ppreserve(cp);
			} else {
			  lcp = lispInteger((long) cp);
			}

			adt = MakeConst ( typeid(tp), len, lcp , 0 );
			/*
			printf("adt %s : %d %d %d\n",CString($1),typeid(tp) ,
			       len,cp);
			       */
			$$ = lispCons  ( lispInteger (typeid(tp)) , adt );
		}
	| spec 
	| a_expr Op a_expr
		{ 
		    $$ = make_op ($2, $1, $3 ) ; 
		}
	| a_expr Op
		{ $$ = make_op($2,LispNil, $1); }
	| Op a_expr
  		{ $$ = make_op($1,$2, LispNil); }
	| '(' a_expr ')'
		{$$ = $2;}
	/* XXX Or other stuff.. */
	;

equals:
	  Op
		{if (strcmp(CString(yylval), "=") != 0) {
			yyerror("syntax error (expected '=')");
			/*YYABORT;*/
		} }
le_op:
	  Op
		{ if (strcmp(CString(yylval), "<") != 0) {
			yyerror("syntax error ('<' should be used)");
			/*YYABORT;*/
		 }
		}

gt_op:
	  Op
		{if (strcmp(CString(yylval), ">") != 0) {
			yyerror("syntax error ('>' should be used)");
			/*YYABORT;*/
		 }
		}

attr:
	  relation_name '.' att_name
		{    
		    INC_NUM_LEVELS(1);		
		    if( RangeTablePosn ( CString ($1),0,0 ) == 0 ) 
		      p_rtable = nappend1 (p_rtable ,
					   MakeRangeTableEntry( CString($1) ,
							       0, CString($1)));
		    $$ = lispCons ( $1 , $3 );
		}
	;


res_target_list:
	  res_target_list ',' res_target_el	
		{ p_target = nappend1(p_target,$3); $$ = p_target;}
	| res_target_el 			
		{ p_target = lispCons ($1,LispNil); $$ = p_target;}
	| relation_name '.' All
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
	| res_target_list ',' relation_name '.' All
		{
			LispValue temp = p_target;
			if (ResdomNoIsAttrNo) {
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
	  Id equals a_expr
		{
		    int type_id,type_len, attrtype, attrlen;
		    int resdomno;
		    Relation rd;
		    type_id = CInteger(CAR($3));
		    type_len = tlen(get_id_type(type_id));

		    if (ResdomNoIsAttrNo) { /* append or replace query */
			/* append, replace work only on one relation,
			   so multiple occurence of same resdomno is bogus */
			rd = CurrentRelationPtr;
			Assert(rd != NULL);
			resdomno = varattno(rd,CString($1));
			attrtype = att_typeid(rd,resdomno);
			attrlen = tlen(get_id_type(attrtype)); 
			if (attrtype != type_id)
				elog(WARN, "unequal type in tlist\n");
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
		    $$ = (LispValue)lispCons (MakeResdom (resdomno,
					      attrtype,
					      attrlen , 
					      CString($1), 0 , 0 ) ,
				  lispCons((Var)CDR($3),LispNil));
		}

	| attr
		{
		      LispValue varnode, temp;
		      Resdom resnode;
		      int type_id, type_len;

		      temp = make_var ( CString(CAR($1)) , CString(CDR($1)));
		      type_id = CInteger(CAR(temp));
		      type_len = tlen(get_id_type(type_id));
		      resnode = MakeResdom ( p_last_resno++ ,
						type_id, type_len, 
						CString(CDR ( $1 )) , 0 , 0 );
		      varnode = CDR(temp);
		      $$ = lispCons(resnode,lispCons(varnode,LispNil));
		}
	;		 

opt_id:
	/*EMPTY*/		{ NULLTREE }
	| Id
	;

relation_name:		Id		/*$$=$1*/;
access_method: 		Id 		/*$$=$1*/;
adt_name:		Id		/*$$=$1*/;
att_name: 		Id		/*$$=$1*/;
class: 			Id		/*$$=$1*/;
col_name:		Id		/*$$=$1*/;
dom_name: 		Id 		/*$$=$1*/;
index_name: 		Id		/*$$=$1*/;
map_rel_name:		Id		/*$$=$1*/;
portal_name: 		Id		/*$$=$1*/;
var_name:		Id		/*$$=$1*/;
name:			Id		/*$$-$1*/;
string: 		Id		/*$$=$1 Sconst ?*/;
adt:			Id		/*$$=$1*/;

date:			Sconst		/*$$=$1*/;
file_name:		SCONST		{$$ = new_filestr($1); };
adt_const:		Sconst		/*$$=$1*/;
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
	  Iconst			{ $$ = make_const ( $1 ) ; }
	| FCONST			{ $$ = make_const ( $1 ) ; }
	| CCONST			{ $$ = make_const ( $1 ) ; }
	| Sconst 			{ $$ = make_const ( $1 ) ; }
	| Pnull			 	{ $$ = make_const( LispNil ); }
	;

NumConst:
	  Iconst			/*$$ = $1 */
	| FCONST 			{ $$ = yylval; }
	;

Iconst: 	ICONST			{ $$ = yylval; }
Sconst:		SCONST			{ $$ = yylval; }

Id:
	  IDENT					
		{ $$ = yylval; }
	/* | Keyword				
		{ extern char yytext[];
		  $$ = lispString( yytext ); } /* XXX - may not be good */
	;
		
 /* Keyword:
	  Sort
	| Before
	;*/

Abort:			ABORT_TRANS	{ $$ = yylval ; } ;
Addattr:		ADD_ATTR	{ $$ = yylval ; } ;
After:			AFTER		{ $$ = yylval ; } ;
All:			ALL		{ $$ = yylval ; } ;
Always:			ALWAYS		{ $$ = yylval ; } ;
And:			AND		{ $$ = yylval ; } ;
Append:			APPEND		{ $$ = yylval ; } ;
Archive:		ARCHIVE		{ $$ = yylval ; } ;
Arg:			ARG		{ $$ = yylval ; } ;
Ascending:		ASCENDING	{ $$ = yylval ; } ;
Attachas:		ATTACH_AS	{ $$ = yylval ; } ;
Backward:		BACKWARD	{ $$ = yylval ; } ;
Before:			BEFORE		{ $$ = yylval ; } ;
Begin:			BEGIN_TRANS	{ $$ = yylval ; } ;
Binary:			BINARY		{ $$ = yylval ; } ;
By:			BY		{ $$ = yylval ; } ;
Close:			CLOSE		{ $$ = yylval ; } ;
Cluster:		CLUSTER		{ $$ = yylval ; } ;
Copy:			COPY		{ $$ = yylval ; } ;
Create:			CREATE		{ $$ = yylval ; } ;
Define:			DEFINE		{ $$ = yylval ; } ;
Delete:			DELETE		{ $$ = yylval ; } ;
Demand:			DEMAND		{ $$ = yylval ; } ;
Descending:		DESCENDING	{ $$ = yylval ; } ;
Destroy:		DESTROY		{ $$ = yylval ; } ;
Empty:			EMPTY		{ $$ = yylval ; } ;
End:			END_TRANS	{ $$ = yylval ; } ;
Execute:		EXECUTE		{ $$ = yylval ; } ;
Fetch:			FETCH		{ $$ = yylval ; } ;
Forward:		FORWARD		{ $$ = yylval ; } ;
From:			FROM		{ $$ = yylval ; } ;
Function:		FUNCTION	{ $$ = yylval ; } ;
Heavy:			HEAVY		{ $$ = yylval ; } ;
In:			IN		{ $$ = yylval ; } ;
Index:			INDEX		{ $$ = yylval ; } ;
Indexable:		INDEXABLE	{ $$ = yylval ; } ;
Inherits:		INHERITS	{ $$ = yylval ; } ;
Input_proc:		INPUTPROC	{ $$ = yylval ; } ;
Intersect:		INTERSECT	{ $$ = yylval ; } ;
Into:			INTO		{ $$ = yylval ; } ;
Is:			IS		{ $$ = yylval ; } ;
Key:			KEY		{ $$ = yylval ; } ;
Leftouter:		LEFTOUTER	{ $$ = yylval ; } ;
Light:			LIGHT		{ $$ = yylval ; } ;
Merge:			MERGE		{ $$ = yylval ; } ;
Move:			MOVE		{ $$ = yylval ; } ;
Never:			NEVER		{ $$ = yylval ; } ;
None:			NONE		{ $$ = yylval ; } ;
Nonulls:		NONULLS		{ $$ = yylval ; } ;
Not:			NOT		{ $$ = yylval ; } ;
On:			ON		{ $$ = yylval ; } ;
Once:			ONCE		{ $$ = yylval ; } ;
Operator:		OPERATOR	{ $$ = yylval ; } ;
Or:			OR		{ $$ = yylval ; } ;
Output_proc:		OUTPUTPROC	{ $$ = yylval ; } ;
Portal:			PORTAL		{ $$ = yylval ; } ;
Priority:		PRIORITY	{ $$ = yylval ; } ;
Purge:			PURGE		{ $$ = yylval ; } ;
Quel:			QUEL		{ $$ = yylval ; } ;
Remove:			REMOVE		{ $$ = yylval ; } ;
Rename:			RENAME		{ $$ = yylval ; } ;
Replace:		REPLACE		{ $$ = yylval ; } ;
Retrieve:		RETRIEVE	{ $$ = yylval ; } ;
Rightouter:		RIGHTOUTER	{ $$ = yylval ; } ;
Rule:			RULE		{ $$ = yylval ; } ;
Sort:			SORT		{ $$ = yylval ; } ;
To:			TO		{ $$ = yylval ; } ;
Type:			P_TYPE		{ $$ = yylval ; } ;
Transaction:		TRANSACTION	{ $$ = yylval ; } ;
Union:			UNION		{ $$ = yylval ; } ;
Unique:			UNIQUE		{ $$ = yylval ; } ;
Using:			USING		{ $$ = yylval ; } ;
Version:		NEWVERSION	{ $$ = yylval ; } ;
Where:			WHERE		{ $$ = yylval ; } ;
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
	p_trange = lispInteger(0);
	p_last_resno = 1;
	p_numlevels = 0;
	p_target_resnos = LispNil;
	ResdomNoIsAttrNo = 0;
}

char *
expand_file_name(file)
char *file;
{
    char *str;
    int ind;

    str = (char *) palloc(MAXPATHLEN * sizeof(*str));
    str[0] = '\0';
    if (file[0] == '~') {
	if (file[1] == '\0' || file[1] == '/') {
	    /* Home directory */
	    strcpy(str, getenv("HOME"));
	    ind = 1;
	} else {
	    /* Someone else's directory */
	    char name[16], *p;
	    struct passwd *pw;
	    int len;

	    if ((p = (char *) index(file, '/')) == NULL) {
		strcpy(name, file+1);
		len = strlen(name);
	    } else {
		len = (p - file) - 1;
		strncpy(name, file+1, len);
		name[len] = '\0';
	    }
	    /*printf("name: %s\n");*/
	    if ((pw = getpwnam(name)) == NULL) {
		elog(WARN, "No such user: %s\n", name);
		ind = 0;
	    } else {
		strcpy(str, pw->pw_dir);
		ind = len + 1;
	    }
	}
    } else {
	ind = 0;
    }
    strcat(str, file+ind);
    return(str);
}

LispValue
new_filestr ( filename )
     LispValue filename;
{
  return (lispString (expand_file_name (CString(filename))));
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
