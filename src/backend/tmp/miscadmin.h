/* ----------------------------------------------------------------
 *   FILE
 *	miscadmin.h
 *
 *   DESCRIPTION
 *	this file contains general postgres administration and
 *	initialization stuff that used to be spread out
 *	between the following files:
 *
 *	globals.h			global variables
 *	magic.h				PG_RELEASE, PG_VERSION, etc defines
 *	pdir.h				directory path crud
 *	pinit.h				postgres initialization
 *	pmod.h				processing modes
 *	pusr.h				postgres user permissions
 *	unix.h				host system port, etc.
 *	os.h				system port, etc.
 *
 *   NOTES
 *	some of the information in this file will be moved to
 *	other files.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef MiscadminHIncluded
#define MiscadminHIncluded 1

/* ----------------
 *	note: <sys/types.h> was in unix.h  This should be moved
 *	to the .c files.
 * ----------------
 */
#include <sys/types.h>

#include "tmp/postgres.h"
#include "storage/backendid.h"

/* ----------------
 *	globals.h
 * ----------------
 */
/*
 * globals.h --
 *
 */

#ifndef	GlobalsIncluded		/* Include this file only once */
#define	GlobalsIncluded	1

#include "storage/sinval.h"

/*
 * from main/main.c
 */
extern char		*DataDir;

/*
 * from tcop/postgres.c
 */
extern bool		override;
extern int		Quiet;

/*
 * from utils/init/globals.c
 */
extern int Portfd;
extern int Noversion;		/* moved from magic.c	*/

extern char	    OutputFileName[];

/*
 * done in storage/backendid.h for now.
 *
 * extern BackendId    MyBackendId;
 * extern BackendTag   MyBackendTag;
 */
extern NameData	    MyDatabaseNameData;
extern Name	    MyDatabaseName;
extern bool	    MyDatabaseIdIsInitialized;
extern ObjectId	    MyDatabaseId;
extern bool	    TransactionInitWasProcessed;

extern bool	    IsUnderPostmaster;
extern bool	    IsPostmaster;

extern short	    DebugLvl;

extern ObjectId	    LastOidProcessed;	/* for query rewrite */

#define MAX_PARSE_BUFFER 8192

/* 
 *	default number of buffers in buffer pool
 * 
 */
#define NDBUFS 64

#endif	/* !defined(GlobalsIncluded) */

/* ----------------
 *	magic.h
 * ----------------
 */
#ifndef	_MAGIC_H_
#define	_MAGIC_H_

/*
 *	magic.h		- definitions of the indexes of the magic numbers
 */

#define	PG_RELEASE	1
#define PG_VERSION	1
#define	PG_VERFILE	"PG_VERSION"

#endif

/* ----------------
 *	pdir.h
 * ----------------
 */
/*
 * pdir.h --
 *	POSTGRES directory path definitions.
 *
 * Identification:
 */

#ifndef	PDirIncluded		/* Include this file only once */
#define PDirIncluded	1

/*
 * GetDatabasePath --
 *	Returns path to database.
 *
 * Exceptions:
 *	BadState if called before InitDatabase.
 */
extern
String		/* XXX Path */
GetDatabasePath ARGS((
	void
));

extern
void
SetDatabasePath ARGS((
	String path
));

void
SetDatabaseName ARGS((String name));

/*
 * GetDatabaseName --
 *	Returns name of database.
 *
 * Exceptions:
 *	BadState if called before InitDatabase.
 */
extern
String		/* XXX Name */
GetDatabaseName ARGS((
	void
));

/*
 * DoChdirAndInitDatabaseNameAndPath --
 *	Sets current directory appropriately for given path and name.
 *
 * Arguments:
 *	Path and name are invalid if it invalid as a string.
 *	Path is "badly formated" if it is not a string containing a path
 *	to a writable directory.
 *	Name is "badly formated" if it contains more than 16 characters or if
 *	it is a bad file name (e.g., it contains a '/' or an 8-bit character).
 *
 * Side effects:
 *	Initially, DatabasePath and DatabaseName are invalid.  They are
 *	set to valid strings before this function returns.
 *
 * Exceptions:
 *	BadState if called more than once.
 *	BadArg if both path and name are "badly formated" or invalid.
 *	BadArg if path and name are both "inconsistent" and valid.
 */
extern
void
DoChdirAndInitDatabaseNameAndPath ARGS((
	String	name,
	String	path
));

#endif	/* !defined(PDirIncluded) */

/* ----------------
 *	pinit.h
 * ----------------
 */
/*
 * pinit.h --
 *	POSTGRES initialization and cleanup definitions.
 *
 * Note:
 *	XXX AddExitHandler not defined yet.
 */

#ifndef	PInitIncluded
#define PInitIncluded	1	/* Include this file only once */

typedef	int16	ExitStatus;

#define	NormalExitStatus	(0)
#define	FatalExitStatus		(127)
/* XXX are there any other meaningful exit codes? */

void
InitMyDatabaseId ARGS(());
void
InitCommunication ARGS(());
void
InitStdio ARGS(());

/*
 * InitPostgres --
 *	Initialize POSTGRES.
 *
 * Note:
 *	...
 *
 * Side effects:
 *	...
 *
 * Exceptions:
 *	none
 */
extern
void
InitPostgres ARGS((
	String	name
));

/*
 * ReinitPostgres --
 *	Resets POSTGRES to a "sane" state.
 *
 * Note:
 *	Some things like communication to a frontend or database cannot
 *	be reset.
 *	...
 *
 * Side effects:
 *	...
 *
 * Exceptions:
 *	none
 */
extern
void
ReinitPostgres ARGS((
	void
));

/*
 * ExitPostgres --
 *	Exit POSTGRES with a status code.
 *
 * Note:
 *	This function never returns.
 *	...
 *
 * Side effects:
 *	...
 *
 * Exceptions:
 *	none
 */
extern
void
ExitPostgres ARGS((
	ExitStatus	status
));

void StatusBackendExit ARGS((int status));
void StatusPostmasterExit ARGS((int status));

/*
 * AbortPostgres --
 *	Abort POSTGRES dumping core.
 *
 * Note:
 *	This function never returns.
 *	...
 *
 * Side effects:
 *	Core is dumped iff EnableAbortEnvVarName is set to a non-empty string.
 *	...
 *
 * Exceptions:
 *	none
 */
extern
void
AbortPostgres ARGS((
	void
));

#endif	/* !defined(PInitIncluded) */

/* ----------------
 *	pmod.h
 * ----------------
 */
/*
 * pmod.h --
 *	POSTGRES processing mode definitions.
 *
 * Description:
 *	There are four processing modes in POSTGRES.  They are NoProcessing
 * or "none," BootstrapProcessing or "bootstrap," InitProcessing or
 * "initialization," and NormalProcessing or "normal."
 *
 *	If a POSTGRES binary is in normal mode, then all code may be executed
 * normally.  In the none mode, only bookkeeping code may be called.  In
 * particular, access method calls may not occur in this mode since the
 * execution state is outside a transaction.
 *
 *	The final two processing modes are used during special times.  When the
 * system state indicates bootstrap processing, transactions are all given
 * transaction id "one" and are consequently guarenteed to commit.  This mode
 * is used during the initial generation of template databases.
 *
 * Finally, the execution state is in initialization mode until all normal
 * initialization is complete.  Some code behaves differently when executed in
 * this mode to enable system bootstrapping.
 */

#ifndef	PModIncluded		/* Include this file only once */
#define PModIncluded	1

/*
 * Identification:
 */

typedef enum ProcessingMode {
	NoProcessing,		/* "nothing" can be done */
	BootstrapProcessing,	/* bootstrap creation of template database */
	InitProcessing,		/* initializing system */
	NormalProcessing,	/* normal processing */
} ProcessingMode;

/*
 * IsNoProcessingMode --
 *	True iff processing mode is NoProcessing.
 */
extern
bool
IsNoProcessingMode ARGS((
	void
));

/*
 * IsBootstrapProcessingMode --
 *	True iff processing mode is BootstrapProcessing.
 */
extern
bool
IsBootstrapProcessingMode ARGS((
	void
));

/*
 * IsInitProcessingMode --
 *	True iff processing mode is InitProcessing.
 */
extern
bool
IsInitProcessingMode ARGS((
	void
));

/*
 * IsNormalProcessingMode --
 *	True iff processing mode is NormalProcessing.
 */
extern
bool
IsNormalProcessingMode ARGS((
	void
));

/*
 * SetProcessingMode --
 *	Sets mode of processing as specified.
 *
 * Exceptions:
 *	BadArg if called with invalid mode.
 *
 * Note:
 *	Mode is NoProcessing before the first time this is called.
 */
extern
void
SetProcessingMode ARGS((
	ProcessingMode	mode
));

extern ProcessingMode GetProcessingMode ARGS((void));

#endif	/* !defined(PModIncluded) */

extern char *GetPGHome ARGS((void));
extern char *GetPGData ARGS((void));

/* ----------------
 *	pusr.h
 * ----------------
 */
/*
 * pusr.h --
 *	POSTGRES user permissions definitions.
 */

#ifndef	PUsrIncluded		/* Include this file only once */
#define PUsrIncluded	1

/*
 * GetUserId --
 *	Returns user id.
 *
 * Exceptions:
 *	BadState if called before InitUser.
 */
extern
ObjectId
GetUserId ARGS((void));

extern
void
SetUserId ARGS((void));

/*
 * InitUser --
 *	Sets user permission information.
 *
 * Exceptions:
 *	BadState if called more than once.
 */
extern
void
InitUser ARGS((
	void
));

#endif	/* !defined(PUsrIncluded) */

/* ----------------
 *	unix.h
 * ----------------
 */
/*
 * unix.h --
 *	UNIX operating system definitions.
 *
 * Note:
 *	This file is for the Sun UNIX OPERATING SYSTEM 3.4 or 3.5 or 3.6!!!
 */

#ifndef	UNIXIncluded		/* Include this file only once */
#define UNIXIncluded	1


/*
 * HostSystemPort --
 *	Unbuffered system I/O port.
 */
typedef int	HostSystemPort;

/*
 * HostSystemFileMode --
 *	File mode.
 */
typedef int	HostSystemFileMode;

/*
 * HostSystemByteCount --
 *	Number of bytes--used in calls to read, write, etc.
 */
typedef int	HostSystemByteCount;

/*
 * HostSystemFilePosition --
 *	Number of bytes from beginning of file.
 */
typedef long	HostSystemFilePosition;

/*
 * HostSystemFileOffset --
 *	Number of offset bytes for lseek.
 */
typedef long	HostSystemFileOffset;

/*
 * HostSystemTime --
 *	Time in seconds from the UNIX epoch.
 */
typedef time_t	HostSystemTime;

/*
 * open --
 *	open(2)
 */
#if old_sun_no_longer_supported
extern
HostSystemPort
open ARGS((
	const String			path,
	const int			flags,
	const HostSystemFileMode	mode
));

/*
 * close --
 *	close(2)
 */
extern
void
close ARGS((
	const HostSystemPort	port
));

/*
 * read --
 *	read(2)
 *
 * Note:
 *	String may not be proper usage here.
 */
extern
HostSystemByteCount
read ARGS((
	const HostSystemPort		port,
	const String			buffer,
	const HostSystemByteCount	numberBytes
));

/*
 * write --
 *	write(2)
 *
 * Note:
 *	String may not be proper usage here.
 */
extern
HostSystemByteCount
write ARGS((
	const HostSystemPort		port,
	const String			buffer,
	const HostSystemByteCount	numberBytes
));

/*
 * lseek --
 *	lseek(2)
 */
extern
HostSystemFilePosition
lseek ARGS((
	const HostSystemPort		port,
	const HostSystemFileOffset	offset,
	const int			whence
));

/*
 * time --
 *	time(3)
 */
extern
HostSystemTime
time ARGS((
	HostSystemTime	*tloc
));
#endif

/*
 * These tests can be made more strict given a particular compiler.
 *
 * XXX Someday, this should go into the compiler port directory.
 */
#ifndef	__SABER__
#define PointerIsToDynamicData(pointer)	((bool)((char *)(pointer) >= end))
#define PointerIsToStaticData(pointer)	((bool)((char *)(pointer) < end))
#else
#define PointerIsToDynamicData(pointer)	(true)
#define PointerIsToStaticData(pointer)	(true)
#endif

#endif	/* !defined(UNIXIncluded) */

/* ----------------
 *	os.h
 * ----------------
 */
/*
 * os.h --
 *	Operating system definitions.
 *
 * Note:
 *	This file is OPERATING SYSTEM dependent!!!
 */

#ifndef	OSIncluded		/* Include this file only once */
#define OSIncluded	1

/*
 * SystemPort --
 *	Unbuffered system I/O port.
 */
typedef HostSystemPort	SystemPort;

/*
 * SystemFileMode --
 *	File mode.
 */
typedef HostSystemFileMode	SystemFileMode;

/*
 * SystemByteCount --
 *	Number of bytes--used in calls to read, write, etc.
 */
typedef HostSystemByteCount	SystemByteCount;

/*
 * SystemFilePosition --
 *	Number of bytes from beginning of file.
 */
typedef HostSystemFilePosition	SystemFilePosition;

/*
 * SystemFileOffset --
 *	Number of offset bytes for lseek.
 */
typedef HostSystemFileOffset	SystemFileOffset;

/*
 * SystemTime --
 *	Time in seconds.
 */
typedef HostSystemTime	SystemTime;

/*
 * open --
 *	open(2)
 */
extern
SystemPort
OpenSystemPortFile ARGS((
	const String		path,
	const int		flags,
	const SystemFileMode	mode
));

/*
 * close --
 *	close(2)
 */
extern
void
CloseSystemPort ARGS((
	const SystemPort	port
));

/*
 * read --
 *	read(2)
 *
 * Note:
 *	String may not be proper usage here.
 */
extern
SystemByteCount
SystemPortRead ARGS((
	const SystemPort	port,
	const String		buffer,
	const SystemByteCount	numberBytes
));

/*
 * write --
 *	write(2)
 *
 * Note:
 *	String may not be proper usage here.
 */
extern
SystemByteCount
SystemPortWrite ARGS((
	const SystemPort	port,
	const String		buffer,
	const SystemByteCount	numberBytes
));

/*
 * lseek --
 *	lseek(2)
 */
extern
SystemFilePosition
SystemPortSeek ARGS((
	const SystemPort	port,
	const SystemFileOffset	offset,
	const int		whence
));

/*
 * PointerIsToStaticData --
 * PointerIsToDynamicData --
 *	True iff pointer points to memory allocated at compile-time or
 *	run-time, respectively.
 *
 * Note:
 *	This is compiler and operating system dependent.
 *	Look in "unix.h", etc. for the appropriate definition.
 */

#endif	/* !defined(OSIncluded) */

/*
 * Prototypes for utils/init/magic.c
 */
int DatabaseMetaGunkIsConsistent ARGS((char database[], char path[]));
int ValidPgVersion ARGS((char path []));
int SetPgVersion ARGS((char path []));

/* ----------------
 *	end of miscadmin.h
 * ----------------
 */
#endif MiscadminHIncluded
