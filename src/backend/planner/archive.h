/* ----------------------------------------------------------------
 *   FILE
 *	archive.h
 *
 *   DESCRIPTION
 *	prototypes for archive.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef archiveIncluded		/* include this file only once */
#define archiveIncluded	1

extern void plan_archive ARGS((List rt));
extern LispValue find_archive_rels ARGS((LispValue relid));

#endif /* archiveIncluded */
