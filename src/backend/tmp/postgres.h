/* ----------------------------------------------------------------
 *   FILE
 *     	postgres.h
 *
 *   DESCRIPTION
 *     	definition of (and support for) postgres system types.
 *
 *   NOTES
 *	this file will eventually contain the definitions for the
 *	following (and perhaps other) system types:
 *
 *		bool		bytea		char		
 *		char16		dt		int2
 *		int28		int4		oid		
 *		oid8		regproc		text
 *
 *	Currently this file is also the home of struct varlena and
 *	some other stuff.  Some of this may change.  -cim 8/6/90
 *
 *   IDENTIFICATION
 *   	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PostgresHIncluded		/* include only once */
#define PostgresHIncluded

#include "tmp/c.h"

/* --------------------------------
 *	simple types
 * --------------------------------
 */
/* ----------------
 *	bool
 *
 *	this is defined in c.h at present, but that will change soon.
 *	Boolean should be obsoleted.
 * ----------------
 */
/* XXX put bool here */
#define Boolean bool

/* ----------------
 *	dt
 * ----------------
 */
typedef long	dt;

/* ----------------
 *	int2
 * ----------------
 */
typedef	int16	int2;

/* ----------------
 *	int4
 * ----------------
 */
typedef int32	int4;

/* ----------------
 *	float4
 * ----------------
 */
typedef float	float4;

/* ----------------
 *	float8
 * ----------------
 */
typedef double	float8;

/* ----------------
 *	oid
 *
 *	this is defined in oid.h at present, but that should change.
 * ----------------
 */
#include "tmp/oid.h"

/* ----------------
 *	regproc
 *
 *	regproc.h should be obsoleted.
 * ----------------
 */
typedef oid regproc;
#include "tmp/regproc.h"

/* --------------------------------
 *	array types
 * --------------------------------
 */
/* ----------------
 *	struct varlena
 * ----------------
 */
struct varlena {
	long	vl_len;
	char	vl_dat[1];
};

#define	VARSIZE(PTR)	(((struct varlena *)(PTR))->vl_len)
#define	VARDATA(PTR)    (((struct varlena *)(PTR))->vl_dat)

/* ----------------
 *	bytea
 * ----------------
 */
typedef struct varlena bytea;

/* ----------------
 *	char16
 * ----------------
 */
typedef struct char16 {
	char	data[16];
} char16;

typedef char16	*Char16;

/* ----------------
 *	int28
 * ----------------
 */
typedef struct int28 {
    int2	data[8];
} int28;

/* ----------------
 *	oid8
 * ----------------
 */
typedef struct oid8 {
    oid		data[8];
} oid8;
 
/* ----------------
 *	text
 * ----------------
 */
typedef struct varlena text;

/* ----------------
 *	stub
 *
 *	this is a new system type used by the rule manager.
 * ----------------
 */
typedef struct varlena stub;

/* ----------------------------------------------------------------
 *	system catalog macros used by the catalog/pg_xxx.h files
 * ----------------------------------------------------------------
 */
#define CATALOG(x) \
    typedef struct CppConcat(FormData_,x)

#define DATA(x)
#define BOOTSTRAP

/* ----------------------------------------------------------------
 *	real old type names, may be obsoleted soon -cim 8/6/90
 * XXX These should disappear!!! -hirohama
 * ----------------------------------------------------------------
 */
typedef	char	XID[5];
#define	CID	unsigned char
#define	ABSTIME	long
#define	RELTIME	long
#define OID	oid
#define	REGPROC	oid		/* for now */

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


/* ----------------
 *	random stuff
 * ----------------
 */
#define	SIGNBIT	(0x8000)	/* msb for int/unsigned */
#define	CSIGNBIT	(1 << 7)	/* msb for char */

/* ----------------
 *	PSIZE
 * ----------------
 */
#define	PSIZE(PTR)	(*((int32 *)(PTR) - 1))
#define	PSIZEALL(PTR)	(*((int32 *)(PTR) - 1) + sizeof (int32))
#define	PSIZESKIP(PTR)	((char *)((int32 *)(PTR) + 1))
#define	PSIZEFIND(PTR)	((char *)((int32 *)(PTR) - 1))
#define	PSIZESPACE(LEN)	((LEN) + sizeof (int32))

/* ----------------
 *	global variable which should probably go someplace else.
 * ----------------
 */
#define	MAXPGPATH	128

#endif PostgresHIncluded
