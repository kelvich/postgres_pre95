/*
 * genam.h --
 *	POSTGRES general access method definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	GenAMIncluded
#define GenAMIncluded	1

#include "tmp/postgres.h"

#include "access/attnum.h"
#include "access/attval.h"	/* XXX for AMgetattr */
#include "access/itup.h"
#include "access/relscan.h"
#include "access/skey.h"

/*
 * RelationNameCreateIndexRelation --
 *	General access method create index routine.
 */
extern
void
RelationNameCreateIndexRelation ARGS((
	Name		heapRelationName,
	Name		indexRelationName,
	ObjectId	accessMethodObjectId,
	AttributeNumber	numberOfAttributes,
	AttributeNumber	attributeNumber[],
	ObjectId	classObjectId[],
	uint16		parameterCount,
	Datum		parameter[]
));

extern
void
AMcreati ARGS((
	Name		heapRelationName,
	Name		indexRelationName,
	ObjectId	accessMethodObjectId,
	AttributeNumber	numberOfAttributes,
	AttributeNumber	attributeNumber[],
	ObjectId	classObjectId[],
	uint16		parameterCount,
	Datum		parameter[]
));

/*
 * DestroyIndexRelationById --
 *	General access method destroy index relation routine.
 */
extern
void
DestroyIndexRelationById ARGS((
	ObjectId	indexId
));

extern
void
AMdestroy ARGS((
	ObjectId	indexId
));

/*
 * ObjectIdOpenIndexRelation --
 *	General access method open index routine by object identifier routine.
 */
extern
Relation
ObjectIdOpenIndexRelation ARGS((
	ObjectId	relationObjectId
));

extern
Relation
AMopen ARGS((
	ObjectId	relationObjectId
));

/*
 * RelationCloseIndexRelation --
 *	General access method close index relation routine.
 */
extern
void
RelationCloseIndexRelation ARGS((
	Relation	relation
));

extern
void
AMclose ARGS((
	Relation	relation
));

/*
 * RelationInsertIndexTuple --
 *	General access method index tuple insertion routine.
 */
extern
GeneralInsertIndexResult
RelationInsertIndexTuple ARGS((
	Relation	relation,
	IndexTuple	indexTuple,
	Pointer		scan,
	double		*offsetOutP
));

extern
GeneralInsertIndexResult
AMinsert ARGS((
	Relation	relation,
	IndexTuple	indexTuple,
	Pointer		scan,
	double		*offsetOutP
));

/*
 * RelationDeleteIndexTuple --
 *	General access method index tuple deletion routine.
 */
extern
void
RelationDeleteIndexTuple ARGS((
	Relation	relation,
	ItemPointer	indexItem
));

extern
void
AMdelete ARGS((
	Relation	relation,
	ItemPointer	indexItem
));

/*
 * RelationSetIndexRuleLock --
 *	General access method lock setting routine.
 */
extern
void
RelationSetIndexRuleLock ARGS((
	Relation	relation,
	ItemPointer	indexItem,
	RuleLock	lock
));

extern
void
AMsetlock ARGS((
	Relation	relation,
	ItemPointer	indexItem,
	RuleLock	lock
));

/*
 * RelationSetIndexItemPointer --
 *	General access method base "tid" setting routine.
 */
extern
void
RelationSetIndexItemPointer ARGS((
	Relation	relation,
	ItemPointer	indexItem,
	ItemPointer	heapItem
));

extern
void
AMsettid ARGS((
	Relation	relation,
	ItemPointer	indexItem,
	ItemPointer	heapItem
));

/*
 * IndexTupleGetAttributeValue --
 *	General access method get attribute routine.
 *
 * Note:
 *	This is unneeded, there will/may be a similar call for
 *	use by access method code.
 */
/*
extern
IndexTupleGetAttributeValue ARGS((
	InsertIndexResult	indexObject,
	AttributeNumber		attributeNumber,
	struct	attribute	*att[],
	Boolean			*attributeIsNullOutP
));
*/
/*
extern
AMgetattr ARGS((
	InsertIndexResult	indexObject,
	AttributeNumber		attributeNumber,
	struct	attribute	*att[],
	Boolean			*attributeIsNullOutP
));
*/

/*
 * RelationGetIndexScan --
 *	General access method initialize index scan routine.
 */
extern
IndexScanDesc
RelationGetIndexScan ARGS((
	Relation	relation,
	Boolean		scanFromEnd,
	uint16		numberOfKeys,
	ScanKey		key
));

extern
IndexScanDesc
AMbeginscan ARGS((
	Relation	relation,
	Boolean		scanFromEnd,
	uint16		numberOfKeys,
	ScanKey		key
));

/*
 * IndexScanRestart --
 *	General access method restart index scan routine.
 */
extern
void
IndexScanRestart ARGS((
	IndexScanDesc	scan,
	bool		scanFromEnd,
	ScanKey		key
));

extern
void
AMrescan ARGS((
	IndexScanDesc	scan,
	bool		scanFromEnd,
	ScanKey		key
));

/*
 * IndexScanEnd --
 *	General access method end index scan routine.
 */
extern
void
IndexScanEnd ARGS((
	IndexScanDesc	scan
));

extern
void
AMendscan ARGS((
	IndexScanDesc	scan
));

/*
 * IndexScanMarkPosition --
 *	General access method mark index scan position routine.
 */
extern
void
IndexScanMarkPosition ARGS((
	IndexScanDesc	scan
));

extern
void
AMmarkpos ARGS((
	IndexScanDesc	scan
));

/*
 * IndexScanRestorePosition --
 *	General access method restore index scan position routine.
 */
extern
void
IndexScanRestorePosition ARGS((
	IndexScanDesc	scan
));

extern
void
AMrestrpos ARGS((
	IndexScanDesc	scan
));

/*
 * IndexScanGetRetrieveIndexResult --
 *	General access method low-level get index tuple routine.
 *
 * Note:
 *	Assumes scan is valid.
 *	This routine is likely to be useful only to the vacuum demon.
 */
extern
RetrieveIndexResult
IndexScanGetRetrieveIndexResult ARGS((
	IndexScanDesc	scan,
	Boolean		backward
));

/*
 * IndexScanGetGeneralRetrieveIndexResult --
 *	General access method general get index tuple routine.
 *
 * Note:
 *	Assumes scan is valid.
 */
extern
GeneralRetrieveIndexResult
IndexScanGetGeneralRetrieveIndexResult ARGS((
	IndexScanDesc	scan,
	Boolean		backward
));

/* OBSOLETE
 * IndexScanGetIndexTuple --
 *	General access method get index tuple routine.
 */
extern
GeneralRetrieveIndexResult
IndexScanGetIndexTuple ARGS((
	IndexScanDesc	scan,
	Boolean		backward
));

extern
GeneralRetrieveIndexResult
AMgettuple ARGS((
	Relation	relation,
	ItemPointer	indexItem,
	int		direction,	/* -1 backwd, 1 fwd, 0 no move XXX */
	uint16		numberOfKeys,
	ScanKey		key
));

/*
 * IndexTupleFree --
 *	General access method free tuple routine.
 */
extern
IndexTupleFree ARGS((
	void
	/* this needs more thought */
));

extern
AMfreetuple ARGS((
	void
	/* this needs more thought */
));

#endif	/* !defined(GenAMIncluded) */
