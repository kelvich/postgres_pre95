/* ----------------------------------------------------------------
 *   FILE
 *	orindxpath.h
 *
 *   DESCRIPTION
 *	prototypes for orindxpath.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef orindxpathIncluded		/* include this file only once */
#define orindxpathIncluded	1

extern LispValue create_or_index_paths ARGS((Rel rel, List clauses));
extern LispValue best_or_subclause_indices ARGS((Rel rel, List subclauses, List indices, List examined_indexids, Cost subcost, List selectivities));
extern LispValue best_or_subclause_index ARGS((Rel rel, LispValue subclause, List indices));

#endif /* orindxpathIncluded */
