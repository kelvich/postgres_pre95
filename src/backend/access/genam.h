/* ----------------------------------------------------------------
 *   FILE
 *	genam.h
 *
 *   DESCRIPTION
 *	POSTGRES general access method definitions.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef	GenAMIncluded
#define GenAMIncluded	1

#include "tmp/postgres.h"

#include "access/attnum.h"
#include "access/attval.h"	/* XXX for AMgetattr */
#include "access/itup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/sdir.h"
#include "access/funcindex.h"

/* ----------------
 *	generalized index_ interface routines
 * ----------------
 */
extern Relation index_open ARGS((ObjectId relationId));
extern Relation index_openr ARGS((Name relationName));
extern void index_close ARGS((Relation relation));
extern GeneralInsertIndexResult index_insert ARGS((Relation relation, IndexTuple indexTuple, double *offsetOutP));
extern void index_delete ARGS((Relation relation, ItemPointer indexItem));
extern IndexScanDesc index_beginscan ARGS((Relation relation, Boolean scanFromEnd, uint16 numberOfKeys, ScanKey key));
extern void index_rescan ARGS((IndexScanDesc scan, bool scanFromEnd, ScanKey key));
extern void index_endscan ARGS((IndexScanDesc scan));
extern void index_markpos ARGS((IndexScanDesc scan));
extern void index_restrpos ARGS((IndexScanDesc scan));
extern RetrieveIndexResult index_getnext ARGS((IndexScanDesc scan, ScanDirection direction));
extern RegProcedure index_getprocid ARGS((Relation irel, AttributeNumber attnum, uint16 procnum));
extern void InsertIndexTuple ARGS((Relation heapRelation, Relation indexRelation, AttributeNumber numberOfAttributes, AttributeNumber attributeNumber[], HeapTuple heapTuple, IndexStrategy indexStrategy, uint16 parameterCount, Datum parameter[]));
extern HeapTuple GetHeapTuple ARGS((GeneralRetrieveIndexResult result, Relation heaprel, Buffer buffer));
extern Datum GetIndexValue ARGS((HeapTuple tuple, TupleDescriptor hTupDesc, AttributeOffset attOff, AttributeNumber attrNums[], FuncIndexInfo *fInfo, Boolean *attNull, Buffer buffer));

/* ----------------
 *	Predefined routines.  Comment from genam.c:
 *
 * All indexed access methods use an identical scan structure.
 * We don't know how the various AMs do locking, however, so we don't
 * do anything about that here.
 *
 * The intent is that an AM implementor will define a front-end routine
 * that calls this one, to fill in the scan, and then does whatever kind
 * of locking he wants.
 * ----------------
 */
/*
 * RelationGetIndexScan --
 *	General access method initialize index scan routine.
 * IndexScanRestart --
 *	General access method restart index scan routine.
 * IndexScanEnd --
 *	General access method end index scan routine.
 * IndexScanMarkPosition --
 *	General access method mark index scan position routine.
 * IndexScanRestorePosition --
 *	General access method restore index scan position routine.
 * IndexScanGetRetrieveIndexResult --
 *	General access method low-level get index tuple routine.
 * Note:
 *	Assumes scan is valid.
 *	This routine is likely to be useful only to the vacuum demon.
 * IndexScanGetGeneralRetrieveIndexResult --
 *	General access method general get index tuple routine.
 * Note:
 *	Assumes scan is valid.
 */
extern IndexScanDesc RelationGetIndexScan ARGS((Relation relation, Boolean scanFromEnd, uint16 numberOfKeys, ScanKey key));
extern void IndexScanRestart ARGS((IndexScanDesc scan, bool scanFromEnd, ScanKey key));
extern void IndexScanEnd ARGS((IndexScanDesc scan));
extern void IndexScanMarkPosition ARGS((IndexScanDesc scan));
extern void IndexScanRestorePosition ARGS((IndexScanDesc scan));
extern RetrieveIndexResult IndexScanGetRetrieveIndexResult ARGS((IndexScanDesc scan, ScanDirection direction));
extern GeneralRetrieveIndexResult IndexScanGetGeneralRetrieveIndexResult ARGS((IndexScanDesc scan, ScanDirection direction));

/* indextuple.h (not found in itup.h) */
extern IndexTuple index_formtuple();
extern char *fastgetiattr();
extern AttributeValue index_getsysattr();

/* ----------------
 *	support macros for old index access method interface
 * ----------------
 */

/*
 * RelationNameCreateIndexRelation --
 *	General access method create index routine.
 *
 * paramter names abbreviated:
 *	hRN --> 	heapRelationName
 *  	iRN --> 	indexRelationName
 *  	aMOI --> 	accessMethodObjectId
 *  	n --> 		numatts
 *  	a -->		attNums
 *  	cOI -->		classObjectId
 *  	pC -->		parameterCount
 *  	p -->		parameter
 */
#define RelationNameCreateIndexRelation(hRN, iRN, aMOI, n, a, cOI, pC, p) \
    index_create(hRN, iRN, NULL, aMOI, n, a, cOI, pC, p, LispNil)

#define AMcreati(hRN, iRN, aMOI, n, a, cOI, pC, p) \
    index_create(hRN, iRN, NULL, aMOI, n, a, cOI, pC, p, LispNil)

/*
 * DestroyIndexRelationById --
 *	General access method destroy index relation routine.
 */
#define DestroyIndexRelationById(indexId) \
    index_destroy(indexId)

#define AMdestroy(indexId) \
    index_destroy(indexId)

/*
 * ObjectIdOpenIndexRelation --
 *	General access method open index routine by object identifier routine.
 */
#define ObjectIdOpenIndexRelation(relationObjectId) \
    index_open(relationObjectId)

#define AMopen(relationObjectId) \
    index_open(relationObjectId)

/*
 * AMopenr
 */
#define AMopenr(relationName) \
    index_openr(relationName)

/*
 * RelationCloseIndexRelation --
 *	General access method close index relation routine.
 */
#define RelationCloseIndexRelation(relation) \
    (void) index_close(relation)

#define AMclose(relation) \
    (void) index_close(relation)

/*
 * RelationInsertIndexTuple --
 *	General access method index tuple insertion routine.
 */
#define RelationInsertIndexTuple(relation, indexTuple, scan, offsetOutP) \
    ((GeneralInsertIndexResult)	index_insert(relation, indexTuple, offsetOutP))

#define AMinsert(relation, indexTuple, scan, offsetOutP) \
    ((GeneralInsertIndexResult)	index_insert(relation, indexTuple, offsetOutP))

/*
 * RelationDeleteIndexTuple --
 *	General access method index tuple deletion routine.
 */
#define RelationDeleteIndexTuple(relation, indexItem) \
    (void) index_delete(relation, indexItem)

#define AMdelete(relation, indexItem) \
    (void) index_delete(relation, indexItem)

/*
 * RelationSetIndexRuleLock --
 *	General access method lock setting routine.
 *
 * XXX unimplemented
 */
#define RelationSetIndexRuleLock(relation, indexItem, lock) \
    elog(DEBUG, "RelationSetIndexRuleLock; unimplemented")

#define AMsetlock(relation, indexItem, lock) \
    elog(DEBUG, "AMsetlock; unimplemented")

/*
 * RelationSetIndexItemPointer --
 *	General access method base "tid" setting routine.
 *
 * XXX unimplemented
 */
#define RelationSetIndexItemPointer(relation, indexItem, heapItem) \
    elog(DEBUG, "RelationSetIndexItemPointer: unimplemented")

#define AMsettid(relation, indexItem, heapItem) \
    elog(DEBUG, "AMsettid: unimplemented")

/*
 * AMbeginscan
 */
#define AMbeginscan(relation, scanFromEnd, numberOfKeys, key) \
    ((IndexScanDesc) index_beginscan(relation, scanFromEnd, numberOfKeys, key))
/*
 * AMrescan
 */
#define AMrescan(scan, scanFromEnd, key) \
    (void) index_rescan(scan, scanFromEnd, key)
/*
 * AMendscan
 */
#define AMendscan(scan) \
    (void) index_endscan(scan)
/*
 * AMmarkpos
 */
#define AMmarkpos(scan) \
    (void) index_markpos(scan)
/*
 * AMrestrpos
 */
#define AMrestrpos(scan) \
    (void) index_restrpos(scan)
/*
 * AMgettuple
 */
#define AMgettuple(scan, direction) \
    ((GeneralRetrieveIndexResult) \
     IndexScanGetGeneralRetrieveIndexResult(scan, direction))

#endif	/* !defined(GenAMIncluded) */
