%{
/* ----------------------------------------------------------------
 *   FILE
 *	backendparse.y
 *
 *   DESCRIPTION
 *	yacc parser grammer for the "backend" initialization
 *	program.  uses backendscan.lex and backendsup.c support
 *	routines.
 *
 *   NOTES
 *
 * ----------------------------------------------------------------
 */
static	char ami_parser_y[] = 
 "$Header$";

#include "access/heapam.h"
#include "bootstrap/bkint.h"
#include "tmp/portal.h" 
#include "storage/smgr.h" 
#include "nodes/pg_lisp.h"
#include "catalog/catname.h"

#undef BOOTSTRAP

#define TRUE 1
#define FALSE 0

#define DO_QUERY(query) { StartTransactionCommand();\
			  query;\
			  CommitTransactionCommand();\
			  if (!Quiet) { EMITPROMPT; }\
			}

#define DO_START { StartTransactionCommand();\
		 }

#define DO_END   { CommitTransactionCommand();\
		   if (!Quiet) { EMITPROMPT; }\
		   fflush(stdout); \
		 }

int num_tuples_read = 0;
extern int DebugMode;
extern int Quiet;
static OID objectid;

%}

%token OPEN XCLOSE XCREATE P_RELN INSERT_TUPLE QUIT
%token ID STRING FLOAT INT XDEFINE MACRO
%token XDECLARE INDEX ON USING XBUILD INDICES
%token COMMA COLON EQUALS LPAREN RPAREN AS XIN XTO
%token XDESTROY XRENAME RELATION ATTR ADD
%token DISPLAY OBJ_ID SHOW BOOTSTRAP NULLVAL
%start TopLevel

%nonassoc low
%nonassoc high

%%

TopLevel:
	  Queries
	|
	;

Queries:
	  Query
	| Queries Query
	;

Query :
  	  OpenStmt
	| CloseStmt 
	| PrintStmt
	| CreateStmt
	| DestroyStmt
	| InsertStmt 
	| QuitStmt
	| DeclareIndexStmt
	| RenameRelnStmt
	| RenameAttrStmt
	| DisplayStmt
	| MacroDefStmt
	| BuildIndsStmt
	;

OpenStmt: 
	  OPEN ident
		{ 
		    DO_START;
		    boot_openrel(LexIDStr($2));
		    DO_END; 
		}	
	| OPEN LPAREN typelist RPAREN AS ident
		{ 
		    if (!Quiet) putchar('\n');
		    DO_START;
		    createrel(LexIDStr($6));
		    DO_END;
		}
	;

CloseStmt:
	  XCLOSE ident %prec low
		{
		    DO_START;
		    closerel(LexIDStr($2));
		    DO_END;
		}
	| XCLOSE %prec high
		{
		    DO_START;
		    closerel(NULL);
		    DO_END;
		}
	;

PrintStmt:
          P_RELN
		{
		    DO_START;
		    printrel();
		    DO_END;
		}
	;

CreateStmt:
	  XCREATE optbootstrap ident LPAREN 
		{ 
		    DO_START; 
		    numattr=(int)0;
		}
	  typelist 
		{ 
		    if (!Quiet) putchar('\n');
		    DO_END;
		}
	  RPAREN 
		{ 
		    DO_START; 
		    if ($2) {
			extern Relation reldesc;

			if (reldesc) {
			    puts("create bootstrap: Warning, open relation");
			    puts("exists, closing first");
			    closerel(NULL);
			}
			if (DebugMode)
			    puts("creating bootstrap relation");
			reldesc = heap_creatr(LexIDStr($3),
					      numattr,
					      DEFAULT_SMGR,
					      attrtypes);
			if (DebugMode)
			    puts("bootstrap relation created ok");
		    } else {
			ObjectId id;
			extern ObjectId heap_create();

			id = heap_create(LexIDStr($3),
					 'n',
					 numattr,
					 DEFAULT_SMGR,
					 attrtypes);
			if (!Quiet)
			    printf("CREATED relation %s with OID %d\n",
				   LexIDStr($3), id);
		    }
		    DO_END;
		    if (DebugMode)
			puts("Commit End");
		}
	;

DestroyStmt:
          XDESTROY ident
                { DO_START; heap_destroy(LexIDStr($2)); DO_END;}
	|
	  XDESTROY INDICES
		{
		    Relation rdesc;
		    HeapScanDesc sdesc;
		    HeapTuple htp;
		    Form_pg_relation rp;

		    DO_START;
		    rdesc = heap_openr(RelationRelationName->data);
		    sdesc = heap_beginscan(rdesc,
					   0,
					   NowTimeQual,
					   (unsigned) 0,
					   (ScanKey) NULL);
		    while (htp = heap_getnext(sdesc, 0, (Buffer *) NULL)) {
			rp = (Form_pg_relation) GETSTRUCT(htp);
			if (issystem(rp->relname.data) &&
			    (rp->relkind == 'i')) {
			    if (!Quiet)
				fprintf(stderr, "Destroying %.16s...\n",
					rp->relname.data);
			    index_destroy(htp->t_oid);
			}
		    }
		    heap_endscan(sdesc);
		    heap_close(rdesc);
		    DO_END;
		}
	;

InsertStmt:
	  INSERT_TUPLE optoideq		/* $1 $2 $3 */
		{ 
		    DO_START;
		    if (DebugMode)
		        printf("tuple %d<", $2);
		    num_tuples_read = 0;
		}
	  LPAREN  tuplelist RPAREN	/* $4 $5 $6 */
  		{
		    HeapTuple tuple;
		    if (num_tuples_read != numattr)
		        elog(WARN,"incorrect number of values for tuple");
		    if (reldesc == (Relation)NULL) {
		        elog(WARN,"must OPEN RELATION before INSERT\n");
		        err();
		    }
		    if (DebugMode)
			puts("Insert Begin");
		    objectid = $2;
		    InsertOneTuple(objectid);
		    if (DebugMode)
			puts("Insert End");
		    if (!Quiet) { putchar('\n'); }
		    DO_END;
		    if (DebugMode)
			puts("Transaction End");
		} 
	;

QuitStmt:
	  QUIT
		{
		  StartTransactionCommand();
		  StartPortalAllocMode(DefaultAllocMode, 0);
		  cleanup();
		}
	;

DeclareIndexStmt:
	  XDECLARE INDEX ident ON ident USING ident LPAREN index_params RPAREN
		{
		  List params;

		  DO_START;

		  params = nappend1(LispNil, (List)$9);
		  defineindex(LexIDStr($5), 
			      LexIDStr($3), 
			      LexIDStr($7),
			      params);
		  DO_END;
		}
	;

BuildIndsStmt:
	  XBUILD INDICES	{ build_indices(); }

index_params:
	index_on ident
		{
		  $$ = (YYSTYPE)nappend1(LispNil, (List)$1);
		  nappend1((List)$$, lispString(LexIDStr($2)));
		}

index_on:
	  ident
		{
		  $$ = (YYSTYPE)lispString(LexIDStr($1));
		}
	| ident LPAREN arg_list RPAREN
		{
		  $$ = (YYSTYPE)nappend1(LispNil, lispString(LexIDStr($1)));
		  rplacd((List)$$, (List)$3);
		}

arg_list:
	  ident
		{
		  $$ = (YYSTYPE)nappend1(LispNil, lispString(LexIDStr($1)));
		}
	| arg_list COMMA ident
		{
		  $$ = (YYSTYPE)nappend1((List)$1, lispString(LexIDStr($3)));
		}

RenameRelnStmt:
	  XRENAME RELATION ident AS ident
		{
		    DO_START;
		    renamerel(LexIDStr($3), LexIDStr($5));
		    DO_END;
		}
	;

RenameAttrStmt:
	  XRENAME ATTR ident AS ident XIN ident
		{ DO_START;
		  renameatt(LexIDStr($7), LexIDStr($3), LexIDStr($5));
		  DO_END;
		}
	;

DisplayStmt:
	  DisplayMacro
	| DisplayReln
	;

DisplayMacro:
	  DISPLAY MACRO 
		{ 
		  DO_START;
		  printmacros(); 
		  DO_END;
		}
	;

DisplayReln:
	  DISPLAY RELATION
		{
		  DO_START;
		  boot_openrel(RelationRelationName->data);
		  printrel();
		  closerel(RelationRelationName->data);
		  DO_END;
		}
	| SHOW
		{
		  DO_START;
		  printrel();
		  DO_END;
		}
	;

MacroDefStmt:
	  XDEFINE MACRO ident EQUALS ident
		{ /* for now, simple subst only */
		  DO_START;
		  DefineMacro($3,$5);
		  DO_END;
		}
	;

optbootstrap:
	    BOOTSTRAP	{ $$ = 1; }
	|  		{ $$ = 0; }
	;

typelist:
	  typething
	| typelist COMMA typething
	;

typething:
	  ident EQUALS ident
		{ 
		   char name[16],type[16];
		   if(++numattr > MAXATTR)
			elog(FATAL,"Too many attributes\n");
		   DefineAttr(LexIDStr($1),LexIDStr($3),numattr-1);
		   if (DebugMode)
		       printf("\n");
		}
	;

optoideq:
	    OBJ_ID EQUALS ident { $$ = atol(LexIDStr($3)); 		}
	|			{ extern OID newoid(); $$ = newoid();	}
	;

tuplelist:
	   tuple
	|  tuplelist tuple
	|  tuplelist COMMA tuple
	;

tuple:
	  ident	{ InsertOneValue(objectid, LexIDStr($1), num_tuples_read++); }
	| const	{ InsertOneValue(objectid, LexIDStr($1), num_tuples_read++); }
	| NULLVAL
	    { InsertOneNull(num_tuples_read++); }
	;
  
ident :
	  ID	{ $$=yylval; }
	| MACRO	{ $$=yylval; }
	;

const :
	  FLOAT	{ $$=yylval; }
	| INT	{ $$=yylval; }
	;
%%


