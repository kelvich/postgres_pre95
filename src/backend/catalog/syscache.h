/*
 * syscache.h --
 *	System catalog cache definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SysCacheIncluded
#define SysCacheIncluded

/*#define CACHEDEBUG 	/* turns DEBUG elogs on */

#include "access/htup.h"
#include "tmp/name.h"

/*
 *	Declarations for util/syscache.c.
 *
 *	SysCache identifiers.
 *	These must agree with (and are used to generate) lisplib/cacheids.l.
 */

#define	AMOPOPID	0
#define	AMOPSTRATEGY	1
#define	ATTNAME		2
#define	ATTNUM		3
#define	INDEXRELID	4
#define	LANNAME		5
#define	OPRNAME		6
#define	OPROID		7
#define	PRONAME		8
#define	PROOID		9
#define	RELNAME		10
#define	RELOID		11
#define	TYPNAME		12
#define	TYPOID 		13
#define	AMNAME		14
#define	CLANAME		15
#define INDRELIDKEY	16
#define INHRELID	17
#define PRS2PLANCODE	18
#define	RULOID		19
#define PRS2STUB	20

/*
 *	struct cachedesc:	information needed for a call to InitSysCache()
 */
struct cachedesc {
	Name	*name;	/* this is Name * so that we can initialize it...grr */
	int	nkeys;
	int	key[4];
	int	size;	/* sizeof(appropriate struct) */
};

extern long		SearchSysCacheStruct();
extern HeapTuple	SearchSysCacheTuple();
/*
 * XXX Declarations for LISP stuff deliberately left out.
 *     You shouldn't be using them.
 */

#endif /* !SysCacheIncluded */
