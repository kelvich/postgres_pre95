/*
 * heapam.h --
 *	POSTGRES heap access method definitions.
 */

#ifndef	HeapAMIncluded		/* Include this file only once */
#define HeapAMIncluded	1

/*
 * Identification:
 */
#define HEAPAM_H	"$Header$"

#include <sys/types.h>

#include "tmp/postgres.h"

#include "access/att.h"
#include "access/attnum.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/tupdesc.h"

#include "storage/smgr.h"

#include "rules/rlock.h"
#include "utils/rel.h"

/* ----------------------------------------------------------------
 *		heap access method statistics
 * ----------------------------------------------------------------
 */

typedef struct HeapAccessStatisticsData {
    time_t  init_global_timestamp;	/* time global statistics started */
    time_t  local_reset_timestamp;	/* last time local reset was done */
    time_t  last_request_timestamp;	/* last time stats were requested */

    int global_open;			
    int global_openr;
    int global_close;
    int global_beginscan;
    int global_rescan;
    int global_endscan;
    int global_getnext;
    int global_fetch;
    int global_insert;
    int global_delete;
    int global_replace; 
    int global_markpos; 
    int global_restrpos;
    int global_BufferGetRelation;
    int global_RelationIdGetRelation;
    int global_RelationIdGetRelation_Buf;
    int global_RelationNameGetRelation;
    int global_getreldesc;
    int global_heapgettup;
    int global_RelationPutHeapTuple;
    int global_RelationPutLongHeapTuple;

    int local_open;			
    int local_openr;
    int local_close;
    int local_beginscan;
    int local_rescan;
    int local_endscan;
    int local_getnext;
    int local_fetch;
    int local_insert;
    int local_delete;
    int local_replace; 
    int local_markpos; 
    int local_restrpos;
    int local_BufferGetRelation;
    int local_RelationIdGetRelation;
    int local_RelationIdGetRelation_Buf;
    int local_RelationNameGetRelation;
    int local_getreldesc;
    int local_heapgettup;
    int local_RelationPutHeapTuple;
    int local_RelationPutLongHeapTuple;
} HeapAccessStatisticsData;

typedef HeapAccessStatisticsData *HeapAccessStatistics;

#define IncrHeapAccessStat(x) \
    (heap_access_stats == NULL ? 0 : (heap_access_stats->x)++)

extern HeapAccessStatistics heap_access_stats;

/* ----------------
 *	function prototypes for heap access method
 * ----------------
 */
/* heap_create, heap_creatr, and heap_destroy are declared in catalog/heap.h */
#include "lib/heap.h"
extern Relation 	heap_open();
extern Relation 	heap_openr();
extern void		heap_close();
extern HeapTuple	heap_fetch();
extern ObjectId 	heap_insert();
extern RuleLock 	heap_delete();
extern RuleLock 	heap_replace();
extern char *   	heap_getattr();
extern HeapScanDesc 	heap_beginscan();
extern void 		heap_rescan();
extern void 		heap_endscan();
extern void 		heap_markpos();
extern void 		heap_restrpos();
extern HeapTuple 	heap_getnext();

extern int		heap_attisnull();
extern int		heap_sysattrlen();
extern bool		heap_sysattrbyval();
extern HeapTuple	heap_addheader();
extern HeapTuple	heap_copytuple();
extern HeapTuple	heap_formtuple();
extern HeapTuple	heap_modifytuple();

/* ----------------
 *	misc
 * ----------------
 */
extern
RuleLock
doinsert ARGS((
	Relation	relation,
	HeapTuple	heapTuple
));

/* ----------------------------------------------------------------
 *	       obsolete heap access method interfaces
 * ----------------------------------------------------------------
 */

/*
 * RelationNameCreateHeapRelation --
 *	Returns relation id of a newly created cataloged heap relation.
 */
#define RelationNameCreateHeapRelation(relname, arch, natts, smgr, tupdesc) \
    heap_create(relname, arch, natts, smgr, tupdesc)

#define amcreate(relname, arch, natts, smgr, tupdesc) \
    heap_create(relname, arch, natts, smgr, tupdesc)

/*
 * RelationNameCreateTemporaryRelation --
 *	Creates a temporary heap relation.
 */
#define RelationNameCreateTemporaryRelation(relname, natts, att) \
    heap_creatr(relname, natts, DEFAULT_SMGR, att)

#define amcreatr(relname, natts, smgr, att) \
    heap_creatr(relname, natts, smgr, att)

/*
 * RelationNameDestroyHeapRelation --
 *	Destroys a heap relation.
 */
#define RelationNameDestroyHeapRelation(relationName) \
    heap_destroy(relationName)

#define amdestroy(relationName) \
    heap_destroy(relationName)

/*
 * ObjectIdOpenHeapRelation --
 *	Opens a heap relation by its object identifier.
 */
#define ObjectIdOpenHeapRelation(relationId) \
    heap_open(relationId)

#define amopen(relid) \
    heap_open(relid)

/*
 * RelationNameOpenHeapRelation --
 *	Opens a heap relation by name.
 */
#define RelationNameOpenHeapRelation(relationName) \
    heap_openr(relationName)

#define amopenr(relationName) \
    heap_openr(relationName)

/*
 * RelationCloseHeapRelation --
 *	Closes a heap relation.
 */
#define RelationCloseHeapRelation(relation) \
    heap_close(relation)

#define amclose(relation) \
    heap_close(relation)

/*
 * RelationGetHeapTupleByItemPointer --
 *	Retrieves a heap tuple by its item pointer.
 */
#define RelationGetHeapTupleByItemPointer(relation, timeQual, tid, b) \
    heap_fetch(relation, timeQual, tid, b)

#define amgetunique(relation, timeQual, tid, b) \
    heap_fetch(relation, timeQual, tid, b)

/*
 * RelationInsertHeapTuple --
 *	Inserts a heap tuple.
 */
#define RelationInsertHeapTuple(relation, heapTuple, offsetOutP) \
    heap_insert(relation, heapTuple, offsetOutP)

#define aminsert(relation, heapTuple, offsetOutP) \
    heap_insert(relation, heapTuple, offsetOutP)

/*
 * RelationDeleteHeapTuple --
 *	Deletes a heap tuple.
 */
#define RelationDeleteHeapTuple(relation, heapItem) \
    heap_delete(relation, heapItem)

#define amdelete(relation, heapItem) \
    heap_delete(relation, heapItem)

/*
 * RelationReplaceHeapTuple --
 *	Replaces a heap tuple.  OffsetOutP is obsolete.
 */
#define RelationReplaceHeapTuple(relation, heapItem, tuple, offsetOutP) \
    heap_replace(relation, heapItem, tuple)

#define amreplace(relation, heapItem, tuple) \
    heap_replace(relation, heapItem, tuple)

/*
 * HeapTupleGetAttributeValue --
 *	Returns an attribute of a heap tuple.
 */
#define HeapTupleGetAttributeValue(tup, b, attnum, att, isnull) \
    PointerGetDatum(heap_getattr(tup, b, attnum, att, isnull))

/*
 * RelationBeginHeapScan --
 *	General access method initialize heap scan routine.
 */
#define RelationBeginHeapScan(relation, atend, timeQual, nkeys, key) \
    heap_beginscan(relation, atend, timeQual, nkeys, key)

#define ambeginscan(relation, atend, timeQual, nkeys, key) \
    heap_beginscan(relation, atend, timeQual, nkeys, key)

/*
 * HeapScanRestart --
 *	General access method restart heap scan routine.
 */
#define HeapScanRestart(scan, restartScanAtEnd, key) \
    heap_rescan(scan, restartScanAtEnd, key)

#define amrescan(scan, restartScanAtEnd, key) \
    heap_rescan(scan, restartScanAtEnd, key)

/*
 * HeapScanEnd --
 *	General access method end heap scan routine.
 */
#define HeapScanEnd(scan) \
    heap_endscan(scan)

#define amendscan(scan) \
    heap_endscan(scan)

/*
 * HeapScanMarkPosition --
 *	General access method mark heap scan position routine.
 */
#define HeapScanMarkPosition(scan) \
    heap_markpos(scan)

#define ammarkpos(scan) \
    heap_markpos(scan)

/*
 * HeapScanRestorePosition --
 *	General access method restore heap scan position routine.
 */
#define HeapScanRestorePosition(scan) \
    heap_restrpos(scan)

#define amrestrpos(scan) \
    heap_restrpos(scan)

/*
 * HeapScanGetNextTuple --
 *	General access method get heap tuple routine.
 */
#define HeapScanGetNextTuple(scan, backwards, bufferOutP) \
    heap_getnext(scan, backwards, bufferOutP)

#define amgetnext(scan, backwards, bufferOutP) \
    heap_getnext(scan, backwards, bufferOutP)

/*
 * sysattrlen
 */
#define sysattrlen(attno) \
    heap_sysattrlen(attno)

/*
 * sysattrbyval
 */
#define sysattrbyval(attno) \
    heap_sysattrbyval(attno)

/*
 * amgetattr
 */
#define amgetattr(tup, b, attnum, att, isnull) \
    heap_getattr(tup, b, attnum, att, isnull)

/*
 * palloctup
 */
#define palloctup(tuple, buffer, relation) \
    heap_copytuple(tuple, buffer, relation)

#endif	/* !defined(HeapAMIncluded) */
