/*
 * printtup.c --
 *	Routines to print out tuples to the standard output.
 */

#include <sys/file.h>
#include <stdio.h>
#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/skey.h"
#include "storage/buf.h"
#include "utils/memutils.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"


static ObjectId	typtoout();

/* ----------------
 *	convtypeinfo
 *
 *	Converts an old-style typeinfo (struct attribute array) into
 *	the new-style typeinfo (struct attribute (*) array).
 *
 *	XXX temporary, but still called by the executor.
 * ----------------
 */
struct attribute **
convtypeinfo(natts, att)
    int 		natts;
    struct attribute	*att;
{
    struct	attribute *ap, **rpp, **retatt;
    
    rpp = retatt = (struct attribute **) 
	palloc(natts * sizeof(*rpp) + natts * sizeof(**rpp));
    MemoryCopy(rpp + natts, att, natts * sizeof(**rpp));
    
    /* ----------------
     *	REMOVED:
     *
     * pfree(att);	
     *
     *	executor now does the freeing.. -cim 8/8/89
     * ----------------
     */
    ap = (struct attribute *)(rpp + natts);
    while (--natts >= 0)
	*rpp++ = ap++;
    return(retatt);
}

/* ----------------------------------------------------------------
 *	printtup / debugtup support
 * ----------------------------------------------------------------
 */
/* ----------------
 *	typtoout - used by printtup and debugtup
 * ----------------
 */
static ObjectId
typtoout(type)
    ObjectId	type;
{
    HeapTuple	typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
				    (char *) type,
				    (char *) NULL,
				    (char *) NULL,
				    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
	return((ObjectId)
	       ((TypeTupleForm) GETSTRUCT(typeTuple))->typoutput);

    elog(WARN, "typtoout: Cache lookup of type %d failed", type);
    return(InvalidObjectId);
}

/* ----------------
 *	printtup
 * ----------------
 */
printtup(tuple, typeinfo)
    HeapTuple		tuple;
    struct attribute 	*typeinfo[];
{
    int		i, j, k;
    char		*outputstr, *attr;
    Boolean		isnull;
    ObjectId	typoutput;
    
    /* ----------------
     *	tell the frontend to expect new tuple data
     * ----------------
     */
    pq_putnchar("D", 1);
    
    /* ----------------
     *	send a bitmap of which attributes are null
     * ----------------
     */
    j = 0;
    k = 1 << 7;
    for (i = 0; i < tuple->t_natts; ) {
	attr = heap_getattr(tuple, InvalidBuffer, ++i, typeinfo, &isnull);
	if (!isnull)
	    j |= k;
	k >>= 1;
	if (!(i & 7)) {
	    pq_putint(j, 1);
	    j = 0;
	    k = 1 << 7;
	}
    }
    if (i & 7)
	pq_putint(j, 1);
    
    /* ----------------
     *	send the attributes of this tuple
     * ----------------
     */
    for (i = 0; i < tuple->t_natts; ++i) {
	attr = heap_getattr(tuple, InvalidBuffer, i+1, typeinfo, &isnull);
	typoutput = typtoout((ObjectId) typeinfo[i]->atttypid);
	
	if (!isnull && ObjectIdIsValid(typoutput)) {
	    outputstr = fmgr(typoutput, attr);
	    pq_putint(strlen(outputstr)+4, 4);
	    pq_putnchar(outputstr, strlen(outputstr));
	    pfree(outputstr);
	}
    }
}

/* ----------------
 *	printatt
 * ----------------
 */
void
printatt(attributeId, attributeP, value)
    unsigned		attributeId;
    struct attribute	*attributeP;
    char			*value;
{
    printf("\t%2d: %s%s%s%s\t(typeid = %lu, len = %d, byval = %c)\n",
	   attributeId,
	   attributeP->attname,
	   value != NULL ? " = \"" : "",
	   value != NULL ? value : "",
	   value != NULL ? "\"" : "",
	   attributeP->atttypid,
	   attributeP->attlen,
	   attributeP->attbyval ? 't' : 'f');
}

/* ----------------
 *	showatts
 * ----------------
 */
showatts(name, natts, attinfo)
    char			*name;
    int			natts;
    struct attribute	*attinfo[];
{
    register int	i;
    
    puts(name);
    for (i = 0; i < natts; ++i)
	printatt((unsigned) i+1, attinfo[i], (char *) NULL);
    printf("\t----\n");
}

/* ----------------
 *	debugtup
 * ----------------
 */
debugtup(tuple, typeinfo)
    HeapTuple		tuple;
    struct attribute 	*typeinfo[];
{
    register int	i;
    char		*attr, *value;
    Boolean		isnull;
    ObjectId		typoutput;
    
    for (i = 0; i < tuple->t_natts; ++i) {
	attr = heap_getattr(tuple, InvalidBuffer, i+1, typeinfo, &isnull);
	typoutput = typtoout((ObjectId) typeinfo[i]->atttypid);
	
	if (!isnull && ObjectIdIsValid(typoutput)) {
	    value = fmgr(typoutput, attr);
	    printatt((unsigned) i+1, typeinfo[i], value);
	    pfree(value);
	}
    }
    printf("\t----\n");
}

