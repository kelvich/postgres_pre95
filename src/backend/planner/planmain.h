extern Plan query_planner ARGS((LispValue command_type, LispValue tlist, LispValue qual, LispValue currentlevel, LispValue maxlevel));
extern Plan subplanner ARGS((LispValue flat_tlist, LispValue original_tlist, LispValue qual, LispValue level, LispValue sortkeys));
