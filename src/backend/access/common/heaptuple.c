/* ----------------------------------------------------------------
 *   FILE
 *	heaptuple.c
 *	
 *   DESCRIPTION
 *	This file contains heap tuple accessor and mutator
 *	routines, as well as a few various tuple utilities.
 *
 *   INTERFACE ROUTINES
 *	heap_attisnull		
 *	heap_sysattrlen
 *	heap_sysattrbyval
 *	heap_getsysattr
 *	heap_getattr
 *	heap_addheader		
 *	heap_copytuple
 *	heap_formtuple 
 *	heap_modifytuple
 *	
 *   NOTES
 *	The old interface functions have been converted to macros
 *	and moved to heapam.h
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include "tmp/c.h"

#include "access/htup.h"
#include "access/itup.h"
#include "access/tupmacs.h"
#include "access/skey.h"
#include "rules/rac.h"
#include "storage/buf.h"
#include "storage/bufpage.h"		/* for MAXTUPLEN */
#include "storage/itempos.h"
#include "storage/itemptr.h"
#include "storage/page.h"
#include "storage/form.h"
#include "utils/memutils.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "utils/rel.h"
#include "rules/prs2.h"

/* this is so the sparcstation debugger works */

#ifndef NO_ASSERT_CHECKING
#ifdef sparc
#define register
#endif /* sparc */
#endif /* NO_ASSERT_CHECKING */

RcsId("$Header$");

void
set_use_cacheoffgetattr(x)
    int x;
{}

/* ----------------------------------------------------------------
 *			misc support routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	ComputeDataSize
 * ----------------
 */
Size
ComputeDataSize(numberOfAttributes, att, value, nulls)
    AttributeNumber	numberOfAttributes;
    Attribute           att[];
    Datum		value[];
    char		nulls[];
{
    register uint32	length;
    register int	i;

    for (length = 0, i = 0; i < numberOfAttributes; i++)
    {
	if (nulls[i] != ' ') continue;

	switch (att[i]->attlen) 
	{
	    case -1:
		length = LONGALIGN(length)
		       + PSIZE(DatumGetPointer(value[i]))
		       + sizeof (long);
		break;
	    case sizeof(char):
		length++;
		break;
	    case sizeof(short):
		length = SHORTALIGN(length + sizeof(short));
		break;
	    default:
		length = LONGALIGN(length) + att[i]->attlen;
		break;
	}
    }

    return length;
}

/* ----------------
 *	DataFill
 * ----------------
 */

void
DataFill(data, numberOfAttributes, att, value, nulls, infomask, bit)
    Pointer data;
    AttributeNumber	numberOfAttributes;
    Attribute		att[];
    Datum		value[];
    char		nulls[];
    char		*infomask;
    bits8		bit[];
{
    bits8	*bitP;
    int		bitmask;
    uint32	length;
    int		i;

    if (bit != NULL)
    {
        bitP = &bit[-1];
        bitmask = CSIGNBIT;
    }

    *infomask = 0;

    for (i = 0; i < numberOfAttributes; i++)
    {
	if (bit != NULL)
	{
	    if (bitmask != CSIGNBIT) {
	        bitmask <<= 1;
	    } else {
	        bitP += 1;
	        *bitP = 0x0;
	        bitmask = 1;
	    }

	    if (nulls[i] == 'n') 
	    {
	        *infomask |= 0x1;
	        continue;
	    }

	    *bitP |= bitmask;
	}

	switch (att[i]->attlen)
	{
	    case -1:
	        *infomask |= 0x2;
	        data = (Pointer) LONGALIGN((char *) data);
	        * (long *) data = length = PSIZE(DatumGetPointer(value[i]));
		data += sizeof(long);
	        bcopy(DatumGetPointer(value[i]), data, length);
	        data += length;
		break;
	    case sizeof(char):
		* (char *) data = (att[i]->attbyval ?
				   DatumGetChar(value[i]) :
				   * (char *) value[i]);
		data += sizeof(char);
		break;
	    case sizeof(short):
		data = (Pointer) SHORTALIGN(data);
		* (short *) data = (att[i]->attbyval ?
		                    DatumGetInt16(value[i]) :
				    * (short *) value[i]);
		data += sizeof(short);
		break;
	    case 3: /* XXX */
	    case sizeof(long):
		data = (Pointer) LONGALIGN(data);
		* (long *) data = (att[i]->attbyval ?
				   DatumGetInt32(value[i]) :
				   * (long *) value[i]);
		data += sizeof(long);
		break;
	    default:
		data = (Pointer) LONGALIGN(data);
		bcopy(DatumGetPointer(value[i]), data, att[i]->attlen);
		data += att[i]->attlen;
		break;

	}
    }
}

/* ----------------------------------------------------------------
 *			heap tuple interface
 * ----------------------------------------------------------------
 */

/* ----------------
 *	heap_attisnull	- returns 1 iff tuple attribute is not present
 * ----------------
 */
int
heap_attisnull(tup, attnum)
    HeapTuple	tup;
    int		attnum;
{
    register char	*bp;	
    register int	byte;
    register int	finalbit;

    if (attnum > (int)tup->t_natts)
	return (1);

    if (HeapTupleNoNulls(tup)) return(0);

    if (attnum > 0) {
	return(att_isnull(attnum - 1, tup->t_bits));
    } else
	switch (attnum) {
	case SelfItemPointerAttributeNumber:
	case RuleLockAttributeNumber:
	case ObjectIdAttributeNumber:
	case MinTransactionIdAttributeNumber:
	case MinCommandIdAttributeNumber:
	case MaxTransactionIdAttributeNumber:
	case MaxCommandIdAttributeNumber:
	case ChainItemPointerAttributeNumber:
	case AnchorItemPointerAttributeNumber:
	case MinAbsoluteTimeAttributeNumber:
	case MaxAbsoluteTimeAttributeNumber:
	case VersionTypeAttributeNumber:
	    break;

	case 0:
	    elog(WARN, "heap_attisnull: zero attnum disallowed");

	default:
	    elog(WARN, "heap_attisnull: undefined negative attnum");
	}

    return (0);
}

/* ----------------------------------------------------------------
 *		 system attribute heap tuple support
 * ----------------------------------------------------------------
 */

/* ----------------
 *	heap_sysattrlen
 *
 *	This routine returns the length of a system attribute.
 * ----------------
 */
int
heap_sysattrlen(attno)
    AttributeNumber	attno;
{
    HeapTupleData	f;
    int			len;
    switch (attno) {
	case SelfItemPointerAttributeNumber:
	    len = sizeof (f.t_ctid);
	    break;
	case RuleLockAttributeNumber:
	    len = sizeof f.t_lock;
	    break;
	case ObjectIdAttributeNumber:
	    len = sizeof f.t_oid;
	    break;
	case MinTransactionIdAttributeNumber:
	    len = sizeof f.t_xmin;
	    break;
	case MinCommandIdAttributeNumber:
	    len = sizeof f.t_cmin;
	    break;
	case MaxTransactionIdAttributeNumber:
	    len = sizeof f.t_xmax;
	    break;
	case MaxCommandIdAttributeNumber:
	    len = sizeof f.t_cmax;
	    break;
	case ChainItemPointerAttributeNumber:
	    len = sizeof (f.t_chain);
	    break;
	case AnchorItemPointerAttributeNumber:
		elog(WARN, "heap_sysattrlen: field t_anchor does not exist!");
	    break;
	case MinAbsoluteTimeAttributeNumber:
	    len = sizeof f.t_tmin;
	    break;
	case MaxAbsoluteTimeAttributeNumber:
	    len = sizeof f.t_tmax;
	    break;
	case VersionTypeAttributeNumber:
	    len = sizeof f.t_vtype;
	    break;
	default:
	    elog(WARN, "sysattrlen: System attribute number %d unknown.",
		 attno);
	    len = 0;
	    break;
    }
    return (len);
}

/* ----------------
 *	heap_sysattrbyval
 *
 *	This routine returns the "by-value" property of a system attribute.
 * ----------------
 */
bool
heap_sysattrbyval(attno)
    AttributeNumber	attno;
{
    HeapTupleData	f;
    bool		byval;
	
    switch (attno) {
	case SelfItemPointerAttributeNumber:
	    byval = false;
	    break;
	case RuleLockAttributeNumber:
	    byval = false;
	    break;
	case ObjectIdAttributeNumber:
	    byval = true;
	    break;
	case MinTransactionIdAttributeNumber:
	    byval = false;
	    break;
	case MinCommandIdAttributeNumber:
	    byval = true;
	    break;
	case MaxTransactionIdAttributeNumber:
	    byval = false;
	    break;
	case MaxCommandIdAttributeNumber:
	    byval = true;
	    break;
	case ChainItemPointerAttributeNumber:
	    byval = false;
	    break;
	case AnchorItemPointerAttributeNumber:
	    byval = false;
	    break;
	case MinAbsoluteTimeAttributeNumber:
	    byval = true;
	    break;
	case MaxAbsoluteTimeAttributeNumber:
	    byval = true;
	    break;
	case VersionTypeAttributeNumber:
	    byval = true;
	    break;
	default:
	    byval = true;
	    elog(WARN, "sysattrbyval: System attribute number %d unknown.",
		 attno);
	    break;
    }
    
    return byval;
}

/* ----------------
 *	heap_getsysattr
 * ----------------
 */
char *
heap_getsysattr(tup, b, attnum)
    HeapTuple	tup;
    Buffer	b;
    int		attnum;
{
    RuleLock	lock;
		
    switch (attnum) {
    case SelfItemPointerAttributeNumber:
	return ((char *)&tup->t_ctid);

    case RuleLockAttributeNumber:
	/*---------------
	 * A rule lock is ALWAYS non-null.
	 * 'HeapTupleGetRuleLock' will always return a valid
	 * rule lock.
	 * So the following 3 lines of code are obsolete &
	 * commented out.
	 *
	 * >>if (PointerIsValid(isnull) && !RuleLockIsValid(lock)) {
	 * >>  *isnull = (bool)1;
	 * >>}
	 *
	 * BTW, the reason for all that is that "ExecEvalExpr"
	 * refuses to evaluate an expression containing an
	 * InvalidRuleLock (if the 'isNull' atrgument is true)
	 * and returns a null Const node.
	 *---------------
	 */
	lock = HeapTupleGetRuleLock(tup, b);
	return ((char *)lock);
	
    case ObjectIdAttributeNumber:
	return ((char *)tup->t_oid);
    case MinTransactionIdAttributeNumber:
	return (tup->t_xmin);
    case MinCommandIdAttributeNumber:
	return ((char *)tup->t_cmin);
    case MaxTransactionIdAttributeNumber:
	return (tup->t_xmax);
    case MaxCommandIdAttributeNumber:
	return ((char *)tup->t_cmax);
    case ChainItemPointerAttributeNumber:
	return ((char *)&tup->t_chain);
    case AnchorItemPointerAttributeNumber:
	elog(WARN, "heap_getsysattr: t_anchor does not exist!");
	break;
    case MinAbsoluteTimeAttributeNumber:
	return ((char *)tup->t_tmin);
    case MaxAbsoluteTimeAttributeNumber:
	return ((char *)tup->t_tmax);
    case VersionTypeAttributeNumber:
	return ((char *)tup->t_vtype);
    default:
	elog(WARN, "heap_getsysattr: undefined attnum %d", attnum);
    }
}

/* ----------------
 *	fastgetattr
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
fastgetattr(tup, attnum, att, isnull)
    HeapTuple	tup;
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

    if (HeapTupleNoNulls(tup))
    {
	/* first attribute is always at position zero */

	attnum--;
	if (att[attnum]->attcacheoff > 0)
	{
	    return((char *) fetchatt(att + attnum, (Pointer) tup
				+ tup->t_hoff + att[attnum]->attcacheoff));
	}
	else if (attnum == 0)
	{
 	    return((char *) fetchatt(att, (Pointer) tup + tup->t_hoff));
	}

	tp = (Pointer) tup + tup->t_hoff;

	slow = 0;
    }
    else /* there's a null somewhere in the tuple */
    {
	bp = tup->t_bits;
	slow = 0;
	tp = (Pointer) tup + tup->t_hoff;

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
	    return((char *) fetchatt(att + attnum, tp + att[attnum]->attcacheoff));
	}
	else if (!HeapTupleAllFixed(tup))
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

	return((char *) fetchatt(att + attnum, tp + att[attnum]->attcacheoff));
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
	    if (!HeapTupleNoNulls(tup))
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

	return((char *) fetchatt(att + attnum, tp + off));
    }
}

/* ----------------
 *	heap_getattr
 *
 *	returns an attribute from a heap tuple.  uses 
 * ----------------
 */
char *
heap_getattr(tup, b, attnum, att, isnull)
    HeapTuple	tup;
    Buffer	b;
    int		attnum;
    struct	attribute *att[];
    bool	*isnull;
{
    bool	localIsNull;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(tup != NULL);

    if (! PointerIsValid(isnull))
	isnull = &localIsNull;
    
    if (attnum > (int) tup->t_natts) {
	*isnull = true;
	return ((char *) NULL);
    }

    /* ----------------
     *	take care of user defined attributes
     * ----------------
     */
    if (attnum > 0) {
	char  *datum;
	datum = fastgetattr(tup, attnum, att, isnull);
	
	return (datum);
    }

    /* ----------------
     *	take care of system attributes
     * ----------------
     */
    *isnull = false;
    return
	heap_getsysattr(tup, b, attnum);
}

/* ----------------
 *	heap_copytuple
 *
 *	returns a copy of an entire tuple
 * ----------------
 */

HeapTuple
heap_copytuple(tuple, buffer, relation)
    HeapTuple	tuple;
    Buffer	buffer;
    Relation	relation;
{
    RuleLock	ruleLock;
    HeapTuple	newTuple = NULL;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(BufferIsValid(buffer) || RelationIsValid(relation));
    
    if (! HeapTupleIsValid(tuple))
	return (NULL);
    
#ifndef	BUGFREEEXECUTOR
    /* XXX For now, just prevent an undetectable executor related error */
    if (tuple->t_len > MAXTUPLEN) {
	elog(WARN, "palloctup: cannot handle length %d tuples",
	     tuple->t_len);
    }
#endif BUGFREEEXECUTOR

    /* ----------------
     *  fetch rule locks from the source tuple
     *
     * If the rule lock of the tuple passed to this routine
     * is a pointer to a "disk memory representation" of a rule
     * lock, convert it to the main memory representation (routine
     * HeapTupleGetRuleLock does that).
     * NOTE: HeapTupleGetRuleLock returns a COPY of the lock
     * (which is exactly what we want...)
     * ----------------
     */
    ruleLock = HeapTupleGetRuleLock(tuple, buffer);

    /* ----------------
     *	allocate a new tuple
     * ----------------
     */
    newTuple = (HeapTuple) palloc(tuple->t_len);
    
    /* ----------------
     *	copy the tuple
     * ----------------
     */
    if ((! BufferIsValid(buffer)) || tuple->t_len < MAXTUPLEN) {
	/* ----------------
	 *  if tuple is not on disk, or it's small enough, then we
	 *  can just do a bcopy.
	 * ----------------
	 */
	bcopy((char *)tuple, (char *)newTuple, (int)tuple->t_len);
    } else {
	/* ----------------
	 *  otherwise we have a disk tuple that spans several pages.
	 * ----------------
	 */
	ItemSubposition	opos;
	ItemSubposition	startpskip();
	int		pfill();
	extern		endpskip();
	
	opos = startpskip(buffer, relation, &tuple->t_ctid);
	pfill(opos, (Pointer)newTuple);
	endpskip();
    }

    /* ----------------
     *	fill in the rule lock information
     * ----------------
     */
    newTuple->t_lock.l_lock = ruleLock;
    newTuple->t_locktype = MEM_RULE_LOCK;
    
    /* ----------------
     *	return the new copy
     * ----------------
     */
    return
	newTuple;
}

/* ----------------
 *	heap_formtuple 
 *
 *	constructs a tuple from the given value[] and null[] arrays
 *
 * old comments
 *	Handles alignment by aligning 2 byte attributes on short boundries
 *	and 3 or 4 byte attributes on long word boundries on a vax; and
 *	aligning non-byte attributes on short boundries on a sun.  Does
 *	not properly align fixed length arrays of 1 or 2 byte types (yet).
 *
 *	Null attributes are indicated by a 'n' in the appropriate byte
 *	of the null[].  Non-null attributes are indicated by a ' ' (space).
 *
 *	Fix me.  (Figure that must keep context if debug--allow give oid.)
 *	Assumes in order.
 * ----------------
 */

HeapTuple
heap_formtuple(numberOfAttributes, tupleDescriptor, value, nulls)
    AttributeNumber	numberOfAttributes;
    TupleDescriptor	tupleDescriptor;
    Datum		value[];
    char		nulls[];
{
    char	*tp;	/* tuple pointer */
    HeapTuple	tuple;	/* return tuple */
    int		bitmaplen;
    long	len;
    int		hoff;
    bool	hasnull = false;
    int		i;
    
    len = sizeof *tuple - sizeof tuple->t_bits;

    for (i = 0; i < numberOfAttributes && !hasnull; i++)
    {
	if (nulls[i] != ' ') hasnull = true;
    }

    if (numberOfAttributes > MaxHeapAttributeNumber)
	elog(WARN, "heap_formtuple: numberOfAttributes of %d > %d",
	     numberOfAttributes, MaxHeapAttributeNumber);

    if (hasnull)
    {
        bitmaplen = BITMAPLEN(numberOfAttributes);
        len       += bitmaplen;
    }
    hoff = len;

    len += ComputeDataSize(numberOfAttributes, tupleDescriptor, value, nulls);
    
    tp = 	(char *) palloc(len);
    tuple =	 LintCast(HeapTuple, tp);
    
    bzero(tp, (int)len);
    
    tuple->t_len = 	len;
    tuple->t_natts = 	numberOfAttributes;
    tuple->t_hoff = hoff;
    
    DataFill((Pointer) tuple + tuple->t_hoff,
	     numberOfAttributes,
	     tupleDescriptor,
	     value,
	     nulls,
             &tuple->t_infomask,
	     (hasnull ? tuple->t_bits : NULL));
    
    /*
     * initialize rule lock information to an EMPTY lock
     * (not to be confused with an InvalidRuleLock).
     */
    tuple->t_locktype = MEM_RULE_LOCK;
    tuple->t_lock.l_lock = prs2MakeLocks();
    
    return (tuple);
}

/* ----------------
 *	heap_modifytuple
 *
 *	forms a new tuple from an old tuple and a set of replacement values.
 * ----------------
 */

HeapTuple
heap_modifytuple(tuple, buffer, relation, replValue, replNull, repl)
    HeapTuple	tuple;
    Buffer	buffer;
    Relation	relation;
    Datum	replValue[];
    char	replNull[];
    char	repl[];
{
    AttributeOffset	attoff;
    AttributeNumber	numberOfAttributes;
    Datum		*value;
    char		*nulls;
    bool		isNull;
    HeapTuple		newTuple;
    int			madecopy;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(HeapTupleIsValid(tuple));
    Assert(BufferIsValid(buffer) || RelationIsValid(relation));
    Assert(HeapTupleIsValid(tuple));
    Assert(PointerIsValid(replValue));
    Assert(PointerIsValid(replNull));
    Assert(PointerIsValid(repl));
    
    /* ----------------
     *	if we're pointing to a disk page, then first
     *  make a copy of our tuple so that all the attributes
     *  are available.  XXX this is inefficient -cim
     * ----------------
     */
    madecopy = 0;
    if (BufferIsValid(buffer) == true) {
	relation = 	(Relation) BufferGetRelation(buffer);
	tuple = 	heap_copytuple(tuple, buffer, relation);
	madecopy = 1;
    }
    
    numberOfAttributes = RelationGetRelationTupleForm(relation)->relnatts;

    /* ----------------
     *	allocate and fill value[] and nulls[] arrays from either
     *  the tuple or the repl information, as appropriate.
     * ----------------
     */
    value = (Datum *)	palloc(numberOfAttributes * sizeof *value);
    nulls =  (char *)	palloc(numberOfAttributes * sizeof *nulls);
    
    for (attoff = 0;
	 attoff < numberOfAttributes;
	 attoff += 1) {
	
	if (repl[ attoff ] == ' ') {
	    value[ attoff ] =
		PointerGetDatum( heap_getattr(tuple,
					      InvalidBuffer, 
				AttributeOffsetGetAttributeNumber(attoff),
				RelationGetTupleDescriptor(relation),
					      &isNull) );
	    
	    nulls[ attoff ] = (isNull) ? 'n' : ' ';
	    
	} else if (repl[ attoff ] != 'r') {
	    elog(WARN, "heap_modifytuple: repl is \\%3d", repl[ attoff ]);
	    
	} else { /* == 'r' */
	    value[ attoff ] = replValue[ attoff ];
	    nulls[ attoff ] =  replNull[ attoff ];
	}
    }

    /* ----------------
     *	create a new tuple from the values[] and nulls[] arrays
     * ----------------
     */
    newTuple = heap_formtuple(numberOfAttributes,
			     RelationGetTupleDescriptor(relation),
			     value,
			     nulls);
	
    /* ----------------
     *	copy the header except for the initial t_len and final t_bits
     * ----------------
     */
    bcopy((char *) &tuple->t_ctid,
	  (char *) &newTuple->t_ctid,	/*XXX*/
	  ((char *) &tuple->t_bits[0] - (char *) &tuple->t_ctid)); /*XXX*/
	
    newTuple->t_natts = numberOfAttributes;	/* fix t_natts just in case */

    /* ----------------
     *	if we made a copy of the tuple, then free it.
     * ----------------
     */
    if (madecopy)
	pfree(tuple);
    
    return
	newTuple;
}

/* ----------------------------------------------------------------
 *			other misc functions
 * ----------------------------------------------------------------
 */
/* ----------------
 *	getstruct	- return s pointer to the structure in the tuple
 *
 *	To be called with a tuple from a system relation only, since this
 *	assumes that only system tuples are guarenteed to reside on a single
 *	page.  C code can call the macro GETSTRUCT() in <htup.h>.  This
 *	should probably not be called by user code.
 *
 *	Note:
 *		Does not return a palloc'd version of the structure.
 *
 *	Should a similar call also be made for index tuples?
 * ----------------
 */

char *
getstruct(tup)
    HeapTuple tup;
{
    return (GETSTRUCT(tup));
}

/* ----------------
 *	slowgetattr
 *
 *      XXX - Currently NOT USED, but probably should not go away.
 * ----------------
 */

char	*
slowgetattr(tup, b, attnum, att, isnull)
    HeapTuple	tup;
    Buffer	b;
    unsigned	attnum;
    struct	attribute *att[];
    bool	*isnull;
{
    register int			bitmask;
    register struct	attribute	**ap;	/* attribute pointer */
    register char			*bp;	/* tup->t_bits pointer */
    register char			*tp;	/* tuple pointer */
    ItemSubposition			opos;
    unsigned long			skip, size[2];
    int					byte;
    int					finalbit;
    int					bitrange;
    
    ItemSubposition	startpskip();
    int			pskip(), pfill();
    extern		endpskip();
    
    Assert(PointerIsValid(isnull));
    Assert(attnum > 0);
    
    size[0] = 4l;			/* palloc format */
    byte = --attnum >> 3;
    finalbit = 1 << (attnum & 07);
    bp = tup->t_bits;
    if (! (bp[byte] & finalbit)) {
	*isnull = true;
	return (NULL);
    }
    
    *isnull = false;
    ap = att;
    opos = startpskip(b, (struct reldesc *)NULL, &tup->t_ctid);
    bitrange = CSIGNBIT;
    skip = 0l;
    while (byte >= 0) {
	if (!byte--)
	    bitrange = finalbit >> 1;
	for (bitmask = 1; bitmask <= bitrange; bitmask <<= 1) {
	    if (*bp & bitmask)
		if ((*ap)->attlen < 0) {
		    skip = LONGALIGN(skip);
		    if (pskip(opos, skip) < 0)
			elog(WARN, "slowgetattr: pskip");
		    if (PNOBREAK(opos, sizeof (long))) {
			PSKIP(opos, sizeof (long));
			skip = PSIZE(opos->op_cp);
		    } else {
			if (pfill(opos, (char *)
				  (size + 1)) < 0)
			    elog(WARN, "slowgetattr");
			skip = size[1];
		    }
		} else if ((*ap)->attlen >= 3)
		    skip = LONGALIGN(skip) + (*ap)->attlen;
		else if ((*ap)->attlen == 2)
		    skip = 2 + SHORTALIGN(skip);
		else if (!(*ap)->attlen)
		    elog(WARN, "slowgetattr: 0 attlen");
		else
		    skip++;
	    ap++;
	}
	bp++;
    }
    if ((*ap)->attlen < 0)
	skip = LONGALIGN(skip);
    else if (!(*ap)->attbyval) {
	if ((*ap)->attlen >= 3)
	    skip = LONGALIGN(skip);
	else if ((*ap)->attlen == 2)
	    skip = SHORTALIGN(skip);
    } else
	switch ((int)(*ap)->attlen) {
	case 2:
	    skip = SHORTALIGN(skip);
	    break;
	case 3:
	case 4:
	    skip = LONGALIGN(skip);
	default:
	    ;
	}
    if (pskip(opos, skip) < 0)
	elog(WARN, "slowgetattr: pskip failed");
    if ((*ap)->attlen < 0) {
	if (PNOBREAK(opos, sizeof (long))) {
	    PSKIP(opos, sizeof (long));
	    size[1] = PSIZE(opos->op_cp);
	} else if (pfill(opos, (char *)(size + 1)) < 0)
	    elog(WARN, "slowgetattr: failed pfill");
	if (b == opos->op_db && PNOBREAK(opos, (unsigned)size[1]))
	    tp = opos->op_cp;
	else {
	    tp = (char *) palloc(size[1]);
	    if (pfill(opos, tp) < 0)
		elog(WARN, "slowgetattr: failed pfill$");
	}
    } else if (!(*ap)->attbyval) {
	if (b == opos->op_db && PNOBREAK(opos, (unsigned)(*ap)->attlen))
	    tp = opos->op_cp;
	else {
	    tp = (char *) palloc((*ap)->attlen);
	    if (pfill(opos, tp) < 0)
		elog(WARN, "slowgetattr: failed pfill$2");
	}
    } else
	switch ((int)(*ap)->attlen) {
	case 1:
	    if (!opos->op_len) {
		size[0] = 1l;
		if (pfill(opos, (char *)(size + 1)) < 0)
		    elog(WARN, "slowgetattr: failed pfill$3");
		tp = (char *)*(char *)(size + 1);
	    } else
		tp = (char *)*opos->op_cp;
	    break;
	case 2:
	    if (opos->op_len < 2) {
		size[0] = 2l;
		if (pfill(opos, (char *)(size + 1)) < 0)
		    elog(WARN, "slowgetattr: failed pfill$4");
		tp = (char *)*(short *)(size + 1);
	    } else
		tp = (char *)*(short *)opos->op_cp;
	    break;
	case 3:					/* XXX */
	    elog(WARN, "slowgetattr: no len 3 attbyval yet");
	case 4:
	    if (opos->op_len < 2) {
		if (pfill(opos, (char *)(size + 1)) < 0)
		    elog(WARN, "slowgetattr: failed pfill$5");
		tp = (char *)size[1];
	    } else
		tp = (char *)*(long *)opos->op_cp;
	    break;
	default:
	    elog(WARN, "slowgetattr: len %d attbyval", (*ap)->attlen);
	}
    endpskip(opos);
    return (tp);
}

HeapTuple
heap_addheader(natts, structlen, structure)
    uint32	natts;			/* max domain index */
    int		structlen;		/* its length */
    char	*structure;		/* pointer to the struct */
{
    register char	*tp;	/* tuple data pointer */
    HeapTuple		tup;
    int			bitmasklen;
    int			bitmask;
    long		len;
    int			i;
    int			hoff;
    extern		bzero();
    extern		bcopy();
    
    AssertArg(natts > 0);

    len = sizeof (HeapTupleData) - sizeof (tup->t_bits);

    hoff = len;
    len += structlen;
    tp = (char *) palloc(len);
    tup = (HeapTuple) tp;

    tup->t_len = (short) len;			/* XXX */
    tp += tup->t_hoff = hoff;
    tup->t_natts = natts;
    tup->t_infomask = 0;

    bcopy(structure, tp, structlen);

    /*
     * initialize rule lock
     */
    tup->t_locktype = MEM_RULE_LOCK;
    tup->t_lock.l_lock = NULL;
    
    return (tup);
}
