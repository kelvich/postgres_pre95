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
} smgrid;

/*
 *  StorageManager[] -- List of defined storage managers.
 *
 *	The weird comma placement is to keep compilers happy no matter
 *	which of these is (or is not) defined.
 */

static smgrid StorageManager[] = {
	"magnetic disk"
#ifdef SONY_JUKEBOX
	, "sony jukebox"
#endif /* SONY_JUKEBOX */
#ifdef MAIN_MEMORY
	, "main memory"
#endif /* MAIN_MEMORY */
};

static int NStorageManagers = lengthof(StorageManager);

int2
smgrin(s)
    char *s;
{
    int i;

    for (i = 0; i < NStorageManagers; i++) {
	if (strcmp(s, StorageManager[i].smgr_name) == 0)
	    return((int2) i);
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
