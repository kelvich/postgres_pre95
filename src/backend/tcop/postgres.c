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
#include <stdio.h>
#include <sys/time.h>

#include "tmp/postgres.h"
#include "parser/parse.h"
#include "catalog/catname.h"

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
#include "tcop/tcopdebug.h"

#include "tcop/slaves.h"
#include "parser/parsetree.h"
#include "executor/execdebug.h"
#include "executor/x_execstats.h"
#include "nodes/relation.h"
#include "planner/costsize.h"
#include "planner/planner.h"
#include "planner/xfunc.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"

#include "storage/bufmgr.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"
#include "tmp/miscadmin.h"

#include "nodes/pg_lisp.h"
#include "tcop/dest.h"
#include "nodes/mnodes.h"
#include "utils/mcxt.h"
#include "tcop/pquery.h"
#include "tcop/utility.h"
/* #include "tcop/fastpath.h" - marc */
#include "tcop/parsev.h"

#ifdef PARALLELDEBUG
#include <usclkc.h>
#endif

#ifdef NOFIXADE
#include <sys/syscall.h>
#include <sys/sysmips.h>
#endif

extern int on_exitpg();

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
int		ShowStats;
CommandDest whereToSendOutput;

#ifdef	EBUG
int		ShowLock = 0;
#endif

/* User info  */

extern char	*PG_username;

/* in elog.c */
bool IsEmptyQuery = false;

/* this global is defined in utils/init/postinit.c */
extern int	testFlag;
extern int	lockingOff;
static int	confirmExecute = 0;

Relation	reldesc;		/* current relation descritor */
char		relname[80];		/* current relation name */
jmp_buf		Warn_restart;
extern int	NBuffers;
time_t		tim;
int		EchoQuery = 0;		/* default don't echo */
char pg_pathname[256];
static int	ShowParserStats;
static int	ShowPlannerStats;
int	ShowExecutorStats;
extern FILE	*StatFp;

/* these are hong flags that should go away --mao 10/10/92 */
int		ProcessAffinityOn = 0;
int		MasterPid;
bool            override = false;
ParallelismModes ParallelismMode = INTER_W_ADJ;

/* ----------------
 *	for memory leak debugging
 * ----------------
 */
int query_tuple_count = 0;
int xact_tuple_count = 0;
int query_tuple_max = 0;
int xact_tuple_max = 0;

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

/* ----------------
 *	bushy tree plan flag: if true planner will generate bushy-tree
 *	plans
 * ----------------
 */
int BushyPlanFlag = 0; /* default to false -- consider only left-deep trees */

/*
** Flags for expensive function optimization -- JMH 3/9/92
*/
int XfuncMode = 0;

/* ----------------------------------------------------------------
 *			support functions
 * ----------------------------------------------------------------
 */

/* ----------------
 * If we are running under the Postmaster, we cannot in general be sure that
 * USER is set to the actual name of the user.  (After all, in theory there
 * may be Postgres users that don't exist as Unix users on the database server
 * machine.)  Therefore, the Postmaster sets the PG_USER environment variable
 * so that the actual user can be determined.
 * ----------------
 */

void
GetUserName()
{
    char *t;

    if (IsUnderPostmaster)
	{
		t = (char *) getenv("PG_USER");
		if (t == NULL)
	    	elog(FATAL, "GetUserName(): Can\'t get PG_USER environment name");
		
	}
    else
	{
		t = (char *) getenv("USER");
	}

	/*
	 * Valid Postgres username can be at most 16
	 * characters - leave one for null
	 * 
	 * DO NOT REMOVE THIS MALLOC - apparently some getenv's reuse space and
	 * this hozed on a Sparcstation.
	 */

	PG_username = (char *) malloc(17);
	strcpy(PG_username, t);
}

void
get_pathname(argv)
    char **argv;
{
    char buffer[256];

    if (argv[0][0] == '/') { /* ie, from execve in postmaster */
	strcpy(pg_pathname, argv[0]);
    } else {
	getwd(buffer);
	sprintf(pg_pathname, "%s/%s", buffer, argv[0]);
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

	if (end) {
	    if (!Quiet) puts("EOF");
	    IsEmptyQuery = true;
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

char SocketBackend(inBuf)
    char *inBuf;
{
    char *qtype = "?";
    int pq_qid;

    /* ----------------
     *	get input from the frontend
     * ----------------
     */
    if (pq_getnchar(qtype,0,1) == EOF) {
	/* ------------
	 *  when front-end applications quits/dies
	 * ------------
	 */
	exitpg(0);
    }

    switch(*qtype) {
	/* ----------------
	 *  'Q': user entered a query
	 * ----------------
	 */
    case 'Q':
	pq_qid = pq_getint(4);
	pq_getstr(inBuf, MAX_PARSE_BUFFER);
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
	 *  'X':  frontend is exiting
	 * ----------------
	 */
    case 'X':
	return('X');
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
 *	NON-OBVIOUS-RESTRICTIONS
 * 	this function _MUST_ allocate a new "parsetree" each time, 
 * 	since it may be stored in a named portal and should not 
 * 	change its value.
 *
 * ----------------------------------------------------------------
 */
extern int _exec_repeat_;
GlobalMemory ParserPlannerContext;

List
pg_plan(query_string, typev, nargs, parsetreeP, dest)
    String	query_string;	/* string to execute */
    ObjectId	*typev;		/* argument types */
    int		nargs;		/* number of arguments */
    LispValue	*parsetreeP;	/* pointer to the parse trees */
    CommandDest dest;		/* where results should go */
{
    LispValue parsetree_list = lispList();
    List plan_list = LispNil;
    Plan plan;
    List parsetree;
    LispValue i, x;
    int j;
    List new_list = LispNil;
    List rewritten = LispNil;
    MemoryContext oldcontext;

    if (testFlag) {
	ParserPlannerContext = CreateGlobalMemory("ParserPlannerContext");
	oldcontext = MemoryContextSwitchTo((MemoryContext)ParserPlannerContext);
      }

    /* ----------------
     *	(1) parse the request string into a list of parse trees
     * ----------------
     */
    if (ShowParserStats)
	ResetUsage();
    
    parser(query_string, parsetree_list, typev, nargs);
    
    if (ShowParserStats) {
	fprintf(stderr, "! Parser Stats:\n");
	ShowUsage();
    }
    
    /* ----------------
     *	(2) rewrite the queries, as necessary
     * ----------------
     */
    foreach (i, parsetree_list ) {
	LispValue parsetree = CAR(i);
	
	extern List QueryRewrite();

	ValidateParse(parsetree);

	/* don't rewrite utilites */
	if (atom(CAR(parsetree))) {
	    new_list = nappend1(new_list,parsetree);
	    continue;
	}
	
	if ( DebugPrintParse == true ) {
	    printf("\ninput string is %s\n",query_string);
	    printf("\n---- \tparser outputs :\n");
	    lispDisplay(parsetree);
	    printf("\n");
	}

        /*
         * This crap is here because the notify statement 
	 * has to look like a query to work as a rule action.
	 */

	 if (NOTIFY == LISPVALUE_INTEGER(CAR(CDR(CAR(parsetree))))) {
	   new_list = nappend1(new_list,CDR(CAR(parsetree)));
	   continue;
	 }
	
	/* rewrite queries (retrieve, append, delete, replace) */
	if (EnableRewrite ) {
	    rewritten = QueryRewrite ( parsetree );
	    if (rewritten != LispNil) {
		new_list = append(new_list, rewritten);
	    }
	    continue;
	}
	else  {
	    new_list = nappend1(new_list, parsetree);
	}

    }

    parsetree_list = new_list;

    /* ----------------
     * Fix time range quals
     * this _must_ go here, because it must take place after rewrites
     * ( if they take place ) so that time quals are usable by the executor
     *
     * Also, need to frob the range table entries here to plan union
     * queries for archived relations.
     * ----------------
     */
    foreach ( i , parsetree_list ) {
	List parsetree = CAR(i);
	List l;
	List rt = NULL;
	extern List MakeTimeRange();
	
	/* ----------------
	 *  utilities don't have time ranges
	 * ----------------
	 */
	if (atom(CAR(parsetree))) 
	    continue;

	rt = root_rangetable(parse_root(parsetree));

	foreach (l, rt) {
	    List timequal = rt_time(CAR(l));
	    if ( timequal && consp(timequal) )
		rt_time(CAR(l)) = MakeTimeRange(CAR(timequal),
						CADR(timequal),
						CInteger(CADDR(timequal)));
	}

	/* check for archived relations */
	plan_archive(rt);
    }
    
    if (DebugPrintRewrittenParsetree == true) {
	List i;
	printf("\n=================\n");
	printf("  After Rewriting\n");
	printf("=================\n");
	lispDisplay(parsetree_list); /* takes one arg, not two */
    }

    foreach(i, parsetree_list) {
	int 	  which_util;
	LispValue parsetree = CAR(i);

	/*
	 *  For each query that isn't a utility invocation,
	 *  generate a plan.
	 */

	if (!atom(CAR(parsetree))) {

	    if (IsAbortedTransactionBlockState()) {
		/* ----------------
		 *   the EndCommand() stuff is to tell the frontend
		 *   that the command ended. -cim 6/1/90
		 * ----------------
		 */
		char *tag = "*ABORT STATE*";
		EndCommand(tag, dest);
	    
		elog(NOTICE, "(transaction aborted): %s",
		     "queries ignored until END");
	    
		*parsetreeP = (List)NULL;
		return (List)NULL;
	    }

	    if (! Quiet)
		puts("\tinit_planner()..");

	    init_planner();
	    
	    if (ShowPlannerStats) ResetUsage();
	    plan = planner(parsetree);
	    if (ShowPlannerStats) {
		fprintf(stderr, "! Planner Stats:\n");
		ShowUsage();
	    }
	    plan_list = nappend1(plan_list, (LispValue)plan);
	}
    }

    if (testFlag)
       MemoryContextSwitchTo(oldcontext);

    if (parsetreeP != (LispValue *) NULL)
	*parsetreeP = parsetree_list;

    return (plan_list);
}

void
pg_eval(query_string, argv, typev, nargs)
    String query_string;
    char *argv;
    ObjectId *typev;
    int nargs;
{
    extern CommandDest whereToSendOutput;
    extern void pg_eval_dest();

    pg_eval_dest(query_string, argv, typev, nargs, whereToSendOutput);
}

void
pg_eval_dest(query_string, argv, typev, nargs, dest)
    String	query_string;	/* string to execute */
    char	*argv;		/* arguments */
    ObjectId	*typev;		/* argument types */
    int		nargs;		/* number of arguments */
    CommandDest dest;		/* where results should go */
{
    LispValue parsetree_list;
    List plan_list;
    Plan plan;
    List parsetree;
    int j;
    int which_util;

    /* plan the queries */
    plan_list = pg_plan(query_string, typev, nargs, &parsetree_list, dest);

    while (parsetree_list != LispNil) {
	parsetree = CAR(parsetree_list);
	parsetree_list = CDR(parsetree_list);

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
	    
	    which_util = LISPVALUE_INTEGER(CAR(parsetree));
	    ProcessUtility(which_util, CDR(parsetree), query_string, dest);

	    /*
	     *  XXX -- ugly hack:  vacuum spans transactions (it calls
	     *  Commit and Start itself), and so if this was a vacuum
	     *  command, all the memory we were using is gone.  the right
	     *  thing to do is to allocate the parse tree in a distinguished
	     *  memory context.  until i get around to doing that, just
	     *  bounce out of here after vacuuming.  --mao 4 may 91
	     */

	    if (which_util == VACUUM)
		return;
	} else {
	    plan = (Plan) CAR(plan_list);
	    plan_list = CDR(plan_list);

	    /* ----------------
	     *	print plan if debugging
	     * ----------------
	     */
	    if ( DebugPrintPlan == true ) {
		printf("\nPlan is :\n");
		lispDisplay((LispValue)plan);
		printf("\n");
	    }

	    if (testFlag && IsA(plan,Choose)) {
		/* ----------------
		 *	 this is my experiment stuff, will be taken away soon.
		 *   ignore it. -- Wei
		 * ----------------
		 */
		Plan 	  p;
		List 	  planlist;
		LispValue x;
		List 	  setRealPlanStats();
		List 	  pruneHashJoinPlans();

		planlist = get_chooseplanlist((Choose)plan);
		planlist = setRealPlanStats(parsetree, planlist);
		planlist = pruneHashJoinPlans(planlist);
		
		foreach (x, planlist) {
		    char s[10];
		    p = (Plan) CAR(x);
		    p_plan(p);
		    if (confirmExecute) {
			printf("execute (y/n/q)? ");
			scanf("%s", s);
			if (s[0] == 'n') continue;
			if (s[0] == 'q') break;
		    }
		    BufferPoolBlowaway();
		    
		    CommitTransactionCommand();
		    StartTransactionCommand();
		    
		    ResetUsage();
		    ProcessQuery(parsetree, p, argv, typev, nargs, dest);
		    ShowUsage();
		}
		 GlobalMemoryDestroy(ParserPlannerContext);
	    } else {
		/* ----------------
		 *   execute the plan
		 *
		 *   Note: _exec_repeat_ defaults to 1 but may be changed
		 *	   by a DEBUG command.   If you set this to a large
		 *	   number N, run a single query, and then set it
		 *	   back to 1 and run N queries, you can get an idea
		 *	   of how much time is being spent in the parser and
		 *	   planner b/c in the first case this overhead only
		 *	   happens once.  -cim 6/9/91
		 * ----------------
		 */
		if (ShowExecutorStats)
		    ResetUsage();
		
		for (j = 0; j < _exec_repeat_; j++) {
		    if (! Quiet) {
			time(&tim);
			printf("\tProcessQuery() at %s\n", ctime(&tim));
		    }
		    ProcessQuery(parsetree, plan, argv, typev, nargs, dest);
		}
		
		if (ShowExecutorStats) {
		    fprintf(stderr, "! Executor Stats:\n");
		    ShowUsage();
		}
	    }
	}
	/*
	 *  In a query block, we want to increment the command counter
	 *  between queries so that the effects of early queries are
	 *  visible to subsequent ones.
	 */

	if (parsetree_list != LispNil)
	     CommandCounterIncrement();
    }
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
 *      quickdie() occurs when signalled by the postmaster, some backend
 *      has bought the farm we need to stop what we're doing and exit.
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
quickdie()
{
  elog(NOTICE, "I have been signalled by the postmaster.");
  elog(NOTICE, "Some backend process has died unexpectedly and possibly");
  elog(NOTICE, "corrupted shared memory.  The current transaction was");
  elog(NOTICE, "aborted, and I am going to exit.  Please resend the");
  elog(NOTICE, "last query. -- The postgres backend");
  ExitPostgres(0);
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
    register 	int	i;
    int			flagC;
    int			flagQ;
    int			flagS;
    int			flagE;
    int			flag;

    int			errs = 0;
    char		*DatabaseName;

    char		firstchar;
    char		parser_input[MAX_PARSE_BUFFER];
    
    extern	int	Noversion;		/* util/version.c */
    extern	int	Quiet;
    extern	jmp_buf	Warn_restart;
    extern	int	optind;
    extern	char	*optarg;
    extern      char	*DBName; /* a global name for current database */
    extern	short	DebugLvl;

#ifdef NOFIXADE
    /* under ultrix, disable unaligned address fixups */
    syscall(SYS_sysmips, MIPS_FIXADE, 0, NULL, NULL, NULL);
#endif

    /* ----------------
     * 	register signal handlers.
     * ----------------
     */
    signal(SIGHUP, die);
    signal(SIGINT, die);
    signal(SIGTERM, die);
    signal(SIGPIPE, die);
    signal(SIGUSR1, quickdie);
    /*
     * Turn off async portals for 4.0.1
     */
#if 0
    {				/* asynchronous notification */
	extern void Async_NotifyFrontEnd();
	signal(SIGUSR2, Async_NotifyFrontEnd);
    }
#endif

#ifdef PARALLELDEBUG
    usclk_init();
#endif
#ifdef sequent
    /* -------------------
     * increase the maximum number of file descriptors on sequent
     * the default is only 20.
     * the following call may not guarantee to give you 256 file descriptors
     * usually you only get 64.
     * ------------------
     */
    setdtablesize(256);
#endif

    /* ----------------
     *	initialize palloc memory tracing
     * ----------------
     */
    set_palloc_debug(false, false);
    
    /* ----------------
     *	parse command line arguments
     * ----------------
     */
    flagC = flagQ = flagS = flagE = ShowStats = 0;
    ShowParserStats = ShowPlannerStats = ShowExecutorStats = 0;
    MasterPid = getpid();
    
    while ((flag = getopt(argc, argv, "A:B:bCd:Ef:GiLNP:pQSst:Tx:")) != EOF)
      switch (flag) {
	  	  
      case 'A':
	  /* ----------------
	   *	tell postgres to turn on memory tracing
	   *
	   *	-An = print alloctions/deallocations when they occur
	   *	-Ar = record activity silently in a list
	   *	-Ab = record and print activity
	   *	-AQ# = dump activity list every # tuples processed (per query)
	   *	-AX# = dump activity list every # transactions processed
	   * ----------------
	   */
	  switch (optarg[0]) {
	  case 'n':  set_palloc_debug(true, false);  break;
	  case 'r':  set_palloc_debug(false, true);  break;
	  case 'b':  set_palloc_debug(true, true);   break;
	  case 'Q':  set_palloc_debug(false, true);
		     query_tuple_max = atoi(&optarg[1]); break;
	  case 'X':  set_palloc_debug(false, true);
		     xact_tuple_max = atoi(&optarg[1]); break;
	  default:   errs++; break;
	  } 
	  break;
	  
      case 'b':
	  /* ----------------
	   *	set BushyPlanFlag to true.
	   * ----------------
	   */
	  BushyPlanFlag = 1;
	  break;
      case 'B':
	  /* ----------------
	   *	specify the size of buffer pool
	   * ----------------
	   */
	  NBuffers = atoi(optarg);
	  break;
	  
      case 'C':
	  /* ----------------
	   *	don't print version string (don't know why this is 'C' --mao)
	   * ----------------
	   */
	  flagC = 1;
	  break;
	  
	  /* ----------------
	   *	-debug mode
	   * ----------------
	   */
      case 'd':
	  /* DebugMode = true;  */
	  flagQ = 0;
	  DebugPrintPlan = true;
	  DebugPrintParse = true;
	  DebugPrintRewrittenParsetree = true;
	  DebugLvl = (short)atoi(optarg);
	  break;
	  
      case 'E':
	  /* ----------------
	   *	E - echo the query the user entered
	   * ----------------
	   */
	  flagE = 1;
	  break;
	  	  
      case 'f':
          /* -----------------
           *    f - forbid generation of certain plans
           * -----------------
           */
          switch (optarg[0]) {
          case 's': /* seqscan */
                _enable_seqscan_ = false;
                break;
          case 'i': /* indexscan */
                _enable_indexscan_ = false;
                break;
          case 'n': /* nestloop */
                _enable_nestloop_ = false;
                break;
          case 'm': /* mergejoin */
                _enable_mergesort_ = false;
                break;
          case 'h': /* hashjoin */
                _enable_hashjoin_ = false;
                break;
          default:
                errs++;
          }
	  break;

      case 'G':
	  /* ----------------
	   *	G - use cacheoffgetattr instead of fastgetattr
	   * ----------------
	   */
	  set_use_cacheoffgetattr(1);
	  break;

      case 'i':
	  confirmExecute = 1;
	  break;

       case 'L':
	  /* --------------------
	   *  turn off locking
	   * --------------------
	   */
	  lockingOff = 1;
	  break;

      case 'N':
	  /* ----------------
	   *	N - Don't use newline as a query delimiter
	   * ----------------
	   */
	  UseNewLine = 0;
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
	   *	P - Use the passed file descriptor number as the port
	   *    on which to communicate with the user.  This is ONLY
	   *    useful for debugging when fired up by the postmaster.
	   * ----------------
	   */
          Portfd = atoi(optarg);
	  break;

      case 'Q':
	  /* ----------------
	   *	Q - set Quiet mode (reduce debugging output)
	   * ----------------
	   */
	  flagQ = 1;
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

      case 's':
          /* ----------------
           *    s - report usage statistics (timings) after each query
           * ----------------
           */
          ShowStats = 1;
	  StatFp = stderr;
          break;

      case 't':
	  /* ----------------
	   *	tell postgres to report usage statistics (timings) for
	   *	each query
	   *
	   *	-tpa[rser] = print stats for parser time of each query
	   *	-tpl[anner] = print stats for planner time of each query
	   *	-te[xecutor] = print stats for executor time of each query
	   *	caution: -s can not be used together with -t.
	   * ----------------
	   */
	  StatFp = stderr;
	  switch (optarg[0]) {
	  case 'p':  if (optarg[1] == 'a')
			ShowParserStats = 1;
		     else if (optarg[1] == 'l')
			ShowPlannerStats = 1;
		     else
			errs++;
		     break;
	  case 'e':  ShowExecutorStats = 1;   break;
	  default:   errs++; break;
	  } 
	  break;

      case 'T':
          /* ----------------
           *    T - Testing mode: execute all the possible plans instead
           *    of just the optimal one
	   *
	   *    XXX not implemented yet,  currently it will just print
	   *    the statistics of the executor, like the -s option
	   *	but does not include parsing and planning time.
           * ---------------
           */
          testFlag = 1;
          break;

      case 'x':
	  /* control joey hellerstein's expensive function optimization */
	  if (strcmp(optarg, "off") == 0)
	    XfuncMode = XFUNC_OFF;
	  else if (strcmp(optarg, "nor") == 0)
	    XfuncMode = XFUNC_NOR;
	  else if (strcmp(optarg, "nopull") == 0)
	    XfuncMode = XFUNC_NOPULL;
	  else if (strcmp(optarg, "nopm") == 0)
	    XfuncMode = XFUNC_NOPM;
	  else if (strcmp(optarg, "noprune") == 0)
	    XfuncMode = XFUNC_NOPRUNE;
	  else {
	       fprintf(stderr, "use -x {off,nor,nopull,nopm,noprune}\n");
	       errs++;
	  }
	  break;

      default:
	  /* ----------------
	   *	default: bad command line option
	   * ----------------
	   */
	  errs++;
      }

    if (errs || argc - optind > 1) {
	fprintf(stderr, "Usage: %s [-A alloctype] [-B nbufs] [-d lvl] ] [-f plantype]\n", argv[0]);
	fprintf(stderr,"\t[-P portno] [-t tracetype] [-x opttype] [-bCEGiLNpQSsT] [dbname]\n");
	fprintf(stderr, "    A: trace memory allocations\n");
	fprintf(stderr, "    b: consider bushy plan trees during optimization\n");
	fprintf(stderr, "    B: set number of buffers in buffer pool\n");
	fprintf(stderr, "    C: supress version info\n");
	fprintf(stderr, "    d: set debug level\n");
	fprintf(stderr, "    E: echo query before execution\n");
	fprintf(stderr, "    f: forbid plantype generation\n");
	fprintf(stderr, "    G: use cacheoffgetattr instead of fastgetattr\n");
	fprintf(stderr, "    i: prompt for confirmation before execution\n");
	fprintf(stderr, "    L: turn off locking\n");
	fprintf(stderr, "    N: don't use newline as query delimiter\n");
	fprintf(stderr, "    p: backend started by postmaster\n");
	fprintf(stderr, "    P: set port file descriptor\n");
	fprintf(stderr, "    Q: suppress informational messages\n");
	fprintf(stderr, "    S: assume stable main memory\n");
	fprintf(stderr, "    s: show stats after each query\n");
	fprintf(stderr, "    t: trace component execution times\n");
	fprintf(stderr, "    T: execute all possible plans for each query\n");
	fprintf(stderr, "    x: control expensive function optimization\n");
	exitpg(1);
    } else if (argc - optind == 1) {
	DBName = DatabaseName = argv[optind];
    } else if ((DBName = DatabaseName = PG_username) == NULL) {
	fprintf(stderr, "%s: USER undefined and no database specified\n",
	      argv[0]);
	exitpg(1);
    }

    if (ShowStats && 
	(ShowParserStats || ShowPlannerStats || ShowExecutorStats)) {
	fprintf(stderr, "-s can not be used together with -t.\n");
	exitpg(1);
    }

    /* ----------------
     *	get user name and pathname and check command line validity
     * ----------------
     */
    GetUserName();
    get_pathname(argv);

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
	printf("\tstable    =    %c\n", flagS     ? 't' : 'f');
	printf("\ttimings   =    %c\n", ShowStats ? 't' : 'f');
	printf("\tbufsize   =    %d\n", NBuffers);
	
	printf("\tquery echo =   %c\n", EchoQuery ? 't' : 'f');
	printf("\tDatabaseName = [%s]\n", DatabaseName);
	puts("\t----------------\n");
    }
    
    /* ----------------
     *	initialize portal file descriptors
     * ----------------
     */
    if (IsUnderPostmaster == true) {
	if (Portfd < 0) {
	    fprintf(stderr,
		    "Postmaster flag set, but no port number specified\n");
	    exitpg(1);
	}
	pq_init(Portfd);
    }
    
    if (IsUnderPostmaster)
	whereToSendOutput = Remote;
    else 
	whereToSendOutput = Debug;

    SetProcessingMode(InitProcessing);
    
    /* initialize */
    if (! Quiet)
	puts("\tInitPostgres()..");
    InitPostgres(DatabaseName);

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
	
	if (! Quiet)
	    printf("\tAbortCurrentTransaction() at %s\n", ctime(&tim));

	for ( i = 0 ; i < MAX_PARSE_BUFFER ; i++ ) 
	  parser_input[i] = 0;

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

    /* ----------------
     * if stable main memory is assumed (-S flag is set), it is necessary
     * to flush all dirty shared buffers before exit
     * plai 8/7/90
     * ----------------
     */
    if (!TransactionFlushEnabled())
        on_exitpg(FlushBufferPool, (caddr_t) 0);

    for (;;) {
	/* ----------------
	 *   (1) read a command. 
	 * ----------------
	 */
	for (i=0; i< MAX_PARSE_BUFFER; i++) 
	    parser_input[i] = 0;

	firstchar = ReadCommand(parser_input);

	/* process the command */
	switch (firstchar) {
	    /* ----------------
	     *	'F' incicates a fastpath call.
	     *      XXX HandleFunctionRequest
	     * ----------------
	     */
	case 'F':
	    IsEmptyQuery = false;

	    /* start an xact for this funciton invocation */
	    if (! Quiet) {
		time(&tim);
		printf("\tStartTransactionCommand() at %s\n", ctime(&tim));
	    }

	    StartTransactionCommand();
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
		IsEmptyQuery = true;
	    } else {
		/* ----------------
		 *  otherwise, process the input string.
		 * ----------------
		 */
	        IsEmptyQuery = false;
		if (ShowStats)
		    ResetUsage();

		/* start an xact for this query */
		if (! Quiet) {
		    time(&tim);
		    printf("\tStartTransactionCommand() at %s\n", ctime(&tim));
		}
		StartTransactionCommand();

		pg_eval(parser_input, (char *) NULL, (ObjectId *) NULL, 0);

		if (ShowStats)
		    ShowUsage();
	    }
	    break;
	    
	/* ----------------
	 *	'X' means that the frontend is closing down the socket
	 * ----------------
	 */
	case 'X':
	    IsEmptyQuery = true;
	    pq_close();
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
	if (! IsEmptyQuery) {
	    if (! Quiet) {
		time(&tim);
		printf("\tCommitTransactionCommand() at %s\n", ctime(&tim));
	    }
	    CommitTransactionCommand();

#ifdef PALLOC_DEBUG	
	    /* ----------------
	     *	if debugging memory allocations, print memory dump
	     *	every so often
	     * ----------------
	     */
	    if (xact_tuple_max && xact_tuple_max == ++xact_tuple_count) {
		xact_tuple_count = 0;
		dump_palloc_list("PostgresMain", true);
	    }
#endif PALLOC_DEBUG
	
	} else {
	    if (IsUnderPostmaster)
		NullCommand(Remote);
	}

    }
}

#include <sys/resource.h>

struct rusage Save_r;
struct timeval Save_t;

ResetUsage()
{
        struct timezone tz;
	getrusage(RUSAGE_SELF, &Save_r);
        gettimeofday(&Save_t, &tz);
	ResetBufferUsage();
	ResetTupleCount();
}

ShowUsage()
{
	struct rusage r;
	struct timeval user, sys;
        struct timeval elapse_t;
        struct timezone tz;

	getrusage(RUSAGE_SELF, &r);
	gettimeofday(&elapse_t, &tz);
	bcopy(&r.ru_utime, &user, sizeof(user));
	bcopy(&r.ru_stime, &sys, sizeof(sys));
        if (elapse_t.tv_usec < Save_t.tv_usec) {
                elapse_t.tv_sec--;
                elapse_t.tv_usec += 1000000;
        }
	if (r.ru_utime.tv_usec < Save_r.ru_utime.tv_usec) {
		r.ru_utime.tv_sec--;
		r.ru_utime.tv_usec += 1000000;
	}
	if (r.ru_stime.tv_usec < Save_r.ru_stime.tv_usec) {
		r.ru_stime.tv_sec--;
		r.ru_stime.tv_usec += 1000000;
	}

	/*
	 *  the only stats we don't show here are for memory usage -- i can't
	 *  figure out how to interpret the relevant fields in the rusage
	 *  struct, and they change names across o/s platforms, anyway.
	 *  if you can figure out what the entries mean, you can somehow
	 *  extract resident set size, shared text size, and unshared data
	 *  and stack sizes.
	 */

	fprintf(StatFp, "! system usage stats:\n");
        fprintf(StatFp, 
		"!\t%ld.%06ld elapsed %ld.%06ld user %ld.%06ld system sec\n",
                elapse_t.tv_sec - Save_t.tv_sec,
                elapse_t.tv_usec - Save_t.tv_usec,
		r.ru_utime.tv_sec - Save_r.ru_utime.tv_sec,
		r.ru_utime.tv_usec - Save_r.ru_utime.tv_usec,
		r.ru_stime.tv_sec - Save_r.ru_stime.tv_sec,
		r.ru_stime.tv_usec - Save_r.ru_stime.tv_usec);
	fprintf(StatFp,
		"!\t[%ld.%06ld user %ld.%06ld sys total]\n",
		user.tv_sec, user.tv_usec, sys.tv_sec, sys.tv_usec);
	fprintf(StatFp, 
		"!\t%d/%d [%d/%d] filesystem blocks in/out\n",
		r.ru_inblock - Save_r.ru_inblock,
		/* they only drink coffee at dec */
		r.ru_oublock - Save_r.ru_oublock,
		r.ru_inblock, r.ru_oublock);
	fprintf(StatFp, 
		"!\t%d/%d [%d/%d] page faults/reclaims, %d [%d] swaps\n",
		r.ru_majflt - Save_r.ru_majflt,
		r.ru_minflt - Save_r.ru_minflt,
		r.ru_majflt, r.ru_minflt,
		r.ru_nswap - Save_r.ru_nswap,
		r.ru_nswap);
	fprintf(StatFp, 
		"!\t%d [%d] signals rcvd, %d/%d [%d/%d] messages rcvd/sent\n",
		r.ru_nsignals - Save_r.ru_nsignals,
		r.ru_nsignals,
		r.ru_msgrcv - Save_r.ru_msgrcv,
		r.ru_msgsnd - Save_r.ru_msgsnd,
		r.ru_msgrcv, r.ru_msgsnd);
	fprintf(StatFp, 
		"!\t%d/%d [%d/%d] voluntary/involuntary context switches\n",
		r.ru_nvcsw - Save_r.ru_nvcsw,
		r.ru_nivcsw - Save_r.ru_nivcsw,
		r.ru_nvcsw, r.ru_nivcsw);
	fprintf(StatFp, "! postgres usage stats:\n");
	PrintBufferUsage(StatFp);
	DisplayTupleCount(StatFp);
	ShowPrs2Stats(StatFp);
}
