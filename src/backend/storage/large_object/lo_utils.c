/*
 * large_object.c - routines for manipulating and accessing large objects
 *
 * $Header$
 */

#include <sys/file.h>
#include "tmp/c.h"
#include "utils/large_object.h"

/*
 * Creates a new large object descriptor.
 */

LargeObject *
NewLargeObject(filename, type)

char *filename;
long type;

{
    LargeObject *retval;
    long obj_len, filename_len = strlen(filename) + 1;

    obj_len = filename_len
            + sizeof(LargeObject) - sizeof(LargeObjectDataPtr)
            + sizeof(long);

    retval = (LargeObject *) palloc(obj_len);

    retval->lo_length = obj_len;

    retval->lo_storage_type = type;

    /*
     * If these entries are relevent (for Postgres-owned large objects), they
     * must be set by the caller.  Otherwise, they cannot be trusted and should
     * be zero anyway.
     */

    retval->lo_nblocks = retval->lo_lastoff = retval->lo_version = 0;

    strcpy(retval->lo_ptr.filename, filename);

    return(retval);
}
