/*
 *	DYNAMIC_LOADER.H
 *
 *	$Header$
 */

#ifndef Dynamic_loaderHIncluded
#define Dynamic_loaderHIncluded 1 /* include once only */

#ifdef MIN
#undef MIN
#undef MAX
#endif /* MIN */

#include <sys/param.h>			/* for MAXPATHLEN */
#include <sys/types.h>			/* for dev_t, ino_t, etc. */

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

typedef struct df_list {
    char *funcname;			/* Name of function */
    func_ptr func;			/* Function address */
    struct df_list *next;
} DynamicFunctionList;


/*
 * List of dynamically loaded files.
 */

typedef struct df_files {
    char filename[MAXPATHLEN];		/* Full pathname of file */
    dev_t device;			/* Device file is on */
    ino_t inode;			/* Inode number of file */
    void *handle;			/* a handle for pg_dl* functions */
    struct df_files *next;
} DynamicFileList;

void *pg_dlopen ARGS((char *filename, char **errmsg));
func_ptr pg_dlsym  ARGS((void *handle, char *funcname));
void pg_dlclose ARGS((void *handle));

#endif Dynamic_loaderHIncluded
