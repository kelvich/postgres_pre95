extern LispValue relation_sortkeys ARGS((LispValue tlist));
extern LispValue sort_list_carARGS((LispValue list));
extern void *sort_relation_paths ARGS((LispValue pathlist, LispValue sortkeys, LispValue rel, LispValue width));
extern LispValue sort_level_result ARGS((LispValue plan, LispValue numkeys));
