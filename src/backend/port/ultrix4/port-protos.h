/*
 * $Header$
 */

/* dynamic_syms.c */

/* dynloader.c */
DynamicFunctionList *dynamic_file_load ARGS((char **err , char *filename , char **start_addr , long *size ));
DynamicFunctionList *load_symbols ARGS((char *filename , int entry_addr ));

/* on_exit.c */
int on_exit ARGS((void (*)(), caddr_t arg ));

/* port.c */

/* sendannounce.c */
int sendannounce ARGS((char *line ));
