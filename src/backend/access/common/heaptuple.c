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
 *	HeapTupleGetForm
 *
 *   OLD INTERFACE FUNCTIONS
 *	isnull
 *	sysattrlen
 *	sysattrbyval
 *	amgetattr
 *	addtupleheader
 *	palloctup
 *	formtuple, FormHeapTuple
 *	modifytuple, ModifyHeapTuple
 *	getstruct
 *	
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include "tmp/c.h"

#include "access/htup.h"
#include "access/itup.h"
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

RcsId("$Header$");

/* ----------------
 *	XXX replace these with constants from lib/H/catalog/pg_type.h 
 * ----------------
 */
#ifndef	TYP_BOOL
#define	TYP_BOOL	16
#define	TYP_BYTEA	17
#define	TYP_CHAR	18
#define	TYP_CHAR16	19
#define	TYP_DATETIME	20
#define	TYP_INT2	21
#define	TYP_INT28	22
#define	TYP_INT4	23
#define	TYP_REGPROC	24
#define	TYP_TEXT	25
#define	TYP_OID		26
#define	TYP_TID		27
#define	TYP_XID		28
#define	TYP_IID		29
#define	TYP_OID8	30
#define	TYP_LOCK	31
#endif


/* ----------------------------------------------------------------
 *			misc support routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	HeapTupleGetForm
 * ----------------
 */
Form
HeapTupleGetForm(tuple)
    HeapTuple	tuple;
{
    return (Form)
	((char *)tuple + tuple->t_hoff);
}

/* ----------------
 *	ComputeDataSize
 * ----------------
 */
Size
ComputeDataSize(numberOfAttributes, tupleDescriptor, value, nulls)
    AttributeNumber	numberOfAttributes;
    TupleDescriptor	tupleDescriptor;
    Datum		value[];
    char		nulls[];
{
    register Attribute	*attributeP;
    register Datum	*valueP;
    char		*nullP;
    uint32		length;
    int			i;

    attributeP = &tupleDescriptor->data[0];
    valueP = value;
    nullP = nulls;
    length = 0;

    for (i = numberOfAttributes;
	 i != 0;
	 i--, attributeP++, valueP++, nullP++) {

	if (*nullP == 'n')
	    continue;

	if (*nullP != ' ')
	    elog(DEBUG, "ComputeDataSize called with '\\0%o' nulls[%d]",
		 *nullP, numberOfAttributes - i);

	if ((*attributeP)->attlen < 0) {
	    length = LONGALIGN(length) +
		     PSIZE(DatumGetPointer(*valueP)) +
		     sizeof (long);
	} else if ((*attributeP)->attlen >= 3) {
	    length = LONGALIGN(length) + (*attributeP)->attlen;
	} else if ((*attributeP)->attlen == 2) {
	    length = SHORTALIGN(length + 2);
	} else if (!(*attributeP)->attlen) {
	    elog(WARN, "ComputeDataSize: 0 attlen");
	} else {
	    length++;
	}
    }

    return length;
}

/* ----------------
 *	DataFill
 * ----------------
 */
void
DataFill(data, numberOfAttributes, tupleDescriptor, value, nulls, bit)
    Pointer		data;
    AttributeNumber	numberOfAttributes;
    TupleDescriptor	tupleDescriptor;
    Datum		value[];
    char		nulls[];
    bits8		bit[];
{
    Attribute	*attributeP;
    Datum	*valueP;
    char	*nullP;
    bits8	*bitP;
    int		bitmask;
    uint32	length;
    int		i;

    bitP = &bit[-1];
    bitmask = CSIGNBIT;

    attributeP = &tupleDescriptor->data[0];
    valueP = value;
    nullP = nulls;
    for (i = numberOfAttributes;
	 i != 0;
	 i -= 1, attributeP += 1, valueP += 1, nullP += 1) {

	if (bitmask != CSIGNBIT) {
	    bitmask <<= 1;
	} else {
	    bitP += 1;
	    *bitP = 0x0;
	    bitmask = 1;
	}

	/* ----------------
	 *  skip null attributes
	 * ----------------
	 */
	if (*nullP == 'n') 
	    continue;

	*bitP |= bitmask;
	
	if ((*attributeP)->attlen < 0) {
	    /* ----------------
	     *	variable length attribute
	     * ----------------
	     */
	    data = (Pointer) LONGALIGN((char *)data);
	    *(long *)data = length = PSIZE(DatumGetPointer(*valueP));

	    data = (Pointer)((char *)data + sizeof (long));
	    bcopy((char *)DatumGetPointer(*valueP), (char *)data, length);
	    data = (Pointer)((char *)data + length);

	} else 
	    if ((*attributeP)->attbyval)
		switch ((int)(*attributeP)->attlen) {
		case 1:
		    *(char *)data = DatumGetChar(*valueP);
		    data = (Pointer)((char *)data + 1);
		    break;

		case 2:
		    data = (Pointer)SHORTALIGN((char *)data);
		    *(short *)data = DatumGetInt16(*valueP);
		    data = (Pointer)((char *)data + sizeof (short));
		    break;

		case 3:		/* XXX -- which byte ordering best? */
		    elog(WARN, "DataFill: no len 3 attbyval yet");

		case 4:
		    data = (Pointer)LONGALIGN((char *)data);
		    *(long *)data = DatumGetInt32(*valueP);
		    data = (Pointer)((char *)data + sizeof (long));
		    break;

		default:
		    elog(WARN, "DataFill: len %d attbyval",
			 (*attributeP)->attbyval);
		}

	    else if ((*attributeP)->attlen >= 3) {
		data = (Pointer)LONGALIGN((char *)data);
		bcopy((char *)DatumGetPointer(*valueP), (char *)data,
		      (*attributeP)->attlen);
		data = (Pointer)((char *)data + (*attributeP)->attlen);
		
	    } else if ((*attributeP)->attlen == 2) {
		data = (Pointer)SHORTALIGN((char *)data);
		*(short *)data = *(short *)DatumGetPointer(*valueP);
		data = (Pointer)((char *)data + sizeof (short));
		
	    } else if (!(*attributeP)->attlen)
		elog(WARN, "DataFill: length 0 attribute");
	
	    else {
		*(char *)data = *(char *)DatumGetPointer(*valueP);
		data = (Pointer)((char *)data + 1);
	    }
    }
}

/* ----------------------------------------------------------------
 *			heap tuple interface
 * ----------------------------------------------------------------
 */

/* ----------------
 *	heap_attisnull	- returns 1 iff tuple domain is not present
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

    if (attnum > 0) {
	byte = --attnum >> 3;
	finalbit = 1 << (attnum & 07);
	bp = tup->t_bits;
	if (! (bp[byte] & finalbit))
	    return (1);
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
	    len = sizeof (f.t_anchor);
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
	 * >>  *isnull = (Boolean)1;
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
	return ((char *)&tup->t_anchor);
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
 * old comments
 * Note: The caller is responsible for passing an attnum such that
 *	attnum < rdesc->rd_rel.r_natts or a negative (system) attnum
 *	(defined in htup.h).
 *	Does not return a freshly malloc'd version of the attribute.
 *
 *	Also, should document that variable length fields may not
 *	be passed by value.
 *
 * Possible bugs:
 *	Does not work for arrays nor for index relation tuples yet.
 *	Calls elog() upon error--should it return NULL instead?
 *	Handling of variable size arrays may must be fixed.
 *	Non-contiguous tuples should eventually be handled.
 *	Note that the internal structure of TYP is a black-box--
 *	there may be problems if there are array types.
 *	(Ie., struct s3 { short s[3] } S;  will align S on a
 *	short boundry.
 *
 *	***Remove the frequent need of extra dereferencing of ap.
 *
 * ----------------
 */
char *
fastgetattr(tup, attnum, att, isnull)
    HeapTuple	tup;
    unsigned	attnum;
    struct	attribute *att[];
    Boolean	*isnull;
{
    register int			bitmask;
    register struct	attribute	**ap;	/* attribute pointer */
    register char			*bp;	/* tup->t_bits pointer */
    register char			*tp;	/* tuple pointer */
    register int			al;	/* attribute length */
    int				byte;
    int				finalbit;
    int				bitrange;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    Assert(PointerIsValid(isnull));
    Assert(attnum > 0);

    /* ----------------
     *	check for null attributes
     * ----------------
     */
    byte = --attnum >> 3;
    finalbit = 1 << (attnum & 07);
    bp = tup->t_bits;
    if (! (bp[byte] & finalbit)) {
	*isnull = (Boolean)true;
	return ((char *)NULL);
    }

    /* ----------------
     *	now walk the tuple descriptor until we get to the attribute
     *  we want.  we advance tp the length of each attribute we advance
     *  so tp points to our attribute when we're done.
     *
     *  XXX this is pretty inefficient for reasonable sized tuples.
     *      we should cache the attribute offsets when possible.
     *	    null attributes can make this difficult though -cim 1/22/90
     * ----------------
     */
    *isnull = (Boolean) false;
    ap = att;
    tp = (char *)tup + (int)tup->t_hoff;
    
    /*
     *  Assumes < 700 attributes--else use (tp->t_hoff & I1MASK)
     *  check this and the #define MaxIndexAttributeNumber in ../h/htup.h
     */
    
    if ((long)tp != LONGALIGN(tp))
	elog(WARN, "fastgetattr: t_hoff misaligned");
    
    bitrange = CSIGNBIT;
    while (byte >= 0) {
	if (! byte--)
	    bitrange = finalbit >> 1;
	for (bitmask = 1; bitmask <= bitrange; bitmask <<= 1) {
	    if (*bp & bitmask) {
		al = (*ap)->attlen;
		if (al < 0) {
		    tp = ((char *) LONGALIGN(tp))
			+ sizeof (long);
		    tp += PSIZE(tp);
		} else if (al >= 3) {
		    tp = ((char *) LONGALIGN(tp)) + al;
		} else if (al == 2) {
		    tp = (char *) SHORTALIGN(tp + 2);
		} else if (!al) {
		    elog(WARN, "fastgetattr: 0 attlen");
		} else {
		    tp++;
		}
	    }
	    ap++;
	}
	bp++;
    }
    
    /* ----------------
     *	now we're at the attribute we want.  get it's length
     * ----------------
     */
    al = (*ap)->attlen;

    /* ----------------
     *	if the length is negative, it means we have a variable
     *  length attribute.  these have their length in the first
     *  4 bytes and their data in the remaining bytes.
     * ----------------
     */
    if (al < 0)
	return ((char *)LONGALIGN(tp + sizeof (long)));

    /* ----------------
     *	take care of pass-by-reference attributes
     * ----------------
     */
    if (!(*ap)->attbyval) {
	if (al >= 3)
	    return ((char *)LONGALIGN(tp));
	else if (al == 2)
	    return ((char *)SHORTALIGN(tp));
	return (tp);
    }
    
    /* ----------------
     *	take care of pass-by-value attributes.
     * ----------------
     */
    switch (al) {
    case 1:
	return ((char *)*tp);
    case 2:
	tp = (char *)SHORTALIGN(tp);
	return ((char *)*(short *)tp);
    case 3:					/* XXX */
	elog(WARN, "fastgetattr: no len 3 attbyval yet");
    case 4:
	tp = (char *)LONGALIGN(tp);
	return ((char *)*(long *)tp);
    default:
	elog(WARN, "fastgetattr: len %d attbyval", (*ap)->attlen);
    }
    
    /*NOTREACHED*/
}
  
/* ----------------
 *	heap_getattr
 * ----------------
 */
char *
heap_getattr(tup, b, attnum, att, isnull)
    HeapTuple	tup;
    Buffer	b;
    int		attnum;
    struct	attribute *att[];
    Boolean	*isnull;
{
    char	*fastgetattr(), *slowgetattr();
    Boolean	localIsNull;
    char	*datum;
    RuleLock	lock;

    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (tup == NULL)
	elog(WARN, "heap_getattr: called with NULL tuple");

    if (! PointerIsValid(isnull))
	isnull = &localIsNull;
    
    if (attnum > (int)tup->t_natts) {
	*isnull = (Boolean) true;
	return ((char *)NULL);
    }

    /* ----------------
     *	take care of user defined attributes
     * ----------------
     */

    if (attnum > 0) {
	datum = fastgetattr(tup, attnum, att, isnull);
	return (datum);
    }

    /* ----------------
     *	take care of system attributes
     * ----------------
     */
    *isnull = (Boolean) false;
    return
	heap_getsysattr(tup, b, attnum);
}

/* ----------------
 *	heap_addheader	- constructs a tuple containing the given structure
 *
 *	Returns the palloc'd tuple with each of the natts t_bits filled.
 *	Note, this is not general.  It assumes that maxnatts is as small as
 *	possible with respect to natts.  Should this be allowed to be
 *	called by the user?
 * ----------------
 */
HeapTuple
heap_addheader(natts, structlen, structure)
    uint32	natts;			/* max domain index */
    int		structlen;		/* its length */
    char	*structure;		/* pointer to the struct */
{
    register char	*bp;	/* bitfield pointer */
    register char	*tp;	/* tuple data pointer */
    HeapTuple		tup;
    int			bitmasklen;
    int			bitmask;
    long		len;
    int			i;
    int			hoff;
    extern		bzero();
    extern		bcopy();
    
    len = sizeof (HeapTupleData) - sizeof (tup->t_bits);
    if (natts < 1)
	elog(WARN, "addtupleheader: invalid natts == %d", natts);
    
    bitmasklen = BITMAPLEN(natts);
    len += bitmasklen;
    hoff = len;
    len += structlen;
    tp = (char *) palloc(len);
    tup = (HeapTuple)tp;
    bzero(tp, (int)len);			/* probably unneeded */
    tup->t_len = (short)len;			/* XXX */
    tp += tup->t_hoff = hoff;
    tup->t_natts = natts;
    bp = tup->t_bits;
    for (i = natts / 8; i != 0; i--)
	*bp++ = ~0;
    bitmask = 1;
    for (i = natts % 8; i != 0; i--) {	/* array reference faster? */
	*bp |= bitmask;
	bitmask <<= 1;
    }
    bcopy(structure, tp, structlen);
    
    /*
     * initialize rule lock
     */
    tup->t_locktype = MEM_RULE_LOCK;
    tup->t_lock.l_lock = NULL;
    
    return (tup);
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
    
    len = sizeof *tuple - sizeof tuple->t_bits;
    if (numberOfAttributes > MaxHeapAttributeNumber)
	elog(WARN, "heap_formtuple: numberOfAttributes of %d > %d",
	     numberOfAttributes, MaxHeapAttributeNumber);
    bitmaplen = BITMAPLEN(numberOfAttributes);
    len += bitmaplen;
    hoff = len;
    
    len += ComputeDataSize(numberOfAttributes, tupleDescriptor, value,
			   nulls);
    
    tp = (char *) palloc(len);
    tuple = LintCast(HeapTuple, tp);
    bzero(tp, (int)len);
    tuple->t_len = (short)len;			/* XXX */
    tuple->t_natts = numberOfAttributes;
    tp += tuple->t_hoff = hoff;
    
    DataFill((Pointer)tp, numberOfAttributes, tupleDescriptor, value, nulls,
	     tuple->t_bits);
    
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
    Boolean		isNull;
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
    bcopy((char *)&tuple->t_ctid,
	  (char *)&newTuple->t_ctid,	/*XXX*/
	  ((char *)&tuple->t_bits[0] - (char *)&tuple->t_ctid));	/*XXX*/
	
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
 *		      old am interface routines
 * ----------------------------------------------------------------
 */
int
isnull(tup, attnum)
    HeapTuple	tup;
    int		attnum;
{
    return heap_attisnull(tup, attnum);
}

int
sysattrlen(attno)
    AttributeNumber	attno;
{
    return heap_sysattrlen(attno);
}

bool
sysattrbyval(attno)
    AttributeNumber	attno;
{
    return heap_sysattrbyval(attno);
}

char *
amgetattr(tup, b, attnum, att, isnull)
    HeapTuple	tup;
    Buffer	b;
    int		attnum;
    struct	attribute *att[];
    Boolean	*isnull;
{
    return (char *)
	heap_getattr(tup, b, attnum, att, isnull);
}

HeapTuple
addtupleheader(natts, structlen, structure)
    uint32	natts;			/* max domain index */
    int		structlen;		/* its length */
    char	*structure;		/* pointer to the struct */
{
    return heap_addheader(natts, structlen, structure);
}

HeapTuple
palloctup(tuple, buffer, relation)
    HeapTuple	tuple;
    Buffer	buffer;
    Relation	relation;    
{
    return heap_copytuple(tuple, buffer, relation);
}

HeapTuple
formtuple(numberOfAttributes, tupleDescriptor, value, nulls)
    AttributeNumber	numberOfAttributes;	/* max domain index + 1 */
    TupleDescriptor	tupleDescriptor;	/* descriptions */
    Datum		value[];		/* pointers to data */
    char		nulls[];			/* null domain value */
{
    return heap_formtuple(numberOfAttributes, tupleDescriptor, value, nulls);
}

HeapTuple
FormHeapTuple(numberOfAttributes, tupleDescriptor, value, nulls)
	AttributeNumber	numberOfAttributes;
	TupleDescriptor	tupleDescriptor;
	Datum		value[];
	char		nulls[];
{
    return heap_formtuple(numberOfAttributes, tupleDescriptor, value, nulls);
}

HeapTuple
modifytuple(tuple, buffer, relation, replValue, replNull, repl)
    HeapTuple	tuple;
    Buffer	buffer;
    Relation	relation;
    Datum	replValue[];
    char	replNull[];
    char	repl[];
{
    return
	heap_modifytuple(tuple, buffer, relation, replValue, replNull, repl);
}

HeapTuple
ModifyHeapTuple(tuple, buffer, relation, replValue, replNull, repl)
    HeapTuple	tuple;
    Buffer	buffer;
    Relation	relation;
    Datum	replValue[];
    char	replNull[];
    char	repl[];
{
    return
	heap_modifytuple(tuple, buffer, relation, replValue, replNull, repl);
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
 * ----------------
 */
char	*
slowgetattr(tup, b, attnum, att, isnull)
    HeapTuple	tup;
    Buffer	b;
    unsigned	attnum;
    struct	attribute *att[];
    Boolean	*isnull;
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
	*isnull = (Boolean)true;
	return (NULL);
    }
    
    *isnull = (Boolean)false;
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


