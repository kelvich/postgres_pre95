
/**********************************************************************
  postgres.c

  POSTGRES C Backend Interface
  $Header$
 **********************************************************************/

#include "c.h"

#include <signal.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <strings.h>
#include <ctype.h>

#include "catalog.h"		/* XXX to be obsoleted */
#include "globals.h"

#include "command.h"
#include "fmgr.h"	/* for EnableDynamicFunctionManager, fmgr_dynamic */
#include "exc.h"
#include "pg_lisp.h"
#include "pinit.h"
#include "pmod.h"
#include "rel.h"
#include "xcxt.h"
#include "log.h"
#include "execdebug.h"

extern void die();
extern void handle_warn();

/* XXX - I'm not sure if these should be here, but the be/fe comms work 
   when I do - jeff, maybe someone can figure out where they should go*/

RcsId("$Header$");


/*
 * AllocateAttribute --
 *	Returns space for an attribute.
 */
extern
struct attribute *	/* XXX */
	AllocateAttribute ARGS((
	void
));

int		Quiet = 0;
static	int	Warnings = 0;
static	int	ShowTime = 0;
#ifdef	EBUG
static	int	ShowLock = 0;
#endif
int		Userid;
Relation	reldesc;		/* current relation descritor */
char		relname[80];		/* current relation name */
jmp_buf		Warn_restart;
int		NBuffers = 2;
static	time_t	tim;

bool override = false;


/* ----------------------------------------------------------------
 *	misc routines
 * ----------------------------------------------------------------
 */

void
handle_warn()
{
	longjmp(Warn_restart,1);
}

void
die()
{
	ExitPostgres(0);
}

/* ----------------
 * XXX - BeginCommand belongs wherever EndCommand is, but where
 *       EndCommand is seems like the wrong place - jeff
 * ----------------
 */

BeginCommand(pname,attinfo)
    char *pname;
    LispValue attinfo;
{
    struct attribute **attrs;
    int nattrs;

    nattrs = CInteger(CAR(attinfo));
    attrs = (struct attribute **)CADR(attinfo);

    if (IsUnderPostmaster == true) {
	pflush();
	initport(pname,nattrs,attrs);
	pflush();
    } else {
	showatts(pname,nattrs,attrs);
    }
}

/* ----------------------------------------------------------------
 *	routines to obtain user input
 * ----------------------------------------------------------------
 */

/* ----------------
 *  InteractiveBackend() is called for user interactive connections
 *  the string entered by the user is placed in its parameter inBuf.
 * ----------------
 */

char
InteractiveBackend(inBuf)
    char *inBuf;
{
    String stuff;

    /* ----------------
     *	display a prompt and obtain input from the user
     * ----------------
     */

    for (;;) {
	char s[128];
	int v;

	printf("> ");
	stuff = gets(inBuf);
    
	/* ----------------
	 *  if the string is NULL then we aren't going to
	 *  get anything more from the user so abort this transaction
	 *  and exit.
	 * ----------------
	 */
	if (! StringIsValid(stuff)) {
	    if (!Quiet) {
		puts("EOF");
	    }
	    AbortCurrentTransaction();
	    exit(0);
	}

#ifdef EXEC_DEBUGINTERACTIVE
	/* ----------------
	 *  now see if it's a debugging command...
	 * ----------------
	 */
	if (sscanf(inBuf, "DEBUG %s", s) == 1) {
	    if (!DebugVariableProcessCommand(inBuf))
		printf("DEBUG [%s] not recognised\n", inBuf);
		
	    continue;
	}
#endif EXEC_DEBUGINTERACTIVE

	/* ----------------
	 *  otherwise we have a user query so process it.
	 * ----------------
	 */
	break;
    }
    
    return('Q');
}

/* ----------------
 *  SocketBackend()	Is called for frontend-backend connections
 *
 *  If the input is a query (case 'Q') then the string entered by
 *  the user is placed in its parameter inBuf.
 *
 *  If the input is a fastpath function call (case 'F') then
 *  the function call is processed in HandleFunctionRequest().
 *  (now called from main())
 * ----------------
 */

char SocketBackend(inBuf, parseList)
    char *inBuf;
    LispValue parseList;
{
    char *qtype = "?";
    int pq_qid;

    /* ----------------
     *	get input from the frontend
     * ----------------
     */
    getpchar(qtype,0,1);
    
    switch(*qtype) {
    /* ----------------
     *  'Q': user entered a query
     * ----------------
     */
    case 'Q':
	pq_qid = getpint(4);
	getpstr(inBuf, MAX_PARSE_BUFFER);

	if (!Quiet)
	    printf("Received Query: %s\n", inBuf);
	if (inBuf == NULL) {
	    if (! Quiet)
		puts("EOF");
	    AbortCurrentTransaction();
	    exit(0);
	}
	return('Q');
	break;
	
    /* ----------------
     *  'F':  calling user/system functions
     * ----------------
     */
    case 'F':	
        return('F');
        break;

    /* ----------------
     *  otherwise we got garbage from the frontend.
     *
     *  XXX are we certain that we want to do an elog(FATAL) here?
     *      -cim 1/24/90
     * ----------------
     */
    default:
        /* elog(FATAL, "Socket command type $%02x unknown\n", *qtype); */
	elog(FATAL, "Socket command type %c unknown\n", *qtype);
	break;
    }
}

/* ----------------
 *	ReadCommand reads a command from either the frontend or
 *	standard input, places it in inBuf, and returns a char
 *	representing whether the string is a 'Q'uery or a 'F'astpath
 *	call.
 * ----------------
 */
char ReadCommand(inBuf)
    char *inBuf;
{
    if (IsUnderPostmaster == true)
	return SocketBackend(inBuf);
    else
	return InteractiveBackend(inBuf);
}

/* ----------------------------------------------------------------
 *	pg_eval()
 *	
 *	Takes a querystring, runs the parser/utilities or
 *	parser/planner/executor over it as necessary
 *	Begin Transaction Should have been called before this
 *	and CommitTransaction After this is called
 *	This is strictly because we do not allow for nested xactions.
 *
 *
 *	NON-OBVIOUS-RESTRICTIONS
 * 	this function _MUST_ allocate a new "parsetree" each time, 
 * 	since it may be stored in a named portal and should not 
 * 	change its value.
 * ----------------------------------------------------------------
 */

pg_eval( query_string )
    String	query_string;
{
    LispValue parsetree = lispList();

    /* ----------------
     *	(1) parse the request string
     * ----------------
     */
    parser(query_string,parsetree);
    
    ValidateParse(parsetree);
    
    /* ----------------
     *	display parse strings
     * ----------------
     */
    if (! Quiet) {
	printf("\ninput string is %s\n",query_string);
	printf("\n---- \tparser outputs :\n");
	lispDisplay(parsetree,0);
	printf("\n");
    }
	
    /* ----------------
     *   (2) process the request
     * ----------------
     */
    if (atom(CAR(parsetree))) {
	/* ----------------
	 *   process utility functions (create, destroy, etc..)
	 * ----------------
	 */
	if (! Quiet) {
	    time(&tim);
	    printf("\tProcessUtility() at %s\n", ctime(&tim));
	}
	ProcessUtility(LISPVALUE_INTEGER(CAR(parsetree)),
		       CDR(parsetree),
		       query_string);
	
    } else {
	/* ----------------
	 *   process optimisable queries (retrieve, append, delete, replace)
	 * ----------------
	 */
	Node plan;
	extern Node planner();
	extern void init_planner();

	/* ----------------
	 *   initialize the planner
	 * ----------------
	 */
	if (! Quiet)
	    puts("\tinit_planner()..");
	
	init_planner();
	
	/* ----------------
	 *   generate the plan
	 * ----------------
	 */
	plan = planner(parsetree);

	if (! Quiet) {
	    printf("\nPlan is :\n");
	    (*(plan->printFunc))(stdout, plan);
	    printf("\n");
	}
	    
	/* ----------------
	 *   execute the plan
	 * ----------------
	 */
	if (! Quiet) {
	    time(&tim);
	    printf("\tProcessQuery() at %s\n", ctime(&tim));
	}
	ProcessQuery(parsetree, plan);
    }
}



/* ----------------------------------------------------------------
 *	main
 * ----------------------------------------------------------------
 */

main(argc, argv)
    int	argc;
    char	*argv[];
{
    register 	int	i;
    int			flagC;
    int			flagQ;
    int			flag;
    int			errs = 0;
    char		*DatabaseName;
    extern	int	Noversion;		/* util/version.c */
    extern	int	Quiet;

    extern	jmp_buf	Warn_restart;

    extern	int	optind;
    extern	char	*optarg;

    int		setjmp(), chdir();
    char	*getenv();

    char	parser_input[MAX_PARSE_BUFFER];
    
    /* ----------------
     *	process arguments
     * ----------------
     */
    flagC = flagQ = 0;
    while ((flag = getopt(argc, argv, "CQOdpP:B:b:")) != EOF)
      switch (flag) {
	case 'd':	/* -debug mode */
	  /* DebugMode = true; */
	  flagQ = 0;
	  break;
	case 'C':
	  flagC = 1;
	  break;
	case 'Q':
	  flagQ = 1;
	  break;
	case 'O':
	  override = true;
	  break;
	case 'p':	/* started by postmaster */
	  IsUnderPostmaster = true;
	  break;
	case 'P':
          /* Portfd = atoi(optarg); */
	  break;
	case 'b':
	case 'B':
	  NBuffers = atoi(optarg);
	  break;
	default:
	  errs += 1;
      }

    if (errs || argc - optind > 1) {
	fputs("Usage: amiint [-C] [-O] [-Q] [datname]\n", stderr);
	fputs("	-C  =  ??? \n", stderr);
	fputs(" -O  =  Override Transaction System\n", stderr);
	fputs(" -Q  =  Quiet mode (less debugging output)\n", stderr);
	exit(1);
    } else if (argc - optind == 1) {
	DatabaseName = argv[optind];
    } else if ((DatabaseName = getenv("USER")) == NULL) {
	fputs("amiint: failed getenv(\"USER\") and no database specified\n");
	exit(1);
    }
    Noversion = flagC;
    Quiet = flagQ;

    /* ----------------
     * 	print flags
     * ----------------
     */
    if (! Quiet) {
	puts("\t---debug info---");
	printf("\tQuiet =        %c\n", Quiet 	     ? 't' : 'f');
	printf("\tNoversion =    %c\n", Noversion   ? 't' : 'f');
	printf("\toverride  =    %c\n", override    ? 't' : 'f');
	printf("\tDatabaseName = [%s]\n", DatabaseName);
	puts("\t----------------\n");
    }

    if (! Quiet && ! override)
	puts("\t**** Transaction System Active ****");

    if (IsUnderPostmaster == true && Portfd < 0) {
	fprintf(stderr,"Postmaster flag set, but no port number specified\n");
	exit(1);
    }

    /* ----------------
     *	initialize portals
     * ----------------
     */
    if (IsUnderPostmaster == true)
	pinit();

    /* ----------------
     * 	various initalization stuff
     * ----------------
     */
    signal(SIGHUP, die);
    signal(SIGINT, die);
    signal(SIGTERM, die);
	
    /* XXX the -C version flag should be removed and combined with -O */
    SetProcessingMode((override) ? BootstrapProcessing : InitProcessing);

    if (! Quiet)
	puts("\tEnableDynamicFunctionManager()..");
    EnableDynamicFunctionManager(fmgr_dynamic);

    if (! Quiet)
	puts("\tInitPostgres()..");
    InitPostgres(NULL, DatabaseName);

    signal(SIGHUP, handle_warn);

    if (setjmp(Warn_restart) != 0) {
	Warnings++;
	if (! Quiet) {
	    time(&tim);
	    printf("\tAbortCurrentTransaction() at %s\n", ctime(&tim));
	}
	AbortCurrentTransaction();
    }

    /* ----------------
     *	POSTGRES main processing loop begins here
     * ----------------
     */
    if (IsUnderPostmaster == false) {
	puts("\nPOSTGRES backend interactive interface");
	puts("$Revision$ $Date$");
    }

    for (;;) {
	/* ----------------
	 *   (1) start the current transaction
	 * ----------------
	 */
	if (! Quiet) {
	    time(&tim);
	    printf("\tStartTransactionCommand() at %s\n", ctime(&tim));
	}
	StartTransactionCommand();

	/* ----------------
	 *   (2) read and process a command. 
	 * ----------------
	 */
	switch (ReadCommand(parser_input)) {
	    /* ----------------
	     *	'F' incicates a fastpath call.
	     *      XXX HandleFunctionRequest
	     * ----------------
	     */
	case 'F':
	    HandleFunctionRequest();
	    break;
	    /* ----------------
	     *	'Q' indicates a user query
	     * ----------------
	     */
	case 'Q':
	    pg_eval(parser_input);
	    break;
	}

	/* ----------------
	 *   (3) commit the current transaction
	 * ----------------
	 */
	if (! Quiet) {
	    time(&tim);
	    printf("\tCommitTransactionCommand() at %s\n", ctime(&tim));
	}
	CommitTransactionCommand();
    }
}
