/*
 * lo_regprocs.c - registered "in" and "out" functions for large object
 * ADT's.
 *
 * $Header$
 */

#include <sys/file.h>
#include "tmp/c.h"
#include "utils/large_object.h"

/*
 * These routines allow the user to import and export Postgres PURE_FILE
 * large objects.
 */

extern LargeObject *NewLargeObject();

char *
lo_filein(filename)

char *filename;

{
	return((char *) NewLargeObject(filename, PURE_FILE));
}

char *
lo_fileout(object)

LargeObject *object;

{
    char *retval; 

    Assert(PointerIsValid(object));

    retval = (char *) palloc(strlen(object->lo_ptr.filename) + 1);

    strcpy(retval, object->lo_ptr.filename);
    return(retval);
}
