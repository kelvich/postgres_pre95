#ifndef	_POSTGRES_H_
#define	_POSTGRES_H_		"$Header$"

/*
 *	postgres.h	- Central and misc definitions for POSTGRES
 *
 *	This is bound to grow without bound.
 */

#ifndef C_H
#include "c.h"
#endif
#include "oid.h"

#ifndef NULL
#define	NULL	0L
#endif
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

#if defined(sun) || defined(sequent) || defined(mips)
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
