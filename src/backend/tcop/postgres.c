static	char	amiint_c[] = "$Header$";

/**********************************************************************
  ami_sup.c

  support routines for the new amiint

 **********************************************************************/

#include <signal.h>

#include "c.h"
#include "pg_lisp.h"
#include "parse.h"

#include "exc.h"	/* for ExcAbort and <setjmp.h> */

#include "heapam.h"
#include "mcxt.h" 
#include "oid.h"
#include "pinit.h"
#include "pmod.h"
#include "portal.h"

#include "bkint.h"



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

macro		*strtable [STRTABLESIZE];
hashnode	*hashtable [HASHTABLESIZE];

FILE		*source = NULL;
static int	strtable_end = -1;    /* Tells us last occupied string space */
static int	num_of_errs = 0;      /* Total number of errors encountered */

static char *   (typestr[15]) = {
	"bool", "bytea", "char", "char16", "dt", "int2", "int28",
	"int4", "regproc", "text", "oid", "tid", "xid", "iid",
	"oid8"
};

/* functions associated with each type */
static	long	Procid[15][7] = {
	{ 16, 1, 0, F_BOOLIN, F_BOOLOUT, F_BOOLEQ, NULL },
	{ 17, -1, 1, F_BYTEAIN, F_BYTEAOUT, NULL, NULL },
	{ 18, 1, 0, F_CHARIN, F_CHAROUT, F_CHAREQ, NULL },
	{ 19, 16, 1, F_CHAR16IN, F_CHAR16OUT, F_CHAR16EQ, NULL },
	{ 20, 4, 0, F_DATETIMEIN, F_DATETIMEOUT, NULL, NULL },
	{ 21, 2, 0, F_INT2IN, F_INT2OUT, F_INT2EQ, F_INT2LT },
	{ 22, 16, 1, F_INT28IN, F_INT28OUT, NULL, NULL },
	{ 23, 4, 0, F_INT4IN, F_INT4OUT, F_INT4EQ, F_INT4LT },
	{ 24, 4, 0, F_REGPROCIN, F_REGPROCOUT, NULL, NULL },
	{ 25, -1, 1, F_TEXTIN, F_TEXTOUT, F_TEXTEQ, NULL },
	{ 26, 4, 0, F_INT4IN, F_INT4OUT, F_INT4EQ, NULL },
	{ 27, 6, 1, F_TIDIN, F_TIDOUT, NULL, NULL },
	{ 28, 5, 1, F_XIDIN, F_XIDOUT, NULL, NULL },
	{ 29, 1, 0, F_CIDIN, F_CIDOUT, NULL, NULL },
	{ 30, 32, 1, F_OID8IN, F_OID8OUT, NULL, NULL }
};

struct	typmap {			/* a hack */
	ObjectId	am_oid;
	struct type	am_typ;
};

static	struct	typmap	**Typ = (struct typmap **)NULL;
static	struct	typmap	*Ap = (struct typmap *)NULL;

/*
static struct context	*RelationOpenContext = NULL;
*/

static	int	Quiet = 0;
static	int	Warnings = 0;
static	int	ShowTime = 0;
#ifdef	EBUG
static	int	ShowLock = 0;
#endif
static	char	Blanks[MAXATTR];
static	char	Buf[MAXPGPATH];
int		Userid;
Relation	reldesc;		/* current relation descritor */
char		relname[80];		/* current relation name */
struct	attribute *attrtypes[MAXATTR];  /* points to attribute info */
char		*values[MAXATTR];	/* cooresponding attribute values */
int		numattr;		/* number of attributes for cur. rel */
static TimeRange	StandardTimeRange = DefaultTimeRange;
static TimeRangeSpace	StandardTimeRangeSpace;
jmp_buf		Warn_restart;

bool override = false;

Portal	BlankPortal = NULL;



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
	extern	char	Blanks[], Buf[];
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

	/* various initiailization routines */

	signal(SIGHUP, die);
	signal(SIGINT, die);
	signal(SIGTERM, die);

	/* XXX the -C version flag should be removed and combined with -O */
	SetProcessingMode((override) ? BootstrapProcessing : InitProcessing);
	InitPostgres(NULL, dat);
	BlankPortal = CreatePortal("<blank>");
/*
	(void)MemoryContextSwitchTo(
		(MemoryContext)PortalGetHeapMemory(BlankPortal));
*/
	i = MAXATTR;
	dat = Blanks;
	while (i--) {
		attrtypes[i-1]=(struct attribute *)NULL;
		*dat++ = ' ';
	}
	for(i=0;i<STRTABLESIZE;i++)
		strtable[i]=NULL;                    
	for(i=0;i<HASHTABLESIZE;i++)
		hashtable[i]=NULL;                   

	signal(SIGHUP, handle_warn);

	if (setjmp(Warn_restart) != 0) {
		Warnings++;
		AbortCurrentTransaction();
/*		reldesc = NULL;		/* relations are freed after aborts */
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
		
	  StartTransactionCommand(BlankPortal);
	  EMITPROMPT;
	  parser_input = gets(parser_input);
	  printf("input string is %s\n",parser_input);
	  parser(parser_input,parser_output);
	  printf("parser outputs :\n");
	  lispDisplay(parser_output,0);
	  printf("\n");
	  
	  if (atom CAR(parser_output))
	    /*utility_invoke(CAR(parser_output),CDR(parser_output));*/
	    ;
	  else {
	      Node plan;
	      extern Node planner();
	      extern void init_planner();

	      init_planner();
	      plan = planner(parser_output);
	      printf("\nPlan is :\n");
	      (*(plan->printFunc))(plan);
	      printf("\n");
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

