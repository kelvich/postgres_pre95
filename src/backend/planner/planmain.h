/* $Header$ */

extern Plan query_planner ARGS((int command_type, List tlist, List qual, int currentlevel, int maxlevel));
extern Plan subplanner ARGS((LispValue flat_tlist, LispValue original_tlist, LispValue qual, int level, LispValue sortkeys));
extern Result make_result ARGS(( List tlist, List resrellevelqual, List resconstantqual, Plan left, Plan right));
extern Plan make_aggplan ARGS((Plan subplan, List agg_tlist, int aggidnum));
