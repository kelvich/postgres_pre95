/*----------------------------------------------------------------------------
 *   FILE
 *	printtup.c
 *
 *   DESCRIPTION
 *	Routines to print out tuples to the destination (binary or non-binary
 *	portals, frontend/interactive backend, etc.).
 *
 *   INTERFACE ROUTINES
 *	typtoout
 *	printtup
 *	showatts
 *	debugtup
 *	printtup_internal
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 *----------------------------------------------------------------------------
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
#include "utils/palloc.h"
#include "fmgr.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"

#include "tmp/libpq.h"

/* ----------------------------------------------------------------
 *	printtup / debugtup support
 * ----------------------------------------------------------------
 */
/* ----------------
 *	typtoout - used by printtup and debugtup
 * ----------------
 */
ObjectId
typtoout(type)
    ObjectId	type;
{
    HeapTuple	typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
				    (char *) (long) type,
				    (char *) NULL,
				    (char *) NULL,
				    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
	return((ObjectId)
	       ((TypeTupleForm) GETSTRUCT(typeTuple))->typoutput);

    elog(WARN, "typtoout: Cache lookup of type %d failed", type);
    return(InvalidObjectId);
}

static
ObjectId
gettypelem(type)
    ObjectId	type;

{
    HeapTuple	typeTuple;

    typeTuple = SearchSysCacheTuple(TYPOID,
				    (char *) (long) type,
				    (char *) NULL,
				    (char *) NULL,
				    (char *) NULL);

    if (HeapTupleIsValid(typeTuple))
	return((ObjectId)
	       ((TypeTupleForm) GETSTRUCT(typeTuple))->typelem);

    elog(WARN, "typtoout: Cache lookup of type %d failed", type);
    return(InvalidObjectId);
}

/* ----------------
 *	printtup
 * ----------------
 */
void
printtup(tuple, typeinfo)
    HeapTuple		tuple;
    struct attribute 	*typeinfo[];
{
    int		i, j, k;
    char	*outputstr, *attr;
    Boolean	isnull;
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
	    outputstr = fmgr(typoutput, attr, gettypelem(typeinfo[i]->atttypid));
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
static
void
printatt(attributeId, attributeP, value)
    unsigned		attributeId;
    struct attribute	*attributeP;
    char			*value;
{
    printf("\t%2d: %.*s%s%s%s\t(typeid = %lu, len = %d, byval = %c)\n",
	   attributeId,
	   NAMEDATALEN,		/* attname is a char16 */
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
void
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
	    value = fmgr(typoutput, attr, gettypelem(typeinfo[i]->atttypid));
	    printatt((unsigned) i+1, typeinfo[i], value);
	    pfree(value);
	}
    }
    printf("\t----\n");
}

/*#define IPORTAL_DEBUG*/

/* ----------------
 *	printtup_internal
 *      Protocol expects either T, D, C, E, or N.
 *      We use a different data prefix, e.g. 'B' instead of 'D' to
 *      indicate a tuple in internal (binary) form.
 *
 *      This is same as printtup, except we don't use the typout func.
 * ----------------
 */
void
printtup_internal(tuple, typeinfo)
    HeapTuple		tuple;
    struct attribute 	*typeinfo[];
{
    int		i, j, k;
    char	*outputstr, *attr;
    Boolean	isnull;
    ObjectId	typoutput;
    
    /* ----------------
     *	tell the frontend to expect new tuple data
     * ----------------
     */
    pq_putnchar("B", 1);
    
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
#ifdef IPORTAL_DEBUG
    fprintf(stderr, "sending tuple with %d atts\n", tuple->t_natts);
#endif
    for (i = 0; i < tuple->t_natts; ++i) {
	int32 len = typeinfo[i]->attlen;

	attr = heap_getattr(tuple, InvalidBuffer, i+1, typeinfo, &isnull);
	if (!isnull) {
	    /* # of bytes, and opaque data */
	    if (len == -1) {
		/* variable length, assume a varlena structure */
		len = VARSIZE(attr) - VARHDRSZ;

		pq_putint(len, sizeof(int32));
		pq_putnchar(VARDATA(attr), len);
#ifdef IPORTAL_DEBUG
		{
		    char *d = VARDATA(attr);

		    fprintf(stderr, "length %d data %x%x%x%x\n",
			    len, *d, *(d+1), *(d+2), *(d+3));
		}
#endif
	    } else {
		/* fixed size */
		if (typeinfo[i]->attbyval) {
		    int8 i8;
		    int16 i16;
		    int32 i32;

		    pq_putint(len, sizeof(int32));
		    switch (len) {
		    case sizeof(int8):
			i8 = DatumGetChar(attr);
			pq_putnchar((char *) &i8, len);
			break;
		    case sizeof(int16):
			i16 = DatumGetInt16(attr);
			pq_putnchar((char *) &i16, len);
			break;
		    case sizeof(int32):
			i32 = DatumGetInt32(attr);
			pq_putnchar((char *) &i32, len);
			break;
		    }
#ifdef IPORTAL_DEBUG
		    fprintf(stderr, "byval length %d data %d\n", len, attr);
#endif
		} else {
		    pq_putint(len, sizeof(int32));
		    pq_putnchar(attr, len);
#ifdef IPORTAL_DEBUG
		    fprintf(stderr, "byref length %d data %x\n", len, attr);
#endif
		}
	    }
	}
    }
}
