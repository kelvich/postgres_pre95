/*
 * dfmgr.c --
 *    Dynamic function manager code.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "tmp/c.h"

#include "fmgr.h"
#include "utils/dynamic_loader.h"
#include "utils/log.h"
#ifndef UFP
#include "access/heapam.h"
#include "nodes/pg_lisp.h"
#endif /* !UFP */

#include "port-protos.h"     /* system specific function prototypes */

#ifndef UFP
#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#endif /* !UFP */

RcsId("$Header$");

static DynamicFileList *file_list = (DynamicFileList *) NULL;
static DynamicFileList *file_tail = (DynamicFileList *) NULL;

#define NOT_EQUAL(A, B) (((A).st_ino != (B).inode) \
                      || ((A).st_dev != (B).device))

#ifndef UFP
static ObjectId procedureId_save = -1;
static int pronargs_save;
static func_ptr user_fn_save = (func_ptr) NULL;

/* XXX can't include builtins.h on hpux - hibyte macro conflict */
extern char *textout ARGS((struct varlena *t));

func_ptr
fmgr_dynamic(procedureId, pronargs)
    ObjectId    procedureId;
    int         *pronargs;
{
    register    i;
    HeapTuple   procedureTuple;
    struct proc *procedureStruct;
    char        *proname;
    OID         proid, prolang;
    Boolean     proisinh,proistrusted,proiscachable;
    OID         prorettype;
    char        *probinattr, *probinstring;
    func_ptr    user_fn, handle_load();
    char        *uarg[7];
    Relation    rdesc;
    Boolean     isnull;

    if (procedureId == procedureId_save) {
        *pronargs = pronargs_save;
        return(user_fn_save);
    }

    /*
     * The procedure isn't a builtin, so we'll have to do a catalog
     * lookup to find its pg_proc entry.
     */
    procedureTuple = SearchSysCacheTuple(PROOID, (char *) procedureId,
                     NULL, NULL, NULL);
    if (!HeapTupleIsValid(procedureTuple)) {
        elog(WARN, "fmgr: Cache lookup failed for procedure %d\n",
             procedureId);
        return((func_ptr) NULL);
    }

    procedureStruct = (struct proc *) GETSTRUCT(procedureTuple);
    proname = procedureStruct->proname;
    pronargs_save = *pronargs = procedureStruct->pronargs;

    /*
     * Extract the procedure info from the pg_proc tuple.
     * Since probin is varlena, do a amgetattr() on the procedure
     * tuple.  To do that, we need the rdesc for the procedure
     * relation, so...
     */

    /* open pg_procedure */

    rdesc = amopenr(ProcedureRelationName->data);
    if (!RelationIsValid(rdesc)) {
        elog(WARN, "fmgr: Could not open relation %s",
             ProcedureRelationName->data);
        return((func_ptr) NULL);
    }
    probinattr = amgetattr(procedureTuple, (Buffer) 0,
                   ProcedureBinaryAttributeNumber,
                   &rdesc->rd_att, &isnull);
    if (!PointerIsValid(probinattr) /*|| isnull*/) {
        amclose(rdesc);
        elog(WARN, "fmgr: Could not extract probin for %d from %s",
             procedureId, ProcedureRelationName->data);
        return((func_ptr) NULL);
    }
    probinstring = textout((struct varlena *) probinattr);

    user_fn = handle_load(probinstring, proname);

    procedureId_save = procedureId;
    user_fn_save = user_fn;

    return(user_fn);
}
#endif /* !UFP */

func_ptr
handle_load(filename, funcname)
    char *filename, *funcname;
{
    DynamicFileList     *file_scanner = (DynamicFileList *) NULL;
    func_ptr            retval = (func_ptr) NULL;
    char                *load_error;
    char                *start_addr;
    long                size;
    struct stat		stat_buf;

    /*
     * Do this because loading files may screw up the dynamic function
     * manager otherwise.
     */

#ifndef UFP
    procedureId_save = -1;
#endif /* !UFP */

    /*
     * Scan the list of loaded FILES to see if the function
     * has been loaded.  
     */

    if (filename != (char *) NULL) {
        for (file_scanner = file_list;
             file_scanner != (DynamicFileList *) NULL
             && file_scanner->filename != (char *) NULL
             && strcmp(filename, file_scanner->filename) != 0;
             file_scanner = file_scanner->next)
	    ;
        if (file_scanner == (DynamicFileList *) NULL) {
            if (stat(filename, &stat_buf) == -1) {
                elog(WARN, "stat failed on file %s", filename);
            }

            for (file_scanner = file_list;
                 file_scanner != (DynamicFileList *) NULL
                 && (NOT_EQUAL(stat_buf, *file_scanner));
                 file_scanner = file_scanner->next)
		;
            /*
             * Same files - different paths (ie, symlink or link)
             */
            if (file_scanner != (DynamicFileList *) NULL)
		(void) strcpy(file_scanner->filename, filename);

        }
    } else {
        file_scanner = (DynamicFileList *) NULL;
    }

    /*
     * File not loaded yet.
     */

    if (file_scanner == (DynamicFileList *) NULL) {
        if (file_list == (DynamicFileList *) NULL) {
            file_list = (DynamicFileList *)
		malloc(sizeof(DynamicFileList));
            file_scanner = file_list;
        } else {
            file_tail->next = (DynamicFileList *)
		malloc(sizeof(DynamicFileList));
            file_scanner = file_tail->next;
        }
	bzero((char *) file_scanner, sizeof(DynamicFileList));

        (void) strcpy(file_scanner->filename, filename);
        file_scanner->device = stat_buf.st_dev;
        file_scanner->inode = stat_buf.st_ino;
        file_scanner->next = (DynamicFileList *) NULL;

	file_scanner->handle = pg_dlopen(filename, &load_error);
        if (file_scanner->handle == (void *)NULL) {
            if (file_scanner == file_list) {
                file_list = (DynamicFileList *) NULL;
            } else {
                file_tail->next = (DynamicFileList *) NULL;
            }

            free((char *) file_scanner);
            elog(WARN, "Load of file %s failed: %s", filename, load_error);
        }

/*
 * Just load the file - we are done with that so return.
 */
        file_tail = file_scanner;

        if (funcname == (char *) NULL)
	    return((func_ptr) NULL);
    }

    retval = pg_dlsym(file_scanner->handle, funcname);
	
    if (retval == (func_ptr) NULL) {
	elog(WARN, "Can't find function %s in file %s", funcname, filename);
    }

    return(retval);
}

/*
 * This function loads files by the following:
 *
 * If the file is already loaded:
 * o  Zero out that file's loaded space (so it doesn't screw up linking)
 * o  Free all space associated with that file 
 * o  Free that file's descriptor.
 *
 * Now load the file by calling handle_load with a NULL argument as the
 * function.
 */

load_file(filename)
    char *filename;
{
    DynamicFileList *file_scanner, *p;
    struct stat stat_buf;
    int done = 0;

    if (stat(filename, &stat_buf) == -1) {
        elog(WARN, "stat failed on file %s", filename);
    }

    if (file_list != (DynamicFileList *) NULL
	&& !NOT_EQUAL(stat_buf, *file_list)) {
        file_scanner = file_list;
        file_list = file_list->next;
	pg_dlclose(file_scanner->handle);
        free((char *) file_scanner);
    } else if (file_list != (DynamicFileList *) NULL) {
        file_scanner = file_list;
        while (!done) {
            if (file_scanner->next == (DynamicFileList *) NULL) {
                done = 1;
            } else if (!NOT_EQUAL(stat_buf, *(file_scanner->next))) {
                done = 1;
            } else {
                file_scanner = file_scanner->next;
            }
        }

        if (file_scanner->next != (DynamicFileList *) NULL) {
            p = file_scanner->next;
            file_scanner->next = file_scanner->next->next;
	    pg_dlclose(file_scanner->handle);
            free((char *)p);
        }
    }
    handle_load(filename, (char *) NULL);
}
