/* $Header$ */

extern Plan planner ARGS((LispValue parse));
extern Plan make_sortplan ARGS((List tlist, List sortkeys, List sortops, List plannode));
extern Plan make_aggplan ARGS((List agg_tlist, List tlist, List qual, List currentlevel, List sortkeys));
extern Plan init_query_planner ARGS((LispValue root, LispValue tlist, LispValue qual));
extern Existential make_existential ARGS((Plan left, Plan right));
