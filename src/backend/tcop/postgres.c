
/**********************************************************************
  postgres.c

  POSTGRES C Backend Interface
  $Header$
 **********************************************************************/

#include "c.h"

#include <signal.h>

#include <sys/file.h>
#include <stdio.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>

#include "atoms.h"		/* for ScanKeywordLookup */
#include "command.h"		/* for EndCommand */
#include "executor.h"
#include "globals.h"		/* for IsUnderPostmaster */
#include "relation.h"

#include "pg_lisp.h"
#include "parse.h"

#include "exc.h"

#include "heapam.h"
#include "mcxt.h"
#include "oid.h"
#include "pinit.h"
#include "pmod.h"
#include "portal.h"

#include "fmgr.h"
#include "catalog.h"
#include "postgres.h"
#include "c.h"
#include "bufmgr.h"
#include "log.h"
#include "htup.h"
#include "portal.h"
#include "rel.h"
#include "relscan.h"
#include "skey.h"
#include "tim.h"
#include "trange.h"
#include "xcxt.h"
#include "xid.h"

#include "parse.h"

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
 *	MakeQueryDesc
 * ----------------
 */

List
MakeQueryDesc(command, parsetree, plantree, state, feature)
     List  command;
     List  parsetree;
     List  plantree;
     List  state;
     List  feature;
{
   return (List)
      lispCons(command,
	       lispCons(parsetree,
			lispCons(plantree,
				 lispCons(state,
					  lispCons(feature, LispNil)))));

}

/* ----------------
 *	doretblank
 *
 *      XXX this should disappear when the tcop is converted
 *          this is just a temporary hack for now.. -cim 8/3/89
 *
 * ----------------
 */

void
ProcessQuery(parser_output, plan)
    List parser_output;
    Plan plan;
{
    bool	isPortal = false;
    bool	isInto = false;
    Relation	intoRelation;
    List	parseRoot;
    List	resultDesc;

    List 	queryDesc;
    EState 	state;
    List 	feature;
    List 	attinfo;		/* returned by ExecMain */

    int		 commandType;
    String	 tag;
    ObjectId	 intoRelationId;
    String	 intoRelationName;
    int		 numatts;
    AttributePtr tupleDesc;
    
    /* ----------------
     *	resolve the status of our query... are we retrieving
     *  into a portal, into a relation, etc.
     * ----------------
     */
    parseRoot = parse_root(parser_output);
	
    commandType = root_command_type(parseRoot);
    switch (commandType) {
    case RETRIEVE:
	tag = "RETRIEVE";
	break;
    case APPEND:
	tag = "APPEND";
	break;
    case DELETE:
	tag = "DELETE";
	break;
    case EXECUTE:
	tag = "EXECUTE";
	break;
    case REPLACE:
	tag = "REPLACE";
	break;
    default:
	elog(WARN, "ProcessQuery: unknown command type %d", commandType);
    }

    if (root_command_type(parseRoot) == RETRIEVE) {
	resultDesc = root_result_relation(parseRoot);
	if (!null(resultDesc)) {
	    int	destination;
	    destination = CAtom(CAR(resultDesc));

	    switch (destination) {
	    case INTO:
		isInto = true;
		intoRelationName = CString(CADR(resultDesc));
		break;
	    case PORTAL:
		isPortal = true;
		break;
	    default:
		elog(WARN, "ProcessQuery: bad result %d", destination);
	    }
	}
    }

    /* ----------------
     *	create the Executor State structure
     * ----------------
     */
    {
	ScanDirection   direction;                    
	abstime         time;                         
	ObjectId        owner;                        
	List            locks;                        
	List            subPlanInfo;                 
	Name            errorMessage;                
	List            rangeTable;                  
	HeapTuple       qualTuple;
	ItemPointer     qualTupleID;
	Relation        relationRelationDesc;
	Index        	resultRelationIndex;
	Relation        resultRelationDesc; 

	direction = 	EXEC_FRWD;
	time = 		0;
	owner = 	0;                
	locks = 	LispNil;                
	qualTuple =	NULL;
	qualTupleID =	0;

	/* ----------------
	 *   currently these next are initialized in InitPlan.  For now
	 *   we pass dummy variables.. Eventually this should be cleaned up.
	 *   -cim 8/5/89
	 * ----------------
	 */
	rangeTable = 	  	LispNil;
	subPlanInfo = 	  	LispNil;   
	errorMessage = 	  	NULL;
	relationRelationDesc = 	NULL;
	resultRelationIndex =	0;
	resultRelationDesc =   	NULL;

	state = MakeEState(direction,    
			   time,                 
			   owner,                
			   locks,                
			   subPlanInfo,          
			   errorMessage,         
			   rangeTable,           
			   qualTuple,            
			   qualTupleID,          
			   relationRelationDesc, 
			   resultRelationIndex,       
			   resultRelationDesc);   
    }
   
    /* ----------------
     *	now, prepare the plan for execution by calling ExecMain()
     *	feature = '(start)
     * ----------------
     */
   
    feature = lispCons(lispInteger(EXEC_START), LispNil);
    queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))),
			      parser_output,
			      plan,
			      state,
			      feature);

    attinfo = ExecMain(queryDesc);
    
    /* ----------------
     *   extract result type information from attinfo
     *	 returned by ExecMain()
     * ----------------
     */
    numatts = 	CInteger(CAR(attinfo));
    tupleDesc = (AttributePtr) CADR(attinfo);
    
    /* ----------------
     *   display the result of the first call to ExecMain()
     * ----------------
     */

    showatts("blank", numatts, tupleDesc);

    /* ----------------
     *   now how in the hell is this ever supposed to work?
     *   we have only initialized the plan..  Why do we then
     *   return here if isPortal?  This *CANNOT* be correct.
     *   but portals aren't *my* problem (yet..) -cim 8/29/89
     * ----------------
     */
    if (isPortal) {
	BlankPortalAssignName(intoRelationName);
	return;			/* XXX see previous comment */
    }

    /* ----------------
     *	if we are retrieveing into a result relation, then
     *  open it..  This should probably be done in InitPlan
     *  so I am going to move it there soon. -cim 8/29/89
     * ----------------
     */
    if (isInto) {
	char		archiveMode;
	/*
	 * Archive mode must be set at create time.  Unless this
	 * mode information is made specifiable in POSTQUEL, users
	 * will have to COPY, rename, etc. to change archive mode.
	 */
	archiveMode = 'n';

	intoRelationId =
	    RelationNameCreateHeapRelation(intoRelationName,
					   archiveMode,
					   numatts,
					   tupleDesc);

	setheapoverride(true);	/* XXX change "amopen" args instead */

	intoRelation = ObjectIdOpenHeapRelation(intoRelationId);

	setheapoverride(false);	/* XXX change "amopen" args instead */
    }

    /* ----------------
     *   Now we get to the important call to ExecMain() where we
     *   actually run the plan..
     * ----------------
     */

    if (isInto) {
	/* ----------------
	 *  XXX hack - we are passing the relation
	 *  descriptor as an integer.. we should either
	 *  think of a better way to do this or come up
	 *  with a node type suited to handle it..
	 * ----------------
	 */
	feature = lispCons(lispInteger(EXEC_RESULT),
			   lispCons(lispInteger(intoRelation),
				    LispNil));
	
    } else if (IsUnderPostmaster) {
	feature = lispCons(lispInteger(EXEC_DUMP), LispNil);
    } else {
	feature = lispCons(lispInteger(EXEC_DEBUG), LispNil);
    }

    queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))),
			      parser_output,
			      plan,
			      state,
			      feature);

    (void) ExecMain(queryDesc);

    /* ----------------
     *   final call to ExecMain.. we close down all the scans
     *   and free allocated resources...
     * ----------------
     */
    
    feature = lispCons(lispInteger(EXEC_END), LispNil);
    queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))),
			     parser_output,
			     plan,
			     state,
			     feature);

    (void) ExecMain(queryDesc);

    /* ----------------
     *   close result relation
     *   XXX this will be moved to moved to EndPlan soon
     *		-cim 8/29/89
     * ----------------
     */
    if (isInto) {
	RelationCloseHeapRelation(intoRelation);
    }

    /* ----------------
     *   not certain what this does.. -cim 8/29/89
     * ----------------
     */
    if (IsUnderPostmaster) {
	EndCommand(tag);
    }
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
