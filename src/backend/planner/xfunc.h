/* $Header$ */

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
int xfunc_cinfo_compare ARGS((void *arg1 , void *arg2 ));
extern int xfunc_clause_compare ARGS((void *arg1 , void *arg2 ));
Cost xfunc_expense ARGS((LispValue clause ));
Cost xfunc_join_expense ARGS((JoinPath path, int whichchild));
Cost xfunc_local_expense ARGS((LispValue clause ));
Cost xfunc_func_expense ARGS((LispValue node, LispValue args));
List xfunc_find_references ARGS((LispValue clause));
Cost xfunc_rank ARGS((LispValue clause));
extern void xfunc_disjunct_sort ARGS((LispValue clause_list ));
int xfunc_disjunct_compare ARGS((void *arg1 , void *arg2 ));
int xfunc_width ARGS((LispValue clause));
extern void xfunc_trypullup ARGS((Rel rel));
int xfunc_shouldpull ARGS((Path childpath, JoinPath parentpath, int whichchild, CInfo *maxclausept));
CInfo xfunc_pullup ARGS((Path childpath, JoinPath parentpath, LispValue clause, int whichrel, int clausetype));
void xfunc_fixvars ARGS((LispValue clause, Path path, int varno));
LispValue xfunc_primary_join ARGS((JoinPath pathnode));
extern Cost xfunc_get_path_cost ARGS((Path pathnode));
bool xfunc_copyrel ARGS((Rel from, Rel *to));
LispValue xfunc_LispRemove ARGS((LispValue foo, List bar));
int xfunc_func_width ARGS((regproc funcid, LispValue args));
int xfunc_tuple_width ARGS((Relation rd));
Cost xfunc_total_path_cost ARGS((Path pathnode));
Cost xfunc_expense_per_tuple ARGS((JoinPath joinnode, int whichchild));
Count xfunc_card_unreferenced ARGS((LispValue clause));
Count xfunc_card_product ARGS((Relid relids));


/* protos from planner/path/predmig.c */
extern bool xfunc_do_predmig ARGS((Path root));
void xfunc_predmig ARGS((JoinPath pathnode, Stream streamroot, Stream laststream, bool *progressp));
bool xfunc_series_llel ARGS((Stream stream));
bool xfunc_llel_chains ARGS((Stream root, Stream bottom));
Stream xfunc_complete_stream ARGS((Stream stream));
bool xfunc_prdmig_pullup ARGS((Stream origstream, Stream pullme, JoinPath joinpath));
void xfunc_form_groups ARGS((Stream root, Stream bottom));
void xfunc_free_stream ARGS((Stream root));
Stream xfunc_add_clauses ARGS((Stream current));
void xfunc_setup_group ARGS((Stream node, Stream bottom));
Stream xfunc_streaminsert ARGS((CInfo clauseinfo, Stream current, int clausetype));
int xfunc_num_relids ARGS((Stream node));
StreamPtr xfunc_get_downjoin ARGS((Stream node));
StreamPtr xfunc_get_upjoin ARGS((Stream node));
Stream xfunc_stream_qsort ARGS((Stream root, Stream bottom));
int xfunc_stream_compare ARGS((void *arg1, void *arg2));
bool xfunc_check_stream ARGS((Stream node));
bool xfunc_in_stream ARGS((Stream node,stream));
