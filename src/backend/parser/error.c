static char *error_c = "$Header$";

#include <stdio.h>
#include "log.h"

/*
 * Default error handler for syntax errors.
 */
yyerror(message)
char    message[];
{
    extern char     yytext[];

    elog(NOTICE, "parser: %s at or near \"%s\"\n", message, yytext);
}

/*
 *	Scanner error handler.
 */
serror(str)
char str[];
{
	elog(WARN, "*** scanner error: %s\n", str);
}

