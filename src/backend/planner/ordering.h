/* ----------------------------------------------------------------
 *   FILE
 *	ordering.h
 *
 *   DESCRIPTION
 *	prototypes for ordering.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef orderingIncluded		/* include this file only once */
#define orderingIncluded	1

extern bool equal_path_path_ordering ARGS((LispValue path_ordering1, LispValue path_ordering2));
extern bool equal_path_merge_ordering ARGS((LispValue path_ordering, LispValue merge_ordering));
extern bool equal_merge_merge_ordering ARGS((LispValue merge_ordering1, LispValue merge_ordering2));

#endif /* orderingIncluded */
