/*
 *  md.c -- magnetic disk storage manager.
 *
 *	This code manages relations that reside on magnetic disk.
 */

#include <sys/file.h>

#include "tmp/c.h"
#include "tmp/postgres.h"

#include "machine.h"
#include "storage/smgr.h"
#include "storage/block.h"
#include "storage/fd.h"
#include "utils/rel.h"

RcsId("$Header$");

/*
 *  Need to keep track of open file descriptors under the magnetic disk
 *  storage manager.
 */

static int	Nfds = 100;	/* must be same as in storage/file/fd.c */
static int	*Md_fdvec;

#define MDFD_CLEAN	0
#define MDFD_DIRTY	1

/*
 *  mdinit() -- Initialize private state for magnetic disk storage manager.
 *
 *	We keep a private table of all file descriptors.  Whenever we do
 *	a write to one, we mark it dirty in our table.  Whenever we force
 *	changes to disk, we mark the file descriptor clean.  At transaction
 *	commit, we force changes to disk for all dirty file descriptors.
 *	This routine allocates and initializes the table.
 *
 *	Returns SM_SUCCESS or SM_FAIL with errno set as appropriate.
 */

int
mdinit()
{
#ifdef lint
    unused = unused;
#endif /* lint */

    if ((Md_fdvec = (int *) malloc(Nfds * sizeof(int))) == (int *) NULL)
	return (SM_FAIL);

    (void) bzero(Md_fdvec, Nfds * sizeof(int));

    return (SM_SUCCESS);
}

int
mdcreate(reln)
    Relation reln;
{
    int fd;
    int tmp;
    char *path;
    extern char *relpath();
    extern bool IsBootstrapProcessingMode();

    path = relpath(&(reln->rd_rel->relname.data[0]));
    fd = FileNameOpenFile(path, O_RDWR|O_CREAT|O_EXCL, 0600);

    /*
     *  If the file already exists and is empty, we pretend that the
     *  create succeeded.  During bootstrap processing, we skip that check,
     *  because pg_time, pg_variable, and pg_log get created before their
     *  .bki file entries are processed.
     */

    if (fd < 0) {
	if ((fd = FileNameOpenFile(path, O_RDWR, 0600)) >= 0) {
	    if (!IsBootstrapProcessingMode() &&
		FileRead(fd, (char *) &tmp, sizeof(tmp)) != 0) {
		FileClose(fd);
		return (-1);
	    }
	}
    }

    return (fd);
}

/*
 *  mdunlink() -- Unlink a relation.
 */

int
mdunlink(reln)
    Relation reln;
{
    FileUnlink(RelationGetFile(reln));

    return (SM_SUCCESS);
}

/*
 *  mdextend() -- Add a block to the specified relation.
 *
 *	This routine returns SM_FAIL or SM_SUCCESS, with errno set as
 *	appropriate.
 */

int
mdextend(reln, buffer)
    Relation reln;
    char *buffer;
{
    long pos;
    File vfd;

    vfd = RelationGetFile(reln);

    if ((pos = FileSeek(vfd, 0L, L_XTND)) < 0)
	return (SM_FAIL);

    if (FileWrite(vfd, buffer, BLCKSZ) != BLCKSZ)
	return (SM_FAIL);

    /* remember that we did a write, so we can sync at xact commit */
    Md_fdvec[vfd] = MDFD_DIRTY;

    return (SM_SUCCESS);
}

/*
 *  mdopen() -- Open the specified relation.
 *
 *	The magnetic disk storage manager uses one file descriptor per open
 *	relation.  This routine returns the open file descriptor.
 */

int
mdopen(reln)
    Relation reln;
{
    char *path;
    int fd;
    extern char *relpath();

    path = relpath(&(reln->rd_rel->relname.data[0]));

    fd = FileNameOpenFile(path, O_RDWR, 0600);

    /* this should only happen during bootstrap processing */
    if (fd < 0)
	fd = FileNameOpenFile(path, O_RDWR|O_CREAT|O_EXCL, 0600);

    return (fd);
}

/*
 *  mdclose() -- Close the specified relation.
 *
 *	Returns SM_SUCCESS or SM_FAIL with errno set as appropriate.
 */

int
mdclose(reln)
    Relation reln;
{
    File vfd;
    int status;

    /* maybe it's already closed... */
    if ((vfd = RelationGetFile(reln)) < 0)
	return (SM_SUCCESS);

    /*
     *  Need to do it here.  We sync the file descriptor so that we don't
     *  need to reopen it at transaction commit to force changes to disk.
     */

    FileSync(vfd);
    FileClose(vfd);

    /* mark this file descriptor as clean in our private table */
    Md_fdvec[vfd] = MDFD_CLEAN;

    return (SM_SUCCESS);
}

/*
 *  mdread() -- Read the specified block from a relation.
 *
 *	Returns SM_SUCCESS or SM_FAIL.
 */

int
mdread(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    int status;
    File vfd;
    long seekpos;
    int nbytes;

    if ((vfd = RelationGetFile(reln)) < 0) {
	if ((vfd = mdopen(reln)) < 0)
	    return (SM_FAIL);
	reln->rd_fd = vfd;
    }

    seekpos = (long) (BLCKSZ * blocknum);
    if (FileSeek(vfd, seekpos, L_SET) != seekpos) {
	return (SM_FAIL);
    }

    status = SM_SUCCESS;
    if ((nbytes = FileRead(vfd, buffer, BLCKSZ)) != BLCKSZ) {
	if (nbytes == 0) {
	    (void) bzero(buffer, BLCKSZ);
	} else {
	    status = SM_FAIL;
	}
    }

    return (status);
}

/*
 *  mdwrite() -- Write the supplied block at the appropriate location.
 *
 *	Returns SM_SUCCESS or SM_FAIL.
 */

int
mdwrite(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    int status;
    bool found;
    File vfd;
    long seekpos;

    found = true;
    if ((vfd = RelationGetFile(reln)) < 0) {
	found = false;
	if ((vfd = mdopen(reln)) < 0)
	    return (SM_FAIL);
    }

    seekpos = (long) (BLCKSZ * blocknum);
    if (FileSeek(vfd, seekpos, L_SET) != seekpos) {
	if (!found)
	    (void) FileClose(vfd);

	return (SM_FAIL);
    }

    status = SM_SUCCESS;
    if (FileWrite(vfd, buffer, BLCKSZ) != BLCKSZ)
	status = SM_FAIL;

    /*
     *  If we opened this file descriptor especially to write this one block,
     *  force the change to disk and mark the descriptor as clean.  If we
     *  had an open descriptor for the file already, mark it as dirty so
     *  we'll flush it at commit time.
     */

    if (!found) {
	FileSync(vfd);
	FileClose(vfd);
	Md_fdvec[vfd] = MDFD_CLEAN;
    } else {
	Md_fdvec[vfd] = MDFD_DIRTY;
    }

    return (status);
}

/*
 *  mdflush() -- Synchronously write a block to disk.
 *
 *	This is exactly like mdwrite(), but doesn't return until the file
 *	system buffer cache has been flushed.
 */

int
mdflush(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    int status;
    bool found;
    File vfd;
    long seekpos;

    found = true;
    if ((vfd = RelationGetFile(reln)) < 0) {
	found = false;
	if ((vfd = mdopen(reln)) < 0)
	    return (SM_FAIL);
    }

    seekpos = (long) (BLCKSZ * blocknum);
    if (FileSeek(vfd, seekpos, L_SET) != seekpos) {
	if (!found)
	    (void) FileClose(vfd);

	return (SM_FAIL);
    }

    status = SM_SUCCESS;

    /* write and sync the block */
    if (FileWrite(vfd, buffer, BLCKSZ) != BLCKSZ || FileSync(vfd) < 0)
	status = SM_FAIL;

    /*
     *  By here, the block is written and changes have been forced to stable
     *  storage.  Mark the descriptor as clean until the next write, so we
     *  don't sync it again unnecessarily at transaction commit.
     */

    Md_fdvec[vfd] = MDFD_CLEAN;

    if (!found)
	FileClose(vfd);

    return (status);
}

/*
 *  mdblindwrt() -- Write a block to disk blind.
 *
 *	We have to be able to do this using only the name and OID of
 *	the database and relation in which the block belongs.  This
 *	is a synchronous write.
 */

int
mdblindwrt(dbstr, relstr, dbid, relid, blkno, buffer)
    char *dbstr;
    char *relstr;
    OID dbid;
    OID relid;
    BlockNumber blkno;
    char *buffer;
{
    int fd;
    long seekpos;
    int status;
    char path[64];

    /* construct the path to the file and open it */
    sprintf(path, "../%s/%s", (dbid == (OID) 0 ? ".." : dbstr), relstr);
    if ((fd = open(path, O_RDWR, 0600)) < 0)
	return (SM_FAIL);

    /* seek to the right spot */
    seekpos = (long) (BLCKSZ * blkno);
    if (lseek(fd, seekpos, L_SET) != seekpos) {
	(void) close(fd);
	return (SM_FAIL);
    }

    status = SM_SUCCESS;

    /* write and sync the block */
    if (write(fd, buffer, BLCKSZ) != BLCKSZ || fsync(fd) < 0)
	status = SM_FAIL;

    if (close(fd) < 0)
	status = SM_FAIL;

    return (status);
}

/*
 *  mdnblocks() -- Get the number of blocks stored in a relation.
 *
 *	Returns # of blocks or -1 on error.
 */

int
mdnblocks(reln)
    Relation reln;
{
    File vfd;

    vfd = RelationGetFile(reln);

    return (FileGetNumberOfBlocks(vfd));
}

/*
 *  mdcommit() -- Commit a transaction.
 *
 *	All changes to magnetic disk relations must be forced to stable
 *	storage.  This routine makes a pass over the private table of
 *	file descriptors.  Any descriptors to which we have done writes,
 *	but not synced, are synced here.
 *
 *	Returns SM_SUCCESS or SM_FAIL with errno set as appropriate.
 */

int
mdcommit()
{
    int i;

    for (i = 0; i < Nfds; i++) {
	if (Md_fdvec[i] == MDFD_DIRTY) {
	    if (FileSync(i) < 0)
		return (SM_FAIL);

	    Md_fdvec[i] = MDFD_CLEAN;
	}
    }

    return (SM_SUCCESS);
}

/*
 *  mdabort() -- Abort a transaction.
 *
 *	Changes need not be forced to disk at transaction abort.  We mark
 *	all file descriptors as clean here.  Always returns SM_SUCCESS.
 */

int
mdabort()
{
    bzero(Md_fdvec, Nfds * sizeof(int));
    return (SM_SUCCESS);
}
