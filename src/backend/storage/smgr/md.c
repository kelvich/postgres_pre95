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
#include "utils/mcxt.h"
#include "utils/rel.h"
#include "utils/log.h"

RcsId("$Header$");

#undef DIAGNOSTIC

/*
 *  The magnetic disk storage manager keeps track of open file descriptors
 *  in its own descriptor pool.  This happens for two reasons.  First, at
 *  transaction boundaries, we walk the list of descriptors and flush
 *  anything that we've dirtied in the current transaction.  Second, we
 *  have to support relations of > 4GBytes.  In order to do this, we break
 *  relations up into chunks of < 2GBytes and store one chunk in each of
 *  several files that represent the relation.
 */

typedef struct _MdfdVec {
    int			mdfd_vfd;		/* fd number in vfd pool */
    uint16		mdfd_flags;		/* clean, dirty */
    int			mdfd_lstbcnt;		/* most recent block count */
    struct _MdfdVec	*mdfd_chain;		/* for large relations */
} MdfdVec;

static int	Nfds = 100;
static MdfdVec	*Md_fdvec = (MdfdVec *) NULL;
static int	CurFd = 0;
MemoryContext	MdCxt;

/* globals defined elsewhere */
extern char		*DataDir;

#define MDFD_DIRTY	(uint16) 0x01

#define	RELSEG_SIZE	262144		/* (2 ** 31) / 8192 -- 2GB file */

/* routines declared here */
extern MdfdVec	*_mdfd_openseg();
extern MdfdVec	*_mdfd_getseg();

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
    MemoryContext oldcxt;

    MdCxt = (MemoryContext) CreateGlobalMemory("MdSmgr");
    if (MdCxt == (MemoryContext) NULL)
	return (SM_FAIL);

    oldcxt = MemoryContextSwitchTo(MdCxt);
    Md_fdvec = (MdfdVec *) palloc(Nfds * sizeof(MdfdVec));
    (void) MemoryContextSwitchTo(oldcxt);

    if (Md_fdvec == (MdfdVec *) NULL)
	return (SM_FAIL);

    (void) bzero(Md_fdvec, Nfds * sizeof(MdfdVec));

    return (SM_SUCCESS);
}

int
mdcreate(reln)
    Relation reln;
{
    int fd, vfd;
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

    if (CurFd >= Nfds) {
	if (_fdvec_ext() == SM_FAIL)
	    return (-1);
    }

    Md_fdvec[CurFd].mdfd_vfd = fd;
    Md_fdvec[CurFd].mdfd_flags = (uint16) 0;
    Md_fdvec[CurFd].mdfd_chain = (MdfdVec *) NULL;
    Md_fdvec[CurFd].mdfd_lstbcnt = 0;

    vfd = CurFd++;

    return (vfd);
}

/*
 *  mdunlink() -- Unlink a relation.
 */

int
mdunlink(reln)
    Relation reln;
{
    int fd;
    int i;
    MdfdVec *v, *ov;
    MemoryContext oldcxt;
    char fname[20];	/* XXX should have NAMESIZE defined */
    char tname[20];

    bzero(fname, 20);
    strncpy(fname, RelationGetRelationName(reln), 16);

    if (FileNameUnlink(fname) < 0)
	return (SM_FAIL);

    /* unlink all the overflow files for large relations */
    for (i = 1; ; i++) {
	sprintf(tname, "%s.%d", fname, i);
	if (FileNameUnlink(tname) < 0)
	    break;
    }

    /* finally, clean out the mdfd vector */
    fd = RelationGetFile(reln);
    Md_fdvec[fd].mdfd_flags = (uint16) 0;

    oldcxt = MemoryContextSwitchTo(MdCxt);
    for (v = &Md_fdvec[fd]; v != (MdfdVec *) NULL; ) {
	ov = v;
	v = v->mdfd_chain;
	if (ov != &Md_fdvec[fd])
	    pfree(ov);
    }
    Md_fdvec[fd].mdfd_chain = (MdfdVec *) NULL;
    (void) MemoryContextSwitchTo(oldcxt);

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
    int nblocks;
    MdfdVec *v;

    nblocks = mdnblocks(reln);
    v = _mdfd_getseg(reln, nblocks, O_CREAT);

    if ((pos = FileSeek(v->mdfd_vfd, 0L, SEEK_END)) < 0)
	return (SM_FAIL);

    if (FileWrite(v->mdfd_vfd, buffer, BLCKSZ) != BLCKSZ)
	return (SM_FAIL);

    /* remember that we did a write, so we can sync at xact commit */
    v->mdfd_flags |= MDFD_DIRTY;

    /* try to keep the last block count current, though it's just a hint */
    if ((v->mdfd_lstbcnt = (++nblocks % RELSEG_SIZE)) == 0)
	v->mdfd_lstbcnt = RELSEG_SIZE;

#ifdef DIAGNOSTIC
    if (FileGetNumberOfBlocks(v->mdfd_vfd) > RELSEG_SIZE
	|| v->mdfd_lstbcnt > RELSEG_SIZE)
	elog(FATAL, "segment too big!");
#endif

    return (SM_SUCCESS);
}

/*
 *  mdopen() -- Open the specified relation.
 */

int
mdopen(reln)
    Relation reln;
{
    char *path;
    int fd;
    int vfd;
    extern char *relpath();

    if (CurFd >= Nfds) {
	if (_fdvec_ext() == SM_FAIL)
	    return (-1);
    }

    path = relpath(&(reln->rd_rel->relname.data[0]));

    fd = FileNameOpenFile(path, O_RDWR, 0600);

    /* this should only happen during bootstrap processing */
    if (fd < 0)
	fd = FileNameOpenFile(path, O_RDWR|O_CREAT|O_EXCL, 0600);

    Md_fdvec[CurFd].mdfd_vfd = fd;
    Md_fdvec[CurFd].mdfd_flags = (uint16) 0;
    Md_fdvec[CurFd].mdfd_chain = (MdfdVec *) NULL;
    Md_fdvec[CurFd].mdfd_lstbcnt = FileGetNumberOfBlocks(fd);

#ifdef DIAGNOSTIC
    if (Md_fdvec[CurFd].mdfd_lstbcnt > RELSEG_SIZE)
	elog(FATAL, "segment too big on relopen!");
#endif

    vfd = CurFd++;

    return (vfd);
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
    int fd;
    MdfdVec *v;
    int status;

    fd = RelationGetFile(reln);

    for (v = &Md_fdvec[fd]; v != (MdfdVec *) NULL; v = v->mdfd_chain) {

	/* may be closed already */
	if (v->mdfd_vfd < 0)
	    continue;

	/*
	 *  We sync the file descriptor so that we don't need to reopen it at
	 *  transaction commit to force changes to disk.
	 */

	FileSync(v->mdfd_vfd);
	FileClose(v->mdfd_vfd);

	/* mark this file descriptor as clean in our private table */
	v->mdfd_flags &= ~MDFD_DIRTY;
    }

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
    long seekpos;
    int nbytes;
    MdfdVec *v;

    v = _mdfd_getseg(reln, blocknum, 0);

    seekpos = (long) (BLCKSZ * (blocknum % RELSEG_SIZE));

#ifdef DIAGNOSTIC
    if (seekpos >= BLCKSZ * RELSEG_SIZE)
	elog(FATAL, "seekpos too big!");
#endif

    if (FileSeek(v->mdfd_vfd, seekpos, SEEK_SET) != seekpos) {
	return (SM_FAIL);
    }

    status = SM_SUCCESS;
    if ((nbytes = FileRead(v->mdfd_vfd, buffer, BLCKSZ)) != BLCKSZ) {
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
    long seekpos;
    MdfdVec *v;

    v = _mdfd_getseg(reln, blocknum, 0);

    seekpos = (long) (BLCKSZ * (blocknum % RELSEG_SIZE));
#ifdef DIAGNOSTIC
    if (seekpos >= BLCKSZ * RELSEG_SIZE)
	elog(FATAL, "seekpos too big!");
#endif

    if (FileSeek(v->mdfd_vfd, seekpos, SEEK_SET) != seekpos) {
	return (SM_FAIL);
    }

    status = SM_SUCCESS;
    if (FileWrite(v->mdfd_vfd, buffer, BLCKSZ) != BLCKSZ)
	status = SM_FAIL;

    v->mdfd_flags |= MDFD_DIRTY;

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
    long seekpos;
    MdfdVec *v;

    v = _mdfd_getseg(reln, blocknum, 0);

    seekpos = (long) (BLCKSZ * (blocknum % RELSEG_SIZE));
#ifdef DIAGNOSTIC
    if (seekpos >= BLCKSZ * RELSEG_SIZE)
	elog(FATAL, "seekpos too big!");
#endif

    if (FileSeek(v->mdfd_vfd, seekpos, SEEK_SET) != seekpos) {
	return (SM_FAIL);
    }

    /* write and sync the block */
    status = SM_SUCCESS;
    if (FileWrite(v->mdfd_vfd, buffer, BLCKSZ) != BLCKSZ
	|| FileSync(v->mdfd_vfd) < 0)
	status = SM_FAIL;

    /*
     *  By here, the block is written and changes have been forced to stable
     *  storage.  Mark the descriptor as clean until the next write, so we
     *  don't sync it again unnecessarily at transaction commit.
     */

    v->mdfd_flags &= ~MDFD_DIRTY;

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
    int segno;
    long seekpos;
    int status;
    char *path;
    int nchars;

    /* be sure we have enough space for the '.segno', if any */
    segno = blkno / RELSEG_SIZE;
    if (segno > 0)
	nchars = 10;
    else
	nchars = 0;

    /* construct the path to the file and open it */
    if (dbid == (OID) 0) {
	path = (char *) palloc(strlen(DataDir) + sizeof(NameData) + 2 + nchars);
	if (segno == 0)
	    sprintf(path, "%s/%.*s", DataDir, NAMEDATALEN, relstr);
	else
	    sprintf(path, "%s/%.*s.%d", DataDir, NAMEDATALEN, relstr, segno);
    } else {
	path = (char *) palloc(strlen(DataDir) + strlen("/base/") + 2 * sizeof(NameData) + 2 + nchars);
	if (segno == 0)
	    sprintf(path, "%s/base/%.*s/%.*s", DataDir, NAMEDATALEN, 
			dbstr, NAMEDATALEN, relstr);
	else
	    sprintf(path, "%s/base/%.*s/%.*s.%d", DataDir, NAMEDATALEN, dbstr,
			NAMEDATALEN, relstr, segno);
    }

    if ((fd = open(path, O_RDWR, 0600)) < 0)
	return (SM_FAIL);

    /* seek to the right spot */
    seekpos = (long) (BLCKSZ * (blkno % RELSEG_SIZE));
    if (lseek(fd, seekpos, SEEK_SET) != seekpos) {
	(void) close(fd);
	return (SM_FAIL);
    }

    status = SM_SUCCESS;

    /* write and sync the block */
    if (write(fd, buffer, BLCKSZ) != BLCKSZ || fsync(fd) < 0)
	status = SM_FAIL;

    if (close(fd) < 0)
	status = SM_FAIL;

    pfree (path);

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
    int fd;
    MdfdVec *v;
    int nblocks;
    int segno;

    fd = RelationGetFile(reln);
    v = &Md_fdvec[fd];

#ifdef DIAGNOSTIC
    if (FileGetNumberOfBlocks(v->mdfd_vfd) > RELSEG_SIZE)
	elog(FATAL, "segment too big in getseg!");
#endif

    segno = 0;
    for (;;) {
	if (v->mdfd_lstbcnt == RELSEG_SIZE
	    || (nblocks = FileGetNumberOfBlocks(v->mdfd_vfd)) == RELSEG_SIZE) {

	    v->mdfd_lstbcnt = RELSEG_SIZE;
	    segno++;

	    if (v->mdfd_chain == (MdfdVec *) NULL) {
		v->mdfd_chain = _mdfd_openseg(reln, segno, O_CREAT);
		if (v->mdfd_chain == (MdfdVec *) NULL)
		    elog(WARN, "cannot count blocks for %.16s -- open failed",
				RelationGetRelationName(reln));
	    }

	    v = v->mdfd_chain;
	} else {
	    return ((segno * RELSEG_SIZE) + nblocks);
	}
    }
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
    MdfdVec *v;

    for (i = 0; i < CurFd; i++) {
	for (v = &Md_fdvec[i]; v != (MdfdVec *) NULL; v = v->mdfd_chain) {
	    if (v->mdfd_flags & MDFD_DIRTY) {
		if (FileSync(v->mdfd_vfd) < 0)
		    return (SM_FAIL);

		v->mdfd_flags &= ~MDFD_DIRTY;
	    }
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
    int i;
    MdfdVec *v;

    for (i = 0; i < CurFd; i++) {
	for (v = &Md_fdvec[i]; v != (MdfdVec *) NULL; v = v->mdfd_chain) {
	    v->mdfd_flags &= ~MDFD_DIRTY;
	}
    }

    return (SM_SUCCESS);
}

/*
 *  _fdvec_ext() -- Extend the md file descriptor vector.
 *
 *	The file descriptor vector must be large enough to hold at least
 *	'fd' entries.
 */

int
_fdvec_ext()
{
    MdfdVec *nvec;
    int orig;
    MemoryContext oldcxt;

    Nfds *= 2;

    oldcxt = MemoryContextSwitchTo(MdCxt);

    nvec = (MdfdVec *) palloc(Nfds * sizeof(MdfdVec));
    (void) bzero(nvec, Nfds * sizeof(MdfdVec));
    (void) bcopy((char *) Md_fdvec, nvec, (Nfds / 2) * sizeof(MdfdVec));
    pfree(Md_fdvec);

    (void) MemoryContextSwitchTo(oldcxt);

    Md_fdvec = nvec;

    return (SM_SUCCESS);
}

MdfdVec *
_mdfd_openseg(reln, segno, oflags)
    Relation reln;
    int segno;
    int oflags;
{
    MemoryContext oldcxt;
    MdfdVec *v;
    int fd;
    int status;
    bool dofree;
    char *path, *fullpath;

    /* be sure we have enough space for the '.segno', if any */
    path = relpath(RelationGetRelationName(reln));

    dofree = false;
    if (segno > 0) {
	dofree = true;
	fullpath = (char *) palloc(strlen(path) + 10);
	sprintf(fullpath, "%s.%d", path, segno);
    } else
	fullpath = path;

    /* open the file */
    fd = PathNameOpenFile(fullpath, O_RDWR|oflags, 0600);

    if (dofree)
	pfree(fullpath);

    if (fd < 0)
	return ((MdfdVec *) NULL);

    /* allocate an mdfdvec entry for it */
    oldcxt = MemoryContextSwitchTo(MdCxt);
    v = (MdfdVec *) palloc(sizeof(MdfdVec));
    (void) MemoryContextSwitchTo(oldcxt);

    /* fill the entry */
    v->mdfd_vfd = fd;
    v->mdfd_flags = (uint16) 0;
    v->mdfd_chain = (MdfdVec *) NULL;
    v->mdfd_lstbcnt = FileGetNumberOfBlocks(fd);

#ifdef DIAGNOSTIC
    if (v->mdfd_lstbcnt > RELSEG_SIZE)
	elog(FATAL, "segment too big on open!");
#endif

    /* all done */
    return (v);
}

MdfdVec *
_mdfd_getseg(reln, blkno, oflag)
    Relation reln;
    int blkno;
    int oflag;
{
    MdfdVec *v;
    int segno;
    int fd;
    int i;

    fd = RelationGetFile(reln);
    if (fd < 0) {
	if ((fd = mdopen(reln)) < 0)
	    elog(WARN, "cannot open relation %.16s",
			RelationGetRelationName(reln));
	reln->rd_fd = fd;
    }

    for (v = &Md_fdvec[fd], segno = blkno / RELSEG_SIZE, i = 1;
	 segno > 0;
	 i++, segno--) {

	if (v->mdfd_chain == (MdfdVec *) NULL) {
	    v->mdfd_chain = _mdfd_openseg(reln, i, oflag);

	    if (v->mdfd_chain == (MdfdVec *) NULL)
		elog(WARN, "cannot open segment %d of relation %.16s",
			    i, RelationGetRelationName(reln));
	}
	v = v->mdfd_chain;
    }

    return (v);
}
