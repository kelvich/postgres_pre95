/*
 *   FILE
 *	dynloader.c
 *
 *   DESCRIPTION
 *	dynamic loader for SunOS 4 using the shared library mechanism
 *
 *   INTERFACE ROUTINES
 *	pg_dlopen
 *	pg_dlsym
 *	pg_dlclose
 *
 *   NOTES
 *	pg_dlsym and pg_dlclose are actually macros, defined in 
 *	port-protos.h.
 *
 *   IDENTIFICATION
 *	$Header$
 */

#include <stdio.h>
#include <dlfcn.h>

#include "port-protos.h"

RcsId("$Header$");

/*
 * Dynamic Loader on SunOS 4.
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
 *	attempts to dynamically loaded in filename and returns error in
 *	err, if any. 
 */
void *
pg_dlopen(filename, err)
    char *filename; char **err;
{
    void *handle;

    if ((handle = dlopen(filename, 1)) == (void *) NULL) {
	*err = dlerror();
    }
    return((void *) handle);
}
