/* ----------------------------------------------------------------
 *   FILE
 *	syscache.h
 *
 *   DESCRIPTION
 *	System catalog cache definitions.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
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
 *
 *      The order of these must match the order
 *      they are entered into the structure cacheinfo[] in syscache.c 
 *      The best thing to do is to add yours at the END, because some
 *      code assumes that certain caches are at certain places in this
 *      array.
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
#define USENAME		27
#define USESYSID	28
#define GRONAME		29
#define GROSYSID	30
#define	REWRITENAME	31
#define PROSRC          32

/* ----------------
 *	struct cachedesc:	information needed for a call to InitSysCache()
 * ----------------
 */
struct cachedesc {
    Name      *name;	       /* this is Name * so that we can initialize it */
    int	      nkeys;
    int	      key[4];
    int	      size;            /* sizeof(appropriate struct) */
    Name      *indname;	       /* index relation for this cache, if exists */
    HeapTuple (*iScanFunc)();  /* function to handle index scans */
};

/* from utils/init/postinit.c */
int zerocaches ARGS((void));

/* from utils/cache/syscache.c */
extern void InitCatalogCache ARGS((void));
extern HeapTuple SearchSysCacheTuple ARGS((int cacheId, char *key1, char *key2, char *key3, char *key4));
extern int32 SearchSysCacheStruct ARGS((int cacheId, char *returnStruct, char *key1, char *key2, char *key3, char *key4));
extern struct _LispValue *SearchSysCacheGetAttribute ARGS((int cacheId, AttributeNumber attributeNumber, char *key1, char *key2, char *key3, char *key4));
extern struct _LispValue *TypeDefaultRetrieve ARGS((ObjectId typId));

#endif /* !SysCacheIncluded */
