#include <stdio.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "utils/rel.h"
#include "utils/log.h"


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
	        "append pg_database (datname = %s::char16, datdba = %d::oid, \
	         datpath = %s::text", dbname, user_sysid, dbname);
	pg_eval(buf);
}

destroydb(dbname)

char *dbname;

{
	char buf[512];
	char *path;
	int i;

	if (check_security("destroydb", dbname) != 0)
	{
		if (NStriping == 0)
		{
			path = GetDataHome();
			sprintf(buf, "rm -r %s/data/base/%s", path, dbname);
			system(buf);
		}
		else
		{
			for (i = 0; i < NStriping; i++)
			{
				path = PostgresHomes[i];
				sprintf(buf, "rm -r %s/data/base/%s", path, dbname);
				system(buf);
			}
		}
	}

	sprintf(buf, "delete pg_database where datname = %s", dbname);
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
	char dummy, *value, use_createdb;
	int n, done = 0;
	int dbfound = 0;

	HeapTuple u, amgetnext();

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
		char *value;
		u = amgetnext(s, NULL, NULL);
		if (u == NULL) elog(FATAL, "User %s is not in pg_user!!", PG_username);
		value = (char *) HeapTupleGetAttributeValue(u, InvalidBuffer, 1,
										            &(r->rd_att), &dummy);
		if (!strncmp(PG_username, value, n))
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

	if (use_createdb != 'y')
	{
		elog(WARN, "User %s is not allowed to use the createdb query",
			 PG_username);
	}

	/* Check to make sure database is not the currently open database */

	if (!strcmp(dbname, DBName))
	{
		elog(WARN, "%s cannot be executed on an open database", command);
	}

	/*
	 * Now we have determined that the user can create a database.  Now we
	 * have to make sure he can destroy an existing database by checking its
	 * ownership, etc.
	 */

	if (!strcmp(command, "createdb")) return(1);

	/* Check to make sure database is owned by this user */

	r = amopenr("pg_database");
	if (r == NULL) elog(FATAL, "%s: can\'t open pg_database!", command);
	s = ambeginscan(r, 0, NULL, NULL, NULL);
	done = 0;
	n = strlen(dbname);
	while (!done)
	{
		int value;
		char *value2;

		u = amgetnext(s, NULL, NULL);
		if (u == NULL) 
		{
			done = 1;
			dbfound = 0;
		}

		value = (int) HeapTupleGetAttributeValue(u, InvalidBuffer, 2,
		                                         &(r->rd_att), &dummy);
		if (value == user_sysid) {
			value2 = (char *) HeapTupleGetAttributeValue(u, InvalidBuffer, 1,
		                                                 &(r->rd_att), &dummy);
			if (!strncmp(value2, dbname, n))
			{
				done = 1;
				dbfound = 1;
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

	return(1);
}
