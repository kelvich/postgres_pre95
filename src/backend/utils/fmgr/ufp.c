/* ----------------------------------------------------------------
 *   FILE
 *	ufp.c
 *
 *   DESCRIPTION
 *	Implements both sides of the untrusted-function process (UFP)
 *	protocol.
 *
 *   NOTES
 *	Joe-code extraordinaire -- efficient and BSD-independent it
 *	ain't.  (See below for some enhancement ideas, though.)
 *	The whole thing is implemented here -- protocol, server side,
 *	UFP -- so changes are basically invisible to the rest of the
 *	system.  Isn't that convenient?
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <sys/signal.h>
#include "tmp/c.h"
#include "tmp/postgres.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/palloc.h"

RcsId("$Header$");

/*
#define	UFPDEBUG
#define	UFPDEBUG_PROTO
*/

/* ----------------------------------------------------------------
 * Dummy library routines for the UFP
 *	palloc
 *	pfree
 *	elog
 * ----------------------------------------------------------------
 */
#ifdef UFP
char *
palloc(n)
	Size n;
{
	return(malloc(n));
}

void
pfree(p)
	Pointer p;
{
	(void) free(p);
}

void
elog(n, s)
	int n;
	char *s;
{
	char *ns;

	switch (n) {
	case NOTICE:	ns = "NOTICE";	break;
	case DEBUG:	ns = "DEBUG";	break;
	case WARN:	ns = "WARN";	break;
	case FATAL:	ns = "FATAL";	break;
	default:	ns = "OTHER";	break;
	}
	fprintf(stderr, "ufp [%d]: %s: %s\n", getpid(), ns, s);
	if (n == WARN || n == FATAL)
		exit(1);
	return;
}
#endif /* UFP */

/* ----------------------------------------------------------------
 * Common routines between the UFP and the server.
 *	ufp_perror
 *	ufp_send
 *	ufp_recv
 *	ufp_sendval
 *	ufp_recvval
 *
 * XXX notice that we don't try to prevent blocking -- we don't have
 * anything better to do than wait for slow results, and the semantics
 * of the fd's we get from socketpair() is to return an error (or zero
 * bytes in the case of read()) if the other side is dead.
 * XXX we should try to minimize the number of system calls (which are
 * pretty damn expensive).
 * ----------------------------------------------------------------
 */

/* XXX This is here because we don't use the real elog() in the UFP.  Feh. */
static
char *
ufp_perror(msg)
	char *msg;
{
	static char errbuf[256], numbuf[40];
	extern errno, sys_nerr;
	extern char *sys_errlist[];

	strcpy(errbuf, msg ? msg : "ERROR");
	if (errno < 0 || errno >= sys_nerr) {
		(void) sprintf(numbuf, ": system error %d", errno);
		strcat(errbuf, numbuf);
	} else {
		strcat(errbuf, ": ");
		strcat(errbuf, sys_errlist[errno]);
	}
	return(errbuf);
}

static
ufp_send(fd, buf, buflen)
	int fd;
	char *buf;
	unsigned buflen;
{
	uint32 size;
	int sent = 0;
	void (*oldhandler)();

	if (fd < 0 || !buf) {
		elog(NOTICE, "ufp_send: invalid arguments");
		return(-1);
	}

#ifdef UFPDEBUG_PROTO
#ifdef UFP
	fprintf(stderr, "ufp [%d]: ufp_send: sending %d bytes\n",
		getpid(), buflen);
#else
	elog(DEBUG, "ufp_send: sending %d bytes", buflen);
#endif
#endif

	oldhandler = signal(SIGPIPE, SIG_IGN);
	size = buflen;
	if (write(fd, (char *) &size, sizeof(size)) != sizeof(size)) {
		elog(NOTICE, ufp_perror("ufp_send: write failed on length"));
		sent = -1;
	} else if ((sent = write(fd, buf, buflen)) != buflen) {
		elog(NOTICE, ufp_perror("ufp_send: write failed on data"));
		sent = -1;
	}
	(void) signal(SIGPIPE, oldhandler);
	
#ifdef UFPDEBUG_PROTO
#ifdef UFP
	fprintf(stderr, "ufp [%d]: ufp_send: actually sent %d bytes\n",
		getpid(), sent);
#else
	elog(DEBUG, "ufp_send: actually sent %d bytes", sent);
#endif
#endif
	return(sent);
}

static
ufp_recv(fd, bufptr)
	int fd;
	char **bufptr;
{
	int got;
	Datum size;
	static Datum statbuf;
#define	STATSIZE (sizeof(Datum))
	char *buf;

	if (fd < 0) {
		elog(NOTICE, "ufp_recv: invalid arguments");
		return(-1);
	}
	
	if (read(fd, (char *) &size, sizeof(size)) != sizeof(size)) {
		elog(NOTICE, ufp_perror("ufp_recv: read failed on length"));
		return(-1);
	}
#ifdef UFPDEBUG_PROTO
#ifdef UFP
	fprintf(stderr, "ufp [%d]: ufp_recv: trying for %d bytes\n",
		getpid(), size);
#else
	elog(DEBUG, "ufp_recv: trying for %d bytes", size);
#endif
#endif

	/* We assume that most objects that fit in a Datum are pass-by-value
	 * so we skip the palloc()
	 */
	buf = (size <= STATSIZE) ? (char *) &statbuf : palloc(size);
	if ((got = read(fd, buf, size)) != size) {
		elog(NOTICE, ufp_perror("ufp_recv: read failed on data"));
		return(-1);
	}
	*bufptr = buf;

#ifdef UFPDEBUG_PROTO
#ifdef UFP
	fprintf(stderr, "ufp [%d]: ufp_recv: actually got %d bytes\n",
		getpid(), size);
#else
	elog(DEBUG, "ufp_recv: actually got %d bytes", got);
#endif
#endif
	return(got);
}

static
ufp_sendval(fd, value, byvalue, length)
	int fd;
	Datum value;
	Datum byvalue;
	Datum length;
{
	char *vbuf;
	unsigned vbuflen;

	if (fd < 0) {
		elog(NOTICE, "ufp_sendval: invalid arguments");
		return(-1);
	}

	if (DatumGetInt32(byvalue)) {
		vbuflen = sizeof(Datum);
		vbuf = (char *) &value;
	} else {
		if (DatumGetInt32(length) < 0) {
			vbuflen = VARSIZE((struct varlena *)
					  DatumGetPointer(value));
		} else {
			vbuflen = length;
		}
		vbuf = (char *) DatumGetPointer(value);
	}

#ifdef UFPDEBUG_PROTO
#ifdef UFP
	fprintf(stderr, "ufp [%d]: ufp_sendval: sending [%d byval=%d len=%d]\n",
		getpid(), vbuf, byvalue, vbuflen);
#else
	elog(DEBUG, "ufp [%d]: ufp_sendval: sending [%d byval=%d len=%d]",
	     getpid(), vbuf, byvalue, vbuflen);
#endif
#endif
	if (ufp_send(fd, (char *) &byvalue, sizeof(byvalue)) !=
	    sizeof(byvalue)) {
		elog(NOTICE, "ufp_sendval: write failed on byvalue");
		return(-1);
	}
	if (ufp_send(fd, vbuf, vbuflen) != vbuflen) {
		elog(NOTICE, "ufp_sendval: write failed on data");
		return(-1);
	}
	return(0);
}

static
ufp_recvval(fd, value)
	int fd;
	Datum *value;
{
	int got;
	char *bufptr, *tmp;
	Datum byvalue;

	if (fd < 0) {
		elog(NOTICE, "ufp_recvval: invalid arguments");
		return(-1);
	}

	if (ufp_recv(fd, &bufptr) != sizeof(byvalue)) {
		elog(NOTICE, "ufp_recvval: read failed on byvalue");
		return(-1);
	}
	byvalue = *((uint32 *) bufptr);
	if ((got = ufp_recv(fd, &bufptr)) < 0) {
		elog(NOTICE, "ufp_recvval: read failed on data");
		return(-1);
	}
	if (DatumGetInt32(byvalue)) {
		*value = *((Datum *) bufptr);
	} else if (got <= STATSIZE) {
		tmp = (char *) palloc(got);
		if (!tmp) {
			elog(NOTICE, "ufp_recvval: palloc() failed");
			return(-1);
		}
		bcopy(bufptr, tmp, got);
		*value = PointerGetDatum(tmp);
	} else
		*value = PointerGetDatum(bufptr);
	return(0);
}

/* ----------------------------------------------------------------
 * Routines for the UFP
 *	ufp_quit
 *	ufp_rcvstr
 *	main
 *
 * XXX should coordinate with server's elog()s
 * ----------------------------------------------------------------
 */
#ifdef UFP
/* ----------------------------------------------------------------
 * The path to our image (for dynamic_loader.c).
 * ----------------------------------------------------------------
 */
char pg_pathname[256];

ufp_quit()
{
	elog(FATAL, "exiting...");
}

static
ufp_recvstr(fd, value)
	int fd;
	char **value;
{
	char *bufptr, *tmp;
	int got;

	if ((got = ufp_recv(fd, &bufptr)) < 0)
		return(-1);
	tmp = palloc(got + 1);
	if (!tmp)
		return(-1);
	bcopy(bufptr, tmp, got);
	tmp[got] = '\0';
	if (got > STATSIZE)
		pfree(bufptr);
	*value = tmp;
	return(0);
}

main(argc, argv)
	int argc;
	char **argv;
{
	int fd, got, i;
	char *filename, *funcname, *bufptr;
	Datum nargs, retbyval, retsize, retvalue;
	FmgrValues args;
	func_ptr user_fn;
	extern func_ptr handle_load();
	
	(void) signal(SIGCONT, ufp_quit);

	if (setuid(geteuid()) < 0)
		exit(1);

	strcpy(pg_pathname, argv[0]);
	fd = atoi(argv[1]);
	while (1) {
		if (ufp_recvstr(fd, &filename) < 0) {
			fprintf(stderr, "ufp [%d]: file name botch\n",
				getpid());
			ufp_quit();
		}
		if (ufp_recvstr(fd, &funcname) < 0) {
			fprintf(stderr, "ufp [%d]: function name botch\n",
				getpid());
			ufp_quit();
		}
		if (ufp_recv(fd, &bufptr) != sizeof(nargs)) {
			fprintf(stderr, "ufp [%d]: argument count botch\n",
				getpid());
			ufp_quit();
		}
		nargs = *((Datum *) bufptr);
		if (ufp_recv(fd, &retbyval) != sizeof(retbyval)) {
			fprintf(stderr, "ufp [%d]: return-by-value botch\n",
				getpid());
			ufp_quit();
		}
		retbyval = *((Datum *) bufptr);
		if (ufp_recv(fd, &retsize) != sizeof(retsize)) {
			fprintf(stderr, "ufp [%d]: return-value size botch\n",
				getpid());
			ufp_quit();
		}
		retsize = *((Datum *) bufptr);
#ifdef UFPDEBUG_PROTO
		fprintf(stderr, "ufp [%d]: main: got [%s %s %d %d %d]\n",
			getpid(), filename, funcname,
			nargs, retbyval, retsize);
#endif

		for (i = 0; i < nargs; ++i) {
			if (ufp_recvval(fd, &args.data[i]) < 0) {
				fprintf(stderr, "ufp [%d]: argument botch\n",
					getpid());
				ufp_quit();
			}
#ifdef UFPDEBUG_PROTO
			fprintf(stderr, "ufp [%d]: main: argument %d = %d\n",
				getpid(), i, args.data[i]);
#endif
		}
	
#ifdef UFPDEBUG
		fprintf(stderr, "ufp [%d]: started %s[%s]\n",
			getpid(), filename, funcname);
#endif
		user_fn = handle_load(filename, funcname);
#ifdef UFPDEBUG
		fprintf(stderr, "ufp [%d]: completed %s[%s]\n",
			getpid(), filename, funcname);
#endif
		if (!user_fn) {
			fprintf(stderr, "ufp [%d]: load of %s[%s] failed",
				getpid(), filename, funcname);
			ufp_quit();
		}
#ifdef UFPDEBUG
		fprintf(stderr, "ufp [%d]: invoking function...\n", getpid());
#endif
		retvalue = (Datum) (*user_fn)(args);

#ifdef UFPDEBUG
		fprintf(stderr, "ufp [%d]: sending return value %d\n",
			getpid(), retvalue);
#endif
		if (ufp_sendval(fd, retvalue, retbyval, retsize) < 0) {
			fprintf(stderr, "ufp [%d]: return botch\n",
				getpid());
			ufp_quit();
		}
		if (!DatumGetInt32(retbyval))
			pfree((char *) DatumGetPointer(retvalue));
#ifdef UFPDEBUG
		fprintf(stderr, "ufp [%d]: done!\n", getpid());
#endif
	}
}
#endif /* UFP */

/* ----------------------------------------------------------------
 * Routines for the server:
 *
 *	ufp_clear_info
 *	ufp_add_info
 *	ufp_find_info
 *	ufp_kill
 *	ufp_start
 *	ufp_build_info
 *	ufp_execute
 *	fmgr_ufp
 *
 * Anywhere the server/UFP protocol may get botched (e.g., almost anywhere
 * we do a non-debug elog(...)) we must perform a ufp_kill() to reset the
 * UFP.  (The current ufp_kill() actually terminates the UFP just to keep
 * things simple.)
 *
 * We should be careful when stashing catalog information for functions
 * since, theoretically, this information can change from xact to xact.
 * In reality, this information MUST NOT CHANGE once a function is loaded
 * since you can't "unload" or "reload" functions.  The only time it is
 * possible to invalidate stashed catalog information is when the UFP dies.
 * In fact, we should stash information for ALL of the functions in a file
 * when the file is loaded so that the information is consistent.
 *
 * XXX We do not currently keep this information consistent in any way.
 *     What will eventually happen is: we will search pg_proc for all
 *     functions registered in a new file and stash the struct ufp_info for
 *     each one at the same time so that they are consistent.  This will
 *     require keeping a DynamicFileList here as well as in the UFP.
 *     Once this is done, a LOAD command can be implemented -- on the first
 *     reference to a file, the server's struct ufp_info's are shared
 *     with the UFP (and we can stop sending file/function names and
 *     "byvalue" flags on each call!).  Subsequent function references can
 *     just send the function ObjectId and the arguments.
 * ----------------------------------------------------------------
 */
#ifndef UFP

#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/socket.h>
#include "access/heapam.h"
#include "access/htup.h"
#include "utils/rel.h"
#include "catalog/catname.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/syscache.h"
#include "storage/buf.h"

struct ufp_info {
	ObjectId	funcid;
	struct ufp_info	*next;
	Datum		nargs;
	Datum		byvals[MAXFMGRARGS];
	Datum		sizes[MAXFMGRARGS];
	Datum		retbyval;
	Datum		retsize;
	char		funcname[sizeof(NameData)+1];
	char		filename[256];
};

static struct ufp_info *UFPinfo = (struct ufp_info *) NULL;
static UFPid = -1;
static UFPfd = -1;

#define UFPNAME "pg_ufp"

static
void
ufp_clear_info()
{
	struct ufp_info *uip, *next_uip;
	
	for (uip = UFPinfo; uip != (struct ufp_info *) NULL; uip = next_uip) {
		next_uip = uip->next;
		pfree(uip);
	}
	UFPinfo = (struct ufp_info *) NULL;
	return;
}

static
void
ufp_add_info(uip)
	struct ufp_info *uip;
{
	uip->next = UFPinfo;
	UFPinfo = uip;
	return;
}

static
struct ufp_info *
ufp_find_info(func_id)
	ObjectId func_id;
{
	struct ufp_info *uip;

	for (uip = UFPinfo;
	     uip != (struct ufp_info *) NULL && uip->funcid != func_id;
	     uip = uip->next)
		;
	return(uip);
}

static
void
ufp_kill()
{
	int kstat, rstat = 0;

	ufp_clear_info();
	
	/* the child process has a different euid, which means we can't
	 * send it any signal except CONT... so if a user-function resets
	 * the UFP's CONT-handler (which commits harakiri), we may not
	 * actually kill the UFP immediately...
	 * XXX a clever thing to do would be to have each restarted UFP
	 *     perform fratricide on its older sibling after its own
	 *     setuid().
	 */
	if (UFPid > -1) {
		kstat = kill(UFPid, SIGCONT);
		if (kstat < 0 && errno != ESRCH)
			elog(NOTICE, ufp_perror("ufp_kill: kill() failed"));
	}
	UFPid = -1;
	
	/* it's not of much concern, though, since the old UFP will do
	 * a zero-length read() or SIGPIPE itself eventually...
	 */
	if (UFPfd > -1)
		(void) close(UFPfd);
	UFPfd = -1;

	elog(NOTICE, "ufp_kill: UFP cleaned up!");
	return;
}

static
ufp_start()
{
	int fd[2], cpid;
	char *post;
	char fds[20];
	static char pathbuf[256];
	static char *av[] = { (char *) 0, (char *) 0, (char *) 0 };
	extern char pg_pathname[];

	if (UFPid != -1 || UFPfd != -1) {
#ifdef UFPDEBUG
		elog(DEBUG, "ufp_start: clearing old UFP info...");
#endif
		ufp_kill();
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fd) < 0) {
		elog(NOTICE, ufp_perror("ufp_start: socketpair() failed"));
		return(-1);
	}
	if (fcntl(fd[0], F_SETFD, 1) < 0 || /* parent end - close on exec */
	    fcntl(fd[1], F_SETFD, 0) < 0) { /* child end - !close on exec */
		elog(NOTICE, ufp_perror("ufp_start: fcntl() failed"));
		(void) close(fd[0]);
		(void) close(fd[1]);
		return(-1);
	}
	if (!pg_pathname) {
		elog(NOTICE, "ufp_start: can't start UFP without pg_pathname");
		return(-1);
	}
	strcpy(pathbuf, pg_pathname);
	if (!(post = rindex(pathbuf, '/'))) {
		strcpy(pathbuf, UFPNAME);
	} else {
		strcpy(++post, UFPNAME);
	}
	av[0] = pathbuf;
	sprintf(fds, "%d", fd[1]);
	av[1] = fds;
	if ((cpid = vfork()) < 0) {
		elog(NOTICE, ufp_perror("ufp_start: vfork() failed"));
		(void) close(fd[0]);
		(void) close(fd[1]);
		return(-1);
	}
	if (cpid == 0) { /* child */
		if (execve(av[0], av, (char **) NULL) < 0)
			elog(NOTICE, ufp_perror("ufp_start: execve() failed"));
		/* if execve returns we're doomed */
		_exit(1);
	}
	close(fd[1]);
	UFPid = cpid;
	UFPfd = fd[0];

	elog(NOTICE, "ufp_start: forked UFP[%d]", cpid);
	return(0);
}

static
struct ufp_info *
ufp_build_info(func_id, func_htp)
	ObjectId func_id;
	HeapTuple func_htp;
{
	Form_pg_proc fp;
	Relation rdp;
	text *probin;
	unsigned probinlen;
	bool isnull;
	HeapTuple type_htp;
	Form_pg_type tp;
	struct ufp_info *uip;
	int i;

	uip = (struct ufp_info *) palloc(sizeof(struct ufp_info));
	uip->next = (struct ufp_info *) NULL;
	
	uip->funcid = func_id;
	
	fp = ((Form_pg_proc) GETSTRUCT(func_htp));
	(void) bcopy(fp->proname.data, uip->funcname, sizeof(NameData));
	uip->funcname[sizeof(NameData)] = '\0';
	uip->nargs = fp->pronargs;
	
	rdp = heap_openr(ProcedureRelationName->data);
	probin = (text *) heap_getattr(func_htp, InvalidBuffer,
				       Anum_pg_proc_probin,
				       &rdp->rd_att, &isnull);
	heap_close(rdp);
	if (!probin || isnull == true) {
		ufp_kill();
		elog(WARN, "ufp_build_info: null probin for function %d",
		     func_id);
	}
	probinlen = VARSIZE(probin) - sizeof(VARSIZE(probin));
	(void) bcopy(VARDATA(probin), uip->filename, probinlen);
	uip->filename[probinlen] = '\0';
	
	for (i = 0; i < uip->nargs; ++i) {
		type_htp = SearchSysCacheTuple(TYPOID, fp->proargtypes.data[i],
					       NULL, NULL, NULL);
		if (!HeapTupleIsValid(type_htp)) {
			ufp_kill();
			elog(WARN, "ufp_build_info: bad argtype %d for %d",
			     fp->proargtypes.data[i], func_id);
		}
		tp = (Form_pg_type) GETSTRUCT(type_htp);
		uip->byvals[i] = tp->typbyval;
		uip->sizes[i] = tp->typlen;
	}
	
	type_htp = SearchSysCacheTuple(TYPOID, fp->prorettype,
				       NULL, NULL, NULL);
	if (!HeapTupleIsValid(type_htp)) {
		ufp_kill();
		elog(WARN, "ufp_build_info: bad rettype %d for %d",
		     fp->prorettype, func_id);
	}
	tp = (Form_pg_type) GETSTRUCT(type_htp);
	uip->retbyval = tp->typbyval;
	uip->retsize = tp->typlen;

	return(uip);
}

static
Datum
ufp_execute(uip, values)
	struct ufp_info *uip;
	FmgrValues *values;
{
	int filelen, funclen, got, i;
	Datum retvalue, retbyval, retsize;
	
	if (!uip) {
		ufp_kill();
		elog(WARN, "ufp_execute: null argument");
	}
	if (uip->nargs < 0) {
		ufp_kill();
		elog(WARN, "ufp_execute: # of arguments must be >= 0");
	}

#ifdef UFPDEBUG_PROTO
	elog(DEBUG, "ufp_execute: sending [%s %s nargs=%d retbyval=%d retsize=%d]",
	     uip->filename, uip->funcname,
	     uip->nargs, uip->retbyval, uip->retsize);
#endif

	filelen = strlen(uip->filename) * sizeof(char);
	funclen = strlen(uip->funcname) * sizeof(char);
	if (ufp_send(UFPfd, uip->filename, filelen) != filelen) {
		ufp_kill();
		elog(WARN, "ufp_execute: error sending filename \"%s\"",
		     uip->filename);
	}
	if (ufp_send(UFPfd, uip->funcname, funclen) != funclen) {
		ufp_kill();
		elog(WARN, "ufp_execute: error sending function name \"%s\"",
		     uip->funcname);
	}
	if (ufp_send(UFPfd, &uip->nargs, sizeof(uip->nargs)) !=
	    sizeof(uip->nargs)) {
		ufp_kill();
		elog(WARN, "ufp_execute: error sending argument count");
	}
	if (ufp_send(UFPfd, &uip->retbyval, sizeof(uip->retbyval)) !=
	    sizeof(uip->retbyval)) {
		ufp_kill();
		elog(WARN, "ufp_execute: error sending return-by-value");
	}
	if (ufp_send(UFPfd, &uip->retsize, sizeof(uip->retsize)) !=
	    sizeof(uip->retsize)) {
		ufp_kill();
		elog(WARN, "ufp_execute: error sending return value size");
	}
	for (i = 0; i < uip->nargs; ++i) {
#ifdef UFPDEBUG_PROTO
		elog(DEBUG, "ufp_execute: argument %d = %d",
		     i, values->data[i]);
#endif
		if (ufp_sendval(UFPfd, values->data[i],
				uip->byvals[i], uip->sizes[i]) < 0) {
			ufp_kill();
			elog(WARN, "ufp_execute: error sending value");
		}
	}
	
	if (ufp_recvval(UFPfd, &retvalue) < 0) {
		ufp_kill();
		elog(WARN, "ufp_execute: error reading return value");
	}
#ifdef UFPDEBUG_PROTO
	elog(DEBUG, "ufp_execute: received %d", retvalue);
#endif
	return(retvalue);
}

Datum
fmgr_ufp(func_id, values)
	ObjectId func_id;
	Datum *values;
{
	struct ufp_info *uip;
	HeapTuple func_htp;

#ifdef UFPDEBUG
	elog(DEBUG, "fmgr_ufp: calling %d", func_id);
#endif
	func_htp = SearchSysCacheTuple(PROOID, func_id, NULL, NULL, NULL);
	if (!HeapTupleIsValid(func_htp)) {
		ufp_kill();
		elog(WARN, "fmgr_ufp: cache lookup for function %d failed",
		     func_id);
	}

	uip = ufp_find_info(func_id);
	if (uip == (struct ufp_info *) NULL)
		uip = ufp_build_info(func_id, func_htp);
	
	if (UFPid == -1 && ufp_start() < 0)
		elog(WARN, "fmgr_ufp: unable to (re)start UFP");
	
	return(ufp_execute(uip, values));
}
#endif /* !UFP */
