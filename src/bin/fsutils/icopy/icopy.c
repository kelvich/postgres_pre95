/*
 *  icopy -- Inversion file system copy program
 *
 *	Icopy moves files between the Unix file system and Inversion.
 */

#include <sys/file.h>
#include <stdio.h>

#include "tmp/c.h"
#include "tmp/libpq-fe.h"
#include "tmp/libpq-fs.h"
#include "catalog/pg_lobj.h"

RcsId("$Header$");

#define	DIR_IN	0
#define	DIR_OUT	1
#define IBUFSIZ	8092

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif /* ndef TRUE */

char *ProgName;
extern char *PQhost;
extern char *PQport;

/* routines declared here */
extern char	*nextarg();
extern void	icopy_in();
extern void	icopy_out();
extern void	isetup();
extern void	ishutdown();
extern void	usage();

char *SmgrList[] = {
    "magnetic disk",
#ifdef SONY_JUKEBOX
    "sony jukebox",
#endif
#ifdef MAIN_MEMORY
    "main memory",
#endif
    (char *) NULL
};

main(argc, argv)
    int argc;
    char **argv;
{
    int dir;
    char *dbname;
    char *smgr;
    int smgrno;
    char *host, *port;
    char *srcfname, *destfname;
    char *curarg;

    host = port = dbname = smgr = srcfname = destfname = (char *) NULL;
    ProgName = *argv;

    if (--argc == 0)
	usage();

    /* copy direction */
    if (strcmp(*++argv, "in") == 0)
	dir = DIR_IN;
    else if (strcmp(*argv, "out") == 0)
	dir = DIR_OUT;
    else
	usage();

    while (--argc) {
	++argv;
	if (**argv == '-') {
	    curarg = *argv;
	    switch (*++(*argv)) {
	      case 'd':
		dbname = nextarg(&argc, &argv);
		break;

	      case 'h':
		host = nextarg(&argc, &argv);
		break;

	      case 'p':
		port = nextarg(&argc, &argv);
		break;

	      case 's':
		smgr = nextarg(&argc, &argv);
		break;

	      /*
	       *  This case handles "-" as a filename (meaning stdout
	       *  or stdin).
	       */
	      case '\0':
		if (srcfname == (char *) NULL)
		    srcfname = curarg;
		else if (destfname = (char *) NULL)
		    destfname = curarg;
		else
		    usage();
		break;

	      default:
		usage();
	    }
	} else if (srcfname == (char *) NULL)
	    srcfname = *argv;
	else if (destfname == (char *) NULL)
	    destfname = *argv;
	else
	    usage();
    }

    if (srcfname == (char *) NULL || destfname == (char *) NULL)
	usage();

    if (host == (char *) NULL || port == (char *) NULL)
	usage();

    if (dbname == (char *) NULL)
	usage();

    if (dir == DIR_IN &&
	(smgr == (char *) NULL || (smgrno = smgrlookup(smgr)) < 0))
	usage();

    isetup(host, port, dbname);

    if (dir == DIR_IN)
	icopy_in(srcfname, destfname, smgrno);
    else
	icopy_out(srcfname, destfname, smgrno);

    ishutdown();

    exit (0);
}

void
icopy_in(srcfname, destfname, smgrno)
    char *srcfname;
    char *destfname;
    int smgrno;
{
    int srcfd, destfd;
    char *buf, *lbuf;
    int nread, nwrite, totread, nbytes;
    int done;

    /* copy in cannot go to stdout */
    if (strcmp(destfname, "-") == 0)
	usage();

    if (strcmp(srcfname, "-") == 0)
	srcfd = fileno(stdin);
    else if ((srcfd = open(srcfname, O_RDONLY, 0600)) < 0) {
	perror(ProgName);
	exit (1);
    }

    if ((destfd = p_creat(destfname, INV_WRITE|smgrno, Inversion)) < 0) {
	fprintf(stderr, "Cannot create Inversion file %s\n", destfname);
	fflush(stderr);
	exit (1);
    }

    if ((buf = (char *) malloc(IBUFSIZ)) == (char *) NULL) {
	fprintf(stderr, "cannot allocate %d bytes for copy buffer\n", IBUFSIZ);
	fflush(stderr);
	exit (1);
    }

    done = FALSE;
    while (!done) {
	lbuf = buf;
	totread = 0;
	nbytes = IBUFSIZ;

	/* read one inversion file system buffer's worth of data */
	while (nbytes > 0) {
	    if ((nread = read(srcfd, lbuf, nbytes)) < 0) {
		perror(ProgName);
		exit (1);
	    } else if (nread == 0) {
		nbytes = 0;
		done = TRUE;
	    } else {
		nbytes -= nread;
		totread += nread;
		lbuf += nread;
	    }
	}

	/* write it out to the inversion file */
	lbuf = buf;
	while (totread > 0) {
	    if ((nwrite = p_write(destfd, lbuf, totread)) <= 0) {
		fprintf(stderr, "%s: write failed to Inversion file %s\n",
			ProgName, destfname);
		fflush(stderr);
		exit (1);
	    }
	    totread -= nwrite;
	    lbuf += nwrite;
	}
    }

    (void) close(srcfd);
    (void) p_close(destfd);
}

void
icopy_out(srcfname, destfname, smgrno)
    char *srcfname;
    char *destfname;
    int smgrno;
{
    int srcfd, destfd;
    char *buf, *lbuf;
    int nread, nwrite, totread, nbytes;
    int done;

    /* copy out cannot come from stdin */
    if (strcmp(srcfname, "-") == 0)
	usage();

    if ((srcfd = p_open(srcfname, INV_READ)) < 0) {
	fprintf(stderr, "%s: cannot open Inversion file %s\n",
		ProgName, srcfname);
	fflush(stderr);
	exit (1);
    }

    if (strcmp(destfname, "-") == 0)
	destfd = fileno(stdout);
    else if ((destfd = open(destfname, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0) {
	perror(ProgName);
	exit (1);
    }

    if ((buf = (char *) malloc(IBUFSIZ)) == (char *) NULL) {
	fprintf(stderr, "cannot allocate %d bytes for copy buffer\n", IBUFSIZ);
	fflush(stderr);
	exit (1);
    }

    done = FALSE;
    while (!done) {
	lbuf = buf;
	totread = 0;
	nbytes = IBUFSIZ;

	/* read one inversion file system buffer's worth of data */
	while (nbytes > 0) {
	    if ((nread = p_read(srcfd, lbuf, nbytes)) < 0) {
		fprintf(stderr, "%s: read failed from inversion file %s\n",
			ProgName, srcfname);
		fflush(stderr);
		exit (1);
	    } else if (nread == 0) {
		nbytes = 0;
		done = TRUE;
	    } else {
		nbytes -= nread;
		totread += nread;
		lbuf += nread;
	    }
	}

	/* write it out to the unix file */
	lbuf = buf;
	while (totread > 0) {
	    if ((nwrite = write(destfd, lbuf, totread)) <= 0) {
		perror(ProgName);
		exit (1);
	    }
	    totread -= nwrite;
	    lbuf += nwrite;
	}
    }

    (void) p_close(srcfd);
    (void) close(destfd);
}

void
isetup(host, port, dbname)
    char *host;
    char *port;
    char *dbname;
{
    char *res;

    PQport = port;
    PQhost = host;
    PQsetdb(dbname);

    res = PQexec("begin");
    if (*res == 'E') {
	fprintf(stderr, "%s: begin xact failed: %s\n", ProgName, *++res);
	fflush(stderr);
	exit (1);
    }
}

void
ishutdown()
{
    char *res;

    res = PQexec("end");
    if (*res == 'E') {
	fprintf(stderr, "%s: copy failed at commit: %s\n", ProgName, *++res);
	fflush(stderr);
	exit (1);
    }
}
/*
 *  nextarg() -- getopt()-style routine to get next arg.
 *
 *	This routine returns the string beginning after the option letter
 *	in the current argument, if any, or the next argument otherwise.
 *	Argc and argv are updated as appropriate.
 */

char *
nextarg(argc_p, argv_p)
    int *argc_p;
    char ***argv_p;
{
    if (*++(**argv_p) == '\0') {
	if (--(*argc_p) == 0)
	    usage();
	    /* NOTREACHED */
	else
	    return (*++(*argv_p));
    } else
	return (**argv_p);
}

/*
 *  smgrlookup() -- Look up a storage manager by name.
 *
 *	The offsets in the storage manager table compiled into this
 *	program are the same as those used by the backend.  We rely
 *	on this fact.
 */

int
smgrlookup(smgr)
    char *smgr;
{
    int i;

    for (i = 0; SmgrList[i] != (char *) NULL; i++)
	if (strcmp(smgr, SmgrList[i]) == 0)
	    return (i);

    return (-1);
}

void
usage()
{
    int i;

    fprintf(stderr, "usage: %s {in|out} ", ProgName);
    fprintf(stderr, "-h host -p port -d db -s smgr from_file to_file\n\n");

    fprintf(stderr, "    'in' copies to Inversion, and 'out' copies from Inversion.\n\n");

    fprintf(stderr, "    smgr may be:\n");
    for (i = 0; SmgrList[i] != (char *) NULL; i++)
	fprintf(stderr, "\t%s\n", SmgrList[i]);
    fprintf(stderr, "    smgr is ignored for %s out.\n", ProgName);

    fprintf(stderr, "\n    from_file or to_file may be '-', for stdin and stdout, respectively.\n");
    fprintf(stderr, "    stdin may not be used with %s out.\n", ProgName);
    fprintf(stderr, "    stdout may not be used with %s in.\n", ProgName);

    fflush(stderr);
    exit (1);
}
