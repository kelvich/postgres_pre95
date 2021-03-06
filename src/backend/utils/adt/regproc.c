/*
 * regproc.c --
 * 	Functions for the built-in type "RegProcedure".
 */

#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"	/* for NowTimeQual */
#include "fmgr.h"
#include "utils/log.h"

#include "catalog/catname.h"

	    /* ========== USER I/O ROUTINES ========== */


/*
 *	regprocin	- converts "proname" to proid
 *
 *	proid of NULL signifies unknown
 */
int32
regprocin(proname)
	char	*proname;
{
	Relation	proc;
	HeapScanDesc	procscan;
	HeapTuple	proctup;
	ScanKeyEntryData	key;		/* static better ??? */
	RegProcedure	result;
	Boolean		isnull;

	if (proname == NULL)
		return(0);
	proc = amopenr(ProcedureRelationName->data);
	if (!RelationIsValid(proc)) {
		elog(WARN, "regprocin: could not open %s",
		     ProcedureRelationName->data);
		return(0);
	}
	ScanKeyEntryInitialize(&key, 
			       (bits16)0, 
			       (AttributeNumber)1, 
			       (RegProcedure)F_CHAR16EQ,
			       (Datum)proname);

	procscan = ambeginscan(proc, 0, NowTimeQual, 1, &key);
	if (!HeapScanIsValid(procscan)) {
		amclose(proc);
		elog(WARN, "regprocin: could not being scan of %s",
		     ProcedureRelationName);
		return(0);
	}
	proctup = amgetnext(procscan, 0, (Buffer *) NULL);
	switch (HeapTupleIsValid(proctup)) {
	case 1:
		result = (RegProcedure) amgetattr(proctup,
						  InvalidBuffer,
						  ObjectIdAttributeNumber,
						  &proc->rd_att,
						  &isnull);
		if (isnull) {
			elog(FATAL, "regprocin: null procedure %s", proname);
		}
		break;
	case 0:
		result = (RegProcedure) 0;
#ifdef	EBUG
		elog(DEBUG, "regprocin: no such procedure %s", proname);
#endif	/* defined(EBUG) */
	}
	amendscan(procscan);
	amclose(proc);
	return((int32) result);
}

/*
 *	regprocout	- converts proid to "proname"
 */
char *
regprocout(proid)
	RegProcedure proid;
{
	Relation	proc;
	HeapScanDesc	procscan;
	HeapTuple	proctup;
	char		*result;
	ScanKeyEntryData	key;

	result = (char *)palloc(16);
	proc = amopenr(ProcedureRelationName->data);
	if (!RelationIsValid(proc)) {
		elog(WARN, "regprocout: could not open %s",
		     ProcedureRelationName->data);
		return(0);
	}
	ScanKeyEntryInitialize(&key,
			       (bits16)0,
			       (AttributeNumber)ObjectIdAttributeNumber,
			       (RegProcedure)F_INT4EQ,
			       (Datum)proid);

	procscan = ambeginscan(proc, 0, NowTimeQual, 1, &key);
	if (!HeapScanIsValid(procscan)) {
		amclose(proc);
		elog(WARN, "regprocin: could not being scan of %s",
		     ProcedureRelationName);
		return(0);
	}
	proctup = amgetnext(procscan, 0, (Buffer *)NULL);
	switch (HeapTupleIsValid(proctup)) {
		char	*s;
		Boolean	isnull;
	case 1:
		s = (char *) amgetattr(proctup, InvalidBuffer, 1,
				       &proc->rd_att, &isnull);
		if (!isnull) {
			strncpy(result, s, 16);
			break;
		}
		elog(FATAL, "regprocout: null procedure %d", proid);
		/*FALLTHROUGH*/
	case 0:
		bzero(result, 16);
		result[0] = '-';
#ifdef	EBUG
		elog(DEBUG, "regprocout: no such procedure %d", proid);
#endif	/* defined(EBUG) */
	}
	amendscan(procscan);
	amclose(proc);
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */

ObjectId
RegprocToOid(rp)
    RegProcedure rp;
{
    return (ObjectId)rp;
}

	 /* (see int.c for comparison/operation routines) */


	     /* ========== PRIVATE ROUTINES ========== */

