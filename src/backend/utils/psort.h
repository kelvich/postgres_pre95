
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



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
#include <log.h>	
#include <stdio.h>
#include <buf.h>
#include <bufmgr.h>

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

#endif
