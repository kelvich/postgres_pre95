/* ****************************************************************
 *   FILE
 *	postgres.c
 *	
 *   DESCRIPTION
 *	POSTGRES C Backend Interface
 *
 *   INTERFACE ROUTINES
 *	
 *   NOTES
 *	this is the "main" module of the postgres backend and
 *	hence the main module of the "traffic cop".
 *
 *   IDENTIFICATION
 *	$Header$
 * ****************************************************************
 */
#include <signal.h>
#include <setjmp.h>

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) tcopdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "executor/execdebug.h"
#include "tcop/tcopdebug.h"

#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"
#include "tmp/miscadmin.h"

#include "nodes/pg_lisp.h"

extern int on_exitpg();
extern void BufferManagerFlush();
extern int _show_stats_after_query_;
extern int _reset_stats_after_query_;

/* ----------------
 *	global variables
 * ----------------
 */
int		Warnings = 0;
int		ShowTime = 0;
bool		DebugPrintPlan = false;
bool		DebugPrintParse = false;
bool		DebugPrintRewrittenParsetree = false;
static bool	EnableRewrite = true;

#ifdef	EBUG
int		ShowLock = 0;
#endif

int		Userid;
Relation	reldesc;		/* current relation descritor */
char		relname[80];		/* current relation name */
jmp_buf		Warn_restart;
int		NBuffers = 16;
time_t		tim;
bool 		override = false;
int		EchoQuery = 0;		/* default don't echo */

/* ----------------
 *	people who want to use EOF should #define DONTUSENEWLINE in
 *	tcop/tcopdebug.h
 * ----------------
 */
#ifndef TCOP_DONTUSENEWLINE
int UseNewLine = 1;  /* Use newlines query delimiters (the default) */
#else
int UseNewLine = 0;  /* Use EOF as query delimiters */
#endif TCOP_DONTUSENEWLINE

/* ----------------
 *	this is now defined by the executor
 * ----------------
 */
extern int Quiet;

/* ----------------------------------------------------------------
 *			support functions
 * ----------------------------------------------------------------
 */

/* ----------------
 * 	BeginCommand 
 *
 *	I don't know what this does, but I assume it has something
 *	to do with frontend initialization of queries -cim 3/15/90
 * ----------------
 */

BeginCommand(pname, attinfo)
    char *pname;
    LispValue attinfo;
{
    struct attribute **attrs;
    int nattrs;

    nattrs = CInteger(CAR(attinfo));
    attrs = (struct attribute **) CADR(attinfo);

    if (IsUnderPostmaster == true) {
	pflush();
	initport(pname, nattrs, attrs);
	pflush();
    } else {
	showatts(pname, nattrs, attrs);
    }
}

/* ----------------
 * 	EndCommand 
 *
 *	I don't know what this does, but I assume it has something
 *	to do with frontend cleanup after a query is executed
 *	-cim 3/15/90
 * ----------------
 */

void
EndCommand(commandTag)
    String  commandTag;
{
    if (IsUnderPostmaster) {
	putnchar("C", 1);
	putint(0, 4);
	putstr(commandTag);
	pflush();
    }
}

/*
 * NullCommand
 * 
 * Necessary to implement the hacky FE/BE interface to handle multiple-return
 * queries.
 */

void
NullCommand()
{
    if (IsUnderPostmaster) {
	putnchar("I", 1);
	putint(0, 4);
	pflush();
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
    String stuff = inBuf;		/* current place in input buffer */
    char c;				/* character read from getc() */
    bool end = false;			/* end-of-input flag */
    bool backslashSeen = false;		/* have we seen a \ ? */

    /* ----------------
     *	display a prompt and obtain input from the user
     * ----------------
     */
    printf("> ");
    
    for (;;) {
	if (UseNewLine) {
	    /* ----------------
	     *	if we are using \n as a delimiter, then read
	     *  characters until the \n.
	     * ----------------
	     */
	    while ( (c = (char) getc(stdin)) != EOF) {
		if (c == '\n') {
		    if (backslashSeen) {
			stuff--;
			continue;
		    } else {
			*stuff++ = '\0';
			break;
		    }
		} else if (c == '\\')
		    backslashSeen = true;
		else
		    backslashSeen = false;

		*stuff++ = c;
	    }
	    
	    if (c == EOF)
		end = true;
	} else {
	    /* ----------------
	     *	otherwise read characters until EOF.
	     * ----------------
	     */
	    while ( (c = (char)getc(stdin)) != EOF )
		*stuff++ = c;

	    if ( stuff == inBuf )
		end = true;
	}

	/* ----------------
	 *  "end" is true when there are no more queries to process.
	 * ----------------
	 */
	if (end) {
	    if (!Quiet) puts("EOF");
	    AbortCurrentTransaction();
	    exitpg(0);
	}

#ifdef EXEC_DEBUGINTERACTIVE
	/* ----------------
	 *  We have some input, now see if it's a debugging command...
	 * ----------------
	 */
	{
	    char s[1024];
	
	    if (sscanf(inBuf, "DEBUG %s", s) == 1) {
		if (!DebugVariableProcessCommand(inBuf))
		    printf("DEBUG [%s] not recognised\n", inBuf);
		else
		    stuff = inBuf;
		
		printf("> ");
		continue;
	    }
	}
#endif EXEC_DEBUGINTERACTIVE

	/* ----------------
	 *  otherwise we have a user query so process it.
	 * ----------------
	 */
	break;
    }
    
    /* ----------------
     *	if the query echo flag was given, print the query..
     * ----------------
     */
    if (EchoQuery)
	printf("query is: %s\n", inBuf);
	
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
 *  (now called from PostgresMain())
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
	    printf("Received Query: \"%s\"\n", inBuf);
	
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
extern int _exec_repeat_;

pg_eval( query_string )
    String	query_string;
{
    LispValue parsetree_list = lispList();
    LispValue i;
    int j;
    bool unrewritten = true;

    /* ----------------
     *	(1) parse the request string into a set of plans
     * ----------------
     */
    parser(query_string, parsetree_list);


    /* ----------------
     *	(2) rewrite the queries, as necessary
     * ----------------
     */
    foreach (i, parsetree_list ) {
	LispValue parsetree = CAR(i);
	List rewritten  = LispNil;
	
	extern Node planner();
	extern void init_planner();
	extern List QueryRewrite();

	ValidateParse(parsetree);

	/* ----------------
	 *	if it is a utility, no need to rewrite
	 * ----------------
	 */
	if (atom(CAR(parsetree))) 
	  continue;
	
	/* ----------------
	 *	display parse strings
	 * ----------------
	 */
	if ( DebugPrintParse == true ) {
	    printf("\ninput string is %s\n",query_string);
	    printf("\n---- \tparser outputs :\n");
	    lispDisplay(parsetree,0);
	    printf("\n");
	}
	
	/* ----------------
	 *   rewrite queries (retrieve, append, delete, replace)
	 * ----------------
	 */
	
	if ( unrewritten == true && EnableRewrite ) {
	    if (!Quiet) {
		printf("rewriting query if needed ...\n");
		fflush(stdout);
	    }
	    if (( rewritten = QueryRewrite ( parsetree )) != NULL  ) {
		CAR(i) =  CAR(rewritten);
		CDR(last(rewritten)) = CDR(i);
		CDR(i) = CDR(rewritten);
		unrewritten = false;
		continue;
	    }
	    
	    unrewritten = false;	
	    
	} /* if unrewritten, then rewrite */

    } /* foreach parsetree in the list */

    if ( DebugPrintRewrittenParsetree == true ) {
	List i;
	printf("\n=================\n");
	printf("  After Rewriting\n");
	printf("=================\n");
	lispDisplay(parsetree_list,0);
    }

    /* ----------------
     *	(3) for each parse tree, plan and execute the query or process
     *      the utility reqyest.
     * ----------------
     */
    foreach(i,parsetree_list )   {
	LispValue parsetree = CAR(i);
	Node plan  	= NULL;

	if (atom(CAR(parsetree))) {
	    /* ----------------
	     *   process utility functions (create, destroy, etc..)
	     *
	     *   Note: we do not check for the transaction aborted state
	     *   because that is done in ProcessUtility.
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
	     *   process queries (retrieve, append, delete, replace)
	     *
	     *	 Note: first check the abort state to avoid doing
	     *   unnecessary work if we are in an aborted transaction
	     *   block.
	     * ----------------
	     */
	    if (IsAbortedTransactionBlockState()) {
		/* ----------------
		 *   the EndCommand() stuff is to tell the frontend
		 *   that the command ended. -cim 6/1/90
		 * ----------------
		 */
		char *tag = "*ABORT STATE*";
		EndCommand(tag);
	    
		elog(NOTICE, "(transaction aborted): %s",
		     "queries ignored until END");
	    
		return;
	    }

	    /* ----------------
	     *	initialize the planner
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
	    
	    if ( DebugPrintPlan == true ) {
		printf("\nPlan is :\n");
		(*(plan->printFunc))(stdout, plan);
		printf("\n");
	    }
	    
	    /* ----------------
	     *   execute the plan
	     *
	     *   Note: _exec_repeat_ defaults to 1 but may be changed
	     *	       by a DEBUG command.
	     * ----------------
	     */
	    for (j = 0; j < _exec_repeat_; j++) {
		if (! Quiet) {
		    time(&tim);
		    printf("\tProcessQuery() at %s\n", ctime(&tim));
		}
		ProcessQuery(parsetree, plan);
	    }
	    
	} /* if atom car */

    } /* foreach parsetree in parsetree_list */

} /* pg_eval */

/* ----------------------------------------------------------------
 *	ProcessPostgresArguments
 *
 *	This routine is solely responsible for setting parameters
 *	dependent on the command line arguments.  It does no other
 *	initialization -- that kind of thing is done by the
 *	InitializePostgres() function.
 * ----------------------------------------------------------------
 */
void
ProcessPostgresArguments(argc, argv)
    int	  argc;
    char  *argv[];
{
    int			flagC;
    int			flagQ;
    int			flagM;
    int			flagS;
    int			flagE;
    int			flag;

    int			numslaves;
    int			errs = 0;
    char		*DatabaseName;
    extern char		*DBName;  /* a global name for current database */
    
    extern	int	Noversion;		/* util/version.c */
    extern	int	optind;
    extern	char	*optarg;
    
    int		setjmp();
    char	*getenv();
    
    /* ----------------
     *	parse command line arguments
     * ----------------
     */
    numslaves = 0;
    flagC = flagQ = flagM = flagS = flagE = 0;
    
    while ((flag = getopt(argc, argv, "CQONM:dnpP:B:b:SE")) != EOF)
      switch (flag) {
	  
      case 'd':
	  /* -debug mode */
	  /* DebugMode = true;  */
	  flagQ = 0;
	  DebugPrintPlan = true;
	  DebugPrintParse = true;
	  DebugPrintRewrittenParsetree = true;
	  break;
	  
      case 'C':
	  /* ----------------
	   *	I don't know what this does -cim 5/12/90
	   * ----------------
	   */
	  flagC = 1;
	  break;
	  
      case 'Q':
	  /* ----------------
	   *	Q - set Quiet mode (reduce debugging output)
	   * ----------------
	   */
	  flagQ = 1;
	  break;
	  
      case 'O':
	  /* ----------------
	   *	O - override transaction system.  This used
	   *	    to be used to set the AMI_OVERRIDE flag for
	   *	    creation of initial system relations at
	   *	    bootstrap time.  I think MikeH changed its
	   *	    meaning and am not sure it works now.. -cim 5/12/90
	   * ----------------
	   */
	  override = true;
	  break;
	  
      case 'M':
	  /* ----------------
	   *	M # - process queries in parallel.  The system
	   *	      will initially fork several slave backends
	   *	      and the initial backend will plan parallel
	   *	      queries and pass them to the slaves. -cim 5/12/90
	   * ----------------
	   */
	  numslaves = atoi(optarg);
	  if (numslaves > 0) {
	      SetParallelExecutorEnabled(true);
	      SetNumberSlaveBackends(numslaves);
	  } else
	      errs += 1;
	  flagM = 1;
	  break;
	  
      case 'p':	/* started by postmaster */
	  /* ----------------
	   *	p - special flag passed if backend was forked
	   *	    by a postmaster.
	   * ----------------
	   */
	  IsUnderPostmaster = true;
	  
	  break;
	  
      case 'P':
	  /* ----------------
	   *	??? -cim 5/12/90
	   * ----------------
	   */
          Portfd = atoi(optarg);
	  break;
	  
      case 'b':
      case 'B':
	  /* ----------------
	   *	??? -cim 5/12/90
	   * ----------------
	   */
	  NBuffers = atoi(optarg);
	  break;
	  
      case 'n': /* do nothing for now - no debug mode */
	  /* ----------------
	   *	??? -cim 5/12/90
	   * ----------------
	   */
	  /* DebugMode = false; */
	  break;

      case 'N':
	  /* ----------------
	   *	Don't use newline as a query delimiter
	   * ----------------
	   */
	  UseNewLine = 0;
	  break;
	  
      case 'S':
	  /* ----------------
	   *	S - assume stable main memory
	   *	    (don't flush all pages at end transaction)
	   * ----------------
	   */
	  flagS = 1;
	  SetTransactionFlushEnabled(false);
	  break;

      case 'E':
	  /* ----------------
	   *	E - echo the query the user entered
	   * ----------------
	   */
	  flagE = 1;
	  break;
	  	  
      default:
	  errs += 1;
      }

    /* ----------------
     *	check for errors and get the database name
     * ----------------
     */
    if (errs || argc - optind > 1) {
	fputs("Usage: postgres [-C] [-M #] [-O] [-Q] [-N] [datname]\n",
	      stderr);
	fputs("	-C   =  ??? \n", stderr);
	fputs(" -M # =  Enable Parallel Query Execution\n", stderr);
	fputs("          (# is number of slave backends)\n", stderr);
	fputs(" -O   =  Override Transaction System\n", stderr);
	fputs(" -S   =  assume Stable Main Memory\n", stderr);
	fputs(" -Q   =  Quiet mode (less debugging output)\n", stderr);
	fputs(" -N   =  use ^D as query delimiter\n", stderr);
	fputs(" -E   =  echo the query entered\n", stderr);
	exitpg(1);
	
    } else if (argc - optind == 1) {
	DBName = DatabaseName = argv[optind];
    } else if ((DBName = DatabaseName = getenv("USER")) == NULL) {
	fputs("amiint: failed getenv(\"USER\") and no database specified\n");
	exitpg(1);
    }
    
    Noversion = flagC;
    Quiet = flagQ;
    EchoQuery = flagE;
    
    /* ----------------
     * 	print flags
     * ----------------
     */
    if (! Quiet) {
	puts("\t---debug info---");
	printf("\tQuiet =        %c\n", Quiet 	  ? 't' : 'f');
	printf("\tNoversion =    %c\n", Noversion ? 't' : 'f');
	printf("\toverride  =    %c\n", override  ? 't' : 'f');
	printf("\tstable    =    %c\n", flagS     ? 't' : 'f');
	printf("\tparallel  =    %c\n", flagM     ? 't' : 'f');
	if (flagM)
	    printf("\t# slaves  =    %d\n", numslaves);
	
	printf("\tquery echo =   %c\n", EchoQuery ? 't' : 'f');
	printf("\tDatabaseName = [%s]\n", DatabaseName);
	puts("\t----------------\n");
    }
    
    if (! Quiet && ! override)
	puts("\t**** Transaction System Active ****");
}
/* ----------------------------------------------------------------
 *	InitializePostgres
 *
 *	This routine should only be called once (by the "master"
 *  	backend).  Put "re-initialization" type stuff someplace else!
 *  	-cim 10/3/90
 * ----------------------------------------------------------------
 */
bool InitializePostgresCalled = false;

void
InitializePostgres(DatabaseName)
    String    DatabaseName;	/* name of database to use */
{
    /* ----------------
     *	sanity checks
     *
     *  Note: we can't call elog yet..
     * ----------------
     */
    if (InitializePostgresCalled) {
	fprintf(stderr, "InitializePostgresCalled more than once!\n");
	exitpg(1);
    } else
	InitializePostgresCalled = true;

    /* ----------------
     *	set processing mode appropriately depending on weather or
     *  not we want the transaction system running.  When the
     *  transaction system is not running, all transactions are
     *  assumed to have successfully committed and we never go to
     *  the transaction log.
     *
     *  The way things seem to work: we start in InitProcessing and
     *  change to NormalProcessing after InitPostgres() is done.  But
     *  if we run with the wierd override flag, then it means we always
     *  run in "BootstrapProcessing" mode.
     *
     * XXX the -C version flag should be removed and combined with -O
     * ----------------
     */
    SetProcessingMode((override) ? BootstrapProcessing : InitProcessing);

    /* ----------------
     *	if fmgr() is called and the desired function is not
     *  in the builtin table, then we call the desired function using
     *  the routine registered in EnableDynamicFunctionManager().
     *  That routine (fmgr_dynamic) is expected to dynamically
     *  load the desired function and then call it.
     *
     *  dynamic loading only works after EnableDynamicFunctionManager()
     *  is called.
     *
     *  XXX Why can't this go in InitPostgres?? -cim 10/5/90
     * ----------------
     */
    if (! Quiet)
	puts("\tEnableDynamicFunctionManager()..");
    EnableDynamicFunctionManager(fmgr_dynamic);
        	
    /* ----------------
     *	initialize portal file descriptors
     *
     *  XXX Why can't this go in InitPostgres?? -cim 10/5/90
     * ----------------
     */
    if (IsUnderPostmaster == true) {
	if (Portfd < 0) {
	    fprintf(stderr,
		    "Postmaster flag set, but no port number specified\n");
	    exitpg(1);
	}
	pinit();
    }
    
    /* ****************************************************
     *	InitPostgres()
     *
     *  Do all the general initialization.  Anything that can be
     *  done more than once should go in InitPostgres().  Note:
     *  InitPostgres is also used by the various support/ programs
     *  so it has to be linked in with cinterface.a.  Someday this
     *  should be fixed and then InitPostgres moved closer to here.
     * ****************************************************
     */
    if (! Quiet)
	puts("\tInitPostgres()..");
    InitPostgres(DatabaseName);

    /* ----------------
     *  Initialize the Master/Slave shared memory allocator,
     *	fork and initialize the parallel slave backends, and
     *  register the Master semaphore/shared memory cleanup
     *  procedures.
     *
     *  This may *NOT* happen more than once so we can't
     *  put this in InitPostgres() -cim 10/5/90
     * ----------------
     */
    if (ParallelExecutorEnabled()) {
	extern void IPCPrivateSemaphoreKill();
	extern void IPCPrivateMemoryKill();
	
	if (! Quiet)
	    puts("\tInitializing Executor Shared Memory...");
	ExecSMInit();
	
	if (! Quiet)
	    puts("\tInitializing Slave Backends...");
	SlaveBackendsInit();

	if (! Quiet)
	    puts("\tRegistering Master IPC Cleanup Procedures...");
	ExecSemaphoreOnExit(IPCPrivateSemaphoreKill);
	ExecSharedMemoryOnExit(IPCPrivateMemoryKill);
    }
}

/* ----------------
 *	ShowSessionStatistics prints some information after
 *	the user's query has been processed.
 * ----------------
 */
void
ShowSessionStatistics()
{
#ifdef TCOP_SHOWSTATS
    /* ----------------
     *  display the buffer manager statistics
     * ----------------
     */
    if (_show_stats_after_query_ > 0) {
	if (! Quiet)
	    PrintAndFreeBufferStatistics(GetBufferStatistics());
	if (_reset_stats_after_query_ > 0)
	    ResetBufferStatistics();
    }
	
    /* ----------------
     *  display the heap access statistics
     * ----------------
     */
    if (_show_stats_after_query_ > 0) {
	if (! Quiet)
	    PrintAndFreeHeapAccessStatistics(GetHeapAccessStatistics());
	if (_reset_stats_after_query_ > 0)
	    ResetHeapAccessStatistics();
    }
#endif TCOP_SHOWSTATS
}

/* ----------------------------------------------------------------
 *			postgres main loop
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	signal handler routines used in PostgresMain()
 *
 *	handle_warn() is used to catch kill(getpid(),1) which
 *	occurs when elog(WARN) is called.
 *
 *	die() preforms an orderly cleanup via ExitPostgres()
 * --------------------------------
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

/* --------------------------------
 *	PostgresMain
 * --------------------------------
 */

PostgresMain(argc, argv)
    int	argc;
    char	*argv[];
{
    int		i;
    extern	jmp_buf	Warn_restart;
    int		setjmp();
    char	parser_input[MAX_PARSE_BUFFER];
    int		empty_buffer = 0;	
    extern char	*DBName;  /* a global name for current database */
    
    /* ----------------
     * 	register signal handlers.
     * ----------------
     */
    signal(SIGHUP, die);
    signal(SIGINT, die);
    signal(SIGTERM, die);
    
    /* ----------------
     * 	ProcessPostgresArguments() is responsible for setting
     *  parameters dependent on the command line arguments.  It does
     *  not do any other initialization
     * ----------------
     */
    ProcessPostgresArguments(argc, argv);
    
    /* ----------------
     *	InitializePostgres() is responsible for doing all initialization
     *  between ProcessPostgresArguments() and the installation of
     *  the signal handler below.  Anyone tempted to put code between
     *  those places should put it in InitializePostgres instead!  This
     *  currently initializes:
     *
     *	- portal file descriptors	-->	pinit()
     *  - the processing mode		-->	SetProcessingMode()
     *  - dynamic function manager	-->	EnableDynamicFunctionManager()
     *	- stuff done by InitPostgres	-->	InitPostgres
     *  - parallel backend stuff	-->	InitParallelBackends()
     *
     *	and other misc stuff.	Note: we cannot call elog() until after
     *  InitializePostgres() because it initializes the Exception handler
     *  stuff, among other things. -cim 10/3/90
     * ----------------
     */
    if (! Quiet)
	puts("\tInititializePostgres()..");
    InitializePostgres(DBName);
    
    /* ----------------
     *	if an exception is encountered, processing resumes here
     *  so we abort the current transaction and start a new one.
     *  This must be done after we initialize the slave backends
     *  so that the slaves signal the master to abort the transaction
     *  rather than calling AbortCurrentTransaction() themselves.
     *
     *  Note:  elog(WARN) causes a kill(getpid(),1) to occur sending
     *         us back here.
     * ----------------
     */
    signal(SIGHUP, handle_warn);
    
    if (setjmp(Warn_restart) != 0) {
	Warnings++;
	time(&tim);
	
	if (ParallelExecutorEnabled()) {
	    if (! Quiet)
		printf("\tSlaveBackendsAbort() at %s\n", ctime(&tim));

	    SlaveBackendsAbort();
	} else {
	    if (! Quiet)
		printf("\tAbortCurrentTransaction() at %s\n", ctime(&tim));

	    for (i = 0; i<MAX_PARSE_BUFFER; i++) 
		parser_input[i] = 0;

	    AbortCurrentTransaction();
	}
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
	for (i=0; i< MAX_PARSE_BUFFER; i++) 
	    parser_input[i] = 0;

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
	    fflush(stdout);

	    if ( parser_input[0] ==  ' ' && parser_input[1] == '\0' ) {
		/* ----------------
		 *  if there is nothing in the input buffer, don't bother
		 *  trying to parse and execute anything..
		 * ----------------
		 */
		empty_buffer = 1;
	    } else {
		/* ----------------
		 *  otherwise, process the input string.
		 * ----------------
		 */
		pg_eval(parser_input);
	    }
	    break;
	    
	default:
	    elog(WARN,"unknown frontend message was recieved");
	}

	/* ----------------
	 *   (3) commit the current transaction
	 *
	 *   Note: if we had an empty input buffer, then we didn't
	 *   call pg_eval, so we don't bother to commit this transaction.
	 * ----------------
	 */
	if (! empty_buffer) {
	    if (! Quiet) {
		time(&tim);
		printf("\tCommitTransactionCommand() at %s\n", ctime(&tim));
	    }
	    CommitTransactionCommand();
	} else {
	    empty_buffer = 0;
	    NullCommand();
	}

	/* ----------------
	 *   print some statistics, if user desires.
	 * ----------------
	 */
	ShowSessionStatistics();
    }
}
