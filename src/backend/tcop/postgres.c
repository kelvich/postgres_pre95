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


extern int	die();

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
	int		handle_warn();
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

/**********************************************************************

  createrel
  takes the name of the relation to create, creates the file
  and sticks the relation into the syscat
  if the relation already exists in the syscat, then nothing is created

  Note : createrel creates, but does not open the named relation

 **********************************************************************/

createrel(name)
	char *name;
{
	extern	Relation	amcreatr();

/*
	if (RelationOpenContext == NULL) {
		(void)newcontext();
		initmmgr();
		RelationOpenContext = topcontext();
	}
	(void)switchcontext(RelationOpenContext);
	startmmgr(0);
*/
    
	if(strlen(name) > 79) name[79] ='\000';
	bcopy(name,relname,strlen(name)+1);

	printf("Amcreatr: relation %s.\n", relname);

	if ((numattr == 0) || (attrtypes == NULL)) {
		elog(WARN,"Warning: must define attributes before creating rel.\n");
		elog(WARN,"         relation not created.\n");
	} else {
		reldesc = amcreatr(relname, numattr, attrtypes);
		if (reldesc == (Relation)NULL) {
			elog(WARN,"Warning: cannot create relation %s.\n", relname);
			elog(WARN,"         Probably should delete old %s.\n", relname);
		}
	}
/*
	(void)topcontext();
*/
}

openrel(name)
	char *name;
{
	int		i;
	struct	typmap	**app;
	Relation	rdesc;
	HeapScan	sdesc;
	HeapTuple	tup;
	extern	char	TYPE_R[];
	

/*
	if (RelationOpenContext == NULL) {
		(void)newcontext();
		initmmgr();
		RelationOpenContext = topcontext();
	}
	(void)switchcontext(RelationOpenContext);
*/

	if(strlen(name) > 79) name[79] ='\000';
	bcopy(name,relname,strlen(name)+1);
	
	if (Typ == (struct typmap **)NULL) {
		StartPortalAllocMode(DefaultAllocMode, 0);
		rdesc = amopenr(TYPE_R);
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 0,
			(ScanKey)NULL);
		for (i = 0; PointerIsValid(
				tup = amgetnext(sdesc, 0, (Buffer *)NULL));
				i++)
			;
		amendscan(sdesc);
		app = Typ = ALLOC(struct typmap *, i + 1);
		while (i-- > 0)
			*app++ = ALLOC(struct typmap, 1);
		*app = (struct typmap *)NULL;
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, (ScanKey)NULL);
		app = Typ;
		while (PointerIsValid(tup =
				amgetnext(sdesc, 0, (Buffer *)NULL))) {
			(*app)->am_oid = tup->t_oid;
			bcopy((char *)GETSTRUCT(tup), (char *)&(*app++)->am_typ,
				sizeof (*app)->am_typ);
		}
		amendscan(sdesc);
		amclose(rdesc);
		EndPortalAllocMode();
	}
	
	if (reldesc != NULL) {
/*
		(void)topcontext();
*/
		closerel();
/*
		(void)switchcontext(RelationOpenContext);
*/
	}
/*
	startmmgr(0);
*/
	printf("Amopen: relation %s.\n", relname);
	reldesc = amopenr(relname);
	numattr = reldesc->rd_rel->relnatts;
	for (i = 0; i < numattr; i++) {
		if (attrtypes[i] == NULL) {
			attrtypes[i] = AllocateAttribute();
		}
		bcopy((char *)reldesc->rd_att.data[i], (char *)attrtypes[i],
			(int)sizeof *attrtypes[0]);
	}
/*    
	(void)topcontext();
*/
}

closerel(name)
	char *name;
{
	/* ### insert errorchecking to make sure closing same relation we
	 *  have already open
	 */

	if (reldesc == NULL) {
		elog(WARN,"Warning: no opened relation to close.\n");
	} else {
/*
		(void)switchcontext(RelationOpenContext);
*/
		printf("Amclose: relation %s.\n", relname);
		amclose(reldesc);
		reldesc = (Relation)NULL;
/*
		resetmmgr();
		(void)topcontext();
*/
	}
}

printrel()
{
	HeapTuple	tuple;
	HeapScan	scandesc;
	int		i;
	Buffer		b;
	HeapScan	ambeginscan();
	HeapTuple	amgetnext();

	if(Quiet)
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
	scandesc = ambeginscan(reldesc, 0, StandardTimeRange, (unsigned)0,
		(ScanKey)NULL);
	while ((tuple = amgetnext(scandesc, 0, &b)) != NULL) {
		showtup(tuple, b, reldesc);
	}
	amendscan(scandesc);
	EndPortalAllocMode();
	printf("\nEnd of relation\n");
}


#ifdef	EBUG
randomprintrel()
{
	HeapTuple	tuple;
	HeapScan	scandesc;
	Buffer		buffer;
	int		i, j, numattr, typeindex;
	int		count;
	int		mark = 0;
	static bool	isInitialized = false;
	HeapTuple	amgetnext();
	HeapScan	ambeginscan();
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
		printf("]\nWill display %d tuples in a slightly random order\n\n",
			count = 64);
		/* print the tuples in the relation */
		if (!isInitialized) {
			srandom((int)time(0));
			isInitialized = true;
		}
		scandesc = ambeginscan(reldesc, random()&01, StandardTimeRange,
			0, (ScanKey)NULL);
		numattr = reldesc->rd_rel->relnatts;
		while (count-- != 0) {
			if (!(random() & 0xf)) {
				printf("\tRESTARTING SCAN\n");
				amrescan(scandesc, random()&01, (ScanKey)NULL);
				mark = 0;
			}
			if (!(random() & 0x3))
				if (mark) {
					printf("\tRESTORING MARK\n");
					amrestrpos(scandesc);
					mark &= random();
				} else {
					printf("\tSET MARK\n");
					ammarkpos(scandesc);
					mark = 0x1;
				}
			if (!PointerIsValid(tuple =
					amgetnext(scandesc, !(random() & 0x1),
						&buffer))) {
				puts("*NULL*");
				continue;
			}
			showtup(tuple, buffer, reldesc);
		}
		puts("\nDone");
		amendscan(scandesc);
	}
	EndPortalAllocMode();
}
#endif

showtup(tuple, buffer, relation)
	HeapTuple	tuple;
	Buffer		buffer;
	Relation	relation;
{
	char		*value, *fmgr(), *amgetattr(), *abstimeout();
	Boolean		isnull;
	struct	typmap	**app;
	int		i, typeindex;

	value = (char *)amgetattr(tuple, buffer, ObjectIdAttributeNumber,
		(struct attribute **)NULL, &isnull);
	if (isnull) {
		printf("*NULL* < ");
	} else {
		printf("%ld < ", (long)value);
	}
	for (i = 0; i < numattr; i++) {
		value = (char *)amgetattr(tuple, buffer, 1 + i,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("<NULL> ");
		} else if (Typ != (struct typmap **)NULL) {
			app = Typ;
			while (*app != NULL && (*app)->am_oid !=
					reldesc->rd_att.data[i]->atttypid) {
				app++;
			}
			printf("%s ", value =
				fmgr((*app)->am_typ.typoutput, value));
			pfree(value);
		} else {
			typeindex = reldesc->rd_att.data[i]->atttypid -
				FIRST_TYPE_OID;
			printf("%s ", value =
				fmgr(Procid[typeindex][(int) Q_OUT], value));
			pfree(value);
		}
	}
	printf(">\n");
	if (ShowTime) {
		printf("\t[");
		value = amgetattr(tuple, buffer,
			MinAbsoluteTimeAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("{*NULL*} ");
		} else if (TimeIsValid((Time)value)) {
			showtime((Time)value);
		}

		value = amgetattr(tuple, buffer, MinCommandIdAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("(*NULL*,");
		} else {
			printf("(%u,", (CommandId)value);
		}

		value = amgetattr(tuple, buffer,
			MinTransactionIdAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("*NULL*),");
		} else if (!TransactionIdIsValid((TransactionId)value)) {
			printf("-),");
		} else {
			printf("%s)",
				TransactionIdFormString((TransactionId)value));
			if (!TransactionIdDidCommit((TransactionId)value)) {
				if (TransactionIdDidAbort(
						(TransactionId)value)) {
					printf("*A*");
				} else {
					printf("*InP*");
				}
			}
			value = (char *)TransactionIdGetCommitTime(
				(TransactionId)value);
			if (!TimeIsValid((Time)value)) {
				printf("{-},");
			} else {
				showtime((Time)value);
				printf(",");
			}
		}

		value = amgetattr(tuple, buffer,
			MaxAbsoluteTimeAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("{*NULL*} ");
		} else if (TimeIsValid((Time)value)) {
			showtime((Time)value);
		}

		value = amgetattr(tuple, buffer, MaxCommandIdAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("(*NULL*,");
		} else {
			printf("(%u,", (CommandId)value);
		}

		value = amgetattr(tuple, buffer,
			MaxTransactionIdAttributeNumber,
			&reldesc->rd_att.data[0], &isnull);
		if (isnull) {
			printf("*NULL*)]\n");
		} else if (!TransactionIdIsValid((TransactionId)value)) {
			printf("-)]\n");
		} else {
			printf("%s)",
				TransactionIdFormString((TransactionId)value));
			value = (char *)TransactionIdGetCommitTime(
				(TransactionId)value);
			if (!TimeIsValid((Time)value)) {
				printf("{-}]\n");
			} else {
				showtime((Time)value);
				printf("]\n");
			}
		}
	}
}

showtime(time)
	Time	time;
{
	extern char	*abstimeout();

	Assert(TimeIsValid(time));

	printf("{%d=%s}", time, abstimeout(time));
}


/**********************************************************************

  DefineAttr
  define a <field,type> pair
  if there are n fields in a relation to be created, this routine
  will be called n times

 ***********************************************************************/

DefineAttr(name,type,attnum)
	char *name,*type;
	int  attnum;
{
	int	attlen;
	TYPE	t;

	if (reldesc != NULL) {
		fputs("Warning: no open relations allowed with 't' command.\n",
			stderr);
		closerel(relname);
	}

	t = (TYPE)gettype(type);
	if (attrtypes[attnum] == (struct attribute *)NULL)
		attrtypes[attnum] = AllocateAttribute();
	if (Typ != (struct typmap **)NULL) {
		attrtypes[attnum]->atttypid = Ap->am_oid;
		strncpy(attrtypes[attnum]->attname, name, 16);
		printf("<%s %s> ", attrtypes[attnum]->attname, type);
		attrtypes[attnum]->attnum = 1 + attnum; /* fillatt */
		attlen = attrtypes[attnum]->attlen = Ap->am_typ.typlen;
		attrtypes[attnum]->attbyval = Ap->am_typ.typbyval;
	} else {
		attrtypes[attnum]->atttypid = Procid[(int)t][(int)Q_OID];
		strncpy(attrtypes[attnum]->attname,name, 16);
		printf("<%s %s> ", attrtypes[attnum]->attname, type);
		attrtypes[attnum]->attnum = 1 + attnum; /* fillatt */
		attlen = attrtypes[attnum]->attlen =
			Procid[(int)t][(int)Q_LEN]; /* fillatt */
		attrtypes[attnum]->attbyval = (attlen == 1) ||
			(attlen == 2) || (attlen == 4);
	}
}

/**********************************************************************
  
  InsertOneTuple
  assumes that 'oid' will not be zero.

 **********************************************************************/

InsertOneTuple(oid)
	ObjectId	oid;
{
	HeapTuple	formtuple();
	HeapTuple tuple;

	tuple = formtuple(numattr,attrtypes,values,Blanks);
	if(oid !=(OID)0) {
		AssertState(IsNormalProcessingMode());
		SetProcessingMode(BootstrapProcessing);
		tuple->t_oid=oid;
	}
	RelationInsertHeapTuple(reldesc,(HeapTuple)tuple,(double *)NULL);
	SetProcessingMode(NormalProcessing);
	pfree(tuple);
}

InsertOneValue(oid, value, i)
	ObjectId	oid;
	int		i;
	char		*value;
{
	int		typeindex;
	char		*prt;
	struct	typmap	**app;
	HeapTuple	tuple;
	extern	char	Blanks[];
	char		*fmgr();

	if (Typ != (struct typmap **)NULL) {
		app = Typ;
		while (app != NULL && (*app)->am_oid !=
				reldesc->rd_att.data[i]->atttypid)
			app++;
		values[i] = fmgr((*app)->am_typ.typinput, value);
		printf("%s ", prt =
			fmgr((*app)->am_typ.typoutput, values[i]));
		pfree(prt);
	} else {
		typeindex = attrtypes[i]->atttypid - FIRST_TYPE_OID;
		values[i] = fmgr(Procid[typeindex][(int)Q_IN], value);
		printf("%s ", prt =
			fmgr(Procid[typeindex][(int) Q_OUT], values[i]));
		pfree(prt);
	}
/*
	tuple = formtuple(numattr, attrtypes, values, Blanks);
	if (!ObjectIdIsValid(oid)) {
		ENABLE_BOOTSTRAP();
		tuple->t_oid = oid;
	}
	RelationInsertHeapTuple(reldesc, (HeapTuple)tuple, (double *)NULL);
	DISABLE_BOOTSTRAP();
	pfree(tuple);
*/
}
/*
handledot()
{
	char		destname[80], destname2[80], destname3[80];
	Relation	oldrel, newrel;
	extern	char	Buf[];
	extern		addattribute(), renamerel();
	extern		psort();
	extern		openrel(), closerel();

	switch (Buf[0]) {
	case 'S':				
		if (scanf("%s %s", destname, destname2) != 2) {
			fputs("Error: failed to read old ", stderr);
			fputs("and new relation names.\n", stderr);
			err();
		}
		StartPortalAllocMode(DefaultAllocMode, 0);
		oldrel = amopenr(destname);
		amcreate(destname2, 'n', numattr, attrtypes);
		newrel = amopenr(destname2);
		psort(oldrel, newrel, numattr, attrtypes);
		amclose(oldrel);
		amclose(newrel);
		EndPortalAllocMode();
		break;
	case 'I':
		handleindex();
		break;
	default:
		elog(WARN, "Warning: unknown command '.%c'.\n", Buf[0]);
	}
}
*/
/*
handleindex()
{
	int	i;
	int	numberOfAttributes;
	int16	*attributeNumber;
	char	**class;
	char	heapName[80];
	char	indexName[80];
	char	accessMethodName[80];

	if (scanf("%s %s %s", heapName, indexName, accessMethodName) != 3) {
		fputs("Error: failed to read heap relation,", stderr);
		fputs(" index relation, and", stderr);
		fputs(" access method names.\n", stderr);
		err();
	}
	if (scanf("%d", &numberOfAttributes) != 1) {
		fputs("Error: failed to read number of attributes\n", stderr);
		err();
	}
	attributeNumber = (int16 *)palloc(numberOfAttributes *
		sizeof *attributeNumber);
	class = (char **)palloc(numberOfAttributes * sizeof *class);

	for (i = 0; i < numberOfAttributes; i += 1) {
		if (scanf("%hd", &attributeNumber[i]) != 1) {
			fputs("Error: failed to read attribute number\n",
				stderr);
			err();
		}
		class[i] = palloc(80);
		if (scanf("%s", class[i]) != 1) {
			fputs("Error: failed to read class name\n", stderr);
			err();
		}
	}
	DefineIndex(heapName, indexName, accessMethodName, numberOfAttributes,
		attributeNumber, class, 0, 0);
}
*/

handletime()
{
	int	numberOfTimes;
	char	firstTime[80];
	char	secondTime[80];
	Time	time1;
	Time	time2;

	if (scanf("%d", &numberOfTimes) != 1) {
		fputs("Error: failed to read number of time fields\n", stderr);
		err();
	}
	if (numberOfTimes < -1 || numberOfTimes > 2) {
		elog(WARN,
			"number of time fields (%d) is not -1, 0, 1, or 2\n",
			numberOfTimes);
	} else if (numberOfTimes == -1) {
		ShowTime = !ShowTime;
		return;
	} else if (numberOfTimes == 0) {
		StandardTimeRange = DefaultTimeRange;
		puts("Time range reset to \"now\"");
	} else if (numberOfTimes == 1) {
		if (scanf("%[^\n]", firstTime) != 1) {
			fputs("Error: failed to read time\n", stderr);
			err();
		}
		time1 = abstimein(firstTime);
		printf("*** You entered \"%s\"", firstTime);
		if (time1 == INVALID_ABSTIME) {
			printf(" which is INVALID (time range unchanged).\n");
		} else {
			printf(" with representation %d.\n", time1);
			bcopy((char *)TimeFormSnapshotTimeRange(time1),
				(char *)&StandardTimeRangeSpace,
				sizeof StandardTimeRangeSpace);
			StandardTimeRange = (TimeRange)&StandardTimeRangeSpace;
		}
	} else {	/* numberOfTimes == 2 */
		if (scanf("%[^,],%[^\n]", firstTime, secondTime) != 2) {
			fputs("Error: failed to read both times\n", stderr);
			err();
		}
		printf("*** You entered \"%s\" and \"%s\"\n", firstTime,
			secondTime);
		time1 = abstimein(firstTime);
		time2 = abstimein(secondTime);
		if (time1 == INVALID_ABSTIME) {
			time1 = InvalidTime;
			if (time2 == INVALID_ABSTIME) {
				time2 = InvalidTime;
				printf("*** range is [,].\n");
			} else {
				printf("*** range is [,%d].\n", time2);
			}
		} else {
			if (time2 == INVALID_ABSTIME) {
				time2 = InvalidTime;
				printf("*** range is [%d,].\n", time1);
			} else {
				printf("*** range is [%d,%d].\n", time1, time2);
			}
		}
		bcopy(TimeFormRangedTimeRange(time1, time2),
			(char *)&StandardTimeRangeSpace,
			sizeof StandardTimeRangeSpace);
		StandardTimeRange = (TimeRange)&StandardTimeRangeSpace;
	}
}

cleanup()
{
	static	int	beenhere = 0;
     
	if (!beenhere)
		beenhere = 1;
	else {
		elog(FATAL,"Memory manager fault: cleanup called twice.\n", stderr);
		exit(1);
	}
	elog(DEBUG,"** clean up and exit **");
	if (reldesc != (Relation)NULL) {
/*
		(void)switchcontext(RelationOpenContext);
*/
		amclose(reldesc);
/*
		resetmmgr();
		(void)topcontext();
*/
	}
	CommitTransactionCommand();
	exit(Warnings);
}

gettype(type)
	char *type;
{
	int		i;
	Relation	rdesc;
	HeapScan	sdesc;
	HeapTuple	tup;
	struct	typmap	**app;
	extern	char	TYPE_R[];

	if (Typ != (struct typmap **)NULL) {
		for (app = Typ; *app != (struct typmap *)NULL; app++)
			if (strcmp((*app)->am_typ.typname, type) == 0) {
				Ap = *app;
				return((*app)->am_oid);
			}
	} else {
		for (i = 0; i <= (int)TY_LAST; i++) {
			if (strcmp(type, typestr[i]) == 0) {
				return(i);
			}
		}
		rdesc = amopenr(TYPE_R);
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 0,
			(ScanKey)NULL);
		for (i = 0; PointerIsValid(tup = amgetnext(sdesc, 0,
				(Buffer *)NULL)); i++) {
			;
		}
		amendscan(sdesc);
		app = Typ = ALLOC(struct typmap *, i + 1);
		while (i-- > 0)
			*app++ = ALLOC(struct typmap, 1);
		*app = (struct typmap *)NULL;
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange,
			(ScanKey)NULL);
		app = Typ;
		while (PointerIsValid(tup =
				amgetnext(sdesc, 0, (Buffer *)NULL))) {
			(*app)->am_oid = tup->t_oid;
			bcopy((char *)GETSTRUCT(tup), (char *)&(*app++)->am_typ,
				sizeof (*app)->am_typ);
		}
		amendscan(sdesc);
		amclose(rdesc);
		return(gettype(type));
	}
	elog(WARN, "Error: unknown type '%s'.\n", type);
	err();
}

struct attribute *	/* XXX */
AllocateAttribute()
{
	struct attribute	*attribute = new(struct attribute); /* XXX */

	if (!PointerIsValid(attribute)) {
		elog(FATAL, "AllocateAttribute: malloc failed");
	}
	bzero((Pointer)attribute, sizeof *attribute);

	return (attribute);
}

err()
{
	cleanup();
	exit(1);
}

handle_warn()
{
	longjmp(Warn_restart);
}

die()
{
	ExitPostgres(0);
}

/*ARGSUSED*/
void
ExcAbort(excP, detail, data, message)
	Exception	*excP;
	ExcDetail	detail;
	ExcData		data;
	ExcMessage	message;
{
	AbortPostgres();
}

unsigned char 
MapEscape(s)
	char **s;
{
#define isoctal(c) ((c)>='0' && (c) <= '7')
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
	case '0': case '1': case '2': case '3': case '4': case '5' :
	case '6': case '7':
		for (--*s,i=0;isoctal((*s)[i]) && (i<3); i++)
			octal[i]=(*s)[i];
		*s += i;
		octal[i] = '\0';
		if((i=strtol(octal,0,8)) <= 0377) return (char) i;
	default:
		elog(WARN,"Bad Escape sequence");
		return *((*s)++); /* ignore */
	}
}

/**********************************************************************
  EnterString
  returns the string table position of the identifier
  passed to it.  We add it to the table if we can't find it.
 **********************************************************************/

int EnterString (str)
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

/**********************************************************************
  LexIDStr
  when given an idnum into the 'string-table' return the string
  associated with the idnum
 **********************************************************************/

char *
LexIDStr(ident_num) 
	int ident_num;
{
	return(strtable[ident_num]->s);
}    


/*
 * Compute a hash function for a given string.  We look at the first,
 * the last, and the middle character of a string to try to get spread
 * the strings out.  The function is rather arbitrary, except that we
 * are mod'ing by a prime number.
 */


int Hash (str, len)
	char	*str;
	int	len;
{
	register int	result;
	
	result =(NUM * str[0] + NUMSQR * str[len-1] + NUMCUBE * str[(len-1)/2]);

	return (result % HASHTABLESIZE);

}

/*
 * This routine looks for the specified string in the hash
 * table.  It returns a pointer to the hash node found,
 * or NULL if the string is not in the table.
 */

hashnode *
FindStr (str, length, mderef)
	char	*str;
	int	length;
	hashnode   *mderef;
{
	hashnode	*node;
	node = hashtable [Hash (str, length)];
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

/*
 * This function adds the specified string, along with its associated
 * data, to the hash table and the string table.  We return the node
 * so that the calling routine can find out the unique id that AddStr
 * has assigned to this string.
 */

hashnode *
AddStr(str, strlength, mderef)
	char	*str;
	int	strlength;
	int        mderef;
{
	hashnode	*temp, *trail, *newnode;
	int		hashresult;
	char		*stroverflowmesg = 
		"There are too many string constants and\
identifiers for the compiler to handle.";

	if (++strtable_end == STRTABLESIZE) {
		/* Error, string table overflow, so we Punt */
		elog(FATAL,stroverflowmesg);
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

	hashresult = Hash (str, strlength);
	if (hashtable [hashresult] == NULL)
		hashtable [hashresult] = newnode;
	else {				/* There is something in the list */
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

/*
 * This routine dumps out the hash table for inspection.
 */

printhashtable ()
{
	register int	i;
	hashnode	*hashptr;

	fprintf (stderr, "\n\nHash Table:\n");
	for (i = 0; i < HASHTABLESIZE; i++) {
	    
	    if (hashtable[i] != NULL) {
		fprintf (stderr, "Position: %d\n", i);
		hashptr = hashtable[i];
		do {
		    fprintf (stderr, "\tString #%d",
		      hashptr->strnum);

		    hashptr = hashptr->next;
		
		} while (hashptr != NULL);
	    }
	}
}

/*
 * This routine dumps out the string table for inspection.
 */

printstrtable ()
{
	register int	i;


	fprintf (stderr, "\nString Table:\n");
	for (i = 0; i <= strtable_end; i++) {
		fprintf (stderr, "#%d: \"%s\";\t   ", i, strtable[i]->s);
		if (i % 3 == 0 && i > 0)
			fprintf (stderr, "\n");
	}
	fprintf (stderr, "\n");
}

/*
 * Error checked version of malloc.
 */

char *emalloc (size)
	unsigned	size;
{
	char	*p;

	if ( (p = malloc (size)) == NULL)
		elog (FATAL,"Memory Allocation Error\n");
	else
		return (p);
	/*NOTREACHED*/
}

/**********************************************************************
   LookUpMacro
   takes an index into the string table and converts it into the
   appropriate "string"
  
 **********************************************************************/
int
LookUpMacro(foo)
	char *foo;
{
	hashnode *node;
	int macro_indx;

	if(node=FindStr(foo,strlen(foo),0))
		macro_indx = node->strnum;
  
	if(strtable[macro_indx]->mderef == 0)
		elog(WARN,"Dereferencing non-existent macro");
	else
		return(strtable[macro_indx]->mderef);
}

printmacros()
{
	register int i;
	fprintf(stdout,"Macro Table:\n");
	for(i=0;i<strtable_end;i++) {
		if(strtable[i]->mderef != 0)
			printf("\"%s\" = \"%s\"\n",strtable[i]->s,
				strtable[strtable[i]->mderef]->s);
	}
}



/**********************************************************************
   DefineMacro
   takes two identifiers and links them up
   by putting a pointer between id1 and id2
   (the identifiers pointed to by indx1 and indx2

 **********************************************************************/

DefineMacro(indx1,indx2)
     int indx1,indx2;
{
	strtable[indx1]->mderef = indx2;
	if(strtable[indx1]->mderef != 0)
		elog(WARN,"Warning :redefinition of macro\n");
}

/**********************************************************************

  AddAttr adds the attributes in the array attrtypes
  to the relation passed to it

 **********************************************************************/

AddAttr(relation)
	char *relation;
{
	addattribute(relation,numattr,attrtypes);
}

/*************
 old routines
 *************/

/*
 * This routine returns the length of the identifier or
 * string numbered ident_num. It checks if string exists first.
 * If not, it returns a length of -1.

int LexIDLen (strnum)
	int strnum;
{
	if (strnum > strtable_end)
	    return (-1);
	else 
	    return (strlen (strtable[strnum]));
	
}
/*End LexIDLen*/

/*
 * This routine returns the n'th character of the identifier or
 * string numbered ident_num.  We check to see if the string exists,
 * and, if so, if there is a character in that position.
 * Note that for strings, quotes are excluded and char numbering 
 * begins from 1.

char LexIDChar (ident_num, char_pos)
	int ident_num, char_pos;
{
  if (ident_num > strtable_end)
    return ('\0');
  else if (strlen(strtable[ident_num]) < char_pos)
    return ('\0');
  else
    return ((strtable[ident_num]->s)[char_pos - 1]);
}
/*End LexIDChar*/

