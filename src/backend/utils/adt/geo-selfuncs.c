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
