/* ----------------------------------------------------------------
 *   FILE
 *	allpaths.h
 *
 *   DESCRIPTION
 *	prototypes for allpaths.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef allpathsIncluded		/* include this file only once */
#define allpathsIncluded	1

extern LispValue find_paths ARGS((LispValue rels, int nest_level, LispValue sortkeys));
extern LispValue find_rel_paths ARGS((LispValue rels, int level, LispValue sortkeys));
extern LispValue find_join_paths ARGS((LispValue outer_rels, int levels_left, int nest_level));
extern void printclauseinfo ARGS((char *ind, List cl));
extern void printjoininfo ARGS((char *ind, List jl));
extern void printpath ARGS((char *ind, Path p));
extern void printpathlist ARGS((char *ind, LispValue pl));
extern void printrels ARGS((char *ind, List rl));
extern void PrintRels ARGS((List rl));
extern void PRel ARGS((Rel r));

#endif /* allpathsIncluded */
