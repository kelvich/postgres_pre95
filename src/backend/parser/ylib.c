static char *ylib_c = "$Header$";

#include <stdio.h>

#include "log.h"
/*#include "catalog_utils.h"*/
#include "pg_lisp.h"
#include "exc.h"
#include "excid.h"
#include "io.h"

#define DB_PARSE(foo) 

/* Passed a string, result list, calls yacc, and fills
   in l with the proper lisp list.
   */
LispValue parsetree;
#ifdef ALLEGRO

lvparser(str, index)
     char *str;
     int index;
{
	extern long lisp_value();
	return(parser(str,(char *)lisp_value(index)));
}  
#endif     
parser(str, l)
     char *str;
     LispValue l;
{
    LispValue yyl;
    int yyresult;


    init_io();
/*
 * Parser must now return a parse tree in C space.  Thus, it cannot
 * start/end at this high a granularity.
 *
    startmmgr(0);
 */

    /* Set things up to read from the string, if there is one */
    /* DB_PARSE("parser got : %s\n",str);  
    fflush(stdout); */
    if (strlen(str) != 0) {
	StringInput = 1;
	TheString = malloc(strlen(str) + 1);
	bcopy(str,TheString,strlen(str)+1);
    }

    ExcBegin(); {
      parser_init();
      yyresult = yyparse();
      
      /* DB_PARSE("parse_string is %s\n",str); */
      
      fflush(stdout);
      clearerr(stdin);
      
      if (yyresult) {	/* error */
	/* DB_PARSE("parser error\n");
	   printf("parse_string is %s\n",str); */
	fflush(stdout);
	CAR(l) = LispNil;
	CDR(l) = LispNil;
	return;
      }
      
      CAR(l) = CAR(parsetree);
      CDR(l) = CDR(parsetree);
    }

    ExcExcept(); {
	if (exception.id == &SystemError) {
	    elog(FATAL, exception.message);
	} else if (exception.id == &InternalError) {
	    elog(FATAL, exception.message);
	} else if (exception.id == &CatalogFailure) {
	    elog(WARN, exception.message);
	} else if (exception.id == &SemanticError) {
	    elog(WARN, exception.message);
	} else {
	    reraise();
	}
    }
    ExcEnd();
    /* XXX close_rt(); */
/*
 * Parser must now return a parse tree in C space.  Thus, it cannot
 * start/end at this high a granularity.
 *
    endmmgr(NULL);
 */
    if (yyl == NULL) {
	return(-1);
    } else {
	return(0);
    }
}
