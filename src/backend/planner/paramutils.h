/*$Header$ */
extern LispValue find_parameters ARGS((LispValue plan));
extern LispValue find_all_parameters ARGS((LispValue tree));
extern LispValue substitute_parameters ARGS((LispValue plan, int params));
extern LispValue assoc_params ARGS((LispValue plan, LispValue param_alist));
