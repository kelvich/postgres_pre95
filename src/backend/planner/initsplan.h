/* ----------------------------------------------------------------
 *   FILE
 *	initsplan.h
 *
 *   DESCRIPTION
 *	prototypes for initsplan.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef initsplanIncluded		/* include this file only once */
#define initsplanIncluded	1

extern void initialize_targetlist ARGS((List tlist));
extern void initialize_qualification ARGS((LispValue clauses));
extern void add_clause_to_rels ARGS((LispValue clause));
extern void add_join_clause_info_to_rels ARGS((CInfo clauseinfo, List join_relids));
extern void add_vars_to_rels ARGS((List vars, List join_relids));
extern void initialize_join_clause_info ARGS((LispValue rel_list));
extern MergeOrder mergesortop ARGS((LispValue clause));
extern ObjectId hashjoinop ARGS((LispValue clause));

#endif /* initsplanIncluded */
