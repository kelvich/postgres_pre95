#include <stdio.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "tmp/daemon.h"
#include "catalog/catname.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_user.h"
#include "catalog/pg_database.h"
#include "catalog/syscache.h"

extern char *DataDir;
extern char *DBName;

createdb(dbname)
    char *dbname;
{
    ObjectId db_id, user_id;
    char *path;
    char buf[512];
    int i;

    /*
     *  If this call returns, the database does not exist and we're allowed
     *  to create databases.
     */
    check_permissions("createdb", dbname, &db_id, &user_id);

    /* close virtual file descriptors so we can do system() calls */
    closeAllVfds();

    sprintf(buf, "mkdir %s/base/%s", DataDir, dbname);
    system(buf);
    sprintf(buf, "cp %s/base/template1/* %s/base/%s",
		DataDir, DataDir, dbname);
    system(buf);

    sprintf(buf, "append pg_database (datname = \"%s\"::char16, \
                  datdba = \"%d\"::oid, datpath = \"%s\"::text)",
		  dbname, user_id, dbname);
    pg_eval(buf, (char *) NULL, (ObjectId *) NULL, 0);
}

destroydb(dbname)
    char *dbname;
{
    ObjectId user_id, db_id;
    char *path;
    int i;
    char buf[512];

    /*
     *  If this call returns, the database exists and we're allowed to
     *  remove it.
     */
    check_permissions("destroydb", dbname, &db_id, &user_id);

    if (!ObjectIdIsValid(db_id)) {
	elog(FATAL, "impossible: pg_database instance with invalid OID.");
    }

    /* stop the vacuum daemon */
    stop_vacuum(dbname);

    /* remove the data directory */
    sprintf(buf, "rm -r %s/base/%s", DataDir, dbname);
    system(buf);

    /* remove the pg_database tuple */
    sprintf(buf, "delete pg_database where pg_database.oid = \"%d\"::oid",
		  db_id);
    pg_eval(buf, (char *) NULL, (ObjectId *) NULL, 0);

    /* drop pages for this database that are in the shared buffer cache */
    DropBuffers(db_id);
}

HeapTuple
get_pg_dbtup(command, dbname, dbrel)
    char *command;
    char *dbname;
    Relation dbrel;
{
    HeapTuple dbtup;
    HeapTuple tup;
    Buffer buf;
    HeapScanDesc scan;
    ScanKeyData scanKey;

    ScanKeyEntryInitialize(&scanKey.data[0], 0, Anum_pg_database_datname,
				NameEqualRegProcedure, NameGetDatum(dbname));

    scan = heap_beginscan(dbrel, 0, NowTimeQual, 1, &scanKey);
    if (!HeapScanIsValid(scan))
	elog(WARN, "%s: cannot begin scan of pg_database.", command);

    /*
     *  since we want to return the tuple out of this proc, and we're
     *  going to close the relation, copy the tuple and return the copy.
     */
    tup = heap_getnext(scan, 0, &buf);

    if (HeapTupleIsValid(tup)) {
	dbtup = (HeapTuple) palloctup(tup, buf, dbrel);
	ReleaseBuffer(buf);
    } else
	dbtup = tup;

    heap_endscan(scan);
    return (dbtup);
}

/*
 *  check_permissions() -- verify that the user is permitted to do this.
 *
 *  If the user is not allowed to carry out this operation, this routine
 *  elog(WARN, ...)s, which will abort the xact.  As a side effect, the
 *  user's pg_user tuple OID is returned in userIdP and the target database's
 *  OID is returned in dbIdP.
 */

check_permissions(command, dbname, dbIdP, userIdP)
    char *command;
    char *dbname;
    ObjectId *dbIdP;
    ObjectId *userIdP;
{
    Relation urel, dbrel;
    HeapTuple dbtup, utup;
    ObjectId dbowner;
    ObjectId dbid;
    char use_createdb;
    bool dbfound;
    bool use_super;
    NameData user;
    extern void GetUserName();

    GetUserName(&user);
    utup = SearchSysCacheTuple(USENAME, user.data, NULL, NULL, NULL);
    *userIdP = ((Form_pg_user)GETSTRUCT(utup))->usesysid;
    use_super = ((Form_pg_user)GETSTRUCT(utup))->usesuper;
    use_createdb = ((Form_pg_user)GETSTRUCT(utup))->usecreatedb;

    /* Check to make sure user has permission to use createdb */
    if (!use_createdb) {
        elog(WARN, "%s: user \"%-.*s\" is not allowed to create/destroy databases",
             sizeof(NameData), user);
    }

    /* Make sure we are not mucking with the template database */
    if (!strcmp(dbname, "template1")) {
        elog(WARN, "%s cannot be executed on the template database.", command);
    }

    /* Check to make sure database is not the currently open database */
    if (!strcmp(dbname, DBName)) {
        elog(WARN, "%s cannot be executed on an open database", command);
    }

    /* Check to make sure database is owned by this user */

    /* 
     * need the reldesc to get the database owner out of dbtup 
     * and to set a write lock on it.
     */
    dbrel = heap_openr(DatabaseRelationName);

    if (!RelationIsValid(dbrel))
	elog(FATAL, "%s: cannot open relation \"%-.*s\"",
	     command, DatabaseRelationName->data);

    /*
     * Acquire a write lock on pg_database from the beginning to avoid 
     * upgrading a read lock to a write lock.  Upgrading causes long delays 
     * when multiple 'createdb's or 'destroydb's are run simult. -mer 7/3/91
     */
    RelationSetLockForWrite(dbrel);
    dbtup = get_pg_dbtup(command, dbname, dbrel);
    dbfound = HeapTupleIsValid(dbtup);

    if (dbfound) {
	dbowner = (ObjectId) heap_getattr(dbtup, InvalidBuffer,
				          Anum_pg_database_datdba,
				          &(dbrel->rd_att),
				          (char *) NULL);
	*dbIdP = dbtup->t_oid;
    } else {
	*dbIdP = InvalidObjectId;
    }

    heap_close(dbrel);

    /*
     *  Now be sure that the user is allowed to do this.
     */

    if (dbfound && !strcmp(command, "createdb")) {

        elog(WARN, "createdb: database %s already exists.", dbname);

    } else if (!dbfound && !strcmp(command, "destroydb")) {

        elog(WARN, "destroydb: database %s does not exist.", dbname);

    } else if (dbfound && !strcmp(command, "destroydb")
	       && dbowner != *userIdP && use_super == false) {

        elog(WARN, "%s: database %s is not owned by you.", command, dbname);

    }
}

/*
 *  stop_vacuum() -- stop the vacuum daemon on the database, if one is
 *		     running.
 */
stop_vacuum(dbname)
    char *dbname;
{
    int i;
    char *path;
    char buf[512];
    char filename[256];
    FILE *fp;
    int pid;

    sprintf(filename, "%s/base/%s/%s.vacuum", DataDir, dbname, dbname);
    if ((fp = fopen(filename, "r")) != (FILE *) NULL) {
	fscanf(fp, "%d", &pid);
	fclose(fp);
	if (kill(pid, SIGKILLDAEMON1) < 0) {
	    elog(WARN, "can't kill vacuum daemon (pid %d) on %s",
		       pid, dbname);
	}
    }
}
