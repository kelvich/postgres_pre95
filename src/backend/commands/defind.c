/*
 * defind.c --
 *	POSTGRES define index utility code.
 *
 * Note:
 *	XXX Generally lacks argument checking....
 */

#include "c.h"

#include "anum.h"
#include "attnum.h"
#include "catname.h"
#include "genam.h"
#include "heapam.h"
#include "log.h"
#include "name.h"
#include "oid.h"
#include "rel.h"
#include "relscan.h"
#include "rproc.h"
#include "skey.h"
#include "trange.h"

#include "defind.h"

RcsId("$Header$");

/*
 * AccessMethodNameGetObjectId --
 *	Returns the object identifier for an access method given its name.
 *
 * Note:
 *	Assumes name is valid.
 *
 *	Aborts if access method does not exist.
 */
extern
ObjectId
AccessMethodNameGetObjectId ARGS((
	Name		accessMethodName;
));

/*
 * OperatorClassNameGetObjectId --
 *	Returns the object identifier for an operator class given its name.
 *
 * Note:
 *	Assumes name is valid.
 *	Assumes relation descriptor is valid.
 *
 *	Aborts if operator class does not exist.
 */
extern
ObjectId
OperatorClassNameGetObjectId ARGS((
	Name		operatorClassName;
	Relation	operatorClassRelation;
));

void
DefineIndex(heapRelationName, indexRelationName, accessMethodName,
		numberOfAttributes, attributeNumber, className, parameterCount,
		parameter)
	Name		heapRelationName;
	Name		indexRelationName;
	Name		accessMethodName;
	AttributeNumber	numberOfAttributes;
	AttributeNumber	attributeNumber[];
	Name		className[];
	uint16		parameterCount;
	Datum		parameter[];
{
	AttributeOffset	attributeOffset;
	ObjectId	*classObjectId;
	Relation	relation;

	classObjectId = LintCast(ObjectId *,
		palloc(numberOfAttributes * sizeof classObjectId[0]));

	relation = amopenr(OperatorClassRelationName);
	for (attributeOffset = 0; attributeOffset < numberOfAttributes;
			attributeOffset += 1) {
		classObjectId[attributeOffset] =
			OperatorClassNameGetObjectId(className[attributeOffset],
				relation);
	}
	amclose(relation);

	AMcreati(heapRelationName, indexRelationName,
		AccessMethodNameGetObjectId(accessMethodName),
		numberOfAttributes, attributeNumber, classObjectId,
		parameterCount, parameter);
}

static
ObjectId
AccessMethodNameGetObjectId(accessMethodName)
	Name		accessMethodName;
{
	HeapTuple	tuple;
	Relation	relation;
	HeapScanDesc	scan;
	ObjectId	accessMethodObjectId;
	Buffer		buffer;
	ScanKeyData	key[1];

	Assert(NameIsValid(accessMethodName));

	key[0].data[0].flags = 0;
	key[0].data[0].attributeNumber = AccessMethodNameAttributeNumber;
	key[0].data[0].procedure = Character16EqualRegProcedure;
	key[0].data[0].argument.name.value = accessMethodName;

	relation = amopenr(AccessMethodRelationName);
	scan = ambeginscan(relation, 0, DefaultTimeRange, 1, &key[0]);

	tuple = amgetnext(scan, 0, &buffer);

	if (!HeapTupleIsValid(tuple)) {
		amendscan(scan);
		elog(WARN, "AccessMethodNameGetObjectId: unknown AM %s",
			accessMethodName);
	}
	accessMethodObjectId = tuple->t_oid;

	amendscan(scan);
	amclose(relation);

	return (accessMethodObjectId);
}

static
ObjectId
OperatorClassNameGetObjectId(operatorClassName, operatorClassRelation)
	Name		operatorClassName;
	Relation	operatorClassRelation;
{
	HeapTuple	tuple;
	HeapScanDesc	scan;
	ObjectId	classObjectId;
	Buffer		buffer;
	ScanKeyData	key[1];

	Assert(NameIsValid(operatorClassName));
	Assert(RelationIsValid(operatorClassRelation));

	key[0].data[0].flags = 0;
	key[0].data[0].attributeNumber = OperatorClassNameAttributeNumber;
	key[0].data[0].procedure = Character16EqualRegProcedure;
	key[0].data[0].argument.name.value = operatorClassName;

	scan = ambeginscan(operatorClassRelation, 0, DefaultTimeRange, 1,
		&key[0]);

	tuple = amgetnext(scan, 0, &buffer);

	if (!HeapTupleIsValid(tuple)) {
		amendscan(scan);
		elog(WARN, "OperatorClassNameGetObjectId: unknown class %s",
			operatorClassName);
	}
	classObjectId = tuple->t_oid;

	amendscan(scan);

	return (classObjectId);
}
