/* ----------------------------------------------------------------
 *   FILE
 *	joininfo.h
 *
 *   DESCRIPTION
 *	prototypes for joininfo.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef joininfoIncluded		/* include this file only once */
#define joininfoIncluded	1

extern JInfo joininfo_member ARGS((LispValue join_relids, LispValue joininfo_list));
extern JInfo find_joininfo_node ARGS((Rel this_rel, List join_relids));
extern Var other_join_clause_var ARGS((Var var, LispValue clause));

#endif /* joininfoIncluded */
