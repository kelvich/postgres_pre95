
/*
 *	DYNAMIC_LOADER.H
 *
 *	$Header$
 */

#ifndef Dynamic_loaderHIncluded
#define Dynamic_loaderHIncluded 1 /* include once only */

#include "tmp/c.h"

typedef char *	((*func_ptr)());

func_ptr	dynamic_load();

typedef struct {
    func_ptr    func;
    char        *name;
} FList;

extern FList ExtSyms[];

/*
 * The new dynamic loader scheme loads each file one at a time.  Therefore,
 * we must note each function in the file at load time.
 *
 */

/*
 * Dynamically loaded function list.
 */

typedef struct df_list
{
    char funcname[16];				/* Name of function */
    func_ptr func;					/* Function address */
    struct df_list *next;
}
DynamicFunctionList;

/*
 * List of dynamically loaded files.
 */

typedef struct df_files
{
    char filename[256];				/* Full pathname of file */
    DynamicFunctionList *func_list;	/* List of functions */
    struct df_files *next;
}
DynamicFileList;

#endif Dynamic_loaderHIncluded
