/*
 *  FILE
 *	dynloader.c
 *
 *  DESCRIPTION
 *	dynamic loader for HP-UX using the shared library mechanism
 *
 *  INTERFACE ROUTINES
 *	pg_dlopen
 *	pg_dlsym
 *	pg_dlclose
 *
 *  NOTES
 *	all functions are defined here -- it's impossible to trace the 
 *	shl_* routines from the bundled HP-UX debugger.
 *
 *  IDENTIFICATION
 *	$Header$
 */

/* System includes */
#include <stdio.h>
#include <a.out.h>
#include <dl.h>

#include "tmp/c.h"
#include "fmgr.h"
#include "utils/dynamic_loader.h"
#include "port-protos.h"

RcsId("$Header$");

void *
pg_dlopen(filename, err)
     char *filename;
     char **err;
{
    shl_t handle = shl_load(filename, BIND_DEFERRED, NULL);

    if (!handle) {
	*err = "shl_load failed";
    }
    return((void *) handle);
}

func_ptr
pg_dlsym(handle, funcname)
     void *handle;
     char *funcname;
{
    func_ptr f;

    if (shl_findsym((shl_t *) &handle, funcname, TYPE_PROCEDURE, &f) == -1) {
	f = (func_ptr) NULL;
    }
    return(f);
}

void
pg_dlclose(handle)
     void *handle;
{
     shl_unload((shl_t) handle);
}
