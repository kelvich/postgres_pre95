static	char	amiint_c[] = "$Header$";

/**********************************************************************
  ami_sup.c

  support routines for the new amiint

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

/* ----------------
 *	MakeQueryDesc
 * ----------------
 */

LispValue
MakeQueryDesc(command, parsetree, plantree, state, feature)
     LispValue	command;
     LispValue	parsetree;
     LispValue	plantree;
     LispValue  state;
     LispValue  feature;
{
   return (LispValue)
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
     LispValue parser_output;
     Plan      plan;
{
	bool		isPortal = false;
	bool		isInto = false;
	Relation	intoRelation;
	LispValue	parseRoot;
	LispValue	resultDesc;
   LispValue queryDesc;
   EState state;
   LispValue feature;
   LispValue attinfo;		/* returned by ExecMain */

	parseRoot = parse_root(parser_output);
	if (root_command_type(parseRoot) == RETRIEVE) {

		resultDesc = root_result_relation(parseRoot);
		if (!null(resultDesc)) {
			int	destination;

			destination = CAtom(CAR(resultDesc));

			switch (destination) {
			case INTO:
				isInto = true;
				break;
			case PORTAL:
				isPortal = true;
				break;
			default:
				elog(WARN, "ProcessQuery: bad result %d",
					destination);
			}
		}
	}
   {
      /* ----------------
       *	create the Executor State structure
       * ----------------
       */
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
      ObjectId        resultRelation;
      Relation        resultRelationDesc; 

      direction = 		  EXEC_FRWD;
      time = 		  0;
      owner = 		  0;                
      locks = 		  LispNil;                
      qualTuple =		  NULL;
      qualTupleID =	  0;

      /* ----------------
       *   currently these next are initialized in InitPlan.  For now
       *   we pass dummy variables.. Eventually this should be cleaned up.
       *   -cim 8/5/89
       * ----------------
       */
      rangeTable = 	  LispNil;
      subPlanInfo = 	  LispNil;   
      errorMessage = 	  NULL;
      relationRelationDesc = NULL;
      resultRelation =	  NULL;
      resultRelationDesc =   NULL;

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
			 resultRelation,       
			 resultRelationDesc);   
   }
   
   /* ----------------
    *	first time through ExecMain we call it with feature = '(start)
    *
    *   (setq attinfo (ExecMain (list (cadar partree)
    *			      partree plan state '(start))))
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
    *   display the result of the first call to ExecMain()
    * ----------------
    */

   showatts("blank", CInteger(CAR(attinfo)), CADR(attinfo));

	if (isPortal) {
		BlankPortalAssignName(CString(CADR(resultDesc)));
		return;
	}
	if (isInto) {
		ObjectId	relationId;
		/*
		 * Archive mode must be set at create time.  Unless this
		 * mode information is made specifiable in POSTQUEL, users
		 * will have to COPY, rename, etc. to change archive mode.
		 */
		relationId = RelationNameCreateHeapRelation(
			CString(CADR(resultDesc)),
			'n',	/* XXX */
			CInteger(CAR(attinfo)), CADR(attinfo));

		setheapoverride(true);	/* XXX change "amopen" args instead */
		intoRelation = ObjectIdOpenHeapRelation(relationId);
		setheapoverride(false);	/* XXX change "amopen" args instead */
	}
   /* (showatts "blank" (car attinfo) (cadr attinfo)) */

   /* ----------------
    *   second call to ExecMain we have feature = '(debug) meaning
    *   run the plan and generate debugging output
    *
    *   (ExecMain (list (cadar partree) partree plan state '(debug)))
    *
    *	'(debug) == EXEC_DEBUG	("DEBUG" is already used)
    *
    * ----------------
    */

	if (isInto) {
		feature = lispCons(lispInteger(EXEC_RESULT),
			lispCons(lispInteger(intoRelation)));	/* XXX hack */
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
    *   final call to ExecMain we have feature = '(end) meaning
    *   close relations, end scans and free resources
    *
    *   (ExecMain (list (cadar partree) partree plan state '(end)))
    *
    * ----------------
    */

   feature = lispCons(lispInteger(EXEC_END), LispNil);
   queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))),
			     parser_output,
			     plan,
			     state,
			     feature);

   (void) ExecMain(queryDesc);
	if (isInto) {
		RelationCloseHeapRelation(intoRelation);
	}
	if (IsUnderPostmaster) {
		int	commandType;
		String	tag;

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
			elog(WARN, "ProcessQuery: unknown command type %d",
				commandType);
		}
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
	register int	i;
	int		flagC, flagQ;
	int		flag, errs = 0;
	char		*dat;
	extern	int	Noversion;		/* util/version.c */
	extern	int	Quiet;
/*	
        extern	char	Blanks[];
	extern  char    Buf[];
*/  
	extern	jmp_buf	Warn_restart;
	extern	int	optind;
	extern	char	*optarg;
	int		setjmp(), chdir();
	char		*getenv();
	char		*parser_input = malloc(8192);
	LispValue	parser_output;

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
		goto usage;
	} else if (argc - optind == 1) {
		dat = argv[optind];
	} else if ((dat = getenv("USER")) == NULL) {
		fputs("amiint: failed getenv(\"USER\")\n");
		exit(1);
	}
	Noversion = flagC;
	Quiet = flagQ;

	/* initialize the dynamic function manager */
	DynamicLinkerInit(argv[0]);

	/* various initiailization routines */

	signal(SIGHUP, die);
	signal(SIGINT, die);
	signal(SIGTERM, die);

	/* XXX the -C version flag should be removed and combined with -O */
	SetProcessingMode((override) ? BootstrapProcessing : InitProcessing);
	InitPostgres(NULL, dat);

	signal(SIGHUP, handle_warn);

	if (setjmp(Warn_restart) != 0) {
		Warnings++;
		AbortCurrentTransaction();
	}

	/*** new code for handling input */

	puts("POSTGRES backend interactive interface:  $Revision$ ($Date$)");

	parser_output = lispList();
	for (;;) {
/*
		StartTransactionCommand();
		startmmgr(0);
*/
	  /* OverrideTransactionSystem(true); */

	  StartTransactionCommand();
	  printf("> ");
	  parser_input = gets(parser_input);
	  printf("input string is %s\n",parser_input);
	  parser(parser_input,parser_output);
	  printf("parser outputs :\n");
	  lispDisplay(parser_output,0);
	  printf("\n");

	  if (lispIntegerp(CAR(parser_output))) {
		  ProcessUtility(LISPVALUE_INTEGER(CAR(parser_output)),
			  CDR(parser_output));
	  } else {
	      Node plan;
	      extern Node planner();
#ifdef NOTYET	      
	      extern void init_planner();

	      init_planner();
#endif NOTYET
	      
	      /* ----------------
	       *   generate the plan
	       * ----------------
	       */
	      plan = planner(parser_output);
	      printf("\nPlan is :\n");
	      (*(plan->printFunc))(plan);
	      printf("\n");
	      /* ----------------
	       *   call the executor
	       * ----------------
	       */
	      ProcessQuery(parser_output, plan);
	  }
	  CommitTransactionCommand();
     }

 usage:
	fputs("Usage: amiint [-C] [-Q] [datname]\n", stderr);
	exit(1);
}	

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
