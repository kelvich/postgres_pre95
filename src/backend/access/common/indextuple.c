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
 *   NOTES
 *	All the stupid ``GeneralResultXXX'' routines should be
 *	eliminated and replaced with something simple and clean.
 *	
 *	old routines formituple, FormIndexTuple, AMgetattr have been
 *	turned into macros in itup.h.  IsValid predicates have been
 *	macroized too. -cim 4/30/91
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
#include "access/tupmacs.h"

#include "storage/form.h"
#include "storage/itemptr.h"
#include "rules/rlock.h"

#include "utils/log.h"
#include "utils/palloc.h"

RcsId("$Header$");

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
	elog(WARN, "index_formtuple: numberOfAttributes of %d > %d",
	     numberOfAttributes, MaxIndexAttributeNumber);
    
    size += ComputeDataSize(numberOfAttributes, tupleDescriptor, value, null);
    size = LONGALIGN(size);
    
    tp = (char *) palloc(size);
    tuple = (IndexTuple) tp;
    bzero(tp, (int)size);
    
    DataFill((Pointer) &tp[sizeof *tuple],
	     numberOfAttributes,
	     tupleDescriptor,
	     value,
	     null,
		 &tuple->t_infomask,
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
 *
 *	This is a newer version of fastgetattr which attempts to be
 *	faster by caching attribute offsets in the attribute descriptor.
 *
 *	an alternate way to speed things up would be to cache offsets
 *	with the tuple, but that seems more difficult unless you take
 *	the storage hit of actually putting those offsets into the
 *	tuple you send to disk.  Yuck.
 *
 *	This scheme will be slightly slower than that, but should
 *	preform well for queries which hit large #'s of tuples.  After
 *	you cache the offsets once, examining all the other tuples using
 *	the same attribute descriptor will go much quicker. -cim 5/4/91
 * ----------------
 */

char *
fastgetiattr(tup, attnum, att, isnull)
    IndexTuple	tup;
    unsigned	attnum;
    struct	attribute *att[];
    bool	*isnull;
{
    register char		*tp;		/* ptr to att in tuple */
    register char		*bp;		/* ptr to att in tuple */
    int 			slow;		/* do we have to walk nulls? */

    /* ----------------
     *	sanity checks
     * ----------------
     */

    Assert(PointerIsValid(isnull));
    Assert(attnum > 0);

    /* ----------------
	 *   Three cases:
	 * 
	 *   1: No nulls and no variable length attributes.
	 *   2: Has a null or a varlena AFTER att.
	 *   3: Has nulls or varlenas BEFORE att.
	 * ----------------
	 */

    *isnull =  false;

    if (IndexTupleNoNulls(tup))
    {
	/* first attribute is always at position zero */

	if (attnum == 1)
	{
 	    return(fetchatt(att, (Pointer) tup + sizeof(*tup)));
	}
	attnum--;

	if (att[attnum]->attcacheoff > 0)
	{
	    return(fetchatt(att + attnum, (Pointer) tup
				+ sizeof(*tup) + att[attnum]->attcacheoff));
	}

	tp = (Pointer) tup + sizeof(*tup);

	slow = 0;
    }
    else /* there's a null somewhere in the tuple */
    {
	bp = (char *) tup->bits.bits;
	slow = 0;
        /* ----------------
         *	check to see if desired att is null
         * ----------------
         */

	attnum--;
	{
	    if (att_isnull(attnum, bp)) 
	    {
		*isnull = true;
		return NULL;
	    }
	}
        /* ----------------
	 *      Now check to see if any preceeding bits are null...
         * ----------------
	 */
	{
	    register int  i = 0; /* current offset in bp */
	    register int  mask;	 /* bit in byte we're looking at */
	    register char n;	 /* current byte in bp */
	    register int byte, finalbit;
	
	    byte = attnum >> 3;
	    finalbit = attnum & 0x07;

	    for (; i <= byte; i++) {
	        n = bp[i];
	        if (i < byte) {
		    /* check for nulls in any "earlier" bytes */
		    if ((~n) != 0) {
		        slow++;
		        break;
		    }
	        } else {
		    /* check for nulls "before" final bit of last byte*/
		    mask = (finalbit << 1) - 1;
		    if ((~n) & mask)
		        slow++;
	        }
	    }
        }
    }

    /* now check for any non-fixed length attrs before our attribute */

    if (!slow)
    {
	if (att[attnum]->attcacheoff > 0)
	{
	    return(fetchatt(att + attnum, tp + att[attnum]->attcacheoff));
	}
	else if (!IndexTupleAllFixed(tup))
	{
	    register int j = 0;

	    for (j = 0; j < attnum && !slow; j++)
		if (att[j]->attlen < 1) slow = 1;
	}
    }

    /*
     * if slow is zero, and we got here, we know that we have a tuple with
     * no nulls.  We also know that we have to initialize the remainder of
     * the attribute cached offset values.
     */

    if (!slow)
    {
	register int j = 1;
	register long off;

	/*
	 * need to set cache for some atts
	 */

	att[0]->attcacheoff = 0;

	while (att[j]->attcacheoff > 0) j++;

	off = att[j-1]->attcacheoff + att[j-1]->attlen;

	for (; j < attnum + 1; j++)
	{
	    /*
	     * Fix me when going to a machine with more than a four-byte
	     * word!
	     */

	    switch(att[j]->attlen)
	    {
		case sizeof(char) : break;
		case sizeof(short): off = SHORTALIGN(off); break;
		default           : off = LONGALIGN(off); break;
	    }

	    att[j]->attcacheoff = off;
	    off += att[j]->attlen;
	}

	return(fetchatt(att + attnum, tp + att[attnum]->attcacheoff));
    }
    else
    {
	register bool usecache = true;
	register int off = 0;
	register int savelen;
	register int i;

	/*
	 * Now we know that we have to walk the tuple CAREFULLY.
	 */
	
	for (i = 0; i < attnum; i++)
	{
	    if (!IndexTupleNoNulls(tup))
	    {
		if (att_isnull(i, bp))
		{
		    usecache = false;
		    continue;
		}
	    }

	    if (usecache && att[i]->attcacheoff > 0)
	    {
		off = att[i]->attcacheoff;
		if (att[i]->attlen == -1)
		{
		    usecache = false;
		}
		else continue;
	    }

	    if (usecache) att[i]->attcacheoff = off;
	    switch(att[i]->attlen)
	    {
	        case sizeof(char):
	            off++;
	            break;
	        case sizeof(short):
	            off = SHORTALIGN(off + sizeof(short));
	            break;
	        case -1:
	            usecache = false;
		    off = LONGALIGN(off) + sizeof(long);
	    	    off += PSIZE(tp + off);
		    break;
		default:
		    off = LONGALIGN(off + att[i]->attlen);
		    break;
	    }
	}

	return(fetchatt(att + attnum, tp + off));
    }
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
		(AttributeValue)
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
 *	      misc index result stuff (XXX OBSOLETE ME!)
 * ----------------------------------------------------------------
 */

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
