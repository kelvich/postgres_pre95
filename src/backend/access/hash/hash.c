/*
 *  hash.c -- Implementation of Margo Seltzer's Hashing package for
 *	postgres. 
 *
 *	This file contains only the public interface routines.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "storage/page.h"

#include "utils/log.h"
#include "utils/rel.h"
#include "utils/excid.h"

#include "access/heapam.h"
#include "access/genam.h"
#include "access/ftup.h"
#include "access/sdir.h"
#include "access/isop.h"

#include "access/hash.h"
#include "access/funcindex.h"

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

#include "executor/x_qual.h"
#include "executor/x_tuples.h"
#include "executor/tuptable.h"

#include "lib/index.h"

extern ExprContext RMakeExprContext();

bool	BuildingHash = false;

RcsId("$Header$");

/*
 *  hashbuild() -- build a new hash index.
 *
 *	We use a global variable to record the fact that we're creating
 *	a new index.  This is used to avoid high-concurrency locking,
 *	since the index won't be visible until this transaction commits
 *	and since building is guaranteed to be single-threaded.
 */

void
hashbuild(heap, index, natts, attnum, istrat, pcount, params, finfo, predInfo)
    Relation heap;
    Relation index;
    AttributeNumber natts;
    AttributeNumber *attnum;
    IndexStrategy istrat;
    uint16 pcount;
    Datum *params;
    FuncIndexInfo *finfo;
    LispValue predInfo;
{
}

InsertIndexResult
hashinsert(rel, itup)
    Relation rel;
    IndexTuple itup;
{
    return ((InsertIndexResult) NULL);
}


char *
hashgettuple(scan, dir)
    IndexScanDesc scan;
    ScanDirection dir;
{
    return ((char *) NULL);
}


char *
hashbeginscan(rel, fromEnd, keysz, scankey)
    Relation rel;
    Boolean fromEnd;
    uint16 keysz;
    ScanKey scankey;
{

    return ((char *) NULL);
}

void
hashrescan(scan, fromEnd, scankey)
    IndexScanDesc scan;
    Boolean fromEnd;
    ScanKey scankey;
{
}

void
hashendscan(scan)
    IndexScanDesc scan;
{
}
void
hashmarkpos(scan)
    IndexScanDesc scan;
{
}

void
hashrestrpos(scan)
    IndexScanDesc scan;
{
}

void
hashdelete(rel, tid)
    Relation rel;
    ItemPointer tid;
{
}

uint32
hashint2(key)
    int16 key;
{
    return ((uint32) ~key);
}

uint32
hashint4(key)
    uint32 key;
{
    return (~key);
}

uint32
hashfloat4(keyp)
    float32 keyp;
{
    return (uint32) NULL;
}	


uint32
hashfloat8(keyp)
    float64 keyp;
{
    return (uint32) NULL;
}	


uint32
hashoid(key)
    ObjectId key;
{
    return ((uint32) ~key);
}


uint32
hashchar(key)
    char key;
{
    return (uint32) NULL;
}

uint32
hashchar16(key)
    Name key;
{
    return (uint32) NULL;
}


uint32
hashtext(key)
    struct varlena *key;
{
    return (uint32) NULL;
}	



