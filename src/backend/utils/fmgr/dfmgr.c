/*
 * dfmgr.c --
 *    Dynamic function manager code.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "access/heapam.h"
#include "nodes/pg_lisp.h"
#include "utils/dynamic_loader.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "tmp/c.h"
#include "port-protos.h"     /* system specific function prototypes */

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_proc.h"

RcsId("$Header$");

static DynamicFileList *file_list = NULL;
static DynamicFileList *file_tail = NULL;

static ObjectId procedureId_save = -1;
static int pronargs_save;
static func_ptr user_fn_save = NULL;

#define NOT_EQUAL(A, B) (((A).st_ino != (B).inode) \
                      || ((A).st_dev != (B).device))

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

    if (procedureId == procedureId_save)
    {
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
        return(NULL);
    }

    procedureStruct = (struct proc *) GETSTRUCT(procedureTuple);
    proname = (procedureStruct)->proname;
    pronargs_save = *pronargs = (procedureStruct)->pronargs;

    /*
     * Extract the procedure info from the pg_proc tuple.
     * Since probin is varlena, do a amgetattr() on the procedure
     * tuple.  To do that, we need the rdesc for the procedure
     * relation, so...
     *
     * XXX It may not be wise to turn GC off for this long, but some LISPs 
     *     (e.g., KCL) don't provide a way to protect C-allocated LISP
     *     objects.
     */

    /* open pg_procedure */

    rdesc = amopenr(ProcedureRelationName->data);
    if (!RelationIsValid(rdesc)) {
        elog(WARN, "fmgr: Could not open relation %s",
             ProcedureRelationName->data);
        return(NULL);
    }
    probinattr = amgetattr(procedureTuple, (Buffer) 0,
                   ProcedureBinaryAttributeNumber,
                   &rdesc->rd_att, &isnull);
    if (!PointerIsValid(probinattr) /*|| isnull*/) {
        amclose(rdesc);
        elog(WARN, "fmgr: Could not extract probin for %d from %s",
             procedureId, ProcedureRelationName->data);
        return(NULL);
    }
    probinstring = textout((struct varlena *)probinattr);

    user_fn = handle_load(probinstring, proname);

    procedureId_save = procedureId;
    user_fn_save = user_fn;

    return(user_fn);
}

func_ptr
handle_load(filename, funcname)

char *filename, *funcname;

{
    DynamicFileList     *file_scanner = NULL;
    DynamicFunctionList *func_scanner, *func_tail = NULL;
    func_ptr            retval = NULL;
    char                *load_error;
    char                *start_addr;
    long                size;
    struct stat  stat_buf;

    /*
     * Do this because loading files may screw up the dynamic function
     * manager otherwise.
     *
     */

    procedureId_save = -1;

    /*
     * Scan the list of loaded FILES to see if the function
     * has been loaded.  
     */

    if (filename != NULL)
    {
        for (file_scanner = file_list;
             file_scanner != NULL
             && file_scanner->filename != NULL
             && strcmp(filename, file_scanner->filename) != 0;
             file_scanner = file_scanner->next);

        if (file_scanner == NULL)
        {
            if (stat(filename, &stat_buf) == -1)
            {
                elog(WARN, "stat failed on file %s", filename);
            }

            for (file_scanner = file_list;
                 file_scanner != NULL
                 && (NOT_EQUAL(stat_buf, *file_scanner));
                 file_scanner = file_scanner->next);
            /*
             * Same files - different paths (ie, symlink or link)
             */

            if (file_scanner != NULL) strcpy(file_scanner->filename, filename);

        }
    }
    else
    {
        file_scanner = NULL;
    }

    /*
     * File not loaded yet.
     */

    if (file_scanner == NULL)
    {
        if (file_list == NULL)
        {
            file_list = (DynamicFileList *)
                        malloc(sizeof(DynamicFileList));
            file_scanner = file_list;
        }
        else
        {
            file_tail->next = (DynamicFileList *)
                               malloc(sizeof(DynamicFileList));
            file_scanner = file_tail->next;
        }

        strcpy(file_scanner->filename, filename);
        file_scanner->device = stat_buf.st_dev;
        file_scanner->inode = stat_buf.st_ino;
        file_scanner->next = NULL;

        func_scanner = dynamic_file_load(&load_error, filename,
                       &start_addr, &size);
        if (func_scanner == NULL)
        {
            if (file_scanner == file_list)
            {
                file_list = NULL;
            }
            else
            {
                file_tail->next = NULL;
            }

            free((char *)file_scanner);
            elog(WARN,
                 "Load of file %s failed: %s", filename, load_error);
        }

        file_scanner->func_list = func_scanner;
        file_scanner->address = start_addr;
        file_scanner->size = size;

/*
 * Just load the file - we are done with that so return.
 */
        file_tail = file_scanner;

        if (funcname == NULL) return(NULL);

        for (; func_scanner != NULL
             && strcmp(func_scanner->funcname, funcname);
             func_scanner = func_scanner->next);

        if (func_scanner == NULL)
        {
            elog(WARN, "Cant find function %s in file %s", funcname, filename);
        }
        else
        {
            retval = func_scanner->func;
        }
    }
    else
    {
        for (func_scanner = file_scanner->func_list;
             func_scanner != NULL
             && strcmp(func_scanner->funcname, funcname);
             func_scanner = func_scanner->next)
        {
            if (func_scanner->next == NULL) func_tail = func_scanner;
        }

        if (func_scanner == NULL)
        {
            elog(WARN, "Function %s is not in file %s", funcname, filename);
        }
        else
        {
            retval = func_scanner->func;
        }
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

    if (stat(filename, &stat_buf) == -1)
    {
        elog(WARN, "load of file %s failed", filename);
    }

    if (file_list != NULL && !NOT_EQUAL(stat_buf, *file_list))
    {
        file_scanner = file_list;
        file_list = file_list->next;
        zero_loaded_file(file_scanner);
        free((char *)file_scanner);
    }
    else if (file_list != NULL)
    {
        file_scanner = file_list;
        while (!done)
        {
            if (file_scanner->next == NULL)
            {
                done = 1;
            }
            else if (!NOT_EQUAL(stat_buf, *(file_scanner->next)))
            {
                done = 1;
            }
            else
            {
                file_scanner = file_scanner->next;
            }
        }

        if (file_scanner->next != NULL)
        {
            p = file_scanner->next;
            file_scanner->next = file_scanner->next->next;
            zero_loaded_file(p);
            free((char *)p);
        }
    }
    handle_load(filename, NULL);
}

zero_loaded_file(file_data)

DynamicFileList *file_data;

{
    DynamicFunctionList *func_scanner, *throw_away;

    bzero(file_data->address, file_data->size);
/*
 * Old DEC dynamic loader uses brk/sbrk - DON'T TRY TO FREE THIS SPACE.
 */

#ifndef OLD_DEC
    free((char *)file_data->address);
#endif
    func_scanner = file_data->func_list;
    for (throw_away = func_scanner->next; throw_away != NULL;)
    {
        throw_away = func_scanner->next;
        free((char *)func_scanner);
        func_scanner = throw_away;
    }
    free((char *)file_data->func_list);
}
