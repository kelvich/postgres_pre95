
/* ----------------------------------------------------------------
 *   FILE
 *	miscinit.c
 *	
 *   DESCRIPTION
 *	miscellanious initialization support stuff
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <sys/param.h>		/* for MAXPATHLEN */
#include <sys/types.h>		/* for dev_t */
#include <sys/stat.h>

#include "tmp/postgres.h"

#include "tmp/portal.h"		/* for EnablePortalManager, etc. */
#include "utils/exc.h"		/* for EnableExceptionHandling, etc. */
#include "utils/mcxt.h"		/* for EnableMemoryContext, etc. */
#include "utils/log.h"

#include "tmp/miscadmin.h"

#include "catalog/catname.h"
#include "catalog/pg_user.h"
#include "catalog/pg_proc.h"
#include "access/skey.h"
#include "utils/rel.h"
/*
 * EnableAbortEnvVarName --
 *	Enables system abort iff set to a non-empty string in environment.
 */
#define EnableAbortEnvVarName	"POSTGRESABORT"

typedef String	EnvVarName;
extern String getenv ARGS((EnvVarName name));

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
 *	processing mode support stuff (used to be in pmod.c)
 * ----------------------------------------------------------------
 */
static ProcessingMode	Mode = NoProcessing;

bool
IsNoProcessingMode()
{
    return ((bool)(Mode == NoProcessing));
}

bool
IsBootstrapProcessingMode()
{
    return ((bool)(Mode == BootstrapProcessing));
}

bool
IsInitProcessingMode()
{
    return ((bool)(Mode == InitProcessing));
}

bool
IsNormalProcessingMode()
{
    return ((bool)(Mode == NormalProcessing));
}

void
SetProcessingMode(mode)
    ProcessingMode	mode;
{
    AssertArg(mode == NoProcessing || mode == BootstrapProcessing || mode == InitProcessing || mode == NormalProcessing);

    Mode = mode;
}

ProcessingMode
GetProcessingMode()
{
    return (Mode);
}

/* ----------------------------------------------------------------
 *	ReinitAtFirstTransaction()
 *	InitAtFirstTransaction()
 *
 *	This is obviously some half-finished hirohama-ism that does
 *	nothing and should be removed.
 * ----------------------------------------------------------------
 */
void
ReinitAtFirstTransaction()
{
    elog(FATAL, "ReinitAtFirstTransaction: not implemented, yet");
}

void
InitAtFirstTransaction()
{
    if (TransactionInitWasProcessed) {
	ReinitAtFirstTransaction();
    }
    
    /* Walk the relcache? */
    TransactionInitWasProcessed = true;	/* XXX ...InProgress also? */
}

/* ----------------------------------------------------------------
 *		database path / name support stuff
 * ----------------------------------------------------------------
 */
static String	DatabasePath = NULL;
static String	DatabaseName = NULL;

String
GetDatabasePath()
{
    return DatabasePath;
}

String
GetDatabaseName()
{
    return DatabaseName;
}

void
SetDatabasePath(path)
    String path;
{
    DatabasePath = path;
}

void
SetDatabaseName(name)
    String name;
{
    extern NameData MyDatabaseNameData;
    
    /* ----------------
     *	save the database name in MyDatabaseNameData.
     *  XXX we currently have MyDatabaseName, MyDatabaseNameData and
     *  DatabaseName.  What uses each of these?? this duality should
     *  be eliminated! -cim 10/5/90
     * ----------------
     */
    strncpy(&MyDatabaseNameData, name, 16);
    
    DatabaseName = (String) &MyDatabaseNameData;
}

/* ----------------------------------------------------------------
 *	GetUserId and SetUserId support (used to be in pusr.c)
 * ----------------------------------------------------------------
 */
static ObjectId	UserId = InvalidObjectId;

/* ----------------
 *	GetUserId
 * ----------------
 */
ObjectId
GetUserId()
{
    AssertState(ObjectIdIsValid(UserId));
    return (UserId);
}

/* ----------------
 *	SetUserId
 * ----------------
 */

void
SetUserId()
{
    HeapScanDesc s;
    ScanKeyData  key;
    Relation userRel;
    HeapTuple userTup;

    /*
     * Don't do scans if we're bootstrapping, none of the system
     * catalogs exist yet, and they should be owned by postgres
     * anyway.
     */
    if (IsBootstrapProcessingMode())
    {
	UserId = getuid();
	return;
    }

    userRel = heap_openr(UserRelationName);
    ScanKeyEntryInitialize(&key.data[0],
			   (bits16)0x0,
			   Anum_pg_user_usename,
			   Character16EqualRegProcedure,
			   (Datum)PG_username);

    s = heap_beginscan(userRel,0,NowTimeQual,1,(ScanKey)&key);
    userTup = heap_getnext(s, 0, (Buffer *)NULL);
    if (!HeapTupleIsValid(userTup))
    {
	elog(FATAL, "User %s is not in %s", PG_username, UserRelationName);
    }
    UserId = (ObjectId) ((Form_pg_user)GETSTRUCT(userTup))->usesysid;
    heap_endscan(s);
    heap_close(userRel);
}

/* ----------------
 *	pg_username
 * ----------------
 */
char *
pg_username()
{
	return(PG_username);
}

/* ----------------
 *	GetPGHome
 *
 *  Get POSTGRESHOME from environment, or return default.
 * ----------------
 */
char *
GetPGHome()
{
    char *h;

#ifdef USE_ENVIRONMENT
    if ((h = getenv("POSTGRESHOME")) != (char *) NULL)
	return (h);
#endif

    return (POSTGRESDIR);
}

char *
GetPGData()
{
    char *p;
    char *h;
    static char data[MAXPGPATH];

    if ((p = getenv("PGDATA")) != (char *) NULL) {
        return (p);
    }

    return (DATADIR);
}

/*
 * ValidBackend -- is "path" a valid POSTGRES executable file?
 */
ValidBackend(path)
	char	*path;
{
	struct stat	buf;
	char		*p;
	extern char	*rindex();
	
	/* Ensure that the file exists and is executable. */
	
	if (strlen(path) > MAXPGPATH ||
	    (stat(path, &buf) < 0) ||
	    !(buf.st_mode & S_IEXEC))
		return(FALSE);
	
	/* Ensure that we are using an authorized backend */
	
	/*
	 * XXX this is bogus, but it's better than nothing for now.
	 * you used to be able to run emacs by dinking the postmaster
	 * with the right packets.
	 */
	if (!(p = rindex(path, '/')) || strncmp(++p, "postgres", 8))
		return(FALSE);
	
	return(TRUE);
}

/*
 * FindBackend -- find an absolute path to a valid backend executable
 *
 * The reason we have to work so hard to find an absolute path is that
 * we need to feed the backend server the location of its actual 
 * executable file -- otherwise, we can't do dynamic loading.
 */
FindBackend(backend, argv0)
	char	*backend, *argv0;
{
	char		buf[MAXPATHLEN];
	char		*p;
	char		*path, *startp, *endp;
	int		pathlen;
	extern char	*index(), *rindex();

	/*
	 * for the postmaster:
	 * First try: use the backend that's located in the same directory
	 * as the postmaster, if it was invoked with an explicit path.
	 * Presumably the user used an explicit path because it wasn't in
	 * PATH, and we don't want to use incompatible executables.
	 *
	 * This has the neat property that it works for installed binaries,
	 * old source trees (obj/support/post{master,gres}) and new marc
	 * source trees (obj/post{master,gres}) because they all put the 
	 * two binaries in the same place.
	 *
	 * for the backend server:
	 * First try: if we're given some kind of path, use it (making sure
	 * that a relative path is made absolute before returning it).
	 */
	if (argv0 && (p = rindex(argv0, '/')) && *++p) {
		if (*argv0 == '/' || !getwd(buf))
			buf[0] = '\0';
		else
			(void) strcat(buf, "/");
		(void) strcat(buf, argv0);
		if (IsPostmaster) { /* change "postmaster" to "postgres" */
			p = rindex(buf, '/');
			(void) strcpy(++p, "postgres");
		}
		if (ValidBackend(buf)) {
			(void) strncpy(backend, buf, MAXPGPATH);
			if (!Quiet)
				fprintf(stderr, "FindBackend: found backend \"%s\" through argv[0]\n",
					backend);
			return(0);
		}
	}

	/*
	 * Second try: since no explicit path was supplied, the user must
	 * have been relying on PATH.  We'll use the same PATH.
	 */
	if ((p = getenv("PATH")) && *p) {
		pathlen = strlen(p);
		if (!(path = malloc(pathlen + 1)))
			elog(FATAL, "FindBackend: malloc failed");
		(void) strcpy(path, p);
		for (startp = path, endp = index(path, ':');
		     startp && *startp;
		     startp = endp + 1, endp = index(startp, ':')) {
			if (startp == endp)	/* it's a "::" */
				continue;
			if (endp)
				*endp = '\0';
			(void) strcpy(buf, startp);
			(void) strcat(buf, "/postgres");
			if (ValidBackend(buf)) {
				(void) strncpy(backend, buf, MAXPGPATH);
				free(path);
				if (!Quiet)
					fprintf(stderr, "FindBackend: found backend \"%s\" through PATH\n",
						backend);
				return(0);
			}
			if (!endp)		/* last one */
				break;
		}
		free(path);
	}
	
#ifdef USE_ENVIRONMENT
	/*
	 * Desperation time: try the ol' POSTGRESHOME hack.
	 */
	if (p = getenv("POSTGRESHOME")) {
		(void) strcpy(buf, p);
		(void) strcat(buf, "/bin/postgres");
		if (ValidBackend(buf)) {
			(void) strncpy(backend, buf, MAXPGPATH);
			if (!Quiet)
				fprintf(stderr, "FindBackend: found backend \"%s\" through POSTGRESHOME\n",
					backend);
			return(0);
		}
	}
#endif /* USE_ENVIRONMENT */

	fprintf(stderr, "FindBackend: could not find a backend to execute...\n");
	return(-1);
}
