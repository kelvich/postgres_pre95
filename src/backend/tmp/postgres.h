
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



#ifndef	_POSTGRES_H_
#define	_POSTGRES_H_		"$Header$"

/*
 *	postgres.h	- Central and misc definitions for POSTGRES
 *
 *	This is bound to grow without bound.
 */

#include "c.h"
#include "oid.h"

#define	NULL	0
#define	SIGNBIT	(0x8000)	/* msb for int/unsigned */
#define	CSIGNBIT	(1 << 7)	/* msb for char */

/*
 * Postgres default basic datatypes:
 *
 * Boolean, u_int1, int2, u_int2, int4, u_int4
 */

/*
 * Boolean --
 *	temporary--represents a boolean value on disk and in memory
 */
#define	Boolean	char

/*
 * XXX These should disappear!!! -hirohama
 */
typedef	char	XID[5];
#define	OID	ObjectId
#define	CID	unsigned char
#define	ABSTIME	long
#define	RELTIME	long

#if defined(sun)
typedef	char	*DATUM;
#else
typedef	union {
	char	*da_cprt;
	short	*da_shprt;
	long	*da_lprt;
	char	**da_cpprt;
	struct	{
		char	s_c;
		double	s_d;
	}	*da_stptr;
	long	da_long;
}		DATUM;
#endif

#define	REGPROC	OID		/* for now */
#define	SPQUEL	OID		/* XXX (is this used?) */

#define	PSIZE(PTR)	(*((int32 *)(PTR) - 1))
#define	PSIZEALL(PTR)	(*((int32 *)(PTR) - 1) + sizeof (int32))
#define	PSIZESKIP(PTR)	((char *)((int32 *)(PTR) + 1))
#define	PSIZEFIND(PTR)	((char *)((int32 *)(PTR) - 1))
#define	PSIZESPACE(LEN)	((LEN) + sizeof (int32))

struct	varlena	{
	long	vl_len;
	char	vl_dat[1];
};

#define	VARSIZE(PTR)	(((struct varlena *)(PTR))->vl_len)
#define	VARDATA(PTR)    (((struct varlena *)(PTR))->vl_dat)

char	*palloc();
char	*calloc();
#define	ALLOC(t, c)	(t *)calloc((unsigned)(c), sizeof(t))

#define	MAXPGPATH	128

#endif
