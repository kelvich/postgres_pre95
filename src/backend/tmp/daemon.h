/*
 * daemon.h --
 *	Declarations for using POSTGRES system daemons.
 *
 * Identification:
 *	$Header$
 */

#ifndef DaemonIncluded
#define DaemonIncluded

#include <signal.h>

#define	SIGKILLDAEMON1	SIGINT
#define	SIGKILLDAEMON2	SIGTERM


/*
 * DBNameStartVacuumDaemon --
 *	Start a daemon on the given database.
 * DBNameStopVacuumDaemon --
 *	Stop the daemon running on the given database.
 * DBNameCleanupVacuumDaemon --
 *	Clean up after a dead vacuum daemon.
 * DBNameGetVacuumDaemonProcessId --
 *	Find the pid of the daemon running on the given database, if any.
 */
extern
void
DBNameStartVacuumDaemon ARGS((
	char	*dbname
));

extern
void
DBNameStopVacuumDaemon ARGS((
	char	*dbname
));

extern
void
DBNameCleanupVacuumDaemon ARGS((
	char	*dbname
));

extern
short
DBNameGetVacuumDaemonProcessId ARGS((
	char	*dbname
));

#endif /* !DaemonIncluded */
