static	char	magic_c[] = "$Header$";

#include <sys/file.h>
#include <strings.h>
#include "magic.h"
#include "postgres.h"
#include "context.h"
#include "log.h"

/*
 *	magic.c		- magic number management routines
 *
 *	Currently, only POSTGRES versioning is handled.  Eventually,
 *	routines to load the magic numbers should go here.
 *
 *	XXX eventually, should be able to handle version identifiers
 *	of length != 4.
 *
 *	Noversion is used as a global variable to disable checking.
 */

static	char	Pg_verfile[] = PG_VERFILE;
int	Noversion = 0;

/*
 *	ValidPgVersion	- verifies the consistency of the database
 *
 *	Currently, only checks for the existence.  Eventually,
 *	should check that the version numbers are proper.
 *
 *	Returns 1 iff the catalog version is consistent.
 */

int
ValidPgVersion(path)
char	path[];
{
	int	fd, len, retval;
	char	*cp, version[4], buf[MAXPGPATH];
	int	open(), read();
	extern	close(), bcopy();

	if ((len = strlen(path)) > MAXPGPATH - sizeof Pg_verfile - 1)
		elog(FATAL, "ValidPgVersion(%s): path too long");
	retval = 0;
	bcopy(path, buf, len);
	cp = buf + len;
	*cp++ = '/';
	bcopy(Pg_verfile, cp, sizeof Pg_verfile);
	if ((fd = open(buf, O_RDONLY, 0)) < 0) {
		if (!Noversion)
			elog(DEBUG, "ValidPgVersion: %s: %m", buf);
		return(0);
	}
	if (read(fd, version, 4) < 4 || version[1] != '.' || version[3] != '\n')
		elog(FATAL, "ValidPgVersion: %s: bad format", buf);
	if (version[2] != '0' + PG_VERSION || version[0] != '0' + PG_RELEASE) {
		if (!Noversion)
			elog(DEBUG, "ValidPgVersion: should be %d.%d not %c.%c",
				PG_RELEASE, PG_VERSION, version[0], version[2]);
		close(fd);
		return(0);
	}
	close(fd);
	return(1);
}

/*
 *	SetPgVersion	- writes the version to a database directory
 */

SetPgVersion(path)
char	path[];
{
	int	fd, len;
	char	*cp, version[4], buf[MAXPGPATH];
	int	open(), write();

	if ((len = strlen(path)) > MAXPGPATH - sizeof Pg_verfile - 1)
		elog(FATAL, "ValidPgVersion(%s): path too long");
	bcopy(path, buf, len);
	cp = buf + len;
	*cp++ = '/';
	bcopy(Pg_verfile, cp, sizeof Pg_verfile);
	if ((fd = open(buf, O_WRONLY|O_CREAT|O_EXCL, 0666)) < 0)
		elog(FATAL, "SetPgVersion: %s: %m", buf);
	version[0] = '0' + PG_RELEASE;
	version[1] = '.';
	version[2] = '0' + PG_VERSION;
	version[3] = '\n';
	if (write(fd, version, 4) != 4)
		elog(WARN, "SetPgVersion: %s: %m", buf);
	close(fd);
}
