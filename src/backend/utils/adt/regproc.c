
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * regproc.c --
 * 	Functions for the built-in type "RegProcedure".
 */

#include "c.h"
#include "postgres.h"

#include "access.h"
#include "fmgr.h"
#include "log.h"

#include "catname.h"
#include "heapam.h"
#include "relscan.h"

#include "regproc.h"

#include <strings.h>

RcsId("$Header$");


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
	HeapScan	procscan;
	HeapTuple	proctup;
	struct skey	key;		/* static better ??? */
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
	key.sk_attnum = 1;		/* proname */
	key.sk_flags = 0;
	key.sk_opr = F_CHAR16EQ;
	key.sk_data = (DATUM) proname;
	procscan = ambeginscan(proc, 0, DefaultTimeRange, 1, &key);
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
						  T_OID,
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
	HeapScan	procscan;
	HeapTuple	proctup;
	char		*result;
	struct skey	key;
	extern		bzero();

	result = palloc(16);
	proc = amopenr(ProcedureRelationName->data);
	if (!RelationIsValid(proc)) {
		elog(WARN, "regprocout: could not open %s",
		     ProcedureRelationName->data);
		return(0);
	}
	key.sk_attnum = T_OID;		/* proid */
	key.sk_flags = 0;
	key.sk_opr = F_INT4EQ;
	key.sk_data = (DATUM) proid;
	procscan = ambeginscan(proc, 0, DefaultTimeRange, 1, &key);
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

	 /* (see int.c for comparison/operation routines) */


	     /* ========== PRIVATE ROUTINES ========== */


/*
 *	RegProcedureIsValid
 */
bool
RegProcedureIsValid(procedure)
	RegProcedure	procedure;
{
	return (ObjectIdIsValid(procedure));
}
