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

#ifndef	C_H
#include "c.h"
#endif

#include "att.h"	/* XXX */
#include "skey.h"

#ifndef	ATTNUM_H
# include "attnum.h"
#endif
#ifndef	DATUM_H
# include "datum.h"
#endif
#ifndef	HTUP_H
# include "htup.h"
#endif
#ifndef	NAME_H
# include "name.h"
#endif
#ifndef	OID_H
# include "oid.h"
#endif
#ifndef	REL_H
# include "rel.h"
#endif
#ifndef	RELSCAN_H
# include "relscan.h"
#endif
#ifndef	RLOCK_H
# include "rlock.h"
#endif
#ifndef	TQUAL_H
# include "tqual.h"
#endif
#ifndef	TUPDESC_H
# include "tupdesc.h"
#endif

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
