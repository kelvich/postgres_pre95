/*
 *  icopy -- Inversion file system copy program
 *
 *	Icopy moves files between the Unix file system and Inversion.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <stdio.h>

#include "tmp/c.h"
#include "tmp/libpq-fe.h"
#include "tmp/libpq-fs.h"
#include "catalog/pg_lobj.h"

RcsId("$Header$");

#define	DIR_IN		0
#define	DIR_OUT		1
#define IBUFSIZ		8092
#define	DEF_PORT	"4321"

#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif /* ndef TRUE */

typedef struct SLIST {
    char	s_id;
    char	*s_name;
} SLIST;

SLIST SmgrList[] = {
    { 'd',	"magnetic disk" },
#ifdef SONY_JUKEBOX
    { 's',	"sony jukebox" },
#endif
#ifdef MAIN_MEMORY
    { 'r',	"volatile RAM" },
#endif
    { '\0',	(char *) NULL }
};

typedef struct COPYLIST {
    char		*cl_src;
    char		*cl_dest;
    int			cl_isdir;
    struct COPYLIST	*cl_next;
} COPYLIST;

char *ProgName;
extern char *PQhost;
extern char *PQport;
int	CopyAll;		/* do dot files, too */
int	Recurse;		/* traverse directory tree */
int	Verbose;		/* report progress */

/* routines declared here */
extern char	*nextarg();
extern int	icopy_in();
extern int	icopy_out();
extern void	isetup();
extern void	ishutdown();
extern void	usage();
extern void	ibegin();
extern void	icommit();
extern int	docopy_in();
extern int	docopy_out();
extern COPYLIST *build_inlist();
extern COPYLIST *build_outlist();

/* routines declared elsewhere */
extern char	*getenv();

extern char *optarg;
extern int optind, opterr;

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
    int flag;
    int status;

    Verbose = CopyAll = Recurse = FALSE;
    host = port = dbname = smgr = srcfname = destfname = (char *) NULL;
    ProgName = *argv;
    smgrno = 0;

    if (--argc == 0)
	usage();

    /* copy direction */
    if (strcmp(*++argv, "in") == 0)
	dir = DIR_IN;
    else if (strcmp(*argv, "out") == 0)
	dir = DIR_OUT;
    else
	usage();

    while ((flag = getopt(argc, argv, "ad:h:p:Rs:v")) != EOF) {
	switch (flag) {
	  case 'a':
	    CopyAll = TRUE;
	    break;

	  case 'd':
	    dbname = optarg;
	    break;

	  case 'h':
	    host = optarg;
	    break;

	  case 'p':
	    port = optarg;
	    break;

	  case 'R':
	    Recurse = TRUE;
	    break;

	  case 's':
	    smgr = optarg;
	    break;

	  case 'v':
	    Verbose = TRUE;
	    break;

	  default:
	    usage();
	    break;
	}
    }

    if (argc - optind != 2)
	usage();

    srcfname = argv[optind];
    destfname = argv[optind + 1];

    if (host == (char *) NULL) {
	if ((host = getenv("PGHOST")) == (char *) NULL) {
	    fprintf(stderr, "no host specified and PGHOST undefined.\n");
	    usage();
	}
    }

    if (port == (char *) NULL) {
	if ((port = getenv("PGPORT")) == (char *) NULL)
	    port = DEF_PORT;
    }

    if (dbname == (char *) NULL)
	usage();

    if (dir == DIR_IN &&
	(smgr == (char *) NULL || (smgrno = smgrlookup(smgr)) < 0))
	usage();

    isetup(host, port, dbname);

    if (dir == DIR_IN)
	status = icopy_in(srcfname, destfname, smgrno);
    else
	status = icopy_out(srcfname, destfname, smgrno);

    ishutdown();

    exit (status);
}

int
icopy_in(srcfname, destfname, smgrno)
    char *srcfname;
    char *destfname;
    int smgrno;
{
    char *tail;
    char *newdest;
    int src_is_dir, dest_is_dir;
    COPYLIST *cplist;
    int errs;
    struct stat stbuf;
    struct pgstat pgstbuf;

    errs = 0;

    /* eliminate trailing slashes */
    tail = &srcfname[strlen(srcfname)-1];
    while (tail > srcfname && *tail == '/')
	*tail-- = '\0';

    tail = &destfname[strlen(destfname)-1];
    while (tail > destfname && *tail == '/')
	*tail-- = '\0';

    /* start a transaction */
    ibegin();

    /* verify that the source exists */
    if (stat(srcfname, &stbuf) < 0) {
	fprintf(stderr, "%s: ", ProgName);
	perror(srcfname);
	exit (1);
    }

    if ((stbuf.st_mode & S_IFMT) == S_IFDIR)
	src_is_dir = TRUE;
    else
	src_is_dir = FALSE;

    /* icopy of directories only makes sense if -R is supplied */
    if (src_is_dir & !Recurse) {
	fprintf(stderr, "%s: '%s' is a directory; use pmkdir or %s -R\n",
		ProgName, srcfname, ProgName);
	fflush(stderr);
	exit (1);
    }

    /*
     *  If the destination exists, then it must be a directory; we'll
     *  copy the source into that directory.
     */

    dest_is_dir = FALSE;
    if (p_stat(destfname, &pgstbuf) >= 0) {
	if (!((pgstbuf.st_mode & S_IFMT) == S_IFDIR)) {
	    fprintf(stderr, "%s: file '%s' already exists\n",
		    ProgName, destfname);
	    fflush(stderr);
	    exit (1);
	} else
	    dest_is_dir = TRUE;
    }

    if (!dest_is_dir) {
	/* the directory containing the destination must exist */
	tail = rindex(destfname, '/');
	if (tail == (char *) NULL) {
	    fprintf(stderr, "%s: '%s' is not a fully-qualified pathname\n",
		    ProgName, destfname);
	    fflush(stderr);
	    exit (1);
	}

	/* if the target isn't /, stat it */
	if (tail != destfname) {
	    *tail = '\0';
	    if (p_stat(destfname, &pgstbuf) < 0) {
		fprintf(stderr, "%s: %s: no such file or directory\n",
			ProgName, destfname);
		fflush(stderr);
		exit (1);
	    }
	    *tail = '/';
	}
    }

    /* done with verifying validity of arguments */
    icommit();

    /* set up destination file name correctly if supplied name is a dir */
    if (dest_is_dir) {
	tail = rindex(srcfname, '/');
	if (tail == (char *) NULL)
	    tail = srcfname;
	else
	    tail++;

	if (strcmp(destfname, "/") == 0) {
	    newdest = (char *) palloc(strlen(tail) + 2);
	    sprintf(newdest, "/%s", tail);
	} else {
	    newdest = palloc(strlen(destfname) + strlen(tail) + 2);
	    sprintf(newdest, "%s/%s", destfname, tail);
	}

	destfname = newdest;
    }

    cplist = build_inlist(srcfname, destfname);

    /* do it */
    while (cplist != (COPYLIST *) NULL) {
	if (cplist->cl_isdir) {
	    if (Verbose)
		printf("mkdir %s\n", cplist->cl_dest);
	    ibegin();
	    if (p_mkdir(cplist->cl_dest) < 0)
		errs++;
	    icommit();
	} else {
	    if (Verbose)
		printf("%s -> %s\n", cplist->cl_src, cplist->cl_dest);

	    if (docopy_in(cplist->cl_src, cplist->cl_dest, smgrno) < 0)
		errs++;
	}

	cplist = cplist->cl_next;
    }

    return (errs);
}

int
icopy_out(srcfname, destfname, smgrno)
    char *srcfname;
    char *destfname;
    int smgrno;
{
    char *tail;
    char *newdest;
    int src_is_dir, dest_is_dir;
    COPYLIST *cplist;
    int errs;
    struct stat stbuf;
    struct pgstat pgstbuf;

    errs = 0;

    /* eliminate trailing slashes */
    tail = &srcfname[strlen(srcfname)-1];
    while (tail > srcfname && *tail == '/')
	*tail-- = '\0';

    tail = &destfname[strlen(destfname)-1];
    while (tail > destfname && *tail == '/')
	*tail-- = '\0';

    /* start a transaction */
    ibegin();

    /* verify that the source exists */
    if (p_stat(srcfname, &pgstbuf) < 0) {
	fprintf(stderr, "%s: ", ProgName);
	perror(srcfname);
	exit (1);
    }

    if ((pgstbuf.st_mode & S_IFMT) == S_IFDIR)
	src_is_dir = TRUE;
    else
	src_is_dir = FALSE;

    /* icopy of directories only makes sense if -R is supplied */
    if (src_is_dir & !Recurse) {
	fprintf(stderr, "%s: '%s' is a directory; use pmkdir or %s -R\n",
		ProgName, srcfname, ProgName);
	fflush(stderr);
	exit (1);
    }

    /*
     *  If the destination exists, then it must be a directory; we'll
     *  copy the source into that directory.
     */

    dest_is_dir = FALSE;
    if (stat(destfname, &stbuf) >= 0) {
	if (!((stbuf.st_mode & S_IFMT) == S_IFDIR)) {
	    fprintf(stderr, "%s: file '%s' already exists\n",
		    ProgName, destfname);
	    fflush(stderr);
	    exit (1);
	} else
	    dest_is_dir = TRUE;
    }

    if (!dest_is_dir) {
	/* the directory containing the destination must exist */
	tail = rindex(destfname, '/');
	if (tail != (char *) NULL) {

	    /* if the target isn't /, stat it */
	    if (tail != destfname) {
		*tail = '\0';
		if (stat(destfname, &stbuf) < 0) {
		    fprintf(stderr, "%s: ", ProgName);
		    perror(destfname);
		    fflush(stderr);
		    exit (1);
		}
		*tail = '/';
	    }
	}
    }

    /* done with verifying validity of arguments */
    icommit();

    /* set up destination file name correctly if supplied name is a dir */
    if (dest_is_dir) {
	tail = rindex(srcfname, '/');
	if (tail == (char *) NULL)
	    tail = srcfname;
	else
	    tail++;

	if (strcmp(destfname, "/") == 0) {
	    newdest = (char *) palloc(strlen(tail) + 2);
	    sprintf(newdest, "/%s", tail);
	} else {
	    newdest = palloc(strlen(destfname) + strlen(tail) + 2);
	    sprintf(newdest, "%s/%s", destfname, tail);
	}

	destfname = newdest;
    }

    ibegin();
    cplist = build_outlist(srcfname, destfname);
    icommit();

    /* do it */
    while (cplist != (COPYLIST *) NULL) {
	if (cplist->cl_isdir) {
	    if (Verbose)
		printf("mkdir %s\n", cplist->cl_dest);
	    if (mkdir(cplist->cl_dest) < 0)
		errs++;
	} else {
	    if (Verbose)
		printf("%s -> %s\n", cplist->cl_src, cplist->cl_dest);

	    if (docopy_out(cplist->cl_src, cplist->cl_dest, smgrno) < 0)
		errs++;
	}

	cplist = cplist->cl_next;
    }

    return (errs);
}

COPYLIST *
build_inlist(srcfname, destfname)
    char *srcfname;
    char *destfname;
{
    COPYLIST *head, *tail;
    DIR *dirp;
    struct dirent *entry;
    char *newsrc, *newdest;
    struct stat stbuf;

    if (stat(srcfname, &stbuf) < 0) {
	fprintf(stderr, "%s: ", ProgName);
	perror(srcfname);
	exit (1);
    }

    /* don't copy symbolic links */
    if ((stbuf.st_mode & S_IFMT) == S_IFLNK) {
	fprintf(stderr, "%s: %s not copied (symbolic link)\n",
		ProgName, srcfname);
	fflush(stderr);
	return ((COPYLIST *) NULL);
    }

    head = (COPYLIST *) palloc(sizeof(COPYLIST));
    head->cl_src = srcfname;
    head->cl_dest = destfname;
    head->cl_next = (COPYLIST *) NULL;

    head->cl_isdir = ((stbuf.st_mode & S_IFMT) == S_IFDIR);
    if (head->cl_isdir && Recurse) {
	if ((dirp = opendir(srcfname)) == (DIR *) NULL) {
	    fprintf(stderr, "%s: ", ProgName);
	    perror(srcfname);
	    fflush(stderr);
	    return (head);
	}

	tail = head;

	while ((entry = readdir(dirp)) != (struct dirent *) NULL) {
	    if (entry->d_name[0] == '.' && !CopyAll)
		continue;
	    if (strcmp(entry->d_name, ".") == 0
		|| strcmp(entry->d_name, "..") == 0)
		continue;
	    newsrc = (char *) palloc(strlen(srcfname) + entry->d_namlen + 2);
	    sprintf(newsrc, "%s/%s", srcfname, entry->d_name);
	    newdest = (char *) palloc(strlen(destfname) + entry->d_namlen + 2);
	    sprintf(newdest, "%s/%s", destfname, entry->d_name);
	    tail->cl_next = build_inlist(newsrc, newdest);
	    while (tail->cl_next != (COPYLIST *) NULL)
		tail = tail->cl_next;
	}
	(void) closedir(dirp);
    }

    return (head);
}

COPYLIST *
build_outlist(srcfname, destfname)
    char *srcfname;
    char *destfname;
{
    COPYLIST *head, *tail;
    PDIR *dirp;
    struct pgdirent *entry;
    char *newsrc, *newdest;
    struct pgstat stbuf;

    if (p_stat(srcfname, &stbuf) < 0) {
	fprintf(stderr, "%s: ", ProgName);
	perror(srcfname);
	exit (1);
    }

    head = (COPYLIST *) palloc(sizeof(COPYLIST));
    head->cl_src = srcfname;
    head->cl_dest = destfname;
    head->cl_next = (COPYLIST *) NULL;

    head->cl_isdir = ((stbuf.st_mode & S_IFMT) == S_IFDIR);
    if (head->cl_isdir && Recurse) {
	if ((dirp = p_opendir(srcfname)) == (PDIR *) NULL) {
	    fprintf(stderr, "%s: ", ProgName);
	    perror(srcfname);
	    fflush(stderr);
	    return (head);
	}

	tail = head;

	while ((entry = p_readdir(dirp)) != (struct pgdirent *) NULL) {
	    if (entry->d_name[0] == '.' && !CopyAll)
		continue;
	    if (strcmp(entry->d_name, ".") == 0
		|| strcmp(entry->d_name, "..") == 0)
		continue;
	    newsrc = (char *) palloc(strlen(srcfname) + entry->d_namlen + 2);
	    sprintf(newsrc, "%s/%s", srcfname, entry->d_name);
	    newdest = (char *) palloc(strlen(destfname) + entry->d_namlen + 2);
	    sprintf(newdest, "%s/%s", destfname, entry->d_name);
	    tail->cl_next = build_outlist(newsrc, newdest);
	    while (tail->cl_next != (COPYLIST *) NULL)
		tail = tail->cl_next;
	}
	(void) p_closedir(dirp);
    }

    return (head);
}

int
docopy_in(srcfname, destfname, smgrno)
    char *srcfname;
    char *destfname;
    int smgrno;
{
    int srcfd, destfd;
    char *buf, *lbuf;
    int nread, nwrite, totread, nbytes;
    int done;

    if ((srcfd = open(srcfname, O_RDONLY, 0600)) < 0) {
	fprintf(stderr, "%s: ", ProgName);
	perror(srcfname);
	return (-1);
    }

    if ((destfd = p_creat(destfname, INV_WRITE|smgrno, Inversion)) < 0) {
	fprintf(stderr, "Cannot create Inversion file %s\n", destfname);
	fflush(stderr);
	return (-1);
    }

    if ((buf = (char *) malloc(IBUFSIZ)) == (char *) NULL) {
	fprintf(stderr, "cannot allocate %d bytes for copy buffer\n", IBUFSIZ);
	fflush(stderr);
	return (-1);
    }

    done = FALSE;
    while (!done) {
	lbuf = buf;
	totread = 0;
	nbytes = IBUFSIZ;

	/* read one inversion file system buffer's worth of data */
	while (nbytes > 0) {
	    if ((nread = read(srcfd, lbuf, nbytes)) < 0) {
		fprintf(stderr, "%s: ", ProgName);
		perror(read);
		return (-1);
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
		return (-1);
	    }
	    totread -= nwrite;
	    lbuf += nwrite;
	}
    }

    (void) close(srcfd);
    (void) p_close(destfd);

    return (0);
}

int
docopy_out(srcfname, destfname, smgrno)
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
	return (-1);
    }

    if ((destfd = open(destfname, O_WRONLY|O_CREAT|O_TRUNC, 0600)) < 0) {
	fprintf(stderr, "%s: ", ProgName);
	perror(destfname);
	return (-1);
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
		return (-1);
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
		fprintf(stderr, "%s: ", ProgName);
		perror(write);
		return (-1);
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
    PQport = port;
    PQhost = host;
    PQsetdb(dbname);
}

void
ibegin()
{
    char *res;

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
    PQfinish();
}

void
icommit()
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

    for (i = 0; SmgrList[i].s_id != '\0'; i++)
	if (*smgr == SmgrList[i].s_id)
	    return (i);

    return (-1);
}

void
usage()
{
    int i;

    fprintf(stderr, "usage: %s {in|out} ", ProgName);
    fprintf(stderr, "[-h host] [-p port] -d db -s smgr [-R] [-v] [-a] from to\n\n");

    fprintf(stderr, "    'in' copies to Inversion, and 'out' copies from Inversion.\n\n");

    fprintf(stderr, "    smgr may be:\n");
    for (i = 0; SmgrList[i].s_id != '\0'; i++)
	fprintf(stderr, "\t%c   (%s)\n", SmgrList[i].s_id, SmgrList[i].s_name);
    fprintf(stderr, "    smgr is not required for %s out, and will be ignored if it is supplied.\n", ProgName);

    fflush(stderr);
    exit (1);
}
