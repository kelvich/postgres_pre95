/* command line arg flags */
#define XFUNC_OFF -1
#define XFUNC_NOR 2
#define XFUNC_NOPULL 4

extern int XfuncMode;  /* defined in tcop/postgres.c */

/* defaults for function attributes used for expensive function calculations */
#define BYTE_PCT 100
#define PERBYTE_CPU 0
#define PERCALL_CPU 0
#define OUTIN_RATIO 1
#define DEFAULT_WIDTH 1

/* function prototypes from planner/path/xfunc.c */
extern void xfunc_rellist_sortprds ARGS((LispValue rels ));
extern int xfunc_cinfo_compare ARGS((void *arg1 , void *arg2 ));
extern int xfunc_clause_compare ARGS((void *arg1 , void *arg2 ));
extern double xfunc_expense ARGS((LispValue clause ));
extern void xfunc_disjunct_sort ARGS((LispValue clause_list ));
extern int xfunc_disjunct_compare ARGS((void *arg1 , void *arg2 ));
extern int xfunc_width ARGS((LispValue clause));
