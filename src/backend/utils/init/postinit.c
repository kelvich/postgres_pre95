/* ----------------------------------------------------------------
 *   FILE
 *	postinit.c
 *	
 *   DESCRIPTION
 *	postgres initialization utilities
 *
 *   INTERFACE ROUTINES
 *	InitPostgres()
 *
 *   NOTES
 *	InitializePostgres() is the function called from PostgresMain
 *	which does all non-trival initialization, mainly by calling
 *	all the other initialization functions.  InitializePostgres()
 *	is only used within the "postgres" backend and so that routine
 *	is in tcop/postgres.c  InitPostgres() is needed in cinterface.a
 *	because things like the bootstrap backend program need it. Hence
 *	you find that in this file...
 *
 *	If you feel the need to add more initialization code, it should be
 *	done in InitializePostgres() or someplace lower.  Do not start
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
#include "storage/lmgr.h"
#include "storage/bufmgr.h"
#include "tmp/master.h"

#include "tmp/miscadmin.h"
#include "tmp/portal.h"		/* for EnablePortalManager, etc. */

#include "utils/exc.h"		/* for EnableExceptionHandling, etc. */
#include "utils/fmgr.h"		/* for EnableDynamicFunctionManager, etc. */
#include "utils/log.h"
#include "utils/mcxt.h"		/* for EnableMemoryContext, etc. */

#include "catalog/catname.h"
#include "catalog/pg_database.h"

static IPCKey		PostgresIpcKey;

extern char *DataDir;

#ifndef	private
#ifndef	EBUG
#define	private	static
#else	/* !defined(EBUG) */
#define private
#endif	/* !defined(EBUG) */
#endif	/* !defined(private) */

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
    Page	pg;
    PageHeader	ph;
    char	*dbfname;
    Form_pg_database	tup_db;

    /*
     *  At bootstrap time, we don't need to check the oid of the database
     *  in use, since we're not using shared memory.  This is lucky, since
     *  the database may not be in the tables yet.
     */

    if (IsBootstrapProcessingMode()) {
	LockDisable(true);
	return;
    }

    dbfname = (char *) palloc(strlen(DataDir) + strlen("pg_database") + 2);
    sprintf(dbfname, "%s/pg_database", DataDir);
    if ((dbfd = open(dbfname, O_RDONLY, 0666)) < 0)
	elog(FATAL, "Cannot open %s", dbfname);

    pfree(dbfname);

    /* ----------------
     *	read and examine every page in pg_database
     *
     *	Raw I/O! Read those tuples the hard way! Yow!
     *
     *  Why don't we use the access methods or move this code
     *  someplace else?  This is really pg_database schema dependent
     *  code.  Perhaps it should go in lib/catalog/pg_database?
     *  -cim 10/3/90
     *
     *  mao replies 4 apr 91:  yeah, maybe this should be moved to
     *  lib/catalog.  however, we CANNOT use the access methods since
     *  those use the buffer cache, which uses the relation cache, which
     *  requires that the dbid be set, which is what we're trying to do
     *  here.
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

	    /*
	     *  if the tuple has been deleted (the database was destroyed),
	     *  skip this tuple.  XXX warning, will robinson:  violation of
	     *  transaction semantics happens right here.  we should check
	     *  to be sure that the xact that deleted this tuple actually
	     *  committed.  only way to do this at init time is to paw over
	     *  the log relation by hand, too.  let's be optimistic.
	     *
	     *  XXX This is an evil type cast.  tup->t_xmax is char[5] while
	     *  TransactionId is struct * { char data[5] }.  It works but
	     *  if data is ever moved and no longer the first field this 
	     *  will be broken!! -mer 11 Nov 1991.
	     */
	    if (TransactionIdIsValid((TransactionId)tup->t_xmax))
		continue;

	    /*
	     *  Okay, see if this is the one we want.
	     *	XXX 1 july 91:  mao and mer discover that tuples now squash
	     *			t_bits.  Why is this?
	     *
	     *     24 july 92:  mer realizes that the t_bits field is only
	     *                  used in the event of null values.  If no
	     *                  fields are null we reduce the header size
	     *                  by doing the squash.  t_hoff tells you exactly
	     *                  how big the header actually is. use the PC
	     *                  means of getting at sys cat attrs.
	     */
	    tup_db = (Form_pg_database)GETSTRUCT(tup);

	    /* note: MyDatabaseName set by call to SetDatabaseName() */
	    if (strncmp(&(MyDatabaseName->data[0]),
			&(tup_db->datname.data[0]),
			16) == 0)
	    {
		MyDatabaseId = tup->t_oid;
		goto done;
	    }
	}
    }
    
done:
    (void) close(dbfd);
    pfree(pg);

    if (!ObjectIdIsValid(MyDatabaseId))
	elog(FATAL,
	     "Database %s does not exist in %s",
	     MyDatabaseName,
	     &DatabaseRelationName->data[0]);
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

/*
 * Enable this if you are trying to force the backend to run as if it is
 * running under the postmaster.
 *
 * This goto forces Postgres to attach to shared memory instead of using
 * malloc'ed memory (which is the normal behavior if run directly).
 *
 * To enable emulation, run the following shell commands (in addition
 * to enabling this goto)
 *
 *     % setenv POSTID 1
 *     % setenv POSTPORT 4321
 *     % postmaster &
 *     % kill -9 %1
 *
 * Upon doing this, Postmaster will have allocated the shared memory resources
 * that Postgres will attach to if you enable EMULATE_UNDER_POSTMASTER.
 *
 * This comment may well age with time - it is current as of 8 January 1990
 * 
 * Greg
 */

#ifdef EMULATE_UNDER_POSTMASTER

	goto forcesharedmemory;

#endif

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
#ifdef EMULATE_UNDER_POSTMASTER

forcesharedmemory:

#endif

    PostgresIpcKey = key;
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
	(void) DebugFileOpen();
}

/* --------------------------------
 *	InitPostgres
 *
 * Note:
 *	Be very careful with the order of calls in the InitPostgres function.
 * --------------------------------
 */
bool	PostgresIsInitialized = false;
extern int NBuffers;

/*
 *  this global is used by wei for testing his code, but must be declared
 *  here rather than in postgres.c so that it's defined for cinterface.a
 *  applications.
 */

int	testFlag = 0;
int	lockingOff = 0;

/*
 */
void
InitPostgres(name)
    String	name;		/* database name */
{
    bool	bootstrap;	/* true if BootstrapProcessing */
	extern	DestroyLocalRelList();

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

#if 0
    /* ----------------
     *	EnableTrace is poorly understood by the current postgres
     *  implementation people.  Since none of us know how it really
     *  is intended to function, we keep it turned off for now.
     *  Turning it on will cause a core dump in all the non-sequent
     *  ports.  Actually it may fail there too.  Who knows. -cim 10/3/90
     * ----------------
     */
    EnableTrace(false);
#endif

    /* ----------------
     *	Memory system initialization.
     *  (we may call palloc after EnableMemoryContext())
     *
     *  Note EnableMemoryContext() must happen before EnablePortalManager().
     * ----------------
     */
    EnableMemoryContext(true);	/* initializes the "top context" */
    EnablePortalManager(true);	/* memory for portal/transaction stuff */

    /* ----------------
     *	initialize the backend local portal stack used by
     *  internal PQ function calls.  see src/lib/libpq/be-dumpdata.c
     *  This is different from the "portal manager" so this goes here.
     *  -cim 2/12/91
     * ----------------
     */    
    be_portalinit();

    /* ----------------
     *	 attach to shared memory and semaphores, and initialize our
     *   input/output/debugging file descriptors.
     * ----------------
     */
    InitCommunication();
    InitStdio();

    if (!TransactionFlushEnabled())
        on_exitpg(FlushBufferPool, (caddr_t) 0);

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
	
	if (! bootstrap &&
	    ! DatabaseMetaGunkIsConsistent(name, myPath) &&
	    ! Noversion) {
		elog(NOTICE, "InitPostgres: could not locate valid PG_VERSION\n");
		elog(NOTICE, "files for %s and %s.", DataDir, name);
		elog(FATAL,  "Have you run initdb/createdb and set PGDATA properly?");
	}
	
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

    smgrinit();

    /* ----------------
     *	initialize the transaction system and the relation descriptor
     *  cache.  Note we have to make certain the lock manager is off while
     *  we do this.
     * ----------------
     */
    AmiTransactionOverride(IsBootstrapProcessingMode());
    LockDisable(true);

    /*
     * Part of the initialization processing done here sets a read
     * lock on pg_log.  Since locking is disabled the set doesn't have
     * intended effect of locking out writers, but this is ok, since
     * we only lock it to examine AMI transaction status, and this is
     * never written after initdb is done. -mer 15 June 1992
     */
    RelationInitialize();	   /* pre-allocated reldescs created here */
    InitializeTransactionSystem(); /* pg_log,etc init/crash recovery here */
    
    LockDisable(false);

    /* ----------------
     *	anyone knows what this does?  something having to do with
     *  system catalog cache invalidation in the case of multiple
     *  backends, I think -cim 10/3/90
     *  Sets up MyBackendId a unique backend identifier.
     * ----------------
     */
    InitSharedInvalidationState();

    /* ----------------
     * Set up a per backend process in shared memory.  Must be done after
     * InitSharedInvalidationState() as it relies on MyBackendId being
     * initialized already.  XXX -mer 11 Aug 1991
     * ----------------
     */
    InitProcess(PostgresIpcKey);

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
     *   set ourselves to the proper user id and figure out our postgres
     *   user id.  If we ever add security so that we check for valid
     *   postgres users, we might do it here.
     * ----------------
     */
    InitUserid();

    /* ----------------
     *	ok, all done, now let's make sure we don't do it again.
     * ----------------
     */
    PostgresIsInitialized = true;
	on_exitpg(DestroyLocalRelList, NULL);

    /* ----------------
     *  Done with "InitPostgres", now change to NormalProcessing unless
     *  we're in BootstrapProcessing mode.
     * ----------------
     */
    if (!bootstrap)
	SetProcessingMode(NormalProcessing);
    if (testFlag || lockingOff)
	LockDisable(true);
}
