
/*
 *  $Header$
 */

/*
 * New dynamic loader.
 *
 * This dynamic loader uses Andrew Yu's libdl-1.0 package for Ultrix 4.x.
 * (Note that pg_dlsym and pg_dlclose are actually macros defined in
 * "port-protos.h".)
 */ 

#include <stdio.h>
#include "dl.h"
#include "tmp/c.h"
#include "fmgr.h"
#include "port-protos.h"
#include "utils/log.h"

extern char pg_pathname[];

void *pg_dlopen( filename, err )
     char *filename; char **err;
{
    static int dl_initialized= 0;
    void *handle;

    /*
     * initializes the dynamic loader with the executable's pathname.
     * (only needs to do this the first time pg_dlopen is called.)
     */
    if (!dl_initialized) {
        if (!dl_init(pg_pathname)) {
	    *err = dl_error();
	    return NULL;
	}
	dl_initialized= 1;
    }

    /*
     * open the file. We do the symbol resolution right away so that we
     * will know if there are undefined symbols. (This is in fact the
     * same semantics as "ld -A". ie. you cannot have undefined symbols.
     */
    if ((handle=dl_open(filename, DL_NOW))==NULL) {
	int count;
	char **list= dl_undefinedSymbols(&count);

	/* list the undefined symbols, if any */
	if(count) {
	    elog(NOTICE, "dl: Undefined:");
	    while(*list) {
		elog(NOTICE, "  %s", *list);
		list++;
	    }
	}
	*err= dl_error();
    }

    return (void *)handle;
}

