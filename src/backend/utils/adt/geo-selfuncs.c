/***********************************************************************
**
**	geo-selfuncs.c
**
**	Selectivity routines registered in the operator catalog in the
**	"oprrest" and "oprjoin" attributes.
**
**	XXX These are totally bogus.
**
***********************************************************************/

#include "tmp/postgres.h"

#include "access/attnum.h"
#include "utils/geo-decls.h"

RcsId("$Header$");


/*ARGSUSED*/
float64
areasel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 4.0;
	return(result);
}

/*ARGSUSED*/
float64
areajoinsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 4.0;
	return(result);
}

/*
 *  Selectivity functions for rtrees.  These are bogus -- unless we know
 *  the actual key distribution in the index, we can't make a good prediction
 *  of the selectivity of these operators.
 *
 *  In general, rtrees need to search multiple subtrees in order to guarantee
 *  that all occurrences of the same key have been found.  Because of this,
 *  the heuristic selectivity functions we return are higher than they would
 *  otherwise be.
 */

/*
 *  left_sel -- How likely is a box to be strictly left of (right of, above,
 *		below) a given box?
 */

float64
leftsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 6.0;
	return(result);
}

float64
leftjoinsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 6.0;
	return(result);
}

/*
 *  contsel -- How likely is a box to contain (be contained by) a given box?
 */

float64
contsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 10.0;
	return(result);
}

float64
contjoinsel(opid, relid, attno, value, flag)
	ObjectId	opid;
	ObjectId	relid;
	AttributeNumber	attno;
	char		*value;
	int32		flag;
{
	float64	result;

	result = (float64) palloc(sizeof(float64data));
	*result = 1.0 / 10.0;
	return(result);
}
