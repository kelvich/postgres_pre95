extern Plan planner ARGS((LispValue parse));
extern Plan init_query_planner ARGS((LispValue root, LispValue tlist, LispValue qual));
extern Plan make_existential ARGS((Plan left, Plan right));
