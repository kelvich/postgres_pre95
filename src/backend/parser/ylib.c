static char *ylib_c = "$Header$";

#include <stdio.h>

#include "log.h"
/*#include "catalog_utils.h"*/
#include "lispdep.h"
#include "exceptions.h"
#include "io.h"

#define DB_PARSE printf

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
    startmmgr(0);

    /* Set things up to read from the string, if there is one */
    DB_PARSE("parser got : %s\n",str);
    fflush(stdout);
    if (strlen(str) != 0) {
	StringInput = 1;
	TheString = str;
    }

    parser_init();
    yyresult = yyparse();

    fflush(stdout);
    clearerr(stdin);

    if (yyresult) {	/* error */
	DB_PARSE("parser error\n");
	printf("parse_string is %s\n",str);
	fflush(stdout);
	CAR(l) = LispNil;
	CDR(l) = LispNil;
	return;
    }

    ExcBegin(); {
	    CAR(l) = CAR(parsetree);
	    CDR(l) = CDR(parsetree);
    }

    ExcExcept(); {
	if (exception.id == &SystemError) {
	    elog(FATAL, exception.msg);
	} else if (exception.id == &InternalError) {
	    elog(FATAL, exception.msg);
	} else if (exception.id == &CatalogFailure) {
	    elog(WARN, exception.msg);
	} else if (exception.id == &SemanticError) {
	    elog(WARN, exception.msg);
	} else {
	    reraise();
	}
    }
    ExcEnd();
    /* XXX close_rt(); */
    endmmgr(NULL);
    if (yyl == NULL) {
	return(-1);
    } else {
	return(0);
    }
}
