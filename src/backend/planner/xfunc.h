/* ----------------------------------------------------------------
 *   FILE
 *	xfunc.h
 *
 *   DESCRIPTION
 *	prototypes for xfunc.c and predmig.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef xfuncIncluded		/* include this file only once */
#define xfuncIncluded	1

#include "nodes/relation.h"

/* command line arg flags */
#define XFUNC_OFF -1		/* do no optimization of expensive preds */
#define XFUNC_NOR 2		/* do no optimization of OR clauses */
#define XFUNC_NOPULL 4		/* never pull restrictions above joins */
#define XFUNC_NOPM 8		/* don't do predicate migration */
#define XFUNC_WAIT 16		/* don't do pullup until predicate migration */
#define XFUNC_PULLALL 32	/* pull all expensive restrictions up, always */

/* constants for local and join predicates */
#define XFUNC_LOCPRD 1
#define XFUNC_JOINPRD 2
#define XFUNC_UNKNOWN 0

extern int XfuncMode;  /* defined in tcop/postgres.c */

/* defaults for function attributes used for expensive function calculations */
#define BYTE_PCT 100
#define PERBYTE_CPU 0
#define PERCALL_CPU 0
#define OUTIN_RATIO 100

/* default width assumed for variable length attributes */
#define VARLEN_DEFAULT 128;

/* Macro to get group rank out of group cost and group sel */
#define get_grouprank(a) ((get_groupsel(a) - 1) / get_groupcost(a))

/* Macro to see if a path node is actually a Join */
#define is_join(pathnode) (length(get_relids(get_parent(pathnode))) > 1 ? 1 : 0)

/* function prototypes from planner/path/xfunc.c */
extern void xfunc_trypullup ARGS((Rel rel));
extern int xfunc_shouldpull ARGS((Path childpath, JoinPath parentpath, int whichchild, CInfo *maxcinfopt));
extern CInfo xfunc_pullup ARGS((Path childpath, JoinPath parentpath, CInfo cinfo, int whichchild, int clausetype));
extern Cost xfunc_rank ARGS((LispValue clause));
extern Cost xfunc_expense ARGS((LispValue clause));
extern Cost xfunc_join_expense ARGS((JoinPath path, int whichchild));
extern Cost xfunc_local_expense ARGS((LispValue clause));
extern Cost xfunc_func_expense ARGS((LispValue node, LispValue args));
extern int xfunc_width ARGS((LispValue clause));
extern Count xfunc_card_unreferenced ARGS((LispValue clause, Relid referenced));
extern Count xfunc_card_product ARGS((Relid relids));
extern List xfunc_find_references ARGS((LispValue clause));
extern LispValue xfunc_primary_join ARGS((JoinPath pathnode));
extern Cost xfunc_get_path_cost ARGS((Path pathnode));
extern Cost xfunc_total_path_cost ARGS((JoinPath pathnode));
extern Cost xfunc_expense_per_tuple ARGS((JoinPath joinnode, int whichchild));
extern void xfunc_fixvars ARGS((LispValue clause, Rel rel, int varno));
extern int xfunc_cinfo_compare ARGS((void *arg1, void *arg2));
extern int xfunc_clause_compare ARGS((void *arg1, void *arg2));
extern void xfunc_disjunct_sort ARGS((LispValue clause_list));
extern int xfunc_disjunct_compare ARGS((void *arg1, void *arg2));
extern int xfunc_func_width ARGS((regproc funcid, LispValue args));
extern int xfunc_tuple_width ARGS((Relation rd));
extern int xfunc_num_join_clauses ARGS((JoinPath path));
extern LispValue xfunc_LispRemove ARGS((LispValue foo, List bar));
extern bool xfunc_copyrel ARGS((Rel from, Rel *to));

/* function prototypes from planner/path/predmig.c */
#include "planner/predmig.h"

#endif /* xfuncIncluded */
