/*
 * selfuncs.c --
 *	Selectivity functions for system catalogs and builtin types
 *
 *	These routines are registered in the operator catalog in the
 *	"oprrest" and "oprjoin" attributes.
 *
 *	XXX check all the functions--I suspect them to be 1-based.
 */

#include <stdio.h>
#include <strings.h>
#include "anum.h"
#include "catalog.h"
#include "catname.h"
#include "fmgr.h"
#include "heapam.h"
#include "log.h"
#include "syscache.h"

RcsId("$Header$");

/* N is not a valid var/constant or relation id */
#define	NONVALUE(N)	((N) == -1)

/*
 *	eqsel		- Selectivity of "=" for any data type.
 */
/*ARGSUSED*/
float64
eqsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	int32		nvals;
	float64		result;
	extern int32	getattnvals();

	result = (float64) palloc(sizeof(float64data));
	if (NONVALUE(attno) || NONVALUE(relid))
		*result = 0.1;
	else {
		nvals = getattnvals(relid, (int) attno);
		if (nvals == 0)
			*result = 0.0;
		else
			*result = 1.0 / nvals;
	}
	return(result);
}

/*
 *	neqsel		- Selectivity of "!=" for any data type.
 */
float64
neqsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64		result;
	extern float64	eqsel();

	result = eqsel(opid, relid, attno, value, flag);
	*result = 1.0 - *result;
	return(result);
}

/*
 *	intltsel	- Selectivity of "<" for integers.
 *			  Should work for both longs and shorts.
 */
float64
intltsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
/* XXX	char		*value;*/
	int32		value;
	int32		flag;
{
	float64 	result;
	char		*highchar, *lowchar;
	long		val, high, low, top, bottom;
	extern long	atol();
	extern		gethilokey();

	result = (float64) palloc(sizeof(float64data));
	if (NONVALUE(attno) || NONVALUE(relid))
		*result = 1.0 / 3;
	else {
/* XXX		val = atol(value);*/
		val = value;
		gethilokey(relid, (int) attno, opid, &highchar, &lowchar);
		high = atol(highchar);
		low = atol(lowchar);
		if ((flag & SEL_RIGHT && val < low) ||
		    (!(flag & SEL_RIGHT) && val > high))
			*result = 0.0;
		else {
			bottom = high - low;
			if (bottom == 0)
				++bottom;
			if (flag & SEL_RIGHT)
				top = val - low;
			else
				top = high - val;
			if (top > bottom)
				*result = 1.0;
			else {
				if (top == 0)
					++top;
				*result = ((1.0 * top) / bottom);
			}
		}
	}
	return(result);
}

/*
 *	intgtsel	- Selectivity of ">" for integers.
 *			  Should work for both longs and shorts.
 */
float64
intgtsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
/* XXX	char		*value;*/
	int32		value;
	int32		flag;
{
	float64		result;
	int		notflag;
	extern float64	intltsel();

	if (flag & 0)
		notflag = flag & ~SEL_RIGHT;
	else
		notflag = flag | SEL_RIGHT;
	result = intltsel(opid, relid, attno, value, (int32) notflag);
	return(result);
}

/*
 *	eqjoinsel	- Join selectivity of "="
 */
/*ARGSUSED*/
float64
eqjoinsel(opid, relid1, attno1, relid2, attno2)
	ObjectId	opid;
	ObjectId	relid1;
	AttributeNumber	attno1;
	ObjectId	relid2;
	AttributeNumber	attno2;
{
	float64		result;
	int32		num1, num2, max;
	extern int32	getattnvals();

	result = (float64) palloc(sizeof(float64data));
	if (NONVALUE(attno1) || NONVALUE(relid1) ||
	    NONVALUE(attno2) || NONVALUE(relid2))
		*result = 0.1;
	else {
		num1 = getattnvals(relid1, (int) attno1);
		num2 = getattnvals(relid2, (int) attno2);
		max = (num1 > num2) ? num1 : num2;
		if (max == 0)
			*result = 1.0;
		else
			*result = 1.0 / max;
	}
	return(result);
}

/*
 *	neqjoinsel	- Join selectivity of "!="
 */
float64
neqjoinsel(opid, relid1, attno1, relid2, attno2)
	ObjectId	opid;
	ObjectId	relid1;
	AttributeNumber	attno1;
	ObjectId	relid2;
	AttributeNumber	attno2;
{
	float64		result;
	extern float64	eqjoinsel();

	result = eqjoinsel(opid, relid1, attno1, relid2, attno2);
	*result = 1.0 - *result;
	return(result);
}

/*
 *	intltjoinsel	- Join selectivity of "<"
 */
/*ARGSUSED*/
float64
intltjoinsel(opid, relid1, attno1, relid2, attno2)
	ObjectId	opid;
	ObjectId	relid1;
	AttributeNumber	attno1;
	ObjectId	relid2;
	AttributeNumber	attno2;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 3.0;
	return(result);
}

/*
 *	intgtjoinsel	- Join selectivity of ">"
 */
/*ARGSUSED*/
float64
intgtjoinsel(opid, relid1, attno1, relid2, attno2)
	ObjectId	opid;
	ObjectId	relid1;
	AttributeNumber	attno1;
	ObjectId	relid2;
	AttributeNumber	attno2;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 3.0;
	return(result);
}

/*
 *	getattnvals	- Retrieves the number of values within an attribute.
 *
 *	Note:
 *		getattnvals and gethilokey both currently use keyed
 *		relation scans and amgetattr.  Alternatively,
 *		the relation scan could be non-keyed and the tuple
 *		returned could be cast (struct X *) tuple + tuple->t_hoff.
 *		The first method is good for testing the implementation,
 *		but the second may ultimately be faster?!?  In any case,
 *		using the cast instead of amgetattr would be
 *		more efficient.  However, the cast will not work
 *		for gethilokey which accesses stahikey in struct statistic.
 */
int32
getattnvals(relid, attnum)
	ObjectId	relid;
	AttributeNumber	attnum;
{
	HeapTuple	atp;
	int		nvals;

	atp = SearchSysCacheTuple(ATTNUM, relid, attnum, NULL, NULL);
	if (!HeapTupleIsValid(atp)) {
		elog(WARN, "getattnvals: no attribute tuple %d %d",
		     relid, attnum);
		return(0);
	}
	nvals = ((struct attribute *) GETSTRUCT(atp))->attnvals;
	return(nvals);
}

/*
 *	gethilokey	- Returns a pointer to strings containing
 *			  the high and low keys within an attribute.
 *
 *	Currently returns "0", and "0" in high and low if the statistic
 *	catalog does not contain the proper tuple.  Eventually, the
 *	statistic demon should have the tuple maintained, and it should
 *	elog() if the tuple is missing.
 *
 *	XXX Question: is this worth sticking in the catalog caches,
 *	    or will this get invalidated too often?
 */
gethilokey(relid, attnum, opid, high, low)
	ObjectId	relid;
	AttributeNumber	attnum;
	ObjectId	opid;
	char		**high;
	char		**low;
{
	register Relation	rdesc;
	register HeapScan	sdesc;
	static ScanKeyEntryData	key[3] = {
		{ 0, StatisticRelationIdAttributeNumber, F_OIDEQ },
		{ 0, StatisticAttributeNumberAttributeNumber, F_INT2EQ },
		{ 0, StatisticOperatorAttributeNumber, F_OIDEQ }
	};
/* XXX	Boolean			isnull;*/
	HeapTuple		tuple;
	extern char		*textout();

	rdesc = RelationNameOpenHeapRelation(StatisticRelationName);
	key[0].argument.objectId.value = relid;
	key[1].argument.integer16.value = (int16) attnum;
	key[2].argument.objectId.value = opid;
	sdesc = RelationBeginHeapScan(rdesc, 0, NowTimeRange,
				      3, (ScanKey) key);
	tuple = amgetnext(sdesc, 0, (Buffer *) NULL);
	if (!HeapTupleIsValid(tuple)) {
		*high = "0";
		*low = "0";
/* XXX 		elog(WARN, "gethilokey: statistic tuple not found");*/
		return;
	}
/* XXX stahikey doesn't exist for some reason ... */
#ifdef HAS_STAHIKEY
	*high = textout((struct varlena *)
			amgetattr(tuple,
				  InvalidBuffer,
				  StatisticHighKeyAttributeNumber,
				  &rdesc->rd_att,
				  &isnull));
	if (isnull)
		elog(DEBUG, "gethilokey: high key is null");
	*low = textout((struct varlena *)
		       amgetattr(tuple,
				 InvalidBuffer,
				 StatisticLowKeyAttributeNumber,
				 &rdesc->rd_att,
				 &isnull));
	if (isnull)
		elog(DEBUG, "gethilokey: low key is null");
#else /* !HAS_STAHIKEY */
	*high = "0";
	*low = "0";
#endif /* !HAS_STAHIKEY */
	HeapScanEnd(sdesc);
	RelationCloseHeapRelation(rdesc);
}
