
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

extern void die();
extern void handle_warn();

/* XXX - I'm not sure if these should be here, but the be/fe comms work 
   when I do - jeff, maybe someone can figure out where they should go*/

RcsId("$Header$");


/* ----------------------------------------------------------------
 *	misc routines
 * ----------------------------------------------------------------
 */

/*
 * AllocateAttribute --
 *	Returns space for an attribute.
 */
extern
struct attribute *	/* XXX */
	AllocateAttribute ARGS((
	void
));

static	int	Quiet = 0;
static	int	Warnings = 0;
static	int	ShowTime = 0;
#ifdef	EBUG
static	int	ShowLock = 0;
#endif
int		Userid;
Relation	reldesc;		/* current relation descritor */
char		relname[80];		/* current relation name */
jmp_buf		Warn_restart;

bool override = false;

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
 *	main
 * ----------------
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
    List	parser_output;

    time_t	tim;
    
    /* ----------------
     *	process arguments
     * ----------------
     */
    flagC = flagQ = 0;
    while ((flag = getopt(argc, argv, "CQOdpP:")) != EOF)
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
    if (IsUnderPostmaster == true) {
	pinit();
    }


    /* ----------------
     * 	various initalization stuff
     * ----------------
     */
    signal(SIGHUP, die);
    signal(SIGINT, die);
    signal(SIGTERM, die);
	
    /* XXX the -C version flag should be removed and combined with -O */
    SetProcessingMode((override) ? BootstrapProcessing : InitProcessing);
    /*
     * allow functions to be dynamically loaded
     */
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
     *   new code for handling input
     * ----------------
     */
    if (! IsUnderPostmaster ) {
	puts("\nPOSTGRES backend interactive interface");
	puts("$Revision$ $Date$");
    }

    for (;;) {
	/* ----------------
	 *   start the current transaction
	 * ----------------
	 */
	if (! Quiet) {
	    time(&tim);
	    printf("\tStartTransactionCommand() at %s\n", ctime(&tim));
	}
	StartTransactionCommand();

	/* ----------------
	 *	get and parse the input
	 * ----------------
	 */
	/*
	 * allocate a new "list" each time, since it is may be
	 * stored in a named portal and should not suddenly its
	 * contents
	 *
	 * I suspect that having the parser return the parse tree
	 * given no arguments would be a cleaner interface.  Is
	 * there any problem with doing this?
	 *	-hirohama
	 *
	 *    It's possible that we may do stuff to the
	 *    input before parsing.. (i.e. preprocessing)
	 *    In any case keeping the I/O code and the parser
	 *    code is better because they are orthogonal functions.
	 *    -cim 9/18/89
	 */
	parser_output = lispList();
	
	if (IsUnderPostmaster == true)
	    SocketBackend(parser_input, parser_output);
	else
	    InteractiveBackend(parser_input, parser_output);

	ValidateParse(parser_output);

	/* ----------------
	 *	display parse strings
	 * ----------------
	 */
	if (! Quiet) {
	    printf("\ninput string is %s\n",parser_input);
	    printf("\n---- \tparser outputs :\n");
	    lispDisplay(parser_output,0);
	    printf("\n");
	}
	
	/* ----------------
	 *   process the request
	 * ----------------
	 */
	if (lispIntegerp(CAR(parser_output))) {
	    /* ----------------
	     *   process utility functions (create, destroy, etc..)
	     * ----------------
	     */
	    if (! Quiet) {
		time(&tim);
		printf("\tProcessUtility() at %s\n", ctime(&tim));
	    }
	    
	    ProcessUtility(LISPVALUE_INTEGER(CAR(parser_output)),
			   CDR(parser_output));
	} else {
	    /* ----------------
	     *   process queries (retrieve, append, delete, replace)
	     * ----------------
	     */
	    Node plan;
	    extern Node planner();
	    extern void init_planner();

	    if (! Quiet)
		puts("\tinit_planner()..");
	    init_planner();
	      
	    /* ----------------
	     *   generate the plan
	     * ----------------
	     */
	    plan = planner(parser_output);
	    if (! Quiet) {
		printf("\nPlan is :\n");
		(*(plan->printFunc))(stdout, plan);
		printf("\n");
	    }
	    
	    /* ----------------
	     *   call the executor
	     * ----------------
	     */
	    if (! Quiet) {
		time(&tim);
		printf("\tProcessQuery() at %s\n", ctime(&tim));
	    }
	    
	    ProcessQuery(parser_output, plan);
	}
    
	/* ----------------
	 *   commit the current transaction
	 * ----------------
	 */
	if (! Quiet) {
	    time(&tim);
	    printf("\tCommitTransactionCommand() at %s\n", ctime(&tim));
	}
	
	CommitTransactionCommand();
    }
}

/* XXX - begicommand belongs wherever endcommand is, but where
   endcommand is seems like the wrong place - jeff */

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

/*
 *  SocketBackend()	Is called for frontend-backend connections
 */

SocketBackend(inBuf, parseList)
     char *inBuf;
     LispValue parseList;
{
    char *qtype = "?";
    int pq_qid;

    getpchar(qtype,0,1);

    switch(*qtype) {
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
	parser(inBuf,parseList);
	break;
    case 'F':	/* function, not supported at the moment */
	elog(WARN, "Socket command type 'F' not supported yet\n");
	break;
    default:
	/* elog(FATAL, "Socket command type $%02x unknown\n", *qtype); */
	elog(FATAL, "Socket command type %c unknown\n", *qtype);
	break;
    }
}

/*
 *  InteractiveBackend() Is called for user interactive connections
 *  MODIFIES:	parseList
 */

InteractiveBackend(inBuf, parseList)
     char *inBuf;
     LispValue parseList;
{
	String	stuff;

	printf("> ");
	stuff = gets(inBuf);
	if (!StringIsValid(stuff)) {
		if (!Quiet) {
			puts("EOF");
		}
		AbortCurrentTransaction();
		exit(0);
	}
	parser(inBuf, parseList);
}
