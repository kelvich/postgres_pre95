/*
 * $Header$
 */

#include "utils/dynamic_loader.h"

/* dynloader.c */
DynamicFunctionList *load_symbols ARGS((char *filename , int entry_addr ));

typedef struct dfHandle {
    DynamicFunctionList *func_list;
    char		*load_address;
    long		size;
} dfHandle;

/* on_exit.c */
int on_exit ARGS((void (*)(), caddr_t arg ));

/* port.c */

/* sendannounce.c */
int sendannounce ARGS((char *line ));
