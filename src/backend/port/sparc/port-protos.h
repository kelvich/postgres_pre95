/*
 * $Header$
 */

#include "utils/dynamic_loader.h"

/* dynloader.c */
DynamicFunctionList *load_symbols ARGS((char *filename , struct exec *hdr , int entry_addr ));
func_ptr dynamic_load ARGS((char **err ));
int execld ARGS((char *address , char *tmp_file , char *filename ));

typedef struct dfHandle {
    DynamicFunctionList *func_list;
    char		*load_address;
} dfHandle;

/* port.c */

void sparc_bug_set_outerjoincost ARGS((char *p, long val));
