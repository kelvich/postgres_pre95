/*========================================================================
 *
 * FILE:
 *    prs2stub.h
 *
 * IDENTIFICATION:
 *    $Header$
 *
 * DESCRIPTION:
 *
 * This file contains the definitions of the rule 'stub' records.
 * These come in 2 formats: 
 *    Prs2Stub: this is the 'normal' format, and it is a pointer to
 *    		a structure which contains pointers to other structures
 *		and so forth.
 *    Prs2RawStub: this is similar to a pointer to a 'varlena' struct,
 *		and it is a "flat" representation of 'Prs2Stub'.
 *		It can be considered as a stream of raw bytes
 *		(starting with a long int containing size information)
 *		suitable for disk storage.
 *
 * The reason for using 2 formats instead of one, is that we have to
 * have a "flat" representation (like Prs2RawStub) for reading/writing
 * stub records to disk, but on the other hand, as these records
 * contain a lot of variable length information, main memory
 * operations (like adding/deleting stub record entries) would
 * reequire a complex interface (probably inefficient & difficult to
 * write & maintain)
 *
 * So, in order to manipulate stub records, first we use amgetattr to
 * retrieve them in 'Prs2RawStub' format, and then we translate them into
 * 'Prs2Stub' using the routine 'Prs2RawStubToStub()'.
 * In order to write them back to disk, we use 'Prs2StubToRawStub()'
 * to change them back into the 'Prs2RawStub' form and store them back
 * into the tuple. Finally, we free the space occupied by the 'Prs2Stub'
 * form using 'Prs2StubFree()'.
 *
 *========================================================================
 */

#ifndef Prs2StubIncluded
#define Prs2StubIncluded

/*--------------
 * turn on/off debugging....
 */
#define STUB_DEBUG	10

#include "tmp/postgres.h"
#include "access/attnum.h"
#include "rules/prs2locks.h"
#include "rules/prs2.h"

/*------------------------------------------------------------------------
 *		Prs2Stub
 *------------------------------------------------------------------------
 *
 * Prs2StubId: a stub record identifier, used to distinguish between
 * 	the many stub records a rule might use.
 *
 * Prs2Stub: a pointer to a 'PrsStubData' struct.
 *
 * Prs2StubData: this struct contains all the stub records information
 *	of a tuple (a tuple can have 0 or more stub records).
 *	It has the following fields:
 *	numOfStubs: the number of stub records for this tuple.
 *	stubRecors: an array of pointers to 'Prs2OneStubData' structures,
 *	holding information about each individual stub record.
 *
 * Prs2OneStubData: a structure holding information about one
 *	stub record. It has the following fields:
 *	ruleId:	the oid of the rule where this stub belongs.
 *	stubId: used to distinguish between the many stub records
 *		a rule can put.
 *	counter: It is possible for a rule to put many copies of
 *		the same stub record to the same tuple. Instead of
 *		duplicating information, we use a counter of these
 *		copies.
 *	lock: the RuleLock associated with this stub.
 *	qualification: The qualification associated with the stub.
 *		If the qualification is NULL then the qualification
 *		of the stub is always true.
 *
 *	NOTE: XXX!!!!!!!!!!!!!  THIS IS A HACK !!!!!!!!!!
 *	'lock' and 'qualification' are pointers to variable length
 *	fields. In order to make transformation between the 'Prs2Stub'
 *	format to 'Prs2RawStub' and vice versa easier, 
 *	these fields MUST BE THE LAST FIELDS IN THE STRUCTURE
 */

typedef uint16 Prs2StubId;

typedef struct Prs2OneStubData {
    ObjectId ruleId;
    Prs2StubId stubId;
    int counter;
    RuleLock lock;
    LispValue qualification;
} Prs2OneStubData;

typedef Prs2OneStubData *Prs2OneStub;

typedef struct Prs2StubData {
    int numOfStubs;	/* number of Stub Records */
    Prs2OneStub *stubRecords;	/* an array of pointers to the stub records */
} Prs2StubData;

typedef Prs2StubData *Prs2Stub;

/*------------------------------------------------------------------------
 *		Prs2RawStub
 *
 * This is exactly the same as a varlena struct.
 *------------------------------------------------------------------------
 */

typedef struct varlena Prs2RawStubData;
typedef Prs2RawStubData *Prs2RawStub;

/*------------------------------------------------------------------------
 *		Prs2StubStats
 * used to keep some statistics (about rule stubs & rule locks
 * added/deleted.
 *------------------------------------------------------------------------
 */

#define PRS2_ADDSTUB		1
#define PRS2_DELETESTUB		2
#define PRS2_ADDLOCK		3
#define PRS2_DELETELOCK		4

typedef struct Prs2StubStatsData {
    int stubsAdded;
    int stubsDeleted;
    int locksAdded;
    int locksDeleted;
} Prs2StubStatsData;

typedef Prs2StubStatsData *Prs2StubStats;

/*========================================================================
 *		VARIOUS ROUTINES
 *========================================================================
 */

/*================= ROUTINES IN FILE 'stubraw.c' =======================*/

/*-------------------------
 * Prs2StubToRawStub:
 * given a 'Prs2Stub' create the equivalent 'Prs2RawStub'
 */
extern
Prs2RawStub
prs2StubToRawStub ARGS((
	Prs2Stub	stub;
));

/*-------------------------
 * Prs2RawStubToStub:
 * given a 'Prs2RawStub' create the equivalent 'Prs2Stub'
 */
extern
Prs2Stub
prs2RawStubToStub ARGS((
	Prs2RawStub	rawStub;
));


/*================ ROUTINES IN FILE 'stubutil.c' ==================*/

/*-------------------------
 * prs2StubQualIsEqual
 *
 * returns true if the given 'Prs2StubQual' point to the same stub
 * qualifications.
 */
extern
bool
prs2StubQualIsEqual ARGS((
    LispValue		q1,
    LispValue		q2
));

/*-------------------------
 * prs2OneStubIsEqual
 *
 * Return true iff the two given 'Prs2OneStub' pointers point to
 * equal structures.
 */
extern
bool
prs2OneStubIsEqual ARGS((
	Prs2OneStub	stub1,
	Prs2OneStub	stub2
));

/*-------------------------
 * prs2SearchStub:
 * given a `Prs2Stub', return the index of the
 * stub record (Prs2OneStub) that is equal to the given
 * `Prs2OneStub'
 * If no such record exist, return -1
 */
extern
int
prs2SearchStub ARGS((
	Prs2Stub	stubs,
	Prs2OneStub	oneStub,
));

/*-------------------------
 * prs2AddOneStub:
 * add a new stub record (Prs2OneStub) to the given stub records.
 * NOTE 1: We do NOT make a copy of the 'newStub'. So DO NOT pfree it
 */
extern
void
prs2AddOneStub ARGS((
	Prs2Stubs	oldStubs,
	Prs2OneStub	newStub
));

/*-------------------------
 * prs2DeleteOneStub
 * delete the given 'Prs2OneStub'
 */
extern
void
prs2DeleteOneStub ARGS((
	Prs2Stub	oldStubs,
	Prs2OneStub	deletedStub
));

/*-------------------------
 * prs2RemoveStubsOfRule
 * remove (in place) all the stubs of the given rule.
 * Return true if some stubs have been removed, or false if no stubs
 * for this rule were found.
 */
extern
bool
prs2RemoveStubsOfRule ARGS((
	Prs2Stub	stubs,
	ObjectId	ruleId
));

/*-------------------------
 * prs2MakeStub
 * create an empty 'Prs2Stub'
 */
extern
Prs2Stub
prs2MakeStub ARGS((
));

/*-------------------------
 * prs2MakeOneStub
 * create an empty 'Prs2OneStub' record
 */
extern
Prs2OneStub
prs2MakeOneStub ARGS((
));

/*================= ROUTINES IN FILE 'stubinout.c' =======================*/

/*-------------------------
 * prs2StubToString:
 * given a Prs2Stub transform it to a humanly readable string
 */
extern
char *
prs2StubToString ARGS((
	Prs2Stub	stub
));

/*-------------------------
 * prs2StringToStub:
 * given a string (as generated by prs2StubToString)
 * recreate the Prs2Stub
 */
extern
Prs2Stub
prs2StringToStub ARGS((
	char	*string
));

/*-------------------------
 * stubout:
 * given a Prs2RawStub transform it to a humanly readable string
 */
extern
char *
stubout ARGS((
	Prs2RawStub	stub
));

/*-------------------------
 * stubin:
 * given a string (as generated by stubout)
 * recreate the Prs2RawStub
 */
extern
Prs2RawStub
stubin ARGS((
	char	*string
));

/*================= ROUTINES IN FILE 'stubrel.c' =======================*/

/*-------------------------
 * prs2AddRelationStub:
 * add a new relation level stub record to the given relation.
 */
extern
void
prs2AddRelationStub ARGS((
	Relation	relation;
	Prs2OneStub	stub;
));

/*-------------------------
 * prs2DeleteRelationStub:
 * delete the given relation level stub record from the given relation.
 */
extern
void
prs2DeleteRelationStub ARGS((
	Relation	relation;
	Prs2OneStub	stub;
));
/*-------------------------
 * prs2ReplaceRelationStub:
 * Replace the relation stubs of a relation with the given ones
 * (the old ones are completely ignored)
 */
extern
void
prs2ReplaceRelationStub ARGS((
	ObjectId	relationOid,
	Prs2Stub	stubs
));

/*-------------------------
 * prs2GetRelationStubs
 * given a relation OID, find all the associated rule stubs.
 */
extern
Prs2Stub
prs2GetRelationStubs ARGS((
	ObjectId	relOid
));

/*-------------------------
 * prs2GetStubsFromPrs2StubTuple:
 * given a tuple of the pg_prs2stub relation, extract the value
 * of the stubs field.
 */
extern
Prs2Stub
prs2GetStubsFromPrs2StubTuple ARGS((
    	HeapTuple		tuple,
	Buffer			buffer,
	TupleDesccriptor	tupleDesc
));

/*-------------------------
 * prs2PutStubsInPrs2StubTuple:
 * put the given stubs to the appropriate attribute of a 
 * pg_prs2stub relation tuple.
 * Returns the new tuple
 */
extern
HeapTuple
prs2PutStubsInPrs2StubTuple ARGS((
    	HeapTuple		tuple,
	Buffer			buffer,
	TupleDesccriptor	tupleDesc,
	Prs2Stub		stubs
));

/*================= ROUTINES IN FILE 'stubtuple.c' =======================*/

/*-------------------------
 * prs2StubGetLocksForTuple
 * given a collection of stub records and a tuple, find all the locks
 * that the tuple must inherit.
 *-------------------------
 */
extern
RuleLock
prs2StubGetLocksForTuple ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	TupleDescriptor	tupDesc;
	Prs2Stub	stubs
));

/*-------------------------
 * prs2StubQualTestTuple
 * test if a tuple satisfies the qualification of a
 * stub record.
 *-------------------------
 */
extern
bool
prs2StubQualTestTuple ARGS((
    HeapTuple		tuple,
    Buffer		buffer,
    TupleDescriptor	tupDesc,
    LispValue		qual
));

/*================= ROUTINES IN FILE 'stubjoin.c' =======================*/

/*--------------------------
 * prs2MakeStubForInnerRelation:
 * Create the 'prs2OneStub' corresponding to the inner relation
 * when proccessing a Join.
 */
extern
Prs2OneStub
prs2MakeStubForInnerRelation ARGS((
    JoinRuleInfo	ruleInfo,
    HeapTuple		tuple,
    Buffer		buffer,
    TupleDescriptor	outerTupleDesc
));

/*--------------------------
 * prs2AddLocksAndReplaceTuple:
 * test the given tuple to see if it satisfies the stub qualification,
 * and if yes add to it the corresponding rule lock.
 */
extern
bool
prs2AddLocksAndReplaceTuple ARGS((
    HeapTuple	tuple,
    Buffer	buffer,
    Relation	relation,
    Prs2OneStub oneStub,
    RuleLock	lock
));

/*--------------------------
 * prs2UpdateStats:
 * used for benchmarks... Keep some statistics about rule stub records
 * and stuff...
 */
extern
void
prs2UpdateStats ARGS((
    JoinRuleInfo	ruleInfo,
    int			operation;
));

#endif Prs2StubIncluded
