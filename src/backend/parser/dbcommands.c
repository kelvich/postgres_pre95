#include <stdio.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "tmp/daemon.h"


extern int NStriping;
extern char *PostgresHomes[];
extern char *GetDataHome();
extern char *PG_usename;

int user_sysid;

createdb(dbname)

char *dbname;

{
    char *path;
    char buf[512];
    int i;

    if (check_security("createdb", dbname))
    {
        if (NStriping == 0)
        {
            path = GetDataHome();
            sprintf(buf, "mkdir %s/data/base/%s", path, dbname);
            system(buf);
            sprintf(buf, "cp %s/data/base/template1/* %s/data/base/%s",
                    path, path, dbname);
            system(buf);
        }
        else
        {
            for (i = 0; i < NStriping; i++)
            {
                path = PostgresHomes[i];
                sprintf(buf, "mkdir %s/data/base/%s", path, dbname);
                system(buf);
                sprintf(buf, "cp %s/data/base/template1/* %s/data/base/%s",
                        path, path, dbname);
                system(buf);
            }
        }
    }
    sprintf(buf,
            "append pg_database (datname = \"%s\"::char16, \
            datdba = \"%d\"::oid, \
             datpath = \"%s\"::text)", dbname, user_sysid, dbname);
    pg_eval(buf);
}

destroydb(dbname)

char *dbname;

{
    char buf[512];
    char filename[256];
    char *path;
    int i;
    FILE *file, *fopen();

    if (check_security("destroydb", dbname) != 0)
    {
        if (NStriping == 0)
        {
            path = GetDataHome();
            sprintf(filename, "%s/data/base/%s/%s.vacuum",
                    path, dbname, dbname);
            /* stop the vacuum daemon, if one is running */

            if ((file = fopen(filename, "r")) != (FILE *) NULL) {
                int pid;
                fscanf(file, "%d", &pid);
                fclose(file);
                if (kill(pid, SIGKILLDAEMON1) < 0) {
                    elog(WARN, "can\'t kill vacuum daemon on database %s",
                         dbname);
                }
            }
            sprintf(buf, "rm -r %s/data/base/%s", path, dbname);
            system(buf);
        }
        else
        {
            for (i = 0; i < NStriping; i++)
            {
                path = PostgresHomes[i];
                sprintf(filename, "%s/data/base/%s/%s.vacuum",
                        path, dbname, dbname);
                /* stop the vacuum daemon, if one is running */

                if ((file = fopen(filename, "r")) != (FILE *) NULL) {
                    int pid;
                    fscanf(file, "%d", &pid);
                    fclose(file);
                    if (kill(pid, SIGKILLDAEMON1) < 0) {
                        elog(WARN, "can\'t kill vacuum daemon on database %s",
                             dbname);
                    }
                }
                sprintf(buf, "rm -r %s/data/base/%s", path, dbname);
                system(buf);
            }
        }
    }

    sprintf(buf, "delete pg_database where \
            pg_database.datname = \"%s\"::char16", dbname);
    pg_eval(buf);

}

/*
 * Returns true if user is OK to use the database - does an elog(warn)
 * otherwise.
 *
 * Check_security also sets the user_sysid variable as a side-effect.
 */

int
check_security(command, dbname)

char *command, *dbname;

{
    extern char *DBName;
    extern char *PG_username;
    Relation r, amopenr();
    HeapScanDesc s, ambeginscan();
    char dummy, use_createdb;
    int n, done = 0;
    int dbfound = 0;

    HeapTuple u, amgetnext();
    int value;
    char *value2;

    /*
     * We don't have a cache for pg_user or pg_database, so we have to open
     * them "by hand" and scan them using direct access method calls.
     *
     * This code "knows" the layout of pg_user: at least as much as 
     * the following: username:char16, sysid:int2, usecreatedb:bool
     */

    r = amopenr("pg_user");
    if (r == NULL) elog(FATAL, "%s: can\'t open pg_user!", command);
    s = ambeginscan(r, 0, NULL, NULL, NULL);
    n = strlen(PG_username);
    while (!done)
    {
        u = amgetnext(s, NULL, NULL);
        if (u == NULL) elog(FATAL, "User %s is not in pg_user!!", PG_username);
        value2 = (char *) HeapTupleGetAttributeValue(u, InvalidBuffer, 1,
                                                     &(r->rd_att), &dummy);
        if (!strncmp(PG_username, value2, n))
        {
            done = true;
        }
    }

    user_sysid = (int2) HeapTupleGetAttributeValue(u, InvalidBuffer, 2,
                                                   &(r->rd_att), &dummy);
    use_createdb = (char) HeapTupleGetAttributeValue(u, InvalidBuffer, 3,
                                                     &(r->rd_att), &dummy);
    amclose(r);

    /* Check to make sure user has permission to use createdb */

    if (!use_createdb)
    {
        elog(WARN, "User %s is not allowed to use the createdb query",
             PG_username);
    }

    /* Check to make sure database is not the currently open database */

    if (!strcmp(dbname, DBName))
    {
        elog(WARN, "%s cannot be executed on an open database", command);
    }

    /* Make sure we are not mucking with the template database */

    if (!strcmp(dbname, "template1"))
    {
        elog(WARN, "%s cannot be executed on the template database.", command);
    }
    /* Check to make sure database is owned by this user */

    r = amopenr("pg_database");
    if (r == NULL) elog(FATAL, "%s: can\'t open pg_database!", command);
    s = ambeginscan(r, 0, NULL, NULL, NULL);
    done = 0;
    n = strlen(dbname);
    while (!done)
    {

        u = amgetnext(s, NULL, NULL);
        if (u == NULL) 
        {
            done = 1;
            dbfound = 0;
        }

        if (!done) 
        {
            value2 = (char *) HeapTupleGetAttributeValue(u, InvalidBuffer,
                                                         1, &(r->rd_att),
                                                         &dummy);
            
            if (!strncmp(value2, dbname, n))
            {
                done = 1;
                dbfound = 1;
                value = (int) HeapTupleGetAttributeValue(u, InvalidBuffer, 2,
                                                         &(r->rd_att), &dummy);
            }
        }
    }

    amclose(r);

    if (dbfound && !strcmp(command, "createdb"))
    {
        elog(WARN, "createdb: database %s already exists.", dbname);
    }
    else if (!dbfound && !strcmp(command, "destroydb"))
    {
        elog(WARN, "destroydb: database %s does not exist.", dbname);
    }
    else if (dbfound && !strcmp(command, "destroydb") && value != user_sysid)
    {
        elog(WARN, "database %s is not owned by you.", dbname);
    }

    return(1);
}
