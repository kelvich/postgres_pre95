/* ----------------------------------------------------------------
 *   FILE
 *	error.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
static char *error_c = "$Header$";

#include <stdio.h>
#include "utils/log.h"

/*
 * Default error handler for syntax errors.
 */
yyerror(message)
char    message[];
{
    extern char     yytext[];

    elog(WARN, "parser: %s at or near \"%s\"\n", message, yytext);
}

/*
 *	Scanner error handler.
 */
serror(str)
char str[];
{
	elog(WARN, "*** scanner error: %s\n", str);
}

