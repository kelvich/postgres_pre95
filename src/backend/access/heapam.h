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
    int global_ObjectIdOpenHeapRelation;
    int global_RelationNameOpenHeapRelation;
    int global_RelationCloseHeapRelation;
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
    int local_ObjectIdOpenHeapRelation;
    int local_RelationNameOpenHeapRelation;
    int local_RelationCloseHeapRelation;
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
 *	new prototypes
 * ----------------
 */
extern ObjectId 	heap_create();
extern Relation 	heap_creatr();
extern void		heap_destroy();
extern Relation 	heap_open();
extern Relation 	heap_openr();
extern void		heap_close();
extern HeapTuple	heap_fetch();
extern RuleLock 	heap_insert();
extern RuleLock 	heap_delete();
extern RuleLock 	heap_replace();
extern char *   	heap_getattr();
extern HeapScanDesc 	heap_beginscan();
extern void 		heap_rescan();
extern void 		heap_endscan();
extern void 		heap_markpos();
extern void 		heap_restrpos();
extern HeapTuple 	heap_getnext();

/* ----------------
 *	old prototypes
 * ----------------
 */

/*
 * RelationNameCreateHeapRelation --
 *	Returns relation id of a newly created cataloged heap relation.
 */
extern
ObjectId
RelationNameCreateHeapRelation ARGS((
	Name		relationName,
	ArchiveMode	archiveMode,
	AttributeNumber	numberOfAttributes,
	TupleDescriptor	tupleDescriptor
));

extern
ObjectId /* of the newly created relation */
amcreate ARGS((
	char		relationName[16],
	ArchiveMode	archiveMode,
	u_int2		numberOfAttributes,
	Attribute	attribute[]
));

/*
 * RelationNameCreateTemporaryRelation --
 *	Creates a temporary heap relation.
 */
extern
Relation
RelationNameCreateTemporaryRelation ARGS((
	Name		relationName,
	AttributeNumber	numberOfAttributes,
	TupleDescriptor	tupleDescriptor
));

extern
Relation
amcreatr ARGS((
	char		relationName[16],
	u_int2		numberOfAttributes,
	Attribute	attribute[]
));

/*
 * RelationNameCreateVersionRelation --
 *	Creates a version relation.
 */
extern
void
RelationNameCreateVersionRelation ARGS((
	Name	originalRelationName,
	Name	versionRelationName,
	long	time			/* AbsoluteTime XXX */
));

extern
void
amcreatv ARGS((
	char	originalRelationName[16],
	char	versionRelationName[16],
	long	time			/* AbsoluteTime XXX */
));

/*
 * RelationNameDestroyHeapRelation --
 *	Destroys a heap relation.
 */
extern
void
RelationNameDestroyHeapRelation ARGS((
	Name	relationName
));

extern
void
amdestroy ARGS((
	char	relationName[16]
));

/*
 * RelationNameMergeRelations --
 *	Merges two version relations.
 */
extern
void
RelationNameMergeRelations ARGS((
	Name	oldRelationName,
	Name	newRelationName
));

extern
void
ammergev ARGS((
	char	oldRelationName[16],
	char	newRelationName[16]
));

/*
 * ObjectIdOpenHeapRelation --
 *	Opens a heap relation by its object identifier.
 */
extern
Relation
ObjectIdOpenHeapRelation ARGS((
	ObjectId	relationObjectId
));

extern
Relation
amopen ARGS((
	ObjectId	relationObjectId
));

/*
 * RelationNameOpenHeapRelation --
 *	Opens a heap relation by name.
 */
extern
Relation
RelationNameOpenHeapRelation ARGS((
	Name	relationName
));

extern
Relation
amopenr ARGS((
	char	relationName[16]
));

/*
 * RelationCloseHeapRelation --
 *	Closes a heap relation.
 */
extern
void
RelationCloseHeapRelation ARGS((
	Relation	relation
));

extern
void
amclose ARGS((
	Relation	relation
));

/*
 * RelationGetHeapTupleByItemPointer --
 *	Retrieves a heap tuple by its item pointer.
 */
extern
HeapTuple
RelationGetHeapTupleByItemPointer ARGS((
	Relation	relation,
	TimeQual	timeQual,
	ItemPointer	heapItem,
	Buffer		*bufferOutP
));

extern
HeapTuple
amgetunique ARGS((
	Relation	relation,
	TimeQual	timeQual,
	ItemPointer	tid,
	Buffer		*bufferOutP
));

/*
 * RelationInsertHeapTuple --
 *	Inserts a heap tuple.
 */
extern
RuleLock
RelationInsertHeapTuple ARGS((
	Relation	relation,
	HeapTuple	heapTuple,
	double		*offsetOutP
));

extern
RuleLock
aminsert ARGS((
	Relation	relation,
	HeapTuple	heapTuple,
	double		*offsetOutP
));

extern
RuleLock
doinsert ARGS((
	Relation	relation,
	HeapTuple	heapTuple
));

/*
 * RelationDeleteHeapTuple --
 *	Deletes a heap tuple.
 */
extern
RuleLock
RelationDeleteHeapTuple ARGS((
	Relation	relation,
	ItemPointer	heapItem
));

extern
RuleLock
amdelete ARGS((
	Relation	relation,
	ItemPointer	tid
));

/*
 * RelationPhysicallyDeleteHeapTuple --
 *	Physically deletes a heap tuple.
 */
extern
void
RelationPhysicallyDeleteHeapTuple ARGS((
	Relation	relation,
	ItemPointer	heapItem
));

/*
 * RelationReplaceHeapTuple --
 *	Replaces a heap tuple.
 */
extern
RuleLock
RelationReplaceHeapTuple ARGS((
	Relation	relation,
	ItemPointer	heapItem,
	HeapTuple	tuple,
	double		*offsetOutP
));

extern
RuleLock
amreplace ARGS((
	Relation	relation,
	ItemPointer	tid,
	HeapTuple	tuple,
	double		*offsetOutP
));

/*
 * HeapTupleGetAttributeValue --
 *	Returns an attribute of a heap tuple.
 */
extern
Datum
HeapTupleGetAttributeValue ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	AttributeNumber	attributeNumber,
	TupleDescriptor	tupleDescriptor,
	Boolean		*attributeIsNullOutP
));

extern
char *
amgetattr ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	AttributeNumber	attributeNumber,
	Attribute	attribute[],
	Boolean		*attributeIsNullOutP
));

/*
 * RelationGetHeapScan --
 *	General access method initialize heap scan routine.
 */
extern
HeapScanDesc
RelationBeginHeapScan ARGS((
	Relation	relation,
	Boolean		startScanAtEnd,
	TimeQual	timeQual,
	uint16		numberOfKeys,
	ScanKey		key
));

extern
HeapScanDesc
ambeginscan ARGS((
	Relation	relation,
	Boolean		startScanAtEnd,
	TimeQual	timeQual,
	uint16		numberOfKeys,
	ScanKey		key
));

/*
 * HeapScanRestart --
 *	General access method restart heap scan routine.
 */
extern
void
HeapScanRestart ARGS((
	HeapScanDesc	scan,
	bool		restartScanAtEnd,
	ScanKey		key
));

extern
void
amrescan ARGS((
	HeapScanDesc	scan,
	bool		restartScanAtEnd,
	ScanKey		key
));

/*
 * HeapScanEnd --
 *	General access method end heap scan routine.
 */
extern
void
HeapScanEnd ARGS((
	HeapScanDesc	scan
));

extern
void
amendscan ARGS((
	HeapScanDesc	scan
));

/*
 * HeapScanMarkPosition --
 *	General access method mark heap scan position routine.
 */
extern
void
HeapScanMarkPosition ARGS((
	HeapScanDesc	scan
));

extern
void
ammarkpos ARGS((
	HeapScanDesc	scan
));

/*
 * HeapScanRestorePosition --
 *	General access method restore heap scan position routine.
 */
extern
void
HeapScanRestorePosition ARGS((
	HeapScanDesc	scan
));

extern
void
amrestrpos ARGS((
	HeapScanDesc	scan
));

/*
 * HeapScanGetNextTuple --
 *	General access method get heap tuple routine.
 */
extern
HeapTuple
HeapScanGetNextTuple ARGS((
	HeapScanDesc	scan,
	Boolean		backwards,
	Buffer		*bufferOutP
));

extern
HeapTuple
amgetnext ARGS((
	HeapScanDesc	scan,
	Boolean		backwards,
	Buffer		*bufferOutP
));

/*
 * amfreetuple --
 *	General access method free tuple routine.
 */
extern
amfreetuple ARGS((
	void
	/* this needs more thought */
));

#endif	/* !defined(HeapAMIncluded) */
