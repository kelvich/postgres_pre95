/* ----------------------------------------------------------------
 *   FILE
 *	libpq-be.h
 *
 *   DESCRIPTION
 *	This file contains definitions for structures and
 *	externs for functions used by the POSTGRES backend.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef LibpqBeIncluded 	/* include this file only once. */
#define LibpqBeIncluded 	1

/* ----------------
 *	include stuff common to fe and be
 * ----------------
 */
#include "tmp/libpq.h"

/* ----------------
 *	declarations for backend libpq support routines
 * ----------------
 */
extern void be_portalinit ARGS(());
extern void be_portalpush ARGS((PortalEntry *entry));
extern PortalEntry *be_portalpop ARGS(());
extern PortalEntry *be_currentportal ARGS(());
extern PortalEntry *be_newportal ARGS((String pname));
extern void be_typeinit ARGS((PortalEntry *entry, struct attribute **attrs, int natts));
extern void be_printtup ARGS((HeapTuple tuple, struct attribute *typeinfo[]));
extern char *PQfn ARGS((int fnid, int *result_buf, int result_len, int result_is_int, int n args, int nargs));
extern char *PQexec ARGS((char *query));

#endif LibpqBeIncluded
