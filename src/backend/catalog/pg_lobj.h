/* ----------------------------------------------------------------
 *   FILE
 *      pg_lobj.h
 *
 *   DESCRIPTION
 *      mapping of OIDs to large object descriptors
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */
#ifndef PgLargeObjectIncluded
#define PgLargeObjectIncluded 1 /* include this only once */

/* ----------------
 *      postgres.h contains the system type definintions and the
 *      CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *      can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------------------------------------------------------
 *      pg_large_object definition.
 *
 *      cpp turns this into typedef struct FormData_pg_large_object
 * ----------------------------------------------------------------
 */

CATALOG(pg_large_object) {
    oid                 ourid;
    bytea               object_descriptor; /* opaque cookie variable length structure */
} FormData_pg_large_object;

/* ----------------
 *      compiler constants for pg_large_object
 * ----------------
 */
#define Name_pg_large_object    "pg_large_object"
#define Natts_pg_large_object                   2
#define Anum_pg_large_object_oid                1
#define Anum_pg_large_object_object_descriptor  2

#ifndef struct_large_object_Defined
#define struct_large_object_Defined 1

struct  large_object {
    OID                 ourid;
    /* variable length structure */
    struct varlena      object_descriptor;
};
#endif struct_large_object_Defined

int LOputOIDandLargeObjDesc ARGS((oid objOID , LargeObject *desc ));
int CreateLOBJTuple ARGS((oid objOID , LargeObject *desc ));
LargeObject *LOassocOIDandLargeObjDesc ARGS((oid objOID ));

#endif PgLargeObjectIncluded
