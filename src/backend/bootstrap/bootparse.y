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
#include "support/bkint.h"
#include "tmp/portal.h" 
#include "storage/smgr.h" 

#undef BOOTSTRAP
#include "y.tab.h"

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
%token ID STRING FLOAT INT XDEFINE MACRO INDEX ON USING
%token COMMA COLON DOT EQUALS LPAREN RPAREN AS XIN XTO
%token XDESTROY XRENAME RELATION ATTR ADD
%token DISPLAY OBJ_ID SHOW BOOTSTRAP
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
	| DefineIndexStmt
	| RenameRelnStmt
	| RenameAttrStmt
	| DisplayStmt
	| MacroDefStmt
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
		    DO_START;
		    createrel(LexIDStr($6));
		    DO_END;
		}
	;

CloseStmt:
	  XCLOSE ident %prec low
		{
		    DO_START;
		    closerel($2);
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
		        heap_create(LexIDStr($3),
				    'n',
				    numattr,
				    DEFAULT_SMGR,
				    attrtypes);
		    }
		    DO_END;
		    if (DebugMode)
			puts("Commit End");
		}
	;

DestroyStmt:
	  XDESTROY ident
		{ DO_START; heap_destroy(LexIDStr($2)); DO_END;}
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

DefineIndexStmt:
	  XDEFINE INDEX ident ON ident USING ident
		{ 
		  DO_START;
		  defineindex(LexIDStr($5), LexIDStr($3), LexIDStr($7));
		  DO_END;
		}
	;
    

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
		  /* pg_relation is now called pg_class -cim 2/26/90 */
		  boot_openrel("pg_class");
		  printrel();
		  closerel("pg_class");
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
	;
  
ident :
	 ID	{ $$=yylval; }
	| MACRO	{ $$=yylval; }
	;

%%


