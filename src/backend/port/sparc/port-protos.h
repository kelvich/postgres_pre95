RcsId("$Header$");

/* dynamic_syms.c */

/* dynloader.c */
DynamicFunctionList *dynamic_file_load ARGS((char **err , char *filename , char **address , long *size ));
DynamicFunctionList *load_symbols ARGS((char *filename , struct exec *hdr , int entry_addr ));
func_ptr dynamic_load ARGS((char **err ));
int execld ARGS((char *address , char *tmp_file , char *filename ));

/* port.c */
