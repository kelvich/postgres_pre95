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
 * The number of significant bytes are always equal to the length of the
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
#include "catalog/pg_type.h"
#include "utils/log.h"

/*-------------------------------------------------------------------------
 * datumGetSize
 *
 * Find the "real" size of a datum, given the datum value,
 * its type, whether it is a "by value", and its length.
 *
 * To cut a long story short, usually the real size is equal to the
 * type length, with the exception of variable length types which have
 * a length equal to -1. In this case, we have to look at the value of
 * the datum itself (which is a pointer to a 'varlena' struct) to find
 * its size.
 *-------------------------------------------------------------------------
 */
Size
datumGetSize(value, type, byVal, len)
Datum value;
ObjectId type;
bool byVal;
Size len;
{

    struct varlena *s;
    Size size;

    if (byVal) {
	if (len >= 0 && len <= sizeof(Datum)) {
	    size = len;
	} else {
	    elog(WARN,
		"datumGetSize: Error: type=%ld, byVaL with len=%d",
		(long) type, len);
	}
    } else { /*  not byValue */
	if (len == -1) {
	    /*
	     * variable length type
	     * Look at the varlena struct for its real length...
	     */
	    s = (struct varlena *) DatumGetPointer(value);
	    if (!PointerIsValid(s)) {
		elog(WARN,
		    "datumGetSize: Invalid Datum Pointer");
	    }
	    size = (Size) VARSIZE(s);
	} else {
	    /*
	     * fixed length type
	     */
	    size = len;
	}
    }

    return(size);
}

/*-------------------------------------------------------------------------
 * datumCopy
 *
 * make a copy of a datum
 *
 * If the type of the datum is not passed by value (i.e. "byVal=false")
 * then we assume that the datum contains a pointer and we copy all the
 * bytes pointed by this pointer
 *-------------------------------------------------------------------------
 */
Datum
datumCopy(value, type, byVal, len)
Datum value;
ObjectId type;
bool byVal;
Size len;
{

    Size realSize;
    Datum res;
    Pointer s;
    char *palloc();

    realSize = datumGetSize(value, type, byVal, len);

    if (byVal) {
	res = value;
    } else {
	/*
	 * the value is a pointer. Allocate enough space
	 * and copy the pointed data.
	 */
	s = (Pointer) palloc(realSize);
	if (s == NULL) {
	    elog(WARN,"datumCopy: out of memory\n");
	}
	bcopy(DatumGetPointer(value), s, realSize);
	res = PointerGetDatum(s);
    }
    return(res);
}

/*-------------------------------------------------------------------------
 * datumFree
 *
 * Free the space occupied by a datum CREATED BY "datumCopy"
 *
 * NOTE: DO NOT USE THIS ROUTINE with datums returned by amgetattr() etc.
 * ONLY datums created by "datumCopy" can be freed!
 *-------------------------------------------------------------------------
 */
void
datumFree(value, type, byVal, len)
Datum value;
ObjectId type;
bool byVal;
Size len;
{

    Size realSize;
    Pointer s;

    realSize = datumGetSize(value, type, byVal, len);

    if (!byVal) {
	/*
	 * free the space palloced by "datumCopy()"
	 */
	s = DatumGetPointer(value);
	pfree(s);
    }
}

/*-------------------------------------------------------------------------
 * datumIsEqual
 *
 * Return true if two datums are equal, false otherwise
 *
 * NOTE: XXX!
 * We just compare the bytes of the two values, one by one.
 * This routine will return false if there are 2 different
 * representations of the same value (something along the lines
 * of say the representation of zero in one's complement arithmetic).
 *
 *-------------------------------------------------------------------------
 */
bool
datumIsEqual(value1, value2, type, byVal, len)
Datum value1;
Datum value2;
ObjectId type;
bool byVal;
Size len;
{
    Size size1, size2;
    char *s1, *s2;

    if (byVal) {
	/*
	 * just compare the two datums.
	 * NOTE: just comparing "len" bytes will not do the
	 * work, because we do not know how these bytes
	 * are aligned inside the "Datum".
	 */
	if (value1 == value2)
	    return(true);
	else
	    return(false);
    } else {
	/*
	 * byVal = false
	 * Compare the bytes pointed by the pointers stored in the
	 * datums.
	 */
	size1 = datumGetSize(value1, type, byVal, len);
	size2 = datumGetSize(value2, type, byVal, len);
	if (size1 != size2)
	    return(false);
	s1 = (char *) DatumGetPointer(value1);
	s2 = (char *) DatumGetPointer(value2);
	if (!bcmp(s1, s2, size1))
	    return(true);
	else
	    return(false);
    }
}
	
