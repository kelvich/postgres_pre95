/* ----------------------------------------------------------------
 *   FILE
 *	backendsup.c
 *	
 *   DESCRIPTION
 *	"backend" program support routines.
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <signal.h>

#include "tmp/postgres.h"
#include "tcop/tcopprot.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/tqual.h"
#include "tmp/miscadmin.h"
#include "utils/exc.h"	/* for ExcAbort and <setjmp.h> */
#include "utils/fmgr.h"
#include "utils/mcxt.h"
#include "storage/smgr.h"

#include "catalog/pg_type.h"

#undef BOOTSTRAP
#include "y.tab.h"

/* ----------------
 *	prototypes
 *
 *  	NOTE: see also bkint.h
 * ----------------
 */
#include "support/bkint.h"

extern int setjmp();
extern int chdir();
extern char *getenv();

void SocketBackend ARGS((char *, LispValue));
void InteractiveBackend ARGS((char *, LispValue));
void EvalLine ARGS((char *, LispValue));
bool BootstrapAlreadySeen ARGS(( ObjectId id ));

/* ----------------
 *	global variables
 * ----------------
 */
/*
 * In the lexical analyzer, we need to get the reference number quickly from
 * the string, and the string from the reference number.  Thus we have
 * as our data structure a hash table, where the hashing key taken from
 * the particular string.  The hash table is chained.  One of the fields
 * of the hash table node is an index into the array of character pointers.
 * The unique index number that every string is assigned is simply the
 * position of its string pointer in the array of string pointers.
 */

char pg_pathname[256];

macro		*strtable [STRTABLESIZE];
hashnode	*hashtable [HASHTABLESIZE];

FILE		*source = NULL;
static int	strtable_end = -1;    /* Tells us last occupied string space */
static int	num_of_errs = 0;      /* Total number of errors encountered */

static char *   (typestr[16]) = {
	"bool", "bytea", "char", "char16", "dt", "int2", "int28",
	"int4", "regproc", "text", "oid", "tid", "xid", "iid",
	"oid8", "smgr"
};

/* functions associated with each type */
static	long	Procid[16][7] = {
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
	{ 30, 32, 1, F_OID8IN, F_OID8OUT, NULL, NULL },
	{210, 2, 0, F_SMGRIN, F_SMGROUT, F_SMGREQ, NULL }
};

struct	typmap {			/* a hack */
	ObjectId	am_oid;
	struct type	am_typ;
};

#ifdef	EBUG
static	int	ShowLock = 0;
#endif

static	struct	typmap	**Typ = (struct typmap **)NULL;
static	struct	typmap	*Ap = (struct typmap *)NULL;

extern int	Quiet;
static	int		Warnings = 0;
static	int		ShowTime = 0;
static	char		Blanks[MAXATTR];
static	char		Buf[MAXPGPATH];
static TimeQual		StandardTimeQual = NowTimeQual;
static TimeQualSpace	StandardTimeQualSpace;

int		Userid;
Relation	reldesc;		/* current relation descritor */
char		relname[80];		/* current relation name */
struct	attribute *attrtypes[MAXATTR];  /* points to attribute info */
char		*values[MAXATTR];	/* cooresponding attribute values */
int		numattr;		/* number of attributes for cur. rel */
jmp_buf		Warn_restart;
int		DebugMode;
static int UseBackendParseY = 0;

extern bool override;

extern	int	optind;
extern	char	*optarg;


/* ----------------------------------------------------------------
 *			misc functions
 * ----------------------------------------------------------------
 */
/* ----------------
 *	hack so linker links 
 * ----------------
 */

/* ----------------
 *	error handling / abort routines
 * ----------------
 */
void
err()
{
    cleanup();
    exitpg(1);
}


/* ----------------------------------------------------------------
 *				BootstrapMain
 * ----------------------------------------------------------------
 */
void
BootstrapMain(ac, av)
int  ac;
char *av[];
{
    int	  i;
    int	  flagC, flagQ;
    int	  portFd = -1;
    char  *dat;
    char  pathbuf[128], *getwd();
    extern char *DBName;

    /* ----------------
     *	initialize signal handlers
     * ----------------
     */
    signal(SIGHUP, die);
    signal(SIGINT, die);
    signal(SIGTERM, die);

    /* ----------------
     *	process command arguments
     * ----------------
     */
	Quiet = 0;
    flagC = 0;
    flagQ = 0;
    dat = NULL;

    for (i = 2; i < ac; ++i) {
	char *ptr = av[i];
	if (*ptr != '-') {
	    if (dat != NULL) {
		fputs("DB Name specified twice!\n", stderr);
		goto usage;
	    }
	    DBName = dat = ptr;
	    continue;
	}

	while (*++ptr) {
	    switch(*ptr) {
	    case 'd':	/* -debug mode */
		DebugMode = 1;
		ptr = "\0";
		break;
	    case 'a':	/* -ami	*/
		UseBackendParseY = 1;
		ptr = "\0";	/* \0\0 */
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
		ptr = "\0";	/* \0\0 */
		break;
	    case 'P':
		portFd = atoi(ptr + 1);
		ptr = "\0";
		break;
	    default:
		fprintf(stderr, "Option %c is illegal\n", *ptr);
		goto usage;
	    }
	}
    }
    if (dat == NULL)
	DBName = dat = getenv("USER");
    if (dat == NULL) {
	fputs("backend: failed, no db name specified\n", stderr);
	fputs("         and no USER enviroment variable\n", stderr);
	exitpg(1);
    }

    Noversion = flagC;
    Quiet = flagQ;

    /* ----------------
     *	initialize input fd
     * ----------------
     */
    if (IsUnderPostmaster == true && portFd < 0) {
	fputs("backend: failed, no -P option with -postmaster opt.\n", stderr);
	exitpg(1);
    }
    if (portFd >= 0)
	SetPQSocket(portFd);

    /* ----------------
     *	backend initialization
     * ----------------
     */
    /* XXX the -C version flag should be removed and combined with -O */
    GetDataHome();
    SetProcessingMode((override) ? BootstrapProcessing : InitProcessing);
    InitPostgres((String)dat);
    LockDisable(true);
    dat = Blanks;
    
    for (i = MAXATTR - 1; i >= 0; --i) {
	attrtypes[i]=(struct attribute *)NULL;
	*dat++ = ' ';
    }
    for(i = 0; i < STRTABLESIZE; ++i)
	strtable[i] = NULL;                    
    for(i = 0; i < HASHTABLESIZE; ++i)
	hashtable[i] = NULL;                   

    /* ----------------
     *	abort processing resumes here
     * ----------------
     */
    signal(SIGHUP, handle_warn);
    if (setjmp(Warn_restart) != 0) {
	Warnings++;
	AbortCurrentTransaction();
    }

    /* ----------------
     *	process input.
     * ----------------
     */
    for (;;) {
	Int_yyparse();
    }

    /* ----------------
     *	usage
     * ----------------
     */
usage:
    fputs("Usage: backend [-C] [-Q] [datname]\n", stderr);
    exitpg(1);
}

/* ----------------------------------------------------------------
 *		MANUAL BACKEND INTERACTIVE INTERFACE COMMANDS
 * ----------------------------------------------------------------
 */

/* ----------------
 *  	createrel
 *
 *	takes the name of the relation to create, creates the file
 *	and sticks the relation into the syscat.  if the relation
 *	already exists in the syscat, then nothing is created
 *	
 *	Note : createrel creates, but does not open the named relation
 * ----------------
 */
void
createrel(name)
    char *name;
{
    ObjectId toid;

    if (strlen(name) > 79) 
	name[79] ='\000';
    
    bcopy(name,relname,strlen(name)+1);

    printf("Amcreatr: relation %s.\n", relname);

    if ((numattr == 0) || (attrtypes == NULL)) {
	elog(WARN,"Warning: must define attributes before creating rel.\n");
	elog(WARN,"         relation not created.\n");
    } else {
	reldesc = heap_creatr(relname, numattr, DEFAULT_SMGR, attrtypes);
	if (reldesc == (Relation)NULL) {
	    elog(WARN,"Warning: cannot create relation %s.\n", relname);
	    elog(WARN,"         Probably should delete old %s.\n", relname);
	}
	toid = TypeDefine(relname, reldesc->rd_id, -1, -1, 'c', ',',
			  "textin", "textout", "textin", "textout",
			  NULL, "-", (Boolean) 0);

    }
}

/* ----------------
 *	boot_openrel
 * ----------------
 */
void
boot_openrel(name)
    char *name;
{
    int		i;
    struct	typmap	**app;
    Relation	rdesc;
    HeapScanDesc	sdesc;
    HeapTuple	tup;
    extern	char	TYPE_R[];
	
    if(strlen(name) > 79) 
	name[79] ='\000';
    bcopy(name, relname, strlen(name)+1);
	
    if (Typ == (struct typmap **)NULL) {
	StartPortalAllocMode(DefaultAllocMode, 0);
	rdesc = heap_openr(TYPE_R);
	sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 0, (ScanKey)NULL);
      for (i=0; PointerIsValid(tup=heap_getnext(sdesc,0,(Buffer *)NULL)); ++i);
	heap_endscan(sdesc);
	app = Typ = ALLOC(struct typmap *, i + 1);
	while (i-- > 0)
	    *app++ = ALLOC(struct typmap, 1);
	*app = (struct typmap *)NULL;
	sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 0, (ScanKey)NULL);
	app = Typ;
	while (PointerIsValid(tup = heap_getnext(sdesc, 0, (Buffer *)NULL))) {
	    (*app)->am_oid = tup->t_oid;
	    bcopy(
		(char *)GETSTRUCT(tup), 
		(char *)&(*app++)->am_typ, 
		sizeof ((*app)->am_typ)
	    );
	}
	heap_endscan(sdesc);
	heap_close(rdesc);
	EndPortalAllocMode();
    }
	
    if (reldesc != NULL) {
	closerel(NULL);
    }

    if (!Quiet)
    	printf("Amopen: relation %s. attrsize %d\n", relname, 
		sizeof(struct attribute));
    
    reldesc = heap_openr(relname);
    Assert(reldesc);
    numattr = reldesc->rd_rel->relnatts;
    for (i = 0; i < numattr; i++) {
	if (attrtypes[i] == NULL) {
	    attrtypes[i] = AllocateAttribute();
	}
	bcopy(
	    (char *)reldesc->rd_att.data[i], 
	    (char *)attrtypes[i],
	    sizeof (*attrtypes[0])
	);
	if (DebugMode) {
	    struct attribute *at = attrtypes[i];
	    printf("create attribute %d name %s len %d num %d type %d\n",
		i, at->attname, at->attlen, at->attnum, at->atttypid
	    );
	    fflush(stdout);
	}
    }
}

/* ----------------
 *	closerel
 * ----------------
 */
void
closerel(name)
    char *name;
{
    /* ### insert errorchecking to make sure closing same relation we
     *  have already open
     */

    if (reldesc == NULL) {
	elog(WARN,"Warning: no opened relation to close.\n");
    } else {
	if (!Quiet) printf("Amclose: relation %s.\n", relname);
	heap_close(reldesc);
	reldesc = (Relation)NULL;
    }
}

/* ----------------
 *	printrel
 * ----------------
 */
void
printrel()
{
    HeapTuple		tuple;
    HeapScanDesc	scandesc;
    int			i;
    Buffer		b;

    if (Quiet)
	return;
    if (reldesc == NULL) {
	elog(WARN,"Warning: need to open relation to print.\n");
	return;
    }
    /* print the name of the attributes in the relation */
    printf("Relation %s:  ", relname);
    printf("[ ");
    for (i=0; i < reldesc->rd_rel->relnatts; i++)
	printf("%s ", &reldesc->rd_att.data[i]->attname);
    printf("]\n\n");
    StartPortalAllocMode(DefaultAllocMode, 0);
    
    /* print the tuples in the relation */
    scandesc = heap_beginscan(reldesc, 
			      0,
			      NowTimeQual,
			      (unsigned) 0,
			      (ScanKey) NULL);
    while ((tuple = heap_getnext(scandesc, 0, &b)) != NULL) {
	showtup(tuple, b, reldesc);
    }
    heap_endscan(scandesc);
    EndPortalAllocMode();
    printf("\nEnd of relation\n");
}


#ifdef	EBUG
/* ----------------
 *	randomprintrel
 * ----------------
 */
void
randomprintrel()
{
    HeapTuple		tuple;
    HeapScanDesc	scandesc;
    Buffer		buffer;
    int			i, j, numattr, typeindex;
    int			count;
    int			mark = 0;
    static bool		isInitialized = false;
    extern		srandom();
    long		random();
    long		time();

    StartPortalAllocMode(DefaultAllocMode, 0);
    if (reldesc == NULL) {
	elog(WARN,"Warning: need to open relation to (r) print.\n");
    } else {
	/* print the name of the attributes in the relation */
	printf("Relation %s:  ", relname);
	printf("[ ");
	for (i=0; i<reldesc->rd_rel->relnatts; i++) {
	    printf("%s ", &reldesc->rd_att.data[i]->attname);
	}
	printf(
		"]\nWill display %d tuples in a slightly random order\n\n",
		count = 64
	);
	
	/* print the tuples in the relation */
	if (! isInitialized) {
	    srandom((int)time(0));
	    isInitialized = true;
	}
	scandesc = heap_beginscan(reldesc, 
			       random()&1, 
			       NowTimeQual,
			       0, 
			       (ScanKey) NULL);
	
	numattr = reldesc->rd_rel->relnatts;
	while (count-- != 0) {
	    if (!(random() & 0xf)) {
		printf("\tRESTARTING SCAN\n");
		heap_rescan(scandesc, random()&01, (ScanKey)NULL);
		mark = 0;
	    }
	    if (!(random() & 0x3)) {
		if (mark) {
		    printf("\tRESTORING MARK\n");
		    heap_restrpos(scandesc);
		    mark &= random();
		} else {
		    printf("\tSET MARK\n");
		    heap_markpos(scandesc);
		    mark = 0x1;
		}
	    }
	    if (!PointerIsValid(tuple =
			heap_getnext(scandesc, !(random() & 0x1), &buffer))) {
		puts("*NULL*");
		continue;
	    }
	    showtup(tuple, buffer, reldesc);
	}
	puts("\nDone");
	heap_endscan(scandesc);
    }
    EndPortalAllocMode();
}
#endif


/* ----------------
 *	showtup
 * ----------------
 */
void
showtup(tuple, buffer, relation)
    HeapTuple	tuple;
    Buffer	buffer;
    Relation	relation;
{
    char	*value, *fmgr();
    Boolean	isnull;
    struct	typmap	**app;
    int		i, typeindex;

    value = (char *)
	heap_getattr(tuple, buffer, ObjectIdAttributeNumber,
		  (struct attribute **)NULL, &isnull);
    
    if (isnull) {
	printf("*NULL* < ");
    } else {
	printf("%ld < ", (long)value);
    }
    
    for (i = 0; i < numattr; i++) {
	value = (char *)
	    heap_getattr(tuple, buffer, i + 1,
		      &reldesc->rd_att.data[0], &isnull);
	if (isnull) {
	    printf("<NULL> ");
	} else if (Typ != (struct typmap **)NULL) {
	    app = Typ;
	    while (*app != NULL && (*app)->am_oid !=
		reldesc->rd_att.data[i]->atttypid) {
		app++;
	    }
	    if (*app == NULL) {
		printf("Unable to find atttypid in Typ list! %d\n",
		    reldesc->rd_att.data[i]->atttypid
		);
		Assert(0);
	    }
	    printf("%s ", value = fmgr((*app)->am_typ.typoutput, value));
	    pfree(value);
	} else {
	    typeindex = reldesc->rd_att.data[i]->atttypid - FIRST_TYPE_OID;
	    printf("%s ", value = fmgr(Procid[typeindex][(int) Q_OUT], value));
	    pfree(value);
	}
    }
    if (!Quiet) printf(">\n");
    if (ShowTime) {
 	printf("\t[");
	value = heap_getattr(tuple, buffer, MinAbsoluteTimeAttributeNumber,
			  &reldesc->rd_att.data[0], &isnull);
	if (isnull) {
	    printf("{*NULL*} ");
	} else if (TimeIsValid((Time)value)) {
	    showtime((Time)value);
	}

	value = heap_getattr(tuple, buffer, MinCommandIdAttributeNumber,
			  &reldesc->rd_att.data[0], &isnull);
	if (isnull) {
	    printf("(*NULL*,");
	} else {
	    printf("(%u,", (CommandId)value);
	}
	
	value = heap_getattr(tuple, buffer, MinTransactionIdAttributeNumber,
			  &reldesc->rd_att.data[0], &isnull);
	
	if (isnull) {
	    printf("*NULL*),");
	} else if (!TransactionIdIsValid((TransactionId)value)) {
	    printf("-),");
	} else {
	    printf("%s)", TransactionIdFormString((TransactionId)value));
	    if (!TransactionIdDidCommit((TransactionId)value)) {
		if (TransactionIdDidAbort((TransactionId)value)) {
		    printf("*A*");
		} else {
		    printf("*InP*");
		}
	    }
	    value = (char *)TransactionIdGetCommitTime((TransactionId)value);
	    if (!TimeIsValid((Time)value)) {
		printf("{-},");
	    } else {
		showtime((Time)value);
		printf(",");
	    }
	}
	
	value = heap_getattr(tuple, buffer, MaxAbsoluteTimeAttributeNumber,
			  &reldesc->rd_att.data[0], &isnull);
	
	if (isnull) {
	    printf("{*NULL*} ");
	} else if (TimeIsValid((Time)value)) {
	    showtime((Time)value);
	}

	value = heap_getattr(tuple, buffer, MaxCommandIdAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
	
	if (isnull) {
	    printf("(*NULL*,");
	} else {
	    printf("(%u,", (CommandId)value);
	}

	value = heap_getattr(tuple, buffer, MaxTransactionIdAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
	
	if (isnull) {
	    printf("*NULL*)]\n");
	} else if (!TransactionIdIsValid((TransactionId)value)) {
	    printf("-)]\n");
	} else {
	    printf("%s)", TransactionIdFormString((TransactionId)value));
	    value = (char *)TransactionIdGetCommitTime((TransactionId)value);
	    if (!TimeIsValid((Time)value)) {
		printf("{-}]\n");
	    } else {
		showtime((Time)value);
		printf("]\n");
	    }
	}
    }
}

/* ----------------
 *	showtime
 * ----------------
 */
void
showtime(time)
    Time time;
{
    Assert(TimeIsValid(time));
    printf("{%d=%s}", time, nabstimeout(time));
}


/* ----------------
 * DEFINEATTR()
 *
 * define a <field,type> pair
 * if there are n fields in a relation to be created, this routine
 * will be called n times
 * ----------------
 */
void
DefineAttr(name, type, attnum)
    char *name, *type;
    int  attnum;
{
    int     attlen;
    TYPE    t;

    if (reldesc != NULL) {
	fputs("Warning: no open relations allowed with 't' command.\n",stderr);
	closerel(relname);
    }

    t = (TYPE)gettype(type);
    if (attrtypes[attnum] == (struct attribute *)NULL) 
	attrtypes[attnum] = AllocateAttribute();
    if (Typ != (struct typmap **)NULL) {
	attrtypes[attnum]->atttypid = Ap->am_oid;
	strncpy(attrtypes[attnum]->attname, name, 16);
	if (!Quiet) printf("<%s %s> ", attrtypes[attnum]->attname, type);
	attrtypes[attnum]->attnum = 1 + attnum; /* fillatt */
	attlen = attrtypes[attnum]->attlen = Ap->am_typ.typlen;
	attrtypes[attnum]->attbyval = Ap->am_typ.typbyval;
    } else {
	attrtypes[attnum]->atttypid = Procid[(int)t][(int)Q_OID];
	strncpy(attrtypes[attnum]->attname,name, 16);
	if (!Quiet) printf("<%s %s> ", attrtypes[attnum]->attname, type);
	attrtypes[attnum]->attnum = 1 + attnum; /* fillatt */
	attlen = attrtypes[attnum]->attlen = Procid[(int)t][(int)Q_LEN];
	attrtypes[attnum]->attbyval = (attlen==1) || (attlen==2)||(attlen==4);
    }
}


/* ----------------
 *	InsertOneTuple
 *	assumes that 'oid' will not be zero.
 * ----------------
 */
void
InsertOneTuple(objectid)
    ObjectId  objectid;
{
    HeapTuple tuple;
    int i;

    if (DebugMode) {
	printf("InsertOneTuple oid %d, %d attrs\n", objectid, numattr);
	fflush(stdout);
    }

    tuple = heap_formtuple(numattr,attrtypes,values,Blanks);
    if(objectid !=(OID)0) {
	tuple->t_oid=objectid;
    }
    RelationInsertHeapTuple(reldesc,(HeapTuple)tuple,(double *)NULL);
    pfree(tuple);
    if (DebugMode) {
	printf("End InsertOneTuple\n", objectid);
	fflush(stdout);
    }
    /*
     * Reset blanks for next tuple
     */
    for (i = 0; i<numattr; i++)
	Blanks[i] = ' ';
}

/* ----------------
 *	InsertOneValue
 * ----------------
 */
void
InsertOneValue(objectid, value, i)
    ObjectId  objectid;
    int	      i;
    char     *value;
{
    int		typeindex;
    char	*prt;
    struct typmap **app;
    HeapTuple	tuple;
    char	*fmgr();

    if (DebugMode)
	printf("Inserting value: '%s'\n", value);
    if (i < 0 || i >= MAXATTR) {
	printf("i out of range: %d\n", i);
	Assert(0);
    }

    if (Typ != (struct typmap **)NULL) {
	struct typmap *ap;
	if (DebugMode)
	    puts("Typ != NULL");
	app = Typ;
	while (*app && (*app)->am_oid != reldesc->rd_att.data[i]->atttypid)
	   ++app;
	ap = *app;
	if (ap == NULL) {
	    printf("Unable to find atttypid in Typ list! %d\n",
	        reldesc->rd_att.data[i]->atttypid
	    );
	    Assert(0);
	}
	if (!Quiet) printf("FMGR-IN/OUT %d, %d\n", ap->am_typ.typinput, 
	    ap->am_typ.typoutput);
	values[i] = fmgr(ap->am_typ.typinput, value);
	prt = fmgr(ap->am_typ.typoutput, values[i]);
	if (!Quiet) printf("%s ", prt);
	pfree(prt);
    } else {
	typeindex = attrtypes[i]->atttypid - FIRST_TYPE_OID;
	if (DebugMode)
	    printf("Typ == NULL, typeindex = %d idx = %d\n", typeindex, i);
	values[i] = fmgr(Procid[typeindex][(int)Q_IN], value);
	prt = fmgr(Procid[typeindex][(int) Q_OUT], values[i]);
	if (!Quiet) printf("%s ", prt);
	pfree(prt);
    }
    if (DebugMode) {
	puts("End InsertValue");
	fflush(stdout);
    }
}

/* ----------------
 *	InsertOneNull
 * ----------------
 */
void
InsertOneNull(i)
    int	      i;
{
    if (DebugMode)
	printf("Inserting null\n");
    if (i < 0 || i >= MAXATTR) {
	elog(FATAL, "i out of range (too many attrs): %d\n", i);
    }
    values[i] = (char *)NULL;
    Blanks[i] = 'n';
}

/* ----------------
 *	defineindex
 *
 *	This defines an index on the oid attribute of the specified
 *	heap relation.
 * ----------------
 */
void
defineindex(heapName, indexName, accessMethodName, attributeList)
    char  *heapName;
    char  *indexName;
    char  *accessMethodName;
    List  attributeList;
{
    Relation    cur_relation;

    DefineIndex(heapName,
		indexName,
		accessMethodName,
		attributeList,
		LispNil,
		LispNil);
    /*
     * All of the rest of this routine is needed only because in bootstrap
     * processing we don't increment xact id's.  The normal DefineIndex
     * code replaces a pg_class tuple with updated info including the
     * relhasindex flag (which we need to have updated).  Unfortunately, 
     * there are always two indices defined on each catalog causing us to 
     * update the same pg_class tuple twice for each catalog getting an 
     * index during bootstrap resulting in the ghost tuple problem (see 
     * heap_replace).  To get around this we change the relhasindex 
     * field ourselves in this routine keeping track of what catalogs we 
     * already changed so that we don't modify those tuples twice.  The 
     * normal mechanism for updating pg_class is disabled during bootstrap.
     *
     *		-mer 
     */
    cur_relation = heap_openr(heapName);
    if (! RelationIsValid(cur_relation))
            elog(WARN, "defineindex: could not open %s relation", heapName);

    if (!BootstrapAlreadySeen(cur_relation->rd_id))
	UpdateStats(cur_relation, 0);
}

#define MORE_THAN_THE_NUMBER_OF_CATALOGS 256

bool
BootstrapAlreadySeen(id)
    ObjectId id;
{
    static ObjectId seenArray[MORE_THAN_THE_NUMBER_OF_CATALOGS];
    static int nseen = 0;
    bool seenthis;
    int i;

    seenthis = false;

    for (i=0; i < nseen; i++)
    {
	if (seenArray[i] == id)
	{
	    seenthis = true;
	    break;
	}
    }
    if (!seenthis)
    {
	seenArray[nseen] = id;
	nseen++;
    }
    return (seenthis);
}


/* ----------------
 *	handletime
 * ----------------
 */
void
handletime()
{
    int     numberOfTimes;
    char    firstTime[80];
    char    secondTime[80];
    Time    time1;
    Time    time2;

    if (scanf("%d", &numberOfTimes) != 1) {
	fputs("Error: failed to read number of time fields\n", stderr);
	err();
    }
    if (numberOfTimes < -1 || numberOfTimes > 2) {
	elog(
	    WARN, 
	    "number of time fields (%d) is not -1, 0, 1, or 2\n",
	    numberOfTimes
	);
    } else if (numberOfTimes == -1) {
	ShowTime = !ShowTime;
	return;
    } else if (numberOfTimes == 0) {
	StandardTimeQual = NowTimeQual;
	puts("Time range reset to \"now\"");
    } else if (numberOfTimes == 1) {
	if (scanf("%[^\n]", firstTime) != 1) {
	    fputs("Error: failed to read time\n", stderr);
	    err();
	}
	time1 = nabstimein(firstTime);
	if (!Quiet) printf("*** You entered \"%s\"", firstTime);
	if (time1 == INVALID_ABSTIME) {
	    if (!Quiet) 
			printf(" which is INVALID (time range unchanged).\n");
	} else {
	    if (!Quiet) 
	    	printf(" with representation %d.\n", time1);
	    bcopy(
		(char *)TimeFormSnapshotTimeQual(time1),
		(char *)&StandardTimeQualSpace,
		sizeof StandardTimeQualSpace
	    );
	    StandardTimeQual = (TimeQual)&StandardTimeQualSpace;
	}
    } else {	/* numberOfTimes == 2 */
	if (scanf("%[^,],%[^\n]", firstTime, secondTime) != 2) {
	    fputs("Error: failed to read both times\n", stderr);
	    err();
	}
	if (!Quiet) 
		printf("*** You entered \"%s\" and \"%s\"\n", firstTime, secondTime);
	time1 = nabstimein(firstTime);
	time2 = nabstimein(secondTime);
	if (time1 == INVALID_ABSTIME) {
	    time1 = InvalidTime;
	    if (time2 == INVALID_ABSTIME) {
		time2 = InvalidTime;
		if (!Quiet) printf("*** range is [,].\n");
	    } else {
		if (!Quiet) printf("*** range is [,%d].\n", time2);
	    }
	} else {
	    if (time2 == INVALID_ABSTIME) {
		time2 = InvalidTime;
		if (!Quiet) printf("*** range is [%d,].\n", time1);
	    } else {
		if (!Quiet) printf("*** range is [%d,%d].\n", time1, time2);
	    }
	}
	bcopy(
	    (char *)TimeFormRangedTimeQual(time1, time2),
	    (char *)&StandardTimeQualSpace,
	    sizeof StandardTimeQualSpace
	);
	StandardTimeQual = (TimeQual)&StandardTimeQualSpace;
    }
}

/* ----------------
 *	cleanup
 * ----------------
 */
void
cleanup()
{
    static	int	beenhere = 0;
     
    if (!beenhere)
	beenhere = 1;
    else {
	elog(FATAL,"Memory manager fault: cleanup called twice.\n", stderr);
	exitpg(1);
    }
    if (reldesc != (Relation)NULL) {
	heap_close(reldesc);
    }
    CommitTransactionCommand();
    exitpg(Warnings);
}

/* ----------------
 *	gettype
 * ----------------
 */
int
gettype(type)
    char *type;
{
    int		i;
    Relation	rdesc;
    HeapScanDesc	sdesc;
    HeapTuple	tup;
    struct	typmap	**app;
    extern	char	TYPE_R[];

    if (Typ != (struct typmap **)NULL) {
	for (app = Typ; *app != (struct typmap *)NULL; app++) {
	    if (strcmp((*app)->am_typ.typname, type) == 0) {
		Ap = *app;
		return((*app)->am_oid);
	    }
	}
    } else {
	for (i = 0; i <= (int)TY_LAST; i++) {
	    if (strcmp(type, typestr[i]) == 0) {
		return(i);
	    }
	}
	if (DebugMode)
	    printf("backendsup.c: External Type: %s\n", type);
        rdesc = heap_openr(TYPE_R);
        sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 0, (ScanKey)NULL);
	i = 0;
	while (PointerIsValid(tup = heap_getnext(sdesc, 0, (Buffer *)NULL)))
	    ++i;
	heap_endscan(sdesc);
	app = Typ = ALLOC(struct typmap *, i + 1);
	while (i-- > 0)
	    *app++ = ALLOC(struct typmap, 1);
	*app = (struct typmap *)NULL;
	sdesc = heap_beginscan(rdesc, 0, NowTimeQual, 0, (ScanKey)NULL);
	app = Typ;
	while (PointerIsValid(tup = heap_getnext(sdesc, 0, (Buffer *)NULL))) {
	    (*app)->am_oid = tup->t_oid;
	    bcopy(
		(char *)GETSTRUCT(tup), 
		(char *)&(*app++)->am_typ,
	        sizeof ((*app)->am_typ)
	    );
        }
        heap_endscan(sdesc);
        heap_close(rdesc);
        return(gettype(type));
    }
    elog(WARN, "Error: unknown type '%s'.\n", type);
    err();
    /* NOTREACHED */
}

/* ----------------
 *	AllocateAttribute
 * ----------------
 */
struct attribute * /* XXX */
AllocateAttribute()
{
    struct attribute	*attribute = new(struct attribute); /* XXX */

    if (!PointerIsValid(attribute)) {
	elog(FATAL, "AllocateAttribute: malloc failed");
    }
    bzero((Pointer)attribute, sizeof *attribute);

    return (attribute);
}

/* ----------------
 *	MapEscape
 * ----------------
 */
#define isoctal(c) ((c)>='0' && (c) <= '7')

unsigned char 
MapEscape(s)
    char **s;
{
    int i;
    char octal[4];
    switch(*(*s)++) {
    case '\\':
	return '\\';
    case 'b':
	return '\b';
    case 'f':
 	return '\f';
    case 'r':
	return '\r';
    case 't':
	return '\t';
    case '"':
	return '"';
    case '\'':
	return '\'';
    case '0': 
    case '1': 
    case '2': 
    case '3': 
    case '4': 
    case '5':
    case '6': 
    case '7':
	for (--*s,i=0;isoctal((*s)[i]) && (i<3); ++i)
	    octal[i]=(*s)[i];
	*s += i;
	octal[i] = '\0';
	if ((i = strtol(octal,0,8)) <= 0377) 
	    return ((char)i);
    default:
	break;
    }
    elog(WARN,"Bad Escape sequence");
    return *((*s)++); /* ignore */
}

/* ----------------
 *	EnterString
 *	returns the string table position of the identifier
 *	passed to it.  We add it to the table if we can't find it.
 * ----------------
 */
int 
EnterString (str)
    char	*str;
{
    hashnode	*node;
    int        len;
	
    len= strlen(str);
	
    if ( (node = FindStr(str, len, 0)) != NULL)
	return (node->strnum);
    else {
	node = AddStr(str, len, 0);
	return (node->strnum);
    }
}

/* ----------------
 *	LexIDStr
 *	when given an idnum into the 'string-table' return the string
 *	associated with the idnum
 * ----------------
 */
char *
LexIDStr(ident_num) 
    int ident_num;
{
    return(strtable[ident_num]->s);
}    


/* ----------------
 *	CompHash
 *
 * 	Compute a hash function for a given string.  We look at the first,
 * 	the last, and the middle character of a string to try to get spread
 * 	the strings out.  The function is rather arbitrary, except that we
 * 	are mod'ing by a prime number.
 * ----------------
 */
int 
CompHash (str, len)
    char *str;
    int	len;
{
    register int result;
	
    result =(NUM * str[0] + NUMSQR * str[len-1] + NUMCUBE * str[(len-1)/2]);

    return (result % HASHTABLESIZE);

}

/* ----------------
 *	FindStr
 *
 * 	This routine looks for the specified string in the hash
 * 	table.  It returns a pointer to the hash node found,
 * 	or NULL if the string is not in the table.
 * ----------------
 */
hashnode *
FindStr (str, length, mderef)
    char	*str;
    int		length;
    hashnode   *mderef;
{
    hashnode	*node;
    node = hashtable [CompHash (str, length)];
    while (node != NULL) {
	/*
	 * We must differentiate between string constants that
	 * might have the same value as a identifier
	 * and the identifier itself.
	 */
	if (!strcmp(str, strtable[node->strnum]->s)) {
	    return(node);  /* no need to check */
	} else {
	    node = node->next;
	}
    }
    /* Couldn't find it in the list */
    return (NULL);
}

/* ----------------
 *	AddStr
 *
 * 	This function adds the specified string, along with its associated
 * 	data, to the hash table and the string table.  We return the node
 * 	so that the calling routine can find out the unique id that AddStr
 * 	has assigned to this string.
 * ----------------
 */
hashnode *
AddStr(str, strlength, mderef)
    char *str;
    int	 strlength;
    int  mderef;
{
    hashnode	*temp, *trail, *newnode;
    int		hashresult;
    char	*stroverflowmesg =
"There are too many string constants and identifiers for\
 the compiler to handle.";

    if (++strtable_end == STRTABLESIZE) {
	/* Error, string table overflow, so we Punt */
	elog(FATAL, stroverflowmesg);
    }

    strtable[strtable_end] = new(macro);
    strtable [strtable_end]->s = malloc((unsigned) strlength + 1);
    strtable[strtable_end]->mderef = 0;
    strcpy (strtable[strtable_end]->s, str);

    /* Now put a node in the hash table */

    newnode = new(hashnode);
    newnode->strnum = strtable_end;
    newnode->next = NULL;

    /* Find out where it goes */

    hashresult = CompHash (str, strlength);
    if (hashtable [hashresult] == NULL) {
	hashtable [hashresult] = newnode;
    } else {			/* There is something in the list */
	trail = hashtable [hashresult];
	temp = trail->next;
	while (temp != NULL) {
	    trail = temp;
	    temp = temp->next;
	}
	trail->next = newnode;
    }
    return (newnode);
}

/* ----------------
 *	printhashtable
 *
 * 	This routine dumps out the hash table for inspection.
 * ----------------
 */
void
printhashtable()
{
    register int i;
    hashnode *hashptr;

    if (Quiet) return;

    fprintf (stderr, "\n\nHash Table:\n");
    for (i = 0; i < HASHTABLESIZE; i++) {
        if (hashtable[i] != NULL) {
	    fprintf (stderr, "Position: %d\n", i);
	    hashptr = hashtable[i];
	    do {
	        fprintf (stderr, "\tString #%d", hashptr->strnum);
		hashptr = hashptr->next;
	    } while (hashptr != NULL);
	}
    }
}

/* ----------------
 *	printstrtable
 *
 *	This routine dumps out the string table for inspection.
 * ----------------
 */
void
printstrtable ()
{
    register int i;

    if (Quiet) return;

    fprintf (stderr, "\nString Table:\n");
    for (i = 0; i <= strtable_end; i++) {
	fprintf (stderr, "#%d: \"%s\";\t   ", i, strtable[i]->s);
	if (i % 3 == 0 && i > 0)
	    fprintf (stderr, "\n");
    }
    fprintf (stderr, "\n");
}

/* ----------------
 *	emalloc
 *
 *	Error checked version of malloc.
 * ----------------
 */
char *
emalloc (size)
    unsigned size;
{
    char *p;

    if ((p = malloc (size)) == NULL)
	elog (FATAL,"Memory Allocation Error\n");
    return (p);
}

/* ----------------
 *	LookUpMacro()
 *
 *  	takes an index into the string table and converts it into the
 *  	appropriate "string"
 * ----------------
 */
int
LookUpMacro(xmacro)
    char *xmacro;
{
    hashnode *node;
    int macro_indx;

    if (node = FindStr(xmacro,strlen(xmacro),0))
	macro_indx = node->strnum;
  
    if (strtable[macro_indx]->mderef == 0)
	elog(WARN,"Dereferencing non-existent macro");
    
    return(strtable[macro_indx]->mderef);
}

/* ----------------
 *	DefineMacro
 *
 *	takes two identifiers and links them up
 *	by putting a pointer between id1 and id2
 *	(the identifiers pointed to by indx1 and indx2
 * ----------------
 */
void
DefineMacro(indx1, indx2)
    int indx1, indx2;
{
    strtable[indx1]->mderef = indx2;
    if (strtable[indx1]->mderef != 0)
	elog(WARN,"Warning :redefinition of macro\n");
}

/* ----------------
 *	printmacros
 * ----------------
 */
void
printmacros()
{
    register int i;

    fprintf(stdout,"Macro Table:\n");
    for( i = 0;i < strtable_end; ++i) {
	if (strtable[i]->mderef != 0)
	    printf("\"%s\" = \"%s\"\n",strtable[i]->s,
				strtable[strtable[i]->mderef]->s);
    }
}

