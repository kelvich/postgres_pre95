/*
 *  FILE
 *	dynloader.c
 *
 *  DESCRIPTION
 *	dynamic loader for AIX using the shared library mechanism
 *
 *  INTERFACE ROUTINES
 *	pg_dlopen
 *	pg_dlsym
 *	pg_dlclose
 *
 *  NOTES
 *
 *  IDENTIFICATION
 *	$Header$
 */

#include <stdio.h>
#include "dlfcn.h"			/* this is from jum's libdl package */

#include "fmgr.h"			/* for func_ptr */
#include "utils/dynamic_loader.h"	/* for protos */
#include "port-protos.h"		/* for #define'd pg_dl* */

/*
 * Dynamic Loader on AIX.
 *
 * this dynamic loader uses the system dynamic loading interface for shared 
 * libraries (ie. dlopen/dlsym/dlclose). The user must specify a shared
 * library as the file to be dynamically loaded.
 *
 * Note that only pg_dlopen is defined here. pg_dlsym and pg_dlclose
 * are actually macros.
 */

/*
 * pg_dlopen--
 *    attempts to dynamically loaded in filename and returns error in
 *    err, if any. 
 */
void *pg_dlopen( filename, err )
     char *filename; char **err;
{
    void *handle= NULL;

    if ((handle=dlopen(filename, RTLD_LAZY))==NULL) {
	*err = dlerror();
    }
    return (void *)handle;
}

func_ptr
pg_dlsym(handle, funcname)
    void *handle;
    char *funcname;
{
    return(dlsym(handle, funcname));
}

void
pg_dlclose(handle)
    void *handle;
{
    dlclose(handle);
}
