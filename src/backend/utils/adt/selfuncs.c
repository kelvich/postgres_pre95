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

#include "catname.h"
#include "fmgr.h"
#include "heapam.h"
#include "log.h"
#include "syscache.h"
#include "tqual.h"	/* for NowTimeQual */

#include "catalog/pg_statistic.h"

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
		int nvals;

		nvals = getattnvals(relid, (int) attno);
		if (nvals == 0)
			*result = 1.0 / 3.0;
		else
			*result = 3.0 / nvals;
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
	if (nvals > 0) return(nvals);

	atp = SearchSysCacheTuple(RELOID, relid, NULL, NULL, NULL);
	/* XXX -- use number of tuples as number of distinctive values
	   just for now, in case number of distinctive values is
	   not cached */
	if (!HeapTupleIsValid(atp)) {
		elog(WARN, "getattnvals: no relation tuple %d", relid);
		return(0);
	}
	nvals = ((RelationTupleForm) GETSTRUCT(atp))->reltuples;
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
	register HeapScanDesc	sdesc;
	static ScanKeyEntryData	key[3] = {
		{ 0, StatisticRelationIdAttributeNumber, F_OIDEQ },
		{ 0, StatisticAttributeNumberAttributeNumber, F_INT2EQ },
		{ 0, StatisticOperatorAttributeNumber, F_OIDEQ }
	};
/* XXX	Boolean			isnull;*/
	HeapTuple		tuple;
	extern char		*textout();

	rdesc = RelationNameOpenHeapRelation(StatisticRelationName);
	key[0].argument = ObjectIdGetDatum(relid);
	key[1].argument = Int16GetDatum((int16) attnum);
	key[2].argument = ObjectIdGetDatum(opid);
	sdesc = RelationBeginHeapScan(rdesc, 0, NowTimeQual,
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

float64
btreesel(operatorObjectId, indrelid, attributeNumber,
	 constValue, constFlag, nIndexKeys, indexrelid)
ObjectId 	operatorObjectId, indrelid, indexrelid;
AttributeNumber attributeNumber;
char		*constValue;
int32		constFlag;
int32		nIndexKeys;
{
	float64 result;

	result = (float64)fmgr(get_oprrest (operatorObjectId),
				(char*)operatorObjectId,
				(char*)indrelid,
				(char*)attributeNumber,
				(char*)constValue,
				(char*)constFlag);

	if (!PointerIsValid(result))
		elog(WARN, "Btree Selectivity: bad pointer");
	if (*result < 0.0 || *result > 1.0)
		elog(WARN, "Btree Selectivity: bad value %lf", *result);

	return(result);
}

float64
btreenpage(operatorObjectId, indrelid, attributeNumber,
	 constValue, constFlag, nIndexKeys, indexrelid)
ObjectId 	operatorObjectId, indrelid, indexrelid;
AttributeNumber attributeNumber;
char		*constValue;
int32		constFlag;
int32		nIndexKeys;
{
	float64 temp, result;
	HeapTuple atp;
	int npage;

	temp = (float64)fmgr(get_oprrest (operatorObjectId),
				(char*)operatorObjectId,
				(char*)indrelid,
				(char*)attributeNumber,
				(char*)constValue,
				(char*)constFlag);
        atp = SearchSysCacheTuple(RELOID, indexrelid, NULL, NULL, NULL);
        if (!HeapTupleIsValid(atp)) {
                elog(WARN, "btreenpage: no index tuple %d", indexrelid);
                return(0);
        }
	
	npage = ((RelationTupleForm) GETSTRUCT(atp))->relpages;
	result = (float64)palloc(sizeof(float64data));
	*result = *temp * npage;
	return(result);
}
