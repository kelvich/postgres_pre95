/* ----------------------------------------------------------------
 *   FILE
 *	clauseinfo.h
 *
 *   DESCRIPTION
 *	prototypes for clauseinfo.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef clauseinfoIncluded		/* include this file only once */
#define clauseinfoIncluded	1

extern bool valid_or_clause ARGS((LispValue clauseinfo));
extern LispValue get_actual_clauses ARGS((LispValue clauseinfo_list));
extern LispValue get_relattvals ARGS((LispValue clauseinfo_list));
extern LispValue get_joinvars ARGS((LispValue relid, LispValue clauseinfo_list));
extern LispValue get_joinvar ARGS((ObjectId relid, CInfo clauseinfo));
extern LispValue get_opnos ARGS((LispValue clauseinfo_list));

#endif /* clauseinfoIncluded */
