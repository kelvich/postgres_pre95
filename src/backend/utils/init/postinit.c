/* ----------------------------------------------------------------
 *   FILE
 *	postinit.c
 *	
 *   DESCRIPTION
 *	main postgres initialization routines
 *
 *   INTERFACE ROUTINES
 *	InitializePostgres()
 *	InitPostgres()
 *	ReinitPostgres()
 *
 *   NOTES
 *	InitializePostgres() is the function called from PostgresMain
 *	which does all non-trival initialization, mainly by calling
 *	all the other initialization functions.   If you feel the
 *	need to add more initialization code, it should be done
 *	in InitializePostgres() or someplace lower.  Do not start
 *	putting stuff in PostgresMain - if you do then someone
 *	will have to clean it up later, and it's not going to be me!
 *	-cim 10/3/90
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/types.h>
#include <math.h>

#include "tmp/postgres.h"

#include "machine.h"		/* for BLCKSZ, for InitMyDatabaseId() */

#include "access/heapam.h"
#include "access/tqual.h"
#include "storage/bufpage.h"	/* for page layout, for InitMyDatabaseId() */
#include "storage/sinval.h"
#include "storage/sinvaladt.h"
#include "support/master.h"

#include "tmp/hasht.h"		/* for EnableHashTable, etc. */
#include "tmp/miscadmin.h"
#include "tmp/portal.h"		/* for EnablePortalManager, etc. */

#include "utils/exc.h"		/* for EnableExceptionHandling, etc. */
#include "utils/fmgr.h"		/* for EnableDynamicFunctionManager, etc. */
#include "utils/lmgr.h"
#include "utils/log.h"
#include "utils/mcxt.h"		/* for EnableMemoryContext, etc. */

#include "catalog/catname.h"

#include "tmp/miscadmin.h"

/* ----------------
 *	global variables & stuff
 *	XXX clean this up! -cim 10/5/90
 * ----------------
 */
    
extern bool override;
extern int Quiet;

/*
 * EnableAbortEnvVarName --
 *	Enables system abort iff set to a non-empty string in environment.
 */
#define EnableAbortEnvVarName	"POSTGRESABORT"

typedef String	EnvVarName;
extern String getenv ARGS((EnvVarName name));

/*
 * XXX PostgresPath and PostgresDatabase belong somewhere else.
 */
String	PostgresPath = NULL;
String	PostgresDatabase = NULL;

extern int Debugfile, Ttyfile, Dblog, Slog;
extern int Portfd, Packfd, Slog, Pipefd;

extern BackendId	MyBackendId;
extern BackendTag	MyBackendTag;
extern NameData		MyDatabaseNameData;
extern Name		MyDatabaseName;

extern bool		MyDatabaseIdIsInitialized;
extern ObjectId		MyDatabaseId;
extern bool		TransactionInitWasProcessed;

extern struct	bcommon	Ident;	/* moved to dlog */

extern int	Debug_file, Err_file, Noversion;

extern int on_exitpg();
extern void BufferManagerFlush();

#ifndef	private
#ifndef	EBUG
#define	private	static
#else	/* !defined(EBUG) */
#define private
#endif	/* !defined(EBUG) */
#endif	/* !defined(private) */

/* ----------------------------------------------------------------
 *		some of the 19 ways to leave postgres
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ExitPostgres
 * ----------------
 */
void
ExitPostgres(status)
    ExitStatus	status;
{
#ifdef	__SABER__
    saber_stop();
#endif
    exitpg(status);
}

/* ----------------
 *	AbortPostgres
 * ----------------
 */
void
AbortPostgres()
{
    String abortValue = getenv(EnableAbortEnvVarName);

#ifdef	__SABER__
    saber_stop();
#endif

    if (PointerIsValid(abortValue) && abortValue[0] != '\0')
	abort();
    else
	exitpg(FatalExitStatus);
}

/* ----------------
 *	StatusBackendExit
 * ----------------
 */
void
StatusBackendExit(status)
    int	status;
{
    /* someday, do some real cleanup and then call the LISP exit */
    /* someday, call StatusPostmasterExit if running without postmaster */
    exitpg(status);
}

/* ----------------
 *	StatusPostmasterExit
 * ----------------
 */
void
StatusPostmasterExit(status)
    int	status;
{
    /* someday, do some real cleanup and then call the LISP exit */
    exitpg(status);
}

/* ----------------------------------------------------------------
 *			InitPostgres support
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *  InitMyDatabaseId() -- Find and record the OID of the database we are
 *			  to open.
 *
 *	The database's oid forms half of the unique key for the system
 *	caches and lock tables.  We therefore want it initialized before
 *	we open any relations, since opening relations puts things in the
 *	cache.  To get around this problem, this code opens and scans the
 *	pg_database relation by hand.
 *
 *	This algorithm relies on the fact that first attribute in the
 *	pg_database relation schema is the database name.  It also knows
 *	about the internal format of tuples on disk and the length of
 *	the datname attribute.  It knows the location of the pg_database
 *	file.
 *
 *	This code is called from InitDatabase(), after we chdir() to the
 *	database directory but before we open any relations.
 * --------------------------------
 */

void
InitMyDatabaseId()
{
    int		dbfd;
    int		nbytes;
    int		max, i;
    HeapTuple	tup;
    char	*tup_db;
    Page	pg;
    PageHeader	ph;

    /* ----------------
     *  If we're unable to open pg_database, we're not running in the
     *  data/base/dbname directory.  This happens only when we're
     *  creating the first database.  In that case, just use
     *  InvalidObjectId as the dbid, since we're not using shared memory.
     * ----------------
     */
    if ((dbfd = open("../../pg_database", O_RDONLY, 0666)) < 0) {
	LockingIsDisabled = true;
	return;
    }

    /* ----------------
     *	read and examine every page in pg_database
     *
     *	Raw I/O! Read those tuples the hard way! Yow!
     *
     *  Why don't we use the access methods or move this code
     *  someplace else?  This is really pg_database schema dependent
     *  code.  Perhaps it should go in lib/catalog/pg_database?
     *  -cim 10/3/90
     * ----------------
     */
    pg = (Page) palloc(BLCKSZ);
    ph = (PageHeader) pg;

    while ((nbytes = read(dbfd, pg, BLCKSZ)) == BLCKSZ) {
	max = PageGetMaxOffsetIndex(pg);

	/* look at each tuple on the page */
	for (i = 0; i <= max; i++) {
	    int offset;

	    /* if it's a freed tuple, ignore it */
	    if (!(ph->pd_linp[i].lp_flags & LP_USED))
		continue;

	    /* get a pointer to the tuple itself */
	    offset = (int) ph->pd_linp[i].lp_off;
	    tup = (HeapTuple) (((char *) pg) + offset);
	    tup_db = ((char *) tup) + sizeof (*tup);

	    /* see if this is the one we want */
	    /* note: MyDatabaseName set by call to SetDatabaseName() */
	    if (strncmp(MyDatabaseName, tup_db, 16) == 0) {
		MyDatabaseId = tup->t_oid;
		goto done;
	    }
	}
    }
    
done:
    (void) close(dbfd);
    pfree(pg);

    /* ----------------
     *	don't know why this is here -- historical artifact.  --mao 
     * ----------------
     */
    if (ObjectIdIsValid(MyDatabaseId))
	LockingIsDisabled = false;
    else
	LockingIsDisabled = true;
}

/* ----------------
 *	DoChdirAndInitDatabaseNameAndPath
 *
 *	this just chdir's to the proper data/base directory
 *	XXX clean this up more.
 *
 * XXX The following code is an incorrect of the semantics
 * XXX described in the header file.  Handling of defaults
 * XXX should happen here, too.
 * ----------------
 */
void
DoChdirAndInitDatabaseNameAndPath(name, path)
    String	name;		/* name of database */
    String	path;		/* full path to database */
{
    /* ----------------
     *	sanity check (database may not change once set)
     * ----------------
     */
    AssertState(!StringIsValid(GetDatabasePath()));
    AssertState(!StringIsValid(GetDatabaseName()));

    /* ----------------
     *	check the path
     * ----------------
     */
    if (StringIsValid(path))
	SetDatabasePath(path);
    else
	elog(FATAL, "DoChdirAndInitDatabaseNameAndPath: path:%s is not valid",
	     path);

    /* ----------------
     *	check the name
     * ----------------
     */
    if (StringIsValid(name))
	SetDatabaseName(name);
    else 
	elog(FATAL, "DoChdirAndInitDatabaseNameAndPath: name:%s is not valid",
	     name);

    /* ----------------
     *	change to the directory, or die trying.
     * ----------------
     */
    if (chdir(path) < 0)
	elog(FATAL, "DoChdirAndInitDatabaseNameAndPath: chdir(\"%s\"): %m",
	     path);
}

/* --------------------------------
 *	InitUserid
 *
 *	initializes crap associated with the user id.
 * --------------------------------
 */
void
InitUserid() {
    setuid(geteuid());
    SetUserId();
}

/* --------------------------------
 *	InitCommunication
 *
 *	This routine initializes stuff needed for ipc, locking, etc.
 *	it should be called something more informative.
 *
 * Note:
 *	This does not set MyBackendId.  MyBackendTag is set, however.
 * --------------------------------
 */

void
InitCommunication()
{
    String	getenv();	/* XXX style */
    String	postid;
    String	postport;
    IPCKey	key;

    /* ----------------
     *	try and get the backend tag from POSTID
     * ----------------
     */
    MyBackendId = -1;

    postid = getenv("POSTID");
    if (!PointerIsValid(postid)) {
	MyBackendTag = -1;
    } else {
	MyBackendTag = atoi(postid);
	Assert(MyBackendTag >= 0);
    }

    /* ----------------
     *  try and get the ipc key from POSTPORT
     * ----------------
     */
    postport = getenv("POSTPORT");
    
    if (PointerIsValid(postport)) {
	SystemPortAddress address = atoi(postport);

	if (address == 0)
	    elog(FATAL, "InitCommunication: invalid POSTPORT");

	if (MyBackendTag == -1)
	    elog(FATAL, "InitCommunication: missing POSTID");

	key = SystemPortAddressCreateIPCKey(address);

    } else if (IsUnderPostmaster) {
	elog(FATAL,
	     "InitCommunication: under postmaster and POSTPORT not set");
    } else {
	/* ----------------
	 *  assume we're running a postgres backend by itself with
	 *  no front end or postmaster.
	 * ----------------
	 */
	if (MyBackendTag == -1) {
	    MyBackendTag = 1;
	}

	key = PrivateIPCKey;
    }

    /* ----------------
     *  initialize shared memory and semaphores appropriately.
     * ----------------
     */
    AttachSharedMemoryAndSemaphores(key);
}


/* --------------------------------
 *	InitStdio
 *
 *	this routine consists of a bunch of code fragments
 *	that used to be randomly scattered through cinit().
 *	they all seem to do stuff associated with io.
 * --------------------------------
 */
void
InitStdio()
{
    /* ----------------
     *	this appears to be a funky variable used with the IN and OUT
     *  macros for controlling the error message indent level.
     * ----------------
     */
    ElogDebugIndentLevel = 0;

    /* ----------------
     *	old socket initialization crud starts here
     * ----------------
     */
    Ttyfile = 0;
    Debugfile = Debug_file = DebugFileOpen();

    if (IsUnderPostmaster) {
	struct	dpacket	pack;
	    
	/* ----------------
	 *	why the hell are we doing this here???  -cim 
	 * ----------------
	 */
	if (!ValidPgVersion(".") || !ValidPgVersion("../.."))
	    elog(FATAL, "InitStdio: !ValidPgVersion");
	
	Slog = Err_file = 3;
	Portfd = 4;
	Packfd = 5;
	Dblog = 6;
	Pipefd = 7;
	
	pinit();
	    
	read(Packfd, (char *)&pack, sizeof(pack));
	bcopy(pack.data, (char *)&Ident, sizeof(Ident));
    } 

    if (!isatty(fileno(stdout)) && !isatty(fileno(stderr))) {
	setbuf(stdout, (char *)NULL);
	setbuf(stderr, (char *)NULL);
    }

    Dblog = dup(Debugfile);
    Slog = Err_file = ErrorFileOpen();
}

/* --------------------------------
 *	InitPostgres
 *
 * Note:
 *	Be very careful with the order of calls in the InitPostgres function.
 * --------------------------------
 */
static bool	PostgresIsInitialized = false;

/*
 */
void
InitPostgres(name)
    String	name;		/* database name */
{
    bool	bootstrap;	/* true if BootstrapProcessing */

    /* ----------------
     *	see if we're running in BootstrapProcessing mode
     * ----------------
     */
    bootstrap = IsBootstrapProcessingMode();
    
    /* ----------------
     *	turn on the exception handler.  Note: we cannot use elog, Assert,
     *  AssertState, etc. until after exception handling is on.
     * ----------------
     */
    EnableExceptionHandling(true);

    /* ----------------
     *	A stupid check to make sure we don't call this more than once.
     *  But things like ReinitPostgres() get around this by just diddling
     *	the PostgresIsInitialized flag.
     * ----------------
     */
    AssertState(!PostgresIsInitialized);

    /* ----------------
     *	EnableTrace is poorly understood by the current postgres
     *  implementation people.  Since none of us know how it really
     *  is intended to function, we keep it turned off for now.
     *  Turning it on will cause a core dump in all the non-sequent
     *  ports.  Actually it may fail there too.  Who knows. -cim 10/3/90
     * ----------------
     */
    EnableTrace(false);

    /* ----------------
     *	Memory system initialization.
     *  (we may call palloc after EnableMemoryContext())
     *
     *  Note EnableMemoryContext() must happen before EnableHashTable() or
     *  EnablePortalManager().
     * ----------------
     */
    EnableMemoryContext(true);	/* initializes the "top context" */
    EnableHashTable(true);	/* memory for hash table sets */
    EnablePortalManager(true);	/* memory for portal/transaction stuff */

    /* ----------------
     *   set ourselves to the proper user id and figure out our postgres
     *   user id.  If we ever add security so that we check for valid
     *   postgres users, we might do it here.
     * ----------------
     */
    InitUserid();

    /* ----------------
     *	 attach to shared memory and semaphores, and initialize our
     *   input/output/debugging file descriptors.
     * ----------------
     */
    InitCommunication();
    InitStdio();

    /* ----------------
     *	 initialize the buffer manager.  this must happen after
     *   shared memory/semaphore initialization.
     *
     *   if stable main memory is assumed (-S flag is set), it is necessary
     *   to flush all dirty shared buffers before exit (plai 8/7/90) so we
     *   register an on_exitpg() handler also.  This has to be done *after*
     *   InitCommunication or the shared memory will be removed before
     *   BufferManagerFlush will be called, because on_exitpg() handlers are
     *   called in the reverse of the order in which they are registered.
     * ----------------
     */
    BufferManagerInit();
    
    if (!TransactionFlushEnabled())
        on_exitpg(BufferManagerFlush, (caddr_t) 0);

    /* ----------------
     *	check for valid "meta gunk" (??? -cim 10/5/90) and change to
     *  database directory.
     *
     *  Note:  DatabaseName, MyDatabaseName, and DatabasePath are all
     *  initialized with DatabaseMetaGunkIsConsistent(), strncpy() and
     *  DoChdirAndInitDatabase() below!  XXX clean this crap up!
     *  -cim 10/5/90
     * ----------------
     */
    {
	static char  myPath[MAXPGPATH];	/* DatabasePath points here! */
	static char  myName[17]; 	/* DatabaseName points here! */

	/* ----------------
	 *  DatabaseMetaGunkIsConsistent fills in myPath, but what about
	 *  when bootstrap or Noversion is true?? -cim 10/5/90
	 * ----------------
	 */
	myPath[0] = '\0';
	
	if (! bootstrap)
	    if (! DatabaseMetaGunkIsConsistent(name, myPath))
		if (! Noversion)
		    elog(FATAL,
			 "InitPostgres: ! DatabaseMetaGunkIsConsistent");
	
	(void) strncpy(myName, name, sizeof(myName));

	/* ----------------
	 *  ok, we've figured out myName and myPath, now save these
	 *  and chdir to myPath.
	 * ----------------
	 */
	DoChdirAndInitDatabaseNameAndPath(myName, myPath);
    }
    
    /* ********************************
     *	code after this point assumes we are in the proper directory!
     * ********************************
     */
    
    /* ----------------
     *	initialize the database id used for system caches and lock tables
     * ----------------
     */
    InitMyDatabaseId();
    
    /* ----------------
     *	initialize the transaction system and the relation descriptor
     *  cache.  Note we have to make certain the lock manager is off while
     *  we do this.
     * ----------------
     */
    AmiTransactionOverride(IsBootstrapProcessingMode());
    LockingIsDisabled = true;

    RelationInitialize();	   /* pre-allocated reldescs created here */
    InitializeTransactionSystem(); /* pg_log,etc init/crash recovery here */
    
    LockingIsDisabled = false;

    /* ----------------
     *	anyone knows what this does?  something having to do with
     *  system catalog cache invalidation in the case of multiple
     *  backends, I think -cim 10/3/90
     * ----------------
     */
    InitSharedInvalidationState();

    if (MyBackendId > MaxBackendId || MyBackendId <= 0) {
	elog(FATAL, "cinit2: bad backend id %d (%d)",
	     MyBackendTag,
	     MyBackendId);
    }

    /* ----------------
     *  initialize the access methods and set the heap-randomization
     *  behaviour  (if POSTGROWS is set  then we disable the usual
     *  random-search for a free page when we do an insertion).
     * ----------------
     */
    initam();
    InitRandom();		/* XXX move this to initam() */

    /* ----------------
     *	initialize all the system catalog caches.
     * ----------------
     */
    zerocaches();
    InitCatalogCache();

    /* ----------------
     *	ok, all done, now let's make sure we don't do it again.
     * ----------------
     */
    PostgresIsInitialized = true;

    /* ----------------
     *  Done with "InitPostgres", now change to NormalProcessing unless
     *  we're in BootstrapProcessing mode.
     * ----------------
     */
    if (!bootstrap)
	SetProcessingMode(NormalProcessing);
}

/* --------------------------------
 *	ReinitPostgres
 *
 *	It is not clear how this routine is supposed to work, or under
 *	what circumstances it should be called.  But hirohama wrote it
 *	for some reason so it's still here.  -cim 10/3/90
 * --------------------------------
 */
void
ReinitPostgres()
{
    /* assumes exception handling initialized at least once before */
    AssertState(PostgresIsInitialized);

    /* reset all modules */

    EnablePortalManager(false);
    EnableHashTable(false);
    EnableMemoryContext(false);

    EnableExceptionHandling(false);
    EnableTrace(false);

    /* now reinitialize */
    PostgresIsInitialized = false;	/* does it matter where this goes? */
    InitPostgres(NULL);	
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
     *  done more than once should go in InitPostgres().
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

/* ----------------------------------------------------------------
 *		traps to catch calls to obsolete routines
 * ----------------------------------------------------------------
 */
/* ----------------
 *	cinit
 * ----------------
 */
bool
cinit()
{
    fprintf(stderr, "cinit called! eliminate this!!\n");
    AbortPostgres();
}

/* ----------------
 *	cinit2
 * ----------------
 */
bool
cinit2()
{
    fprintf(stderr, "cinit2 called! eliminate this!!\n");
    AbortPostgres();
}
