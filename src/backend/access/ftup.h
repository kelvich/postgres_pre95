/*
 * ftup.h --
 *	POSTGRES definitions for tuple formation and modification.
 *
 * Identification:
 *	$Header$
 */

#ifndef	FTupIncluded	/* Include this file only once. */
#define FTupIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "att.h"
#include "attnum.h"
#include "itup.h"
#include "htup.h"
#include "tupdesc.h"

/*
 * FormHeapTuple --
 *	Returns a palloc'd heap tuple.
 */
extern
HeapTuple
FormHeapTuple ARGS((
	AttributeNumber	numberOfAttributes,
	TupleDescriptor	tupleDescriptor,
	Datum		datum[],
	char		null[]
));

/*
 * FormIndexTuple --
 *	Returns a palloc'd heap tuple.
 */
extern
IndexTuple
FormIndexTuple ARGS((
	AttributeNumber	numberOfAttributes,
	TupleDescriptor	tupleDescriptor,
	Datum		datum[],
	char		null[]
));

/*
 * ModifyHeapTuple --
 *	Returns a palloc'd heap tuple.
 *
 * Note:
 *	Assumes the tuple is valid.
 *	Assumes either the buffer or relation is valid.
 *	For now, assumes replaceValue, replaceNull, and replace are
 *	fully specified.
 */
extern
HeapTuple
ModifyHeapTuple ARGS((
	HeapTuple	tuple,
	Buffer		buffer,
	Relation	relation,
	Datum		replaceValue[],
	char		replaceNull[],
	char		replace[]
));

extern
HeapTuple
addtupleheader ARGS((
));

#endif	/* !defined(FTupIncluded) */
