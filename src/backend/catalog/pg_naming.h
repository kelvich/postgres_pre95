/* ----------------------------------------------------------------
 *   FILE
 *      pg_naming.h
 *
 *   DESCRIPTION
 *      unix style naming of OIDs
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */
#ifndef PgNamingIncluded
#define PgNamingIncluded 1      /* include this only once */

/* ----------------
 *      postgres.h contains the system type definintions and the
 *      CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *      can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------------------------------------------------------
 *      pg_naming definition.
 *
 *      cpp turns this into typedef struct FormData_pg_naming
 * ----------------------------------------------------------------
 */
CATALOG(pg_naming) {
    char16              filename;
    oid                 ourid;
    oid                 parentid;
} FormData_pg_naming;

/* ----------------
 *      Form_pg_naming corresponds to a pointer to a tuple with
 *      the format of pg_naming relation.
 * ----------------
 */
typedef FormData_pg_naming      *Form_pg_naming;

#define NamingTupleFormData FormData_pg_naming;

/* ----------------
 *      compiler constants for pg_naming
 * ----------------
 */
#define Name_pg_naming          "pg_naming"
#define Natts_pg_naming         3
#define Anum_pg_naming_filename 1
#define Anum_pg_naming_oid      2
#define Anum_pg_naming_parent_oid       3

/* ----------------
 *      initial contents of pg_naming
 * ----------------
 */

DATA(insert OID = 811 ( "/" 811 0 ));

#ifndef struct_naming_Defined
#define  struct_naming_Defined 1
struct naming {
    char16              filename;
    OID                 ourid;
    OID                 parentid;
};
#endif  struct_naming_Defined
oid FilenameToOID ARGS((char *fname ));
void CreateNameTuple ARGS((oid parentID , char *name , oid ourid ));
oid CreateNewNameTuple ARGS((oid parentID , char *name ));
oid DeleteNameTuple ARGS((oid parentID , char *name ));
oid LOcreatOID ARGS((char *fname , int mode ));
int LOunlinkOID ARGS((char *fname ));
int LOisemptydir ARGS((char *path ));
int LOisdir ARGS((char *path ));
int LOrename ARGS((char *path , char *newpath ));
int path_parse ARGS((char *pathname , char *pathcomp [], int n_comps ));
void to_basename ARGS((char *fname , char *bname , char *tname ));

#endif PgNamingIncluded
