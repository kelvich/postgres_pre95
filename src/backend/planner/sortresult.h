/* ----------------------------------------------------------------
 *   FILE
 *	sortresult.h
 *
 *   DESCRIPTION
 *	prototypes for sortresult.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef sortresultIncluded		/* include this file only once */
#define sortresultIncluded	1

extern LispValue relation_sortkeys ARGS((LispValue tlist));
extern LispValue sort_list_car ARGS((LispValue list));
extern void sort_relation_paths ARGS((LispValue pathlist, LispValue sortkeys, Rel rel, int width));
extern Plan sort_level_result ARGS((Plan plan, int numkeys));

#endif /* sortresultIncluded */
