/* ----------------------------------------------------------------
 *   FILE
 *	pg_plmap.h
 *
 *   DESCRIPTION
 *	Definition of the system "platter map" relation, pg_plmap.
 *	This relation provides a map of the data that appear on a
 *	Sony WORM optical platter.  If you don't have a Sony WORM
 *	jukebox, this relation won't be of any use to you, but it
 *	will still be created by initdb.
 *
 *	The genbki.sh script reads this file and generates .bki
 *	information from the DATA() statements.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef PgPlmapIncluded
#define PgPlmapIncluded		/* do this exactly once */

/* ----------------
 *	postgres.h contains the system type definintions and the
 *	CATALOG(), BOOTSTRAP and DATA() sugar words so this file
 *	can be read by both genbki.sh and the C compiler.
 * ----------------
 */
#include "tmp/c.h"
#include "tmp/postgres.h"

#ifdef SONY_JUKEBOX

CATALOG(pg_plmap) {
     oid	plid;
     oid	pldbid;
     oid	plrelid;
     int4	plblkno;
     int4	ploffset;
     int4	plextentsz;
} FormData_pg_plmap;

/* ----------------
 *	Form_pg_plmap corresponds to a pointer to a tuple with
 *	the format of pg_plmap relation.
 * ----------------
 */
typedef FormData_pg_plmap	*Form_pg_plmap;

/* ----------------
 *	compiler constants for pg_plmap
 * ----------------
 */
#define Name_pg_plmap			"pg_plmap"

#define Natts_pg_plmap			6
#define Anum_pg_plmap_ploid		1
#define Anum_pg_plmap_pldbid		2
#define Anum_pg_plmap_plrelid		3
#define Anum_pg_plmap_plblkno		4
#define Anum_pg_plmap_ploffset		5
#define Anum_pg_plmap_plextentsz	6

/* ----------------
 *	initial contents of pg_plmap
 * ----------------
 */

#endif /* SONY_JUKEBOX */

#endif /* PgPlmapIncluded */
