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

#include "htup.h"
#include "name.h"

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
#define	RULEPLANCODE	14
#define	AMNAME		15
#define	CLANAME		16
#define INDRELIDKEY	17
#define INHRELID	18
#define PRS2PLANCODE	19

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
