#ifndef	_PSORT_H_
#define	_PSORT_H_	"$Header$"

#define	SORTMEM		(1 << 18)		/* 1/4 M - any static memory */
#define	MAXTAPES	7			/* 7--See Fig. 70, p273 */
#define	TAPEEXT		"pg_psort.XXXXXX"	/* TEMPDIR/TAPEEXT */
#define	FREE(x)		free((char *) x)

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
bool psort ARGS((
	Relation oldrel,
	Relation newrel,
	int nkeys,
	struct skey key []
));
int initpsort ARGS((void ));
int resetpsort ARGS((void ));
HeapTuple tuplecopy ARGS((HeapTuple tup , Relation rdesc , Buffer b ));
int merge ARGS((struct tape *dest ));
int endpsort ARGS((Relation rdesc , FILE *file ));

#endif
