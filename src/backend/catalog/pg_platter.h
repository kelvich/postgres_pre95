/* ----------------------------------------------------------------
 *   FILE
 *	pg_platter.h
 *
 *   DESCRIPTION
 *	definition of the system "platter" relation, pg_platter,
 *	which declares what platters are available for use by the
 *	Sony WORM optical-disk jukebox.  If you don't have a Sony
 *	jukebox, this class won't be of any use, although it will
 *	be constructed by initdb.
 *
 *	the genbki.sh script reads this file and generates .bki
 *	information from the DATA() statements.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgPlatterIncluded
#define PgPlatterIncluded		/* do this exactly once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/c.h"
#include "tmp/postgres.h"

#ifdef SONY_JUKEBOX

CATALOG(pg_platter) {
     char16 	plname;
} FormData_pg_platter;

/* ----------------
 *	Form_pg_platter corresponds to a pointer to a tuple with
 *	the format of pg_platter relation.
 * ----------------
 */
typedef FormData_pg_platter	*Form_pg_platter;

/* ----------------
 *	compiler constants for pg_platter
 * ----------------
 */
#define Name_pg_platter			"pg_platter"

#define Natts_pg_platter		1
#define Anum_pg_platter_plname		1

/* ----------------
 *	initial contents of pg_platter
 * ----------------
 */

#endif /* SONY_JUKEBOX */

#endif /* PgPlatterIncluded */
