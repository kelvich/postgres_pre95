/* ----------------------------------------------------------------
 *   FILE
 *	indxpath.h
 *
 *   DESCRIPTION
 *	prototypes for indxpath.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef indxpathIncluded		/* include this file only once */
#define indxpathIncluded	1

extern LispValue find_index_paths ARGS((Rel rel, LispValue indices, LispValue clauseinfo_list, LispValue joininfo_list, LispValue sortkeys));
extern void match_index_orclauses ARGS((Rel rel, Rel index, LispValue indexkey, LispValue xclass, LispValue clauseinfo_list));
extern bool match_index_to_operand ARGS((LispValue indexkey, LispValue operand, Rel rel, Rel index));
extern LispValue match_index_orclause ARGS((Rel rel, Rel index, LispValue indexkey, LispValue xclass, LispValue or_clauses, LispValue other_matching_indices));
extern LispValue group_clauses_by_indexkey ARGS((Rel rel, Rel index, LispValue indexkeys, LispValue classes, LispValue clauseinfo_list, bool join));
extern CInfo match_clause_to_indexkey ARGS((Rel rel, Rel index, LispValue indexkey, LispValue xclass, CInfo clauseInfo, bool join));
extern bool pred_test ARGS((List predicate_list, List clauseinfo_list, List joininfo_list));
extern bool one_pred_test ARGS((List predicate, List clauseinfo_list));
extern bool one_pred_clause_expr_test ARGS((List predicate, List clause));
extern bool one_pred_clause_test ARGS((List predicate, List clause));
extern bool clause_pred_clause_test ARGS((List predicate, List clause));
extern LispValue indexable_joinclauses ARGS((Rel rel, Rel index, List joininfo_list));
extern LispValue index_innerjoin ARGS((Rel rel, List clausegroup_list, Rel index));
extern LispValue create_index_paths ARGS((Rel rel, Rel index, LispValue clausegroup_list, bool join));
extern bool indexpath_useless ARGS((IndexPath p, List plist));
extern List add_index_paths ARGS((List indexpaths, List new_indexpaths));
extern bool function_index_operand ARGS((LispValue funcOpnd, Rel rel, Rel index));
extern bool SingleAttributeIndex ARGS((Rel index));

#endif /* indxpathIncluded */
