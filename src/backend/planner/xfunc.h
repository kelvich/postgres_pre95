/* $Header$ */

#include "nodes/relation.h"

/* command line arg flags */
#define XFUNC_OFF -1
#define XFUNC_NOR 2
#define XFUNC_NOPULL 4

/* constants for local and join predicates */
#define XFUNC_LOCPRD 1
#define XFUNC_JOINPRD 2

extern int XfuncMode;  /* defined in tcop/postgres.c */

/* defaults for function attributes used for expensive function calculations */
#define BYTE_PCT 100
#define PERBYTE_CPU 10
#define PERCALL_CPU 100
#define OUTIN_RATIO 1

/* default width assumed for variable length attributes */
#define VARLEN_DEFAULT 128;

/* function prototypes from planner/path/xfunc.c */
int xfunc_cinfo_compare ARGS((void *arg1 , void *arg2 ));
extern int xfunc_clause_compare ARGS((void *arg1 , void *arg2 ));
Cost xfunc_expense ARGS((LispValue clause ));
Cost xfunc_func_expense ARGS((LispValue node, LispValue args));
Cost xfunc_measure ARGS((LispValue clause));
extern void xfunc_disjunct_sort ARGS((LispValue clause_list ));
int xfunc_disjunct_compare ARGS((void *arg1 , void *arg2 ));
int xfunc_width ARGS((LispValue clause));
extern void xfunc_trypullup ARGS((Rel rel));
int xfunc_shouldpull ARGS((Path childpath, JoinPath parentpath, CInfo *maxclausept, int whichchild));
void xfunc_pullup ARGS((Path childpath, JoinPath parentpath, LispValue clause, int whichrel, int clausetype));
void xfunc_fixvars ARGS((LispValue clause, Path path, int varno));
LispValue xfunc_primary_join ARGS((JoinPath pathnode));
extern Cost xfunc_get_path_cost ARGS((Path pathnode));
bool xfunc_copyrel ARGS((Rel from, Rel *to));
LispValue xfunc_LispRemove ARGS((LispValue foo, List bar));
int xfunc_func_width ARGS((regproc funcid, LispValue args));
int xfunc_tuple_width ARGS((Relation rd));
Cost xfunc_total_path_cost ARGS((Path pathnode));
Cost xfunc_expense_per_tuple ARGS((JoinPath joinnode, int whichrel));
