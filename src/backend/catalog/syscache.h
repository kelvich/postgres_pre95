/*
 * syscache.h --
 *	System catalog cache definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SysCacheIncluded
#define SysCacheIncluded

/*#define CACHEDEBUG*/ 	/* turns DEBUG elogs on */

#include "tmp/postgres.h"
#include "access/htup.h"

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
#define PRS2RULEID	21
#define PRS2EVENTREL	22
#define AGGNAME		23
#define NAMEREL         24
#define LOBJREL         25
#define LISTENREL       26

/* ----------------
 *	struct cachedesc:	information needed for a call to InitSysCache()
 * ----------------
 */
struct cachedesc {
    Name  *name;	/* this is Name * so that we can initialize it...grr */
    int	  nkeys;
    int	  key[4];
    int	  size;		/* sizeof(appropriate struct) */
    Name  *indname;	/* name of index relation for this cache, if exists */
};

int zerocaches ARGS(( void ));
void InitCatalogCache ARGS((void ));
/*
 * SearchSysCacheStruct and SearchSysCacheTuple take a varying number of 
 * args depending on how many keys the lookup requires.  For this reason 
 * the formal arg list has been left out.
 */
int32 SearchSysCacheStruct();
HeapTuple SearchSysCacheTuple();

/*
 * XXX Declarations for LISP stuff deliberately left out.
 *     You shouldn't be using them.
 */

#endif /* !SysCacheIncluded */
