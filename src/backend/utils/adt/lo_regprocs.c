/*
 * lo_regprocs.c - registered "in" and "out" functions for large object
 * ADT's.
 *
 * $Header$
 */

#include <sys/file.h>
#include "tmp/c.h"
#include "tmp/libpq-fs.h"
#include "access/relscan.h"
#include "access/tupdesc.h"
#include "catalog/pg_naming.h"
#include "catalog/pg_lobj.h"
#include "storage/itemptr.h"
#include "utils/rel.h"
#include "utils/large_object.h"
#include "utils/log.h"

/*
 * These routines allow the user to import and export Postgres PURE_FILE
 * large objects.
 */

extern LargeObject *NewLargeObject();

char *
lo_filein(filename)

char *filename;

{
	return((char *) NewLargeObject(filename, EXTERNAL_FILE));
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
