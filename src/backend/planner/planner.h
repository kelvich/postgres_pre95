/* $Header$ */

extern Plan planner ARGS((LispValue parse));
extern Plan make_sortplan ARGS((List tlist, List sortkeys, List sortops, Plan plannode));
extern Plan init_query_planner ARGS((LispValue root, LispValue tlist, LispValue qual));
extern Existential make_existential ARGS((Plan left, Plan right));
extern void init_planner ARGS(());
