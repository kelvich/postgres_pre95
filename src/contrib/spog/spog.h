/*
 * spog (simple postgres query interface)
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  1993/02/16 23:32:11  aoki
 * Initial revision
 *
 * Revision 2.2  1992/08/13  11:44:48  schoenw
 * options -x and -n added, postgres v4r0 support
 *
 * Revision 2.1  1992/05/22  12:53:33  schoenw
 * this is the public release 1.0
 *
 * Revision 1.1  1992/05/22  12:47:57  schoenw
 * Initial revision
 *
 */

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <strings.h>
#include <malloc.h>

#define HISTSIZE 200
#define MAXQUERYSIZE 1024

#include "tmp/libpq-fe.h"

/* 
 * these variables are defined in pqexec.c and
 * are used in the fe/be comm. protocol
 */

extern char	*getenv();
extern char	*PQhost;     /* machine on which the backend is running */
extern char	*PQport;     /* comm. port with the postgres backend. */
extern char	*PQtty;      /* the tty where postgres msgs are displayed */
extern char	*PQoption;   /* optional args. to the backend  */
extern int	PQportset;   /* 1 if comm. with backend is set */
extern int	PQxactid;    /* xact id of the current xact.  */
extern char	*PQinitstr;  /* initialisation string sent to backend */
extern int	PQtracep;    /* set to 1 if debugging is set */

/*
 * external variables for getopt
 */

extern char *optarg;
extern int optind, opterr;

/*
 * global variables that control spog's behaviour
 */

extern bool verbose;       /* be verbose -- return the no. of tuples fetched */
extern bool silent;        /* silence -- return only the status of the query */
extern bool print;         /* print -- print commands before execution */
extern char *progname;     /* the name of the game */
extern char *histfilename; /* name of the current history file */
extern char *database;     /* name of the current database */

/*
 * functions to read and write the history file
 */

char *open_history();
void close_history();

/* 
 * A structure which contains information on the commands this
 * program can understand.
 */

typedef struct {
        char *name;                 /* User printable name of the function. */
        Function *func;             /* Function to call to do the job. */
        char *doc;                  /* Documentation for this function.  */
	char *syntax;               /* Syntax of this function */
} COMMAND;

/*
 * A structure for command completion
 */

typedef struct {
        char *name;                 /* Name of a token. */
} TOKEN;

/*
 * A structure which contains format information.
 */

struct FMT_NODE {
	char *name;
	int  oid;
	char *fmt;
	struct FMT_NODE *next;
};

extern struct FMT_NODE *att_fmt;  /* format used to print an attribute       */
extern struct FMT_NODE *type_fmt; /* format used to print a specific type    */
extern char *default_fmt;         /* default format                          */
extern char *separator;           /* separator printed at the end of a tupel */
