/* ----------------------------------------------------------------
 *   FILE
 *	semanopt.h
 *
 *   DESCRIPTION
 *	prototypes for semanopt.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef semanoptIncluded		/* include this file only once */
#define semanoptIncluded	1

extern List SemantOpt ARGS((List varlist, List rangetable, List qual, List *is_redundent, int is_first));
extern List SemantOpt2 ARGS((List rangetable, List qual, List modqual, List tlist));
extern void replace_tlist ARGS((Index left, Index right, List tlist));
extern void replace_varnodes ARGS((Index left, Index right, List qual));
extern List find_allvars ARGS((List root, List rangetable, List tlist, List qual));
extern List update_vars ARGS((List rangetable, List varlist, List qual));
extern Index ConstVarno ARGS((List rangetable, Const constnode, char **attname));
extern List MakeTClause ARGS((void));
extern List MakeFClause ARGS((void));

#endif /* semanoptIncluded */
