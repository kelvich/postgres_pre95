/* ----------------------------------------------------------------
 *   FILE
 *      pg_listener.h
 *
 *   DESCRIPTION
 *      Asynchronous notification
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */
#ifndef PgListenerIncluded
#define PgListenerIncluded 1 /* include this only once */

/* ----------------
 *      postgres.h contains the system type definintions and the
 *      CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *      can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/postgres.h"

/* ----------------------------------------------------------------
 *      pg_listener definition.
 *
 *      cpp turns this into typedef struct FormData_pg_listener
 * ----------------------------------------------------------------
 */

CATALOG(pg_listener) {
    char16       relname;
    int4         listenerpid;
    int4         notification;
} FormData_pg_listener;

/* ----------------
 *      compiler constants for pg_listener
 * ----------------
 */
#define Name_pg_listener    "pg_listener"
#define Natts_pg_listener                       3
#define Anum_pg_listener_relname                1
#define Anum_pg_listener_pid                    2
#define Anum_pg_listener_notify                 3

/* ----------------
 *      initial contents of pg_listener are NOTHING.
 * ----------------
 */


#ifndef struct_listener_Defined
#define struct_listener_Defined 1

struct listener  {
    char16        relname;
    int32         listenerpid;
    int32         notification;
};
#endif struct_listener_Defined


#endif PgListenerIncluded
