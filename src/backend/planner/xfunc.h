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

/* function prototypes from planner/path/xfunc.c */
extern int xfunc_cinfo_compare ARGS((void *arg1 , void *arg2 ));
extern int xfunc_clause_compare ARGS((void *arg1 , void *arg2 ));
extern double xfunc_expense ARGS((LispValue clause ));
extern double xfunc_measure ARGS((LispValue clause));
extern void xfunc_disjunct_sort ARGS((LispValue clause_list ));
extern int xfunc_disjunct_compare ARGS((void *arg1 , void *arg2 ));
extern int xfunc_width ARGS((LispValue clause));
extern void xfunc_trypullup ARGS((Rel rel));
extern int xfunc_shouldpull ARGS((Path childpath, JoinPath parentpath, CInfo *maxclausept));
extern void xfunc_pullup ARGS((Path childpath, JoinPath parentpath, LispValue clause, int whichrel, int clausetype));
extern void xfunc_fixvars ARGS((LispValue clause, Path path, int varno));
