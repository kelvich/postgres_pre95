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

#include <grp.h>		/* for getgrgid */
#include <pwd.h>		/* for getpwuid */
#include <string.h>
#include <sys/param.h>		/* for MAXPATHLEN */
#include <sys/types.h>
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
#include "catalog/syscache.h"
/*
 * EnableAbortEnvVarName --
 *	Enables system abort iff set to a non-empty string in environment.
 */
#define EnableAbortEnvVarName	"POSTGRESABORT"

extern	char *getenv ARGS((const char *name));	/* XXX STDLIB */

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
    strncpy(&MyDatabaseNameData, name, sizeof(NameData));
    
    DatabaseName = (String) &MyDatabaseNameData;
}

/* ----------------
 *	GetUserName and SetUserName
 *
 *	SetUserName must be called before InitPostgres, since the setuid()
 *	is done there.
 * ----------------
 */
static NameData UserName = { "" };

void
GetUserName(namebuf)
	Name namebuf;
{
	Assert(UserName.data[0]);
	Assert(PointerIsValid((Pointer) namebuf));
	
	(void) strncpy(namebuf->data, UserName.data, sizeof(NameData));
}

void
SetUserName()
{
	char *p;
	struct passwd *pw;

	Assert(!UserName.data[0]);	/* only once */

	if (IsUnderPostmaster) {
		/* use the (possibly) authenticated name that's provided */
		if (!(p = getenv("PG_USER")))
			elog(FATAL, "SetUserName: PG_USER environment variable unset");
	} else {
		/* setuid() has not yet been done, see above comment */
		if (!(pw = getpwuid(getuid())))
			elog(FATAL, "SetUserName: no entry in passwd file");
		p = pw->pw_name;
	}
	(void) strncpy(UserName.data, p, sizeof(NameData));
	
	Assert(UserName.data[0]);
}

/* ----------------------------------------------------------------
 *	GetUserId and SetUserId
 * ----------------------------------------------------------------
 */
static ObjectId	UserId = InvalidObjectId;

ObjectId
GetUserId()
{
    Assert(ObjectIdIsValid(UserId));
    return(UserId);
}

void
SetUserId()
{
    HeapTuple	userTup;
    NameData	user;

    Assert(!ObjectIdIsValid(UserId));	/* only once */

    /*
     * Don't do scans if we're bootstrapping, none of the system
     * catalogs exist yet, and they should be owned by postgres
     * anyway.
     */
    if (IsBootstrapProcessingMode()) {
	UserId = getuid();
	return;
    }

    GetUserName(&user);
    userTup = SearchSysCacheTuple(USENAME, user.data, NULL, NULL, NULL);
    if (!HeapTupleIsValid(userTup))
	elog(FATAL, "SetUserId: user \"%-*s\" is not in \"%-*s\"",
	     sizeof(NameData), user.data, sizeof(NameData), UserRelationName);
    UserId = (ObjectId) ((Form_pg_user) GETSTRUCT(userTup))->usesysid;
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
 * ValidateBackend -- validate "path" as a POSTGRES executable file
 *
 * returns 0 if the file is found and no error is encountered.
 *	  -1 if the regular file "path" does not exist or cannot be executed.
 *	  -2 if the file is otherwise valid but cannot be read.
 */
ValidateBackend(path)
    char	*path;
{
    struct stat		buf;
    uid_t		euid;
    struct group	*gp;
    struct passwd	*pwp;
    int			i;
    int			is_r = 0;
    int			is_x = 0;
    int			in_grp = 0;
    
    /*
     * Ensure that the file exists and is a regular file.
     *
     * XXX if you have a broken system where stat() looks at the symlink
     *     instead of the underlying file, you lose.
     */
    if (strlen(path) >= MAXPGPATH) {
	if (DebugLvl > 1)
	    fprintf(stderr, "ValidateBackend: pathname \"%s\" is too long\n",
		    path);
	return(-1);
    }
    if (stat(path, &buf) < 0) {
	if (DebugLvl > 1)
	    fprintf(stderr, "ValidateBackend: can't stat \"%s\"\n",
		    path);
	return(-1);
    }
    if (!(buf.st_mode & S_IFREG)) {
	if (DebugLvl > 1)
	    fprintf(stderr, "ValidateBackend: \"%s\" is not a regular file\n",
		    path);
	return(-1);
    }
    
    /*
     * Ensure that we are using an authorized backend. 
     *
     * XXX I'm open to suggestions here.  I would like to enforce ownership
     *     of backends by user "postgres" but people seem to like to run
     *     as users other than "postgres"...
     */

    /*
     * Ensure that the file is both executable and readable (required for
     * dynamic loading).
     *
     * We use the effective uid here because the backend will not have
     * executed setuid() by the time it calls this routine.
     */
    euid = geteuid();
    if (euid == buf.st_uid) {
	is_r = buf.st_mode & S_IRUSR;
	is_x = buf.st_mode & S_IXUSR;
	if (DebugLvl > 1 && !(is_r && is_x))
	    fprintf(stderr, "ValidateBackend: \"%s\" is not user read/execute\n",
		    path);
	return(is_x ? (is_r ? 0 : -2) : -1);
    }
    if (pwp = getpwuid(euid)) {
	if (pwp->pw_gid == buf.st_gid) {
	    ++in_grp;
	} else if (pwp->pw_name &&
		   (gp = getgrgid(buf.st_gid))) {
	    for (i = 0; gp->gr_mem[i]; ++i) {
		if (!strcmp(gp->gr_mem[i], pwp->pw_name)) {
		    ++in_grp;
		    break;
		}
	    }
	}
	if (in_grp) {
	    is_r = buf.st_mode & S_IRGRP;
	    is_x = buf.st_mode & S_IXGRP;
	    if (DebugLvl > 1 && !(is_r && is_x))
		fprintf(stderr, "ValidateBackend: \"%s\" is not group read/execute\n",
			path);
	    return(is_x ? (is_r ? 0 : -2) : -1);
	}
    }
    is_r = buf.st_mode & S_IROTH;
    is_x = buf.st_mode & S_IXOTH;
    if (DebugLvl > 1 && !(is_r && is_x))
	fprintf(stderr, "ValidateBackend: \"%s\" is not other read/execute\n",
		path);
    return(is_x ? (is_r ? 0 : -2) : -1);
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
    char	buf[MAXPATHLEN];
    char	*p;
    char	*path, *startp, *endp;
    int		pathlen;

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
    if (argv0 && (p = strrchr(argv0, '/')) && *++p) {
	if (*argv0 == '/' || !getwd(buf))
	    buf[0] = '\0';
	else
	    (void) strcat(buf, "/");
	(void) strcat(buf, argv0);
	if (IsPostmaster) { /* change "postmaster" to "postgres" */
	    p = strrchr(buf, '/');
	    (void) strcpy(++p, "postgres");
	}
	if (!ValidateBackend(buf)) {
	    (void) strncpy(backend, buf, MAXPGPATH);
	    if (DebugLvl)
		fprintf(stderr, "FindBackend: found \"%s\" using argv[0]\n",
			backend);
	    return(0);
	}
	fprintf(stderr, "FindBackend: invalid backend \"%s\"\n",
		buf);
	return(-1);
    }
    
    /*
     * Second try: since no explicit path was supplied, the user must
     * have been relying on PATH.  We'll use the same PATH.
     */
    if ((p = getenv("PATH")) && *p) {
	if (DebugLvl)
	    fprintf(stderr, "FindBackend: searching PATH ...\n");
	pathlen = strlen(p);
	if (!(path = malloc(pathlen + 1)))
	    elog(FATAL, "FindBackend: malloc failed");
	(void) strcpy(path, p);
	for (startp = path, endp = strchr(path, ':');
	     startp && *startp;
	     startp = endp + 1, endp = strchr(startp, ':')) {
	    if (startp == endp)	/* it's a "::" */
		continue;
	    if (endp)
		*endp = '\0';
	    if (*startp == '/' || !getwd(buf))
		buf[0] = '\0';
	    (void) strcat(buf, startp);
	    (void) strcat(buf, "/postgres");
	    switch (ValidateBackend(buf)) {
	    case 0:		/* found ok */
		(void) strncpy(backend, buf, MAXPGPATH);
		if (DebugLvl)
		    fprintf(stderr, "FindBackend: found \"%s\" using PATH\n",
			    backend);
		free(path);
		return(0);
	    case -1:		/* wasn't even a candidate, keep looking */
		break;
	    case -2:		/* found but disqualified */
		fprintf(stderr, "FindBackend: could not read backend \"%s\"\n",
			buf);
		free(path);
		return(-1);
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
	if (!ValidateBackend(buf)) {
	    (void) strncpy(backend, buf, MAXPGPATH);
	    if (DebugLvl)
		fprintf(stderr, "FindBackend: found \"%s\" through POSTGRESHOME\n",
			backend);
	    return(0);
	}
    }
#endif /* USE_ENVIRONMENT */
    
    fprintf(stderr, "FindBackend: could not find a backend to execute...\n");
    return(-1);
}
