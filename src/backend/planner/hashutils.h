/* ----------------------------------------------------------------
 *   FILE
 *	hashutils.h
 *
 *   DESCRIPTION
 *	prototypes for hashutils.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef hashutilsIncluded		/* include this file only once */
#define hashutilsIncluded	1

extern LispValue group_clauses_by_hashop ARGS((LispValue clauseinfo_list, LispValue inner_relid));
extern HInfo match_hashop_hashinfo ARGS((ObjectId hashop, LispValue hashinfo_list));

#endif /* hashutilsIncluded */
