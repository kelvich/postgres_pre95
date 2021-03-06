/*
 * elog.c -- 
 *	error logger
 */
#include <stdio.h>
#include <strings.h>
#include <time.h>
#include <fcntl.h>
#ifndef O_RDONLY
#include <sys/file.h>
#endif /* O_RDONLY */
#include <sys/types.h>
#include <varargs.h>

#include "tmp/postgres.h"
#include "tmp/miscadmin.h"
#include "utils/log.h"
#include "installinfo.h"

RcsId("$Header$");


static int	Debugfile = -1;
static int	Err_file = -1;
static int	ElogDebugIndentLevel = 0;

extern char	OutputFileName[];

/*
 *  If the parser has detected an empty query from the front end, then
 *  there is a chance that the front end has shut down.  If this happens,
 *  and we try to send a bunch of elog messages over our connection to
 *  the front end, and we're running Ultrix 3.x, then fflush on the
 *  connection socket can cause us to die without a core dump.  Cool, yes?
 *  The global below is set in PostgresMain and used here to work around
 *  this operating system bug.
 */

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
	extern	char	*sys_errlist[];
#ifndef PG_STANDALONE
	extern	FILE	*Pfout;
#endif /* !PG_STANDALONE */
	time_t		tim, time();
	int		len;
	int		i = 0;

	va_start(ap);
	lev = va_arg(ap, int);
	fmt = va_arg(ap, char *);
	if (lev == DEBUG && Debugfile < 0) {
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
	if (Debugfile > -1)
		write(Debugfile, line, len);
	if (lev == DEBUG || lev == NOIND)
		return;

	/*
	 *  If there's an error log file other than our channel to the
	 *  front-end program, write to it first.  This is important
	 *  because there's a bug in the socket code on ultrix.  If the
	 *  front end has gone away (so the channel to it has been closed
	 *  at the other end), then writing here can cause this backend
	 *  to exit without warning -- that is, write() does an exit().
	 *  In this case, our only hope of finding out what's going on
	 *  is if Err_file was set to some disk log.  This is a major pain.
	 */

	if (Err_file > -1 && Debugfile != Err_file) {
		if (write(Err_file, line, len) < 0) {
			write(open("/dev/console", O_WRONLY, 0666), line, len);
			fflush(stdout);
			fflush(stderr);
			exitpg(lev);
		}
		fsync(Err_file);
	}

#ifndef PG_STANDALONE
	/* Send IPC message to the front-end program */
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

	if (lev == WARN) {
		ProcReleaseSpins(NULL);	/* get rid of spinlocks we hold */
		kill(getpid(), 1);	/* abort to traffic cop */
		pause();
		/*NOTREACHED*/
		/*
		 * The pause(3) is just to avoid race conditions where the
		 * thread of control on an MP system gets past here (i.e.,
		 * the signal is not received instantaneously).
		 */
	}

	if (lev == FATAL) {
		/*
		 * Assume that if we have detected the failure we can
		 * exit with a normal exit status.  This will prevent
		 * the postmaster from cleaning up when it's not needed.
		 */
		fflush(stdout);
		fflush(stderr);
		ProcReleaseSpins(NULL);	/* get rid of spinlocks we hold */
		ProcReleaseLocks();	/* get rid of real locks we hold */
		exitpg(0);
	}

	if (lev > FATAL) {
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
extern int	StripingMode;

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
	if (*p >= '0' && *p <= '9') {
	    /* specify raid level */
	    StripingMode = atoi(p);
	  }
	else {
	    PostgresHomes[i++] = p;
	  }
	NStriping = i;
	if (StripingMode == 1) {
	    if (NStriping % 2 != 0)
		elog(FATAL,"RAID Level 1 has to have an even number of disks.");
	    NStriping /= 2;
	  }
	else if (StripingMode < 0 || StripingMode > 5)
	    elog(FATAL, "We only have RAID Level 0 -- 5.");
	else if (StripingMode > 1 && StripingMode < 5)
	    elog(FATAL, "only RAID 0, 1 and 5 are supported.");
	return(home);
}


#ifndef PG_STANDALONE
DebugFileOpen()
{
	int fd, istty;

	Err_file = Debugfile = -1;
	ElogDebugIndentLevel = 0;

	if (OutputFileName[0]) {
		OutputFileName[MAXPGPATH-1] = '\0';
		if ((fd = open(OutputFileName, O_CREAT|O_APPEND|O_WRONLY,
			       0666)) < 0)
			elog(FATAL, "DebugFileOpen: open of %s: %m",
			     OutputFileName);
		istty = isatty(fd);
		(void) close(fd);
		/* If the file is a tty and we're running under the
		 * postmaster, try to send stdout there as well (if it
		 * isn't a tty then stderr will block out stdout, so we
		 * may as well let stdout go wherever it was going before).
		 */
		if (istty &&
		    IsUnderPostmaster &&
		    !freopen(OutputFileName, "a", stdout))
			elog(FATAL, "DebugFileOpen: %s reopen as stdout: %m",
			     OutputFileName);
		if (!freopen(OutputFileName, "a", stderr))
			elog(FATAL, "DebugFileOpen: %s reopen as stderr: %m",
			     OutputFileName);
		Err_file = Debugfile = fileno(stderr);
		return(Debugfile);
	}

	/* If no filename was specified, send debugging output to stderr.
	 * If stderr has been hosed, try to open a file.
	 */
	fd = fileno(stderr);
	if (fcntl(fd, F_GETFD, 0) < 0) {
		sprintf(OutputFileName, "%s/pg.errors.%d",
			GetPGData(), getpid());
		fd = open(OutputFileName, O_CREAT|O_APPEND|O_WRONLY, 0666);
	}

	if (fd < 0)
		elog(FATAL, "DebugFileOpen: could not open debugging file");

	Err_file = Debugfile = fd;
	return(Debugfile);
}
#endif
