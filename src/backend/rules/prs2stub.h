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
 * contain a lot of variabkle length information, main memory
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


#include "c.h"
#include "oid.h"
#include "attnum.h"
#include "prs2.h"

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
 *	stubRecors: an array of 'Prs2OneStubData' structures, holding
 *		information about each individual stub record.
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
 *	numQuals: each stub record has a qual associated with it.
 *		this qualification is the conjuction of one or more
 *		simple qualifications (see Prs2SimpleQualData).
 *		`numQuals' is the number of these simple qualifications.
 *		If it is equal to 0, the qualification always evaluates
 *		to true.
 *	qualification: the array of the simple qualifications mentioned
 *		before.
 *
 *	NOTE: XXX!!!!!!!!!!!!!  THIS IS A HACK !!!!!!!!!!
 *	'lock' and 'qualification' are pointers to variable length
 *	fields. In order to make transformation between the 'Prs2Stub'
 *	format to 'Prs2RawStub' and vice versa easier, 
 *	these fields MUST BE THE LAST FIELDS IN THE STRUCTURE
 *
 * Prs2SimpleQualData: this struct contains information about one
 *	of the "simple qualifications" mentioned before.
 *	These have the format:
 *	<attributeNumber> <operator> <Constant>
 *	Where <operator> must return a boolean value.
 *	In order to test the qualification, we extract
 *	(using amgetattr) the value of the tuple's attribute
 *	specified by <attributeNumber> and pass to the operator
 *	this value and the <Constant> as arguments.
 *	This structure has the following fields:
 *	attrNo: the attribute Number
 *	operator: the oid of the operator.
 *	constType: the (oid of the) type of the constant.
 *	constLength: the length of the constant.
 *	constData: an array of `constLength' bytes, holding the
 *		value of the constant.
 *		NOTE: there might be alignment problems here...
 *		However these bytes are guaranteed to start
 *		at word boundaries (because they have been
 *		allocated using 'palloc()').
 */

typedef uint16 Prs2StubId;

typedef struct Prs2SimpleQualData {
    AttributeNumber attrNo;
    ObjectId operator;
    ObjectId constType;
    Size constLength;
    char * constData;	/* XXX	THIS MUST BE THE LAST FIELD OF THE STRUCT */
} Prs2SimpleQualData;

typedef Prs2SimpleQualData *Prs2SimpleQual;


typedef struct Prs2OneStubData {
    ObjectId ruleId;
    Prs2StubId stubId;
    int counter;
    int numQuals;
    /* XXX THE FOLLOWING FIELDS MUST BE THE LAST FIELD IN THE STRUCT */
    RuleLock lock;
    Prs2SimpleQual qualification;
} Prs2OneStubData;

typedef Prs2OneStubData *Prs2OneStub;

typedef struct Prs2StubData {
    int numOfStubs;	/* number of Stub Records */
    Prs2OneStub stubRecords;
} Prs2StubData;

typedef Prs2StubData *Prs2Stub;

/*------------------------------------------------------------------------
 *		Prs2RawStub
 *------------------------------------------------------------------------
 */

typedef struct Prs2RawStubData {
    long size;
    char data[1];
} Prs2RawStubData;

typedef Prs2RawStubData *Prs2RawStub;

/*------------------------------------------------------------------------
 *		VARIOUS ROUTINES
 *------------------------------------------------------------------------
 */

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

/*-------------------------
 * prs2SearchStub:
 * given a `Prs2Stub', return a pointer to the 
 * stub record (Prs2OneStub) that is equal to the given
 * `Prs2OneStub'
 * If no such record exist, return NULL
 */
extern
Prs2OneStub
prs2SearchStub ARGS((
	Prs2Stub	stubs,
	Prs2OneStub	oneStub,
));

/*-------------------------
 * prs2AddOneStub:
 * add a new stub record (Prs2OneStub) to the given stub records.
 * NOTE: 'oldStubs->stubRecords' might be 'pfreed' so if you have
 * pointers pointing there you might be in trouble!
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
 * prs2AddRelStub:
 * add a new relation level stub record the the given relation.
 */
extern
void
prs2AddRelStub ARGS((
	Relation	relation;
	Prs2Stub	stub;
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
 * prs2FreeStub:
 * free the space occupied by a 'Prs2Stub'
 */
extern
void
prs2FreeStub ARGS((
	Prs2Stub	stubs
));

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


#endif Prs2StubIncluded
