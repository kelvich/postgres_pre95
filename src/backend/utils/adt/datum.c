/*===================================================================
 *
 * FILE:
 * datum.c
 *
 * HEADER:
 * $Header$
 *
 * DESCRIPTION:
 *
 * In the implementation of the next routines we assume the following:
 *
 * A) if a type is "byVal" then all the information is stored in the
 * Datum itself (i.e. no pointers involved!). In this case the
 * length of the type is always greater than zero and less than
 * "sizeof(Datum)"
 * B) if a type is not "byVal" and it has a fixed length, then
 * the "Datum" always contain a pointer to a stream of bytes.
 * The number of significant bytes are always equal to the length of thr
 * type.
 * C) if a type is not "byVal" and is of variable length (i.e. it has
 * length == -1) then "Datum" always points to a "struct varlena".
 * This varlena structure has information about the actual length of this
 * particular instance of the type and about its value.
 *
 */

#include "tmp/c.h"
#include "tmp/postgres.h"
#include "tmp/datum.h"
#include "catalog/syscache.h"
#include "catalog/pg_type.h"
#include "utils/log.h"

/*-------------------------------------------------------------------------
 * datumGetRealLengthAndByVal
 *
 * Given a datum and its type, find the "real" length of the datum's value
 * and if the type is passed "by value" or not.
 * If the type's length is positive (i.e. fixed) then the "real" length is
 * just the type's length.
 * But if it is -1 (i.e. we are dealing with a variable length type)
 * then we have to look at the struct varlena pointed by the datum.
 */
void
datumGetRealLengthAndByVal(value, type, sizeP, byValP)
Datum value;
ObjectId type;
Size *sizeP;
bool *byValP;
{

    Size typeLength;
    HeapTuple typeTuple;
    int i;
    struct varlena *s;

    /*
     * find the type's "length" and "byVal"
     */
    typeTuple = SearchSysCacheTuple(TYPOID,
				    (char *) type,
				    (char *) NULL,
				    (char *) NULL,
				    (char *) NULL);
    if (!HeapTupleIsValid(typeTuple)) {
	elog(WARN,
	    "datumGetRealLengthAndByVal: Cache lookup of type %ld failed",
	    (long) type);
    }
    typeLength	= ((ObjectId) ((TypeTupleForm)
			GETSTRUCT(typeTuple))->typlen);
    *byValP	= ((bool) ((TypeTupleForm)
			GETSTRUCT(typeTuple))->typbyval);

    if (*byValP) {
	if (typeLength >= 0) {
	    *sizeP = typeLength;
	} else {
	    elog(WARN,
		"datumGetRealLengthAndByVal: type %ld: (len<0 and byVal)",
		(long) type);
	}
    } else { /*  not byValue */
	if (typeLength <= -1) {
	    /*
	     * variable length type
	     * Look at the varlena struct for its real length...
	     */
	    s = (struct varlena *) DatumGetPointer(value);
	    if (!PointerIsValid(s)) {
		elog(WARN,
		    "datumGetRealLengthAndByVal: Invalid Datum Pointer");
	    }
	    *sizeP = (Size) PSIZE(s);
	} else {
	    /*
	     * fixed length type
	     */
	    *sizeP = typeLength;
	}
    }
}

/*-------------------------------------------------------------------------
 * copyDatum
 *
 * make a copy of a datum
 *
 * If the type of the datum is not passed by value (i.e. "byVal=flase")
 * then we assume that the datum contains a pointer and we copy all the
 * bytes pointed by this pointer
 */
Datum
copyDatum(value, type)
Datum value;
ObjectId type;
{

    Size length;
    bool byVal;
    Datum res;
    Pointer s;
    char *palloc();

    datumGetRealLengthAndByVal(value, type, &length, &byVal);

    if (byVal) {
	res = value;
    } else {
	s = (Pointer) palloc(length);
	if (s == NULL) {
	    elog(WARN,"copyDatum: out of memory\n");
	}
	bcopy(DatumGetPointer(value), s, length);
	res = PointerGetDatum(s);
    }
    return(res);
}

/*-------------------------------------------------------------------------
 * freeDatum
 *
 * Free the space occupied by a datum CREATED BY "copyDatum"
 *
 * NOTE: DO NOT USE THIS ROUTINE with datums returned by amgetattr() etc.
 * ONLY datums created by "copyDatum" can be freed!
 */
void
freeDatum(value, type)
Datum value;
ObjectId type;
{

    Size length;
    bool byVal;
    Pointer s;

    datumGetRealLengthAndByVal(value, type, &length, &byVal);

    if (!byVal) {
	/*
	 * free the space palloced by "copyDatum()"
	 */
	s = DatumGetPointer(value);
	pfree(s);
    }
}
