
/**********************************************************************
  postgres.c

  POSTGRES C Backend Interface
  $Header$
 **********************************************************************/

#include "c.h"

#include <signal.h>
#include <stdio.h>

#include "catalog.h"		/* XXX to be obsoleted */

#include "command.h"
#include "exc.h"
#include "pg_lisp.h"
#include "pinit.h"
#include "pmod.h"
#include "rel.h"
#include "xcxt.h"

extern void die();
extern void handle_warn();

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

/*
 * In the lexical analyzer, we need to get the reference number quickly from
 * the string, and the string from the reference number.  Thus we have
 * as our data structure a hash table, where the hashing key taken from
 * the particular string.  The hash table is chained.  One of the fields
 * of the hash table node is an index into the array of character pointers.
 * The unique index number that every string is assigned is simply the
 * position of its string pointer in the array of string pointers.
 */


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
	longjmp(Warn_restart);
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

    char	*parser_input = malloc(8192);
    List	parser_output;

    bool	watch_parser =  true;
    bool	watch_planner = true;
    
    /* ----------------
     *	process arguments
     * ----------------
     */
    flagC = flagQ = 0;
    while ((flag = getopt(argc, argv, "CQO")) != EOF)
	switch (flag) {
	case 'C':
	    flagC = 1;
	    break;
	case 'Q':
	    flagQ = 1;
	    break;
	case 'O':
	    override = true;
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
    
    /* ----------------
     * 	initialize the dynamic function manager
     * ----------------
     */
    if (! Quiet)
	puts("\tDynamicLinkerInit()..");
    DynamicLinkerInit(argv[0]);

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
	puts("\tInitPostgres()..");
    InitPostgres(NULL, DatabaseName);

    signal(SIGHUP, handle_warn);

    if (setjmp(Warn_restart) != 0) {
	Warnings++;
	if (! Quiet)
	    puts("\tAbortCurrentTransaction()..");
	AbortCurrentTransaction();
    }

    /* ----------------
     *   new code for handling input
     * ----------------
     */
    puts("\nPOSTGRES backend interactive interface");
    puts("$Revision$ $Date$");

    parser_output = lispList();

    for (;;) {
	/* ----------------
	 *   start the current transaction
	 * ----------------
	 */
	if (! Quiet)
	    puts("\tStartTransactionCommand()..");
	StartTransactionCommand();

	/* ----------------
	 *   get input from the user
	 * ----------------
	 */
	printf("\n> ");
	
	parser_input = gets(parser_input);
	if (! Quiet)
	    printf("\ninput string is %s\n",parser_input);

	if (parser_input == NULL) {
	    if (! Quiet)
		puts("EOF");
	    AbortCurrentTransaction();
	    exit(0);
	}
	    
	/* ----------------
	 *   parse the input
	 * ----------------
	 */
	parser(parser_input, parser_output);

	if (! Quiet) {
	    printf("---- \tparser outputs :\n");
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
	    if (! Quiet)
		puts("\tProcessUtility()..");
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
		(*(plan->printFunc))(plan);
		printf("\n");
	    }
	    
	    /* ----------------
	     *   call the executor
	     * ----------------
	     */
	    if (! Quiet)
		puts("\tProcessQuery()..");
	    ProcessQuery(parser_output, plan);
	}

	/* ----------------
	 *   commit the current transaction
	 * ----------------
	 */
	if (! Quiet)
	    puts("\tCommitTransactionCommand()..");
	CommitTransactionCommand();
     }
}	
