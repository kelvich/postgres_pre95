/* ----------------------------------------------------------------
 *   FILE
 *	clause.h
 *
 *   DESCRIPTION
 *	prototypes for clause.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef clauseIncluded		/* include this file only once */
#define clauseIncluded	1

extern bool lambda1 ARGS((LispValue qual));
extern LispValue pull_constant_clauses ARGS((LispValue quals));
extern LispValue pull_relation_level_clauses ARGS((LispValue quals));
extern LispValue clause_relids_vars ARGS((LispValue clause));
extern int NumRelids ARGS((Expr clause));
extern bool relation_level_clause_p ARGS((LispValue clause));
extern bool contains_not ARGS((LispValue clause));
extern bool join_clause_p ARGS((LispValue clause));
extern bool qual_clause_p ARGS((LispValue clause));
extern void fix_opid ARGS((LispValue clause));
extern LispValue fix_opids ARGS((LispValue clauses));
extern LispValue get_relattval ARGS((LispValue clause));
extern LispValue get_relsatts ARGS((LispValue clause));
extern bool is_clause ARGS((LispValue clause));
extern void CommuteClause ARGS((LispValue clause));

#endif /* clauseIncluded */
