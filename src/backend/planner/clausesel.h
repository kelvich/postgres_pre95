/* ----------------------------------------------------------------
 *   FILE
 *	clausesel.h
 *
 *   DESCRIPTION
 *	prototypes for clausesel.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef clauseselIncluded		/* include this file only once */
#define clauseselIncluded	1

extern void set_clause_selectivities ARGS((LispValue clauseinfo_list, Cost new_selectivity));
extern Cost product_selec ARGS((LispValue clauseinfo_list));
extern void set_rest_relselec ARGS((LispValue rel_list));
extern void set_rest_selec ARGS((LispValue clauseinfo_list));
extern Cost compute_clause_selec ARGS((LispValue clause, LispValue or_selectivities));
extern Cost compute_selec ARGS((LispValue clauses, LispValue or_selectivities));
extern LispValue translate_relid ARGS((LispValue relid));

#endif /* clauseselIncluded */
