/*
 * elog.c -- 
 *	error logger
 */
#include <stdio.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/types.h>
#include <varargs.h>

#include "tmp/postgres.h"
#include "utils/log.h"
#include "installinfo.h"

RcsId("$Header$");


int	Debug_file = -1;
int	Err_file = -1;
int	ElogDebugIndentLevel;


void
EnableELog(enable)
int enable;
{
    static int numEnabled;
    static int fault; 

    Assert(fault == 0);

    if (!enable && --numEnabled)
	return;
    if (enable && numEnabled++)
	return;

    ++fault;
    if (enable) {
	/* XXX */
	;
    } else {
	/* XXX */
	;
    }
    --fault;
}


/*VARARGS2*/
void
elog(va_alist)
va_dcl
{
	va_list ap;
	register int	lev;
	char		*fmt;
	char		buf[ELOG_MAXLEN], line[ELOG_MAXLEN];
	register char	*bp, *cp;
	extern	int	errno, sys_nerr;
	extern	char	*sys_errlist[], *ctime(), *sprintf();
#ifndef PG_STANDALONE
	extern	FILE	*Pfout;
#endif /* !PG_STANDALONE */
	time_t		tim, time();
	int		len;
	int		i = 0;

	va_start(ap);
	lev = va_arg(ap, int);
	fmt = va_arg(ap, char *);
	if (lev == DEBUG && Debug_file < 0) {
		return;
	}
	switch (lev) {
	case NOIND:
		i = ElogDebugIndentLevel-1;
		if (i < 0) i = 0;
		if (i > 30) i = i%30;
		cp = "DEBUG:";
		break;
	case DEBUG:
		i = ElogDebugIndentLevel;
		if (i < 0) i = 0;
		if (i > 30) i = i%30;
		cp = "DEBUG:";
		break;
	case NOTICE:
		cp = "NOTICE:";
		break;
	case WARN:
		cp = "WARN:";
		break;
	default:
		sprintf(line, "FATAL %d:", lev);
		cp = line;
	}
	time(&tim);
	strcat(strcpy(buf, cp), ctime(&tim)+4);
	bp = buf+strlen(buf)-6;
	*bp++ = ':';
	while (i-- >0) *bp++ = ' ';
	for (cp = fmt; *cp; cp++)
		if (*cp == '%' && *(cp+1) == 'm') {
			if (errno < sys_nerr && errno >= 0)
				strcpy(bp, sys_errlist[errno]);
			else
				sprintf(bp, "error %d", errno);
			bp += strlen(bp);
			cp++;
		} else
			*bp++ = *cp;
	*bp = '\0';
	vsprintf(line, buf, ap);
	va_end(ap);
	len = strlen(strcat(line, "\n"));
	if (Debug_file > -1)
		write(Debug_file, line, len);
	if (lev == DEBUG || lev == NOIND)
		return;

#ifndef PG_STANDALONE
	/* Send IPC message to the postmaster */
	if (Pfout != NULL && lev > DEBUG) {
		/* notices are not exactly errors, handle it differently */
		if (lev == NOTICE) 
			pq_putnchar("N", 1);
		else
			pq_putnchar("E", 1);
		pq_putint(-101, 4);	/* should be query id */
		pq_putstr(line);
		pq_flush();
	}
#endif /* !PG_STANDALONE */

	if (Err_file > -1 && Debug_file != Err_file)
		if (write(Err_file, line, len) < 0) {
			write(open("/dev/console", O_WRONLY, 0666), line, len);
			fflush(stdout);
			fflush(stderr);
			exitpg(lev);
		}
	if (lev == WARN)
		kill(getpid(), 1);	/* abort to traffic cop */
	if (lev >= FATAL) {
		fflush(stdout);
		fflush(stderr);
		exitpg(lev);
	}
}


/*
 *	GetDataHome
 *
 *	Extract the installation home directory from the environment.
 */

extern char	*PostgresHomes[];
extern int	NStriping;

char *
GetDataHome()
{
	char		*home;
	char		*p, *p1;
	int		i;
	extern char	*getenv();

	home = getenv("POSTGRESHOME");
	if (!StringIsValid(home)) {
		home = getenv("POSTHOME");
	}
	if (!StringIsValid(home)) {
		home = DATAHOME;
	}
	if (!StringIsValid(home)) return NULL;
	p = home;
	i = 0;
	while ((p1 = index(p, ':')) != NULL) {
	   *p1 = '\0';
	   PostgresHomes[i++] = p;
	   p = p1 + 1;
	 }
	PostgresHomes[i++] = p;
	NStriping = i;
	return(home);
}


ErrorFileOpen()
{
	int		fd;
	char		buffer[MAXPGPATH];
	extern char	*GetDataHome();

	fd = fileno(stderr);
	if (fcntl(fd, F_GETFD, 0) < 0) {
		sprintf(buffer, "%s/data/pg.errors\0", GetDataHome());
/*
		elog(NOTICE, "ErrorFileOpen: opening %s", buffer);
*/
		fd = open(buffer, O_CREAT|O_APPEND|O_WRONLY, 0666);
		if (fd < 0)
			elog(FATAL, "ErrorFileOpen: open(%s) failed", buffer);
	}
/*
	elog(NOTICE, "ErrorFileOpen: opened %s, fd=%d", buffer, fd);
*/
	return(fd);
}


DebugFileOpen()
{
	int		fd;
	char		buffer[MAXPGPATH];
	extern char	*GetDataHome();

	fd = fileno(stderr);
	if (fcntl(fd, F_GETFD, 0) < 0) {
		sprintf(buffer, "%s/data/pg.debug\0", GetDataHome());
/*
		elog(NOTICE, "DebugFileOpen: opening %s", buffer);
*/
		fd = open(buffer, O_CREAT|O_APPEND|O_WRONLY, 0666);
		if (fd < 0)
			elog(FATAL, "DebugFileOpen: open(%s) failed", buffer);
	}
/*
	elog(NOTICE, "DebugFileOpen: opened %s, fd=%d", buffer, fd);
*/
	return(fd);
}
