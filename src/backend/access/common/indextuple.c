/* ----------------------------------------------------------------
 *   FILE
 *	indextuple.c
 *	
 *   DESCRIPTION
 *	This file contains index tuple accessor and mutator
 *	routines, as well as a few various tuple utilities.
 *
 *   INTERFACE ROUTINES
 *	index_formtuple
 *	index_getsysattr
 *	index_getattr
 *
 *   OLD INTERFACE FUNCTIONS
 *	formituple, FormIndexTuple
 *	AMgetattr
 *	
 *   NOTES
 *	All the stupid ``GeneralResultXXX'' routines should be
 *	eliminated and replaced with something simple and clean.
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include "tmp/c.h"
#include "access/ibit.h"
#include "access/itup.h"
#include "access/tupdesc.h"
#include "access/attval.h"

#include "storage/form.h"
#include "storage/itemptr.h"
#include "rules/rlock.h"

#include "utils/log.h"
#include "utils/palloc.h"

RcsId("$Header$");

/* ----------------------------------------------------------------
 *			index tuple predicates
 * ----------------------------------------------------------------
 */
bool
IndexTupleIsValid(tuple)
    IndexTuple	tuple;
{
    return (bool)
	PointerIsValid(tuple);
}

ItemPointer
IndexTupleGetRuleLockItemPointer(tuple)
    IndexTuple	tuple;
{
    Assert(IndexTupleIsValid(tuple));
    return (&tuple->t_lock.l_ltid);
}

RuleLock
IndexTupleGetRuleLock(tuple)
    IndexTuple	tuple;
{
    Assert(IndexTupleIsValid(tuple));
    return (tuple->t_lock.l_lock);
}

bool
IndexAttributeBitMapIsValid(bits)
    IndexAttributeBitMap bits;
{
    return (bool)
	PointerIsValid(bits);
}

IndexAttributeBitMap
IndexTupleGetIndexAttributeBitMap(tuple)
    IndexTuple	tuple;
{
    Assert(IndexTupleIsValid(tuple));
    return (&tuple->bits);
}

Form
IndexTupleGetForm(tuple)
    IndexTuple	tuple;
{
    Assert(IndexTupleIsValid(tuple));
    return ((Form) &tuple[1]);
}


/* ----------------------------------------------------------------
 *	      misc index result stuff (XXX OBSOLETE ME!)
 * ----------------------------------------------------------------
 */
bool
GeneralInsertIndexResultIsValid(result)
    GeneralInsertIndexResult	result;
{
    return (bool)
	PointerIsValid(result);
}

bool
InsertIndexResultIsValid(result)
    InsertIndexResult	result;
{
    return (bool)
	PointerIsValid(result);
}

bool
GeneralRetrieveIndexResultIsValid(result)
    GeneralRetrieveIndexResult	result;
{
    return (bool)
	PointerIsValid(result);
}

bool
RetrieveIndexResultIsValid(result)
    RetrieveIndexResult	result;
{
    return (bool)
	PointerIsValid(result);
}

ItemPointer
GeneralInsertIndexResultGetItemPointer(result)
    GeneralInsertIndexResult	result;
{
    Assert(GeneralInsertIndexResultIsValid(result));
    return (&result->pointerData);
}

RuleLock
GeneralInsertIndexResultGetRuleLock(result)
    GeneralInsertIndexResult	result;
{
    Assert(GeneralInsertIndexResultIsValid(result));
    return (result->lock);
}

GeneralInsertIndexResult
ItemPointerFormGeneralInsertIndexResult(pointer, lock)
    ItemPointer	pointer;
    RuleLock	lock;
{
    GeneralInsertIndexResult	result;

    Assert(ItemPointerIsValid(pointer));
    /* XXX Assert(RuleLockIsValid(lock)); locks don't work yet */

    result = (GeneralInsertIndexResult) palloc(sizeof *result);

    result->pointerData = *pointer;
    result->lock = lock;
    return (result);
}

ItemPointer
InsertIndexResultGetItemPointer(result)
    InsertIndexResult	result;
{
    Assert(InsertIndexResultIsValid(result));
    return (&result->pointerData);
}

RuleLock
InsertIndexResultGetRuleLock(result)
    InsertIndexResult	result;
{
    Assert(InsertIndexResultIsValid(result));
    return (result->lock);
}

double
InsertIndexResultGetInsertOffset(result)
    InsertIndexResult	result;
{
    Assert(InsertIndexResultIsValid(result));
    return (result->offset);
}

InsertIndexResult
ItemPointerFormInsertIndexResult(pointer, lock, offset)
    ItemPointer	pointer;
    RuleLock	lock;
    double		offset;
{
    InsertIndexResult	result;

    Assert(ItemPointerIsValid(pointer));
    /* XXX Assert(RuleLockIsValid(lock)); locks don't work yet */
    /* Assert(InsertOffsetIsValid(offset)); */

    result = (InsertIndexResult) palloc(sizeof *result);

    result->pointerData = *pointer;
    result->lock = lock;
    result->offset = offset;
    return (result);
}

ItemPointer
GeneralRetrieveIndexResultGetHeapItemPointer(result)
    GeneralRetrieveIndexResult	result;
{
    Assert(GeneralRetrieveIndexResultIsValid(result));
    return (&result->heapItemData);
}

GeneralRetrieveIndexResult
ItemPointerFormGeneralRetrieveIndexResult(heapItemPointer)
    ItemPointer	heapItemPointer;
{
    GeneralRetrieveIndexResult	result;
    Assert(ItemPointerIsValid(heapItemPointer));

    result = (GeneralRetrieveIndexResult) palloc(sizeof *result);
    result->heapItemData = *heapItemPointer;
    return (result);
}

ItemPointer
RetrieveIndexResultGetIndexItemPointer(result)
    RetrieveIndexResult	result;
{
    Assert(RetrieveIndexResultIsValid(result));
    return (&result->indexItemData);
}

ItemPointer
RetrieveIndexResultGetHeapItemPointer(result)
    RetrieveIndexResult	result;
{
    Assert(RetrieveIndexResultIsValid(result));
    return (&result->heapItemData);
}

RetrieveIndexResult
ItemPointerFormRetrieveIndexResult(indexItemPointer, heapItemPointer)
    ItemPointer	indexItemPointer;
    ItemPointer	heapItemPointer;
{
    RetrieveIndexResult	result;

    Assert(ItemPointerIsValid(indexItemPointer));
    Assert(ItemPointerIsValid(heapItemPointer));

    result = (RetrieveIndexResult) palloc(sizeof *result);

    result->indexItemData = *indexItemPointer;
    result->heapItemData = *heapItemPointer;

    return (result);
}

/* ----------------------------------------------------------------
 *		  index_ tuple interface routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	index_formtuple
 * ----------------
 */
IndexTuple
index_formtuple(numberOfAttributes, tupleDescriptor, value, null)
    AttributeNumber	numberOfAttributes;
    TupleDescriptor	tupleDescriptor;
    Datum		value[];
    char		null[];
{
    register char	*tp;	/* tuple pointer */
    IndexTuple		tuple;	/* return tuple */
    Size		size;
    
    size = sizeof *tuple;
    if (numberOfAttributes > MaxIndexAttributeNumber)
	elog(WARN, "FormIndexTuple: numberOfAttributes of %d > %d",
	     numberOfAttributes, MaxIndexAttributeNumber);
    
    size += ComputeDataSize(numberOfAttributes, tupleDescriptor, value, null);
    
    tp = (char *) palloc(size);
    tuple = (IndexTuple) tp;
    bzero(tp, (int)size);
    
    DataFill((Pointer) &tp[sizeof *tuple],
	     numberOfAttributes,
	     tupleDescriptor,
	     value,
	     null,
	     &tuple->bits.bits[0]);
    
    /* ----------------
     * initialize metadata
     * ----------------
     */
    tuple->t_size = size;
    tuple->t_locktype = MEM_INDX_RULE_LOCK;
    tuple->t_lock.l_lock = NULL;
    
    return (tuple);
}

/* ----------------
 *	fastgetiattr
 * ----------------
 */
AttributeValue
fastgetiattr(tuple, attributeNumber, tupleDescriptor, isNullOutP)
    IndexTuple		tuple;
    int			attributeNumber;
    TupleDescriptor	tupleDescriptor;
    Boolean		*isNullOutP;
{
    int			bitmask;
    bits8		bitMap;
    Attribute		*ap;	/* attribute pointer */
    char		*tp;	/* tuple pointer */
    int			finalbit;
    int			bitrange;
    AttributeValue	value;
    AttributeNumber	attributeOffset;

    /* XXX quick hack to clear garbage in filler */
    bzero(&value, sizeof(AttributeValue));
	
    /* ----------------
     *	check for null attributes
     * ----------------
     */
    attributeOffset = attributeNumber - 1;
    finalbit = 1 << attributeOffset;
    bitMap = tuple->bits.bits[0];
    if (! (bitMap & finalbit)) {
	if (PointerIsValid(isNullOutP))
	    *isNullOutP = (Boolean)1;
	value = PointerGetDatum(NULL); 
	return (value);
    }
    
    /* ----------------
     *	now walk the tuple descriptor until we get to the attribute
     *  we want.  we advance tp the length of each attribute we advance
     *  so tp points to our attribute when we're done.
     * ----------------
     */
    *isNullOutP = (Boolean)0;
    ap = &tupleDescriptor->data[0];
    tp = (char *)tuple + sizeof *tuple;

    bitrange = finalbit >> 1;
    for (bitmask = 1; bitmask <= bitrange; bitmask <<= 1) {
	if (bitMap & bitmask)
	    if ((*ap)->attlen < 0) {
		tp = (char *) LONGALIGN(tp) + sizeof (long);
		tp += PSIZE(tp);
	    } else if ((*ap)->attlen >= 3) {
		tp = (char *) LONGALIGN(tp);
		tp += (*ap)->attlen;
	    } else if ((*ap)->attlen == 2)
		tp = (char *) SHORTALIGN(tp + 2);
	    else if (!(*ap)->attlen)
		elog(WARN, "amgetattr: 0 attlen");
	    else
		tp++;
	ap++;
    }

    /* ----------------
     *	handle variable length attributes
     * ----------------
     */
    if ((*ap)->attlen < 0) {
	value = PointerGetDatum((char *)LONGALIGN(tp + sizeof (long)));
	return (value);
    }

    if (!(*ap)->attbyval) {
	/* ----------------
	 *  by value attributes
	 * ----------------
	 */
	if ((*ap)->attlen >= 3) {
	    value = PointerGetDatum((char *)LONGALIGN(tp));
	} else if ((*ap)->attlen == 2) {
	    value = PointerGetDatum((char *)SHORTALIGN(tp));
	} else {
	    value = PointerGetDatum(tp);
	   }
    } else {
	/* ----------------
	 *  by reference attributes
	 *
	 *  We can't assume that ptr alignment is correct or that casting
	 *  will work right; we used bcopy() to write the datum, so we
	 *  use bcopy() to read it.
	 * ----------------
	 */
	bcopy(tp, (char *) &value, sizeof value);
    }

    return (value);
}


/* ----------------
 *	index_getsysattr
 * ----------------
 */
AttributeValue
index_getsysattr(tuple, attributeNumber)
    IndexTuple		tuple;
    int			attributeNumber;
{
    static Datum	datum;
    
    switch (attributeNumber) {
    case IndxRuleLockAttributeNumber:
	elog(NOTICE, "index_getsysattr: Rule locks are always NULL");
	return PointerGetDatum((Pointer) NULL);
	break;
	
    case IndxBaseTupleIdAttributeNumber:
	bcopy((char *) &tuple->t_tid, (char *) &datum, sizeof datum);
	return datum;
	break;
	
    default:
	elog(WARN, "IndexTupleGetAttributeValue: undefined attnum %d",
	     attributeNumber);
    }
}

/* ----------------
 *	index_getattr
 * ----------------
 */
AttributeValue
IndexTupleGetAttributeValue(tuple, attNum, tupleDescriptor, isNullOutP)
    IndexTuple		tuple;
    int			attNum;
    TupleDescriptor	tupleDescriptor;
    Boolean		*isNullOutP;
{
    /* ----------------
     *	sanity check
     * ----------------
     */
    if (tuple == NULL)
	elog(WARN, "IndexTupleGetAttributeValue: called with NULL tuple");

    /* ----------------
     *	handle normal attributes
     * ----------------
     */
    if (attNum > 0)
	return
	    fastgetiattr(tuple, attNum, tupleDescriptor, isNullOutP);

    /* ----------------
     *	otherwise handle system attributes
     * ----------------
     */
    *isNullOutP = '\0';
    return
	index_getsysattr(tuple, attNum);
}

Pointer
index_getattr(tuple, attNum, tupDesc, isNullOutP)
    IndexTuple		tuple;
    AttributeNumber	attNum;
    TupleDescriptor	tupDesc;
    Boolean		*isNullOutP;
{
    Datum  datum;
    datum = IndexTupleGetAttributeValue(tuple, attNum, tupDesc, isNullOutP);
    
    return
	DatumGetPointer(datum);
}

/* ----------------------------------------------------------------
 *			old interface routines
 * ----------------------------------------------------------------
 */
IndexTuple
FormIndexTuple(numberOfAttributes, tupleDescriptor, value, null)
    AttributeNumber	numberOfAttributes;
    TupleDescriptor	tupleDescriptor;
    Datum		value[];
    char		null[];
{
    return
	index_formtuple(numberOfAttributes, tupleDescriptor, value, null);
}

IndexTuple
formituple(numberOfAttributes, tupleDescriptor, value, null)
    AttributeNumber	numberOfAttributes;
    TupleDescriptor	tupleDescriptor;
    Datum		value[];
    char		null[];
{
    return
	index_formtuple(numberOfAttributes, tupleDescriptor, value, null);
}

Pointer
AMgetattr(tuple, attNum, tupleDescriptor, isNullOutP)
    IndexTuple		tuple;
    AttributeNumber	attNum;
    TupleDescriptor	tupleDescriptor;
    Boolean		*isNullOutP;
{
    return index_getattr(tuple, attNum, tupleDescriptor, isNullOutP);
}
