/*
 * smgrtype.c -- storage manager type
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "utils/log.h"
#include "storage/smgr.h"

RcsId("$Header$");

typedef struct smgrid {
	char *smgr_name;
	int2 smgr_id;
} smgrid;

static smgrid StorageManager[] = {
	"magnetic disk",	0,
	"sony jukebox", 	1
};

static int NStorageManagers = lengthof(StorageManager);

int2
smgrin(s)
    char *s;
{
    int i;

    for (i = 0; i < NStorageManagers; i++) {
	if (strcmp(s, StorageManager[i].smgr_name) == 0)
	    return(StorageManager[i].smgr_id);
    }
    elog(WARN, "smgrin: illegal storage manager name %s", s);
}

char *
smgrout(i)
    int2 i;
{
    char *s;
    int len;

    if (i >= NStorageManagers || i < 0)
	elog(WARN, "Illegal storage manager id %d", i);

    s = (char *) palloc(strlen(StorageManager[i].smgr_name) + 1);
    strcpy(s, StorageManager[i].smgr_name);
    return (s);
}

bool
smgreq(a, b)
    int2 a, b;
{
    if (a == b)
	return (true);
    return (false);
}

bool
smgrne(a, b)
    int2 a, b;
{
    if (a == b)
	return (false);
    return (true);
}
