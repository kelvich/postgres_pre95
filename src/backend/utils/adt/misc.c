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
    int small;

    if (i >= 100) {
	i -= 100;
	small = 1;
    } else {
	small = 0;
    }
    switch (i) {
	case 0:
	    return (u_setup(small));

	case 1:
	    return (u_read(small));

	case 2:
	    return (u_rndread(small));

	case 3:
	    return (u_locrndread(small));

	case 4:
	    return (u_write(small));

	case 5:
	    return (u_rndwrite(small));

	case 6:
	    return (u_locrndwrite(small));

	default:
	    elog(WARN, "userfntest doesn't grok %d", i);
    }
}

int
u_setup(small)
    int small;
{
    File vfd;
    int fd;
    int tot, nbytes, nwritten;
    struct varlena *buf;

    if ((fd = LOcreat(FILENAME, INV_READ|INV_WRITE, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    CALL(bigsetup,      small,   "setup\n");

    LOclose(fd);

    return (0);
}

int
u_write(small)
    int small;
{
    File vfd;
    int fd;
    int tot, nbytes, nwritten;
    struct varlena *buf;

    if ((fd = LOopen(FILENAME, INV_WRITE, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    if (small) {
	CALL(bigwrite,      small,   "100 KByte sequential write\n");
    } else {
	CALL(bigwrite,      small,   "10 MByte sequential write\n");
    }

    LOclose(fd);

    return (0);
}

int
u_read(small)
    int small;
{
    int fd;

    if ((fd = LOopen(FILENAME, INV_READ, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    if (small) {
	CALL(bigread,      small,   "100 KByte sequential read\n");
    } else {
	CALL(bigread,      small,   "10 MByte sequential read\n");
    }

    LOclose(fd);

    return (0);
}

int
u_rndread(small)
    int small;
{
    int fd;

    if ((fd = LOopen(FILENAME, INV_READ, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    if (small) {
	CALL(rndread,      small,   "10 KByte random read\n");
    } else {
	CALL(rndread,      small,   "1 MByte random read\n");
    }

    LOclose(fd);

    return (0);
}

int
u_locrndread(small)
    int small;
{
    int fd;

    if ((fd = LOopen(FILENAME, INV_READ, Inversion)) < 0) {
	elog(WARN, "benchmark: cannot open %s", FILENAME);
    }

    if (small) {
	CALL(locrndread,      small,   "10 KByte random read with locality\n");
    } else {
	CALL(locrndread,      small,   "1 MByte random read with locality\n");
    }

    LOclose(fd);

    return (0);
}

int
u_rndwrite(small)
{
    return (-1);
}

int
u_locrndwrite(small)
{
    return (-1);
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
	    if (nbytes > 8092)
		nbytes = 8092;
	    buf = (struct varlena *) LOread(fd, nbytes);
	    nbytes = buf->vl_len - sizeof(int32);
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

rndread(fd, small)
    int fd;
    int small;
{
    int nbytes;
    int nread;
    int want;
    int i;
    int iter;
    long off;
    struct varlena *buf;
    extern long random();

    srandom(getpid());

    for (i = 0; i < NITERS_BIG; i++) {

	myResetUsage();
	want = 1024 * 1000;
	if (small)
	    want /= 100;
	nread = 0;
	while (nread < want) {
	    off = random() % (49 * 1024 * 1000);
	    off = (off / 8092) * 8092;
	    if (LOlseek(fd, off, L_SET) != (off_t) off) {
		elog(WARN, "bigread: cannot seek");
	    }
	    nbytes = want - nread;
	    if (nbytes > 8092)
		nbytes = 8092;
	    buf = (struct varlena *) LOread(fd, nbytes);
	    nbytes = buf->vl_len - sizeof(int32);
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

locrndread(fd, small)
    int fd;
    int small;
{
    int nbytes;
    int nread;
    int want;
    int i;
    int iter;
    long off;
    struct varlena *buf;
    long pct;
    extern long random();

    srandom(getpid());

    for (i = 0; i < NITERS_BIG; i++) {

	myResetUsage();
	want = 1024 * 1000;
	if (small)
	    want /= 100;
	nread = 0;
	off = -1;
	while (nread < want) {
	    pct = random() % 10;
	    if (off < 0 || pct > 7) {
		off = random() % (40 * 1024 * 1000);
		off = (off / 8092) * 8092;
		if (LOlseek(fd, off, L_SET) != (off_t) off) {
		    elog(WARN, "bigread: cannot seek");
		}
	    }
	    nbytes = want - nread;
	    if (nbytes > 8092)
		nbytes = 8092;
	    buf = (struct varlena *) LOread(fd, nbytes);
	    nbytes = buf->vl_len - sizeof(int32);
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

    buf = (struct varlena *) palloc(8096);
    sbuf = (char *) &(buf->vl_dat[0]);
    for (i = 0; i < 8096; i++)
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
	    if (nbytes > 8092)
		nbytes = 8092;
	    buf->vl_len = nbytes + sizeof(int32);
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
