/* RcsId("$Header$"); */

#include <sys/file.h>
#include "tmp/c.h"

#define MAO_VLDB

#ifndef MAO_VLDB

int32
userfntest(i)
    int i;
{
    return (i);
}

#else /* MAO_VLDB */

#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "tmp/libpq-fs.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "storage/itemptr.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "utils/log.h"

extern int u_write();
extern int u_read();
extern int u_setup();

#define READSIZE	10000 * 1024	/* ten disk manufacturer megabytes */
#define FILENAME	"/mao_file"

#define NITERS_BIG	1
#define NITERS_RAND	1
#define NITERS_RANDLOC	1

#define BIG	0
#define SMALL	1

#define CALL(f, sz, msg) printf("rem %s", msg); f(fd, sz); printf("stat\n\n")

/* globals for usage stats */
struct rusage Save_r;
struct timeval Save_t;

int32
userfntest(i)
    int i;
{
    switch (i) {
	case 0:
	    return (u_setup());

	case 1:
	    return (u_write());

	case 2:
	    return (u_read());

	default:
	    elog(WARN, "userfntest groks zero and one");
    }
}

int
u_setup()
{
    File vfd;
    int fd;
    int tot, nbytes, nwritten;
    struct varlena *buf;

    if ((fd = LOcreat(FILENAME, INV_READ|INV_WRITE, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    CALL(bigsetup,      BIG,   "setup\n");

    LOclose(fd);

    return (0);
}

int
u_write()
{
    File vfd;
    int fd;
    int tot, nbytes, nwritten;
    struct varlena *buf;

    if ((fd = LOopen(FILENAME, INV_WRITE, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    CALL(bigwrite,      BIG,   "10 MByte sequential write\n");

    LOclose(fd);

    return (0);
}

int
u_read()
{
    int fd;

    if ((fd = LOopen(FILENAME, INV_READ, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    CALL(bigread,      BIG,   "10 MByte sequential read\n");

    LOclose(fd);

    return (0);
}

itest_stop_here(nbytes)
    int nbytes;
{
    nbytes = nbytes;
}

bigread(fd, small)
    int fd;
    int small;
{
    int nbytes;
    int nread;
    int want;
    int i;
    int iter;
    struct varlena *buf;

    for (i = 0; i < NITERS_BIG; i++) {

	/* start of file, please */
	if (LOlseek(fd, 0, L_SET) != (off_t) 0) {
	    elog(WARN, "bigread: cannot seek");
	}

	myResetUsage();
	want = READSIZE;
	if (small)
	    want /= 100;
	nread = 0;
	while (nread < want) {
	    nbytes = want - nread;
	    if (nbytes > 1024000)
		nbytes = 1024000;
	    buf = (struct varlena *) LOread(fd, nbytes);
	    nbytes = buf->vl_len;
	    pfree(buf);
	    if (nbytes < 0) {
		elog(WARN, "cannot read");
	    } else {
		nread += nbytes;
	    }
	}
	myShowUsage();
    }
}

bigwrite(fd, small)
    int fd;
    int small;
{
    char *sbuf;
    int nbytes;
    int nwritten;
    int i;
    int iter;
    int want;
    struct varlena *buf;

    buf = (struct varlena *) palloc(1024000);
    sbuf = (char *) &(buf->vl_dat[0]);
    for (i = 0; i < 1024000; i++)
	*sbuf++ = (char) (i % 255);
    for (i = 0; i < NITERS_BIG; i++) {

	/* start of file, please */
	if (LOlseek(fd, 0, L_SET) != (off_t) 0) {
	    elog(WARN, "bigread: cannot seek");
	}

	myResetUsage();
	want = READSIZE;
	if (small)
	    want /= 100;
	nwritten = 0;
	while (nwritten < want) {
	    nbytes = want - nwritten;
	    if (nbytes > 1024000)
		nbytes = 1024000;
	    buf->vl_len = nbytes;
	    nbytes = LOwrite(fd, buf);
	    if (nbytes < 0) {
		elog(WARN, "cannot read");
	    } else {
		nwritten += nbytes;
	    }
	}
	myShowUsage();
    }
    pfree(buf);
}

bigsetup(fd, small)
    int fd;
    int small;
{
    int nbytes;
    int nwritten;
    int i;
    int iter;
    int want;
    struct varlena *buf;

    buf = (struct varlena *) palloc(8096);
    bzero(&(buf->vl_dat[0]), 8092);

    myResetUsage();
    want = 5 * READSIZE;
    if (small)
	want /= 100;
    nwritten = 0;
    while (nwritten < want) {
	nbytes = want - nwritten;
	if (nbytes > 8092)
	    nbytes = 8092;
	buf->vl_len = nbytes + sizeof(int32);
	nbytes = LOwrite(fd, buf);
	if (nbytes < 0) {
	    elog(WARN, "cannot setup");
	} else {
	    nwritten += nbytes;
	}
    }
    myShowUsage();

    pfree(buf);
}


myResetUsage()
{
    struct timezone tz;
    getrusage(RUSAGE_SELF, &Save_r);
    gettimeofday(&Save_t, &tz);
}

myShowUsage()
{
    struct rusage r;
    struct timeval user, sys;
    struct timeval elapse_t;
    struct timezone tz;

    getrusage(RUSAGE_SELF, &r);
    gettimeofday(&elapse_t, &tz);
    bcopy(&r.ru_utime, &user, sizeof(user));
    bcopy(&r.ru_stime, &sys, sizeof(sys));
    if (elapse_t.tv_usec < Save_t.tv_usec) {
	elapse_t.tv_sec--;
	elapse_t.tv_usec += 1000000;
    }
    if (r.ru_utime.tv_usec < Save_r.ru_utime.tv_usec) {
	r.ru_utime.tv_sec--;
	r.ru_utime.tv_usec += 1000000;
    }
    if (r.ru_stime.tv_usec < Save_r.ru_stime.tv_usec) {
	r.ru_stime.tv_sec--;
	r.ru_stime.tv_usec += 1000000;
    }

    /* print stats in the format grokked by jbstats.awk */
    printf(
	"!\t%ld.%06ld %ld.%06ld %ld.%06ld\n",
	elapse_t.tv_sec - Save_t.tv_sec,
	elapse_t.tv_usec - Save_t.tv_usec,
	r.ru_utime.tv_sec - Save_r.ru_utime.tv_sec,
	r.ru_utime.tv_usec - Save_r.ru_utime.tv_usec,
	r.ru_stime.tv_sec - Save_r.ru_stime.tv_sec,
	r.ru_stime.tv_usec - Save_r.ru_stime.tv_usec);
    fflush(stdout);
}
#endif /* ndef MAO_VLDB */
