
/*
 *  $Header$
 */

#include <stdio.h>
#include <dlfcn.h>

#include "port-protos.h"

/*
 * Dynamic Loader on Alpha OSF/1.x
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
