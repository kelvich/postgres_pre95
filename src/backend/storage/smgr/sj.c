/*
 *  sj.c -- sony jukebox storage manager.
 *
 *	This code manages relations that reside on magnetic disk.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "machine.h"
#include "storage/smgr.h"
#include "utils/rel.h"

RcsId("$Header$");

/*
 *  Only stub routines right now.
 */

int
sjinit()
{
    return (SM_SUCCESS);
}

int
sjshutdown()
{
    return (SM_SUCCESS);
}

int
sjcreate(reln)
    Relation reln;
{
    return (-1);
}

int
sjunlink(reln)
    Relation reln;
{
    return (SM_FAIL);
}

int
sjextend(reln, buffer)
    Relation reln;
    char *buffer;
{
    return (SM_FAIL);
}

int
sjopen(reln)
    Relation reln;
{
    return (-1);
}

int
sjclose(reln)
    Relation reln;
{
    return (SM_FAIL);
}

int
sjread(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    return (SM_FAIL);
}

int
sjwrite(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    return (SM_FAIL);
}

int
sjflush(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    return (SM_FAIL);
}

int
sjblindwrt(dbstr, relstr, dbid, relid, blkno, buffer)
    char *dbstr;
    char *relstr;
    OID dbid;
    OID relid;
    BlockNumber blkno;
    char *buffer;
{
    return (SM_FAIL);
}

int
sjnblocks(reln)
    Relation reln;
{
    return (-1);
}

int
sjcommit()
{
    return (SM_SUCCESS);
}

int
sjabort()
{
    return (SM_SUCCESS);
}
