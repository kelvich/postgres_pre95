#ifndef	_PSORT_H_
#define	_PSORT_H_	"$Header$"

#define	SORTMEM		(1 << 18)		/* 1/4 M - any static memory */
#define	MAXTAPES	7			/* 7--See Fig. 70, p273 */
#define	TAPEEXT		"pg_psort.XXXXXX"	/* TEMPDIR/TAPEEXT */
#define	FREE(x)		free((char *) x)

struct	tape {
	int		tp_dummy;		/* (D) */
	int		tp_fib;			/* (A) */
	FILE		*tp_file;		/* (TAPE) */
	struct	tape	*tp_prev;
};

struct	cmplist {
	int			cp_attn;	/* attribute number */
	int			cp_num;		/* comparison function code */
	int			cp_rev;		/* invert comparison flag */
	struct	cmplist		*cp_next;	/* next in chain */
};

extern	struct	cmplist		*CmpList;
extern	int			SortMemory;	/* free memory */

#ifdef	EBUG
#include <stdio.h>
#include "utils/log.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"

#define	PDEBUG(PROC, S1)\
elog(DEBUG, "%s:%d>> PROC: %s.", __FILE__, __LINE__, S1)

#define	PDEBUG2(PROC, S1, D1)\
elog(DEBUG, "%s:%d>> PROC: %s %d.", __FILE__, __LINE__, S1, D1)

#define	PDEBUG4(PROC, S1, D1, S2, D2)\
elog(DEBUG, "%s:%d>> PROC: %s %d, %s %d.", __FILE__, __LINE__, S1, D1, S2, D2)

#define	VDEBUG(VAR, FMT)\
elog(DEBUG, "%s:%d>> VAR =FMT", __FILE__, __LINE__, VAR)

#define	ASSERT(EXPR, STR)\
if (!(EXPR)) elog(FATAL, "%s:%d>> %s", __FILE__, __LINE__, STR)

#define	TRACE(VAL, CODE)\
if (1) CODE; else

#else
#define	PDEBUG(MSG)
#define	VDEBUG(VAR, FMT)
#define	ASSERT(EXPR, MSG)
#define	TRACE(VAL, CODE)
#endif

/* psort.c */
extern bool psort ARGS((Relation oldrel, Relation newrel, int nkeys, struct skey key[]));
extern int initpsort ARGS((void));
extern int resetpsort ARGS((void));
extern int initialrun ARGS((Relation rdesc));
extern bool createrun ARGS((HeapScanDesc sdesc, FILE *file));
extern HeapTuple tuplecopy ARGS((HeapTuple tup, Relation rdesc, Buffer b));
extern FILE *mergeruns ARGS((void));
extern int merge ARGS((struct tape *dest));
extern int endpsort ARGS((Relation rdesc, FILE *file));
extern FILE *gettape ARGS((void));
extern int resettape ARGS((FILE *file));
extern int destroytape ARGS((FILE *file));

#endif
