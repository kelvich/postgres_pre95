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
#ifdef PORTNAME_hpux
#include <dl.h>				/* for shl_t */
#endif /* PORTNAME_hpux */

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
    DynamicFunctionList *func_list;	/* List of functions */
    char *address;			/* Memory allocated for file */
    long size;				/* Size of memory allocated for file */
    struct df_files *next;
#ifdef PORTNAME_hpux
    shl_t handle;			/* Handle for shared library */
    char shlib[MAXPATHLEN];		/* File of dynamic loader */
#endif /* PORTNAME_hpux */
} DynamicFileList;

#endif Dynamic_loaderHIncluded
