static char *io_c = "$Header$";

#include <stdio.h>
#include "io.h"
#include "log.h"

int StringInput;
char *TheString;
char *Ch;

void
init_io()
{

    Ch = NULL;
}

char
input()
{
    char c;

    if (StringInput) {
	if (Ch == NULL) {
	    Ch = TheString;
	    return(*Ch++);
	} else if (*Ch == '\0') {
	    return(0);
	} else {
	    return(*Ch++);
	}
    } else {
	c = getchar();
	if (c == EOF) {
	    return(0);
	} else {
	    return(c);
	}
    }
}

void
unput(c)
char c;
{
    
    if (StringInput) {
	if (Ch == NULL) {
	    elog(FATAL, "Unput() failed.\n");
	    /* NOTREACHED */
	} else if (c != 0) {
	    *--Ch = c;
	}
    } else {
	ungetc(c, stdin);
    }
}
