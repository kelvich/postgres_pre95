/*
 * Dynamic Loader for Postgres for Linux, generated from those for
 * Ultrix.
 * You need to install the dld library on your Linux system!
 */

#include <stdio.h>
#include <dld.h>
#include "tmp/c.h"
#include "tmp/postgres.h"
#include "port-protos.h"
#include "utils/log.h"
#include "fmgr.h"

extern char pg_pathname[];

void *pg_dlopen( filename, err )
     char *filename; char **err;
{
    static int dl_initialized= 0;

    /*
     * initializes the dynamic loader with the executable's pathname.
     * (only needs to do this the first time pg_dlopen is called.)
     */
    if (!dl_initialized) {
        if (dld_init (dld_find_executable (pg_pathname))) {
	    *err = dld_strerror(dld_errno);
	    return NULL;
	}
	/*
	 * if there are undefined symbols, we want dl to search from the
	 * following libraries also.
	 */
#if 0
	dl_setLibraries("/usr/lib/libm_G0.a:/usr/lib/libc_G0.a");
#endif
	dl_initialized= 1;
    }

    /*
     * link the file, then check for undefined symbols!
     */
    if (dld_link(filename)) {
	*err = dld_strerror(dld_errno);
	return NULL;
    }

    /*
     * If undefined symbols: try to link with the C and math libraries!
     * This could be smarter, if the dynamic linker was able to handle
     * shared libs!
     */
    if(dld_undefined_sym_count > 0) {
	if (dld_link("/usr/lib/libc.a")) {
	    elog(NOTICE, "dld: Cannot link C library!");
	    *err = dld_strerror(dld_errno);
	    return NULL;
	}
	if(dld_undefined_sym_count > 0) {
	    if (dld_link("/usr/lib/libm.a")) {
		elog(NOTICE, "dld: Cannot link math library!");
		*err = dld_strerror(dld_errno);
		return NULL;
	    }
	    if(dld_undefined_sym_count > 0) {
		int count = dld_undefined_sym_count;
		char **list= dld_list_undefined_sym();

		/* list the undefined symbols, if any */
		elog(NOTICE, "dld: Undefined:");
		do {
		    elog(NOTICE, "  %s", *list);
		    list++;
		    count--;
		} while(count > 0);

		dld_unlink_by_file(filename, 1);
		*err = "dynamic linker: undefined symbols!";
		return NULL;
	    }
	}
    }

    return (void *) strdup(filename);
}
