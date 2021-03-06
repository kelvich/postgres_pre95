/* ----------------------------------------------------------------
 *   FILE
 *	prepqual.h
 *
 *   DESCRIPTION
 *	prototypes for prepqual.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef prepqualIncluded		/* include this file only once */
#define prepqualIncluded	1

extern LispValue preprocess_qualification ARGS((LispValue qual, LispValue tlist));
extern LispValue cnfify ARGS((LispValue qual, bool removeAndFlag));
extern LispValue pull_args ARGS((LispValue qual));
extern LispValue pull_ors ARGS((LispValue orlist));
extern LispValue pull_ands ARGS((LispValue andlist));
extern LispValue find_nots ARGS((LispValue qual));
extern LispValue push_nots ARGS((LispValue qual));
extern LispValue normalize ARGS((LispValue qual));
extern LispValue or_normalize ARGS((LispValue orlist));
extern LispValue distribute_args ARGS((LispValue item, LispValue args));
extern LispValue qualcleanup ARGS((LispValue qual));
extern LispValue remove_ands ARGS((LispValue qual));
extern LispValue update_relations ARGS((LispValue tlist));
extern LispValue update_clauses ARGS((LispValue update_relids, LispValue qual, LispValue command));

#endif /* prepqualIncluded */
