/*
 *  sj.c -- sony jukebox storage manager.
 *
 *	This code manages relations that reside on the sony write-once
 *	optical disk jukebox.
 */

#include <sys/file.h>

#include "tmp/c.h"
#include "tmp/postgres.h"

#ifdef SONY_JUKEBOX

#include "machine.h"
#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/smgr.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "utils/hsearch.h"
#include "utils/rel.h"
#include "utils/log.h"

RcsId("$Header$");

/*
 *  When the buffer pool requests a particular page, we load a group of
 *  pages from the jukebox into the mag disk cache for efficiency.
 *  SJCACHESIZE is the number of these groups in the disk cache.  Every
 *  group is represented by one entry in the shared memory cache.  SJGRPSIZE
 *  is the number of 8k pages in a group.
 */

#define	SJCACHESIZE	64		/* # groups in mag disk cache */
#define	SJGRPSIZE	10		/* # 8k pages in a group */
#define SJPATHLEN	64		/* size of path to cache file */

/* misc constants */
#define	SJCACHENAME	"_sj_cache_"	/* relative to $POSTGRESHOME/data */
#define	SJMETANAME	"_sj_meta_"	/* relative to $POSTGRESHOME/data */

/*
 *  SJGroupDesc -- Descriptor block for a cache group.
 *
 *	The first 1024 bytes in a group -- on a platter or in the magnetic
 *	disk cache -- are a descriptor block.  We choose 1024 bytes because
 *	this is the native block size of the jukebox.
 *
 *	This block includes a description of the data that appears in the
 *	group, including relid, dbid, relname, dbname, and a unique OID
 *	that we use to verify cache consistency on startup.  SJGroupDesc
 *	is the structure that contains this information.  It resides at the
 *	start of the 1024-byte block; the rest of the block is unused.
 */

typedef struct SJGroupDesc {
    long	sjgd_magic;
    long	sjgd_version;
    NameData	sjgd_dbname;
    NameData	sjgd_relname;
    ObjectId	sjgd_dbid;
    ObjectId	sjgd_relid;
    long	sjgd_relblkno;
    long	sjgd_jboffset;
    long	sjgd_extentsz;
    ObjectId	sjgd_groupoid;
} SJGroupDesc;

#define SJGDMAGIC	0x060362
#define	SJGDVERSION	0
#define JBBLOCKSZ	1024

/*
 *  SJCacheTag -- Unique identifier for individual groups in the magnetic
 *		  disk cache.
 *
 *	We use this identifier to query the shared memory cache metadata
 *	when we want to find a particular group.  
 */

typedef struct SJCacheTag {
    ObjectId		sjct_dbid;	/* database OID of this group */
    ObjectId		sjct_relid;	/* relation OID of this group */
    BlockNumber		sjct_base;	/* number of first block in group */
} SJCacheTag;

/*
 *  SJHashEntry -- The hash table code returns a pointer to a structure
 *		   that has this layout.
 */

typedef struct SJHashEntry {
    SJCacheTag		sjhe_tag;	/* cache tag -- hash key */
    int			sjhe_groupno;	/* which group this is in cache file */
} SJHashEntry;

/*
 *  SJCacheHeader -- Header data for in-memory metadata cache.
 */

typedef struct SJCacheHeader {
    int			sjh_nentries;
    int			sjh_lruhead;
    int			sjh_lrutail;
    uint32		sjh_flags;

#define SJH_INITING	(1 << 0)
#define SJH_INITED	(1 << 1)

#ifdef HAS_TEST_AND_SET

    slock_t		sjh_initlock;	/* initialization in progress lock */

#endif /* HAS_TEST_AND_SET */

} SJCacheHeader;

/*
 *  SJCacheItem -- Cache item describing blocks on the magnetic disk cache.
 *
 *	An array of these is maintained in shared memory, with one entry
 *	for every group that appears in the magnetic disk block cache.  We
 *	maintain a consistent copy of this array on magnetic disk whenever
 *	we change the cache contents.  This is because the magnetic disk
 *	cache is persistent, and contains data that logically appears on the
 *	jukebox between backend instances.
 *
 *	The OID that appears in this structure is used to detect corruption
 *	of the cache due to crashes during cache metadata update on disk.
 *	When we detect corruption, we recover by marking the group free.  We
 *	are very careful to do this in a way that guarantees no data is lost,
 *	and that does not require log processing.
 *
 *	Since we never return pointers to private data, we don't need to
 *	maintain a free list or pin count on magnetic disk cache groups.
 *	In shared memory, we maintain a list of groups in LRU order (offsets
 *	from the start of cache metadata are stored in this structure).
 *	When we need a group for data transfer, we use the least-recently-used
 *	group's space, kicking it out to the platter if necessary.
 *
 *	Groups on the jukebox include one page (the first) that describes the
 *	group, including its dbid, relid, dbname, relname, and extent size.
 *	This page also includes the OID described above.
 */

typedef struct SJCacheItem {
    SJCacheTag		sjc_tag;		/* dbid, relid, group triple */
    int			sjc_lruprev;		/* LRU list pointer */
    int			sjc_lrunext;		/* LRU list pointer */
    ObjectId		sjc_oid;		/* OID of group */

    uint8		sjc_gflags;		/* flags for entire group */

#define SJG_CLEAR	(uint8) 0x0
#define	SJG_IOINPROG	(1 << 0)

    uint8		sjc_flags[SJGRPSIZE];	/* flag bytes, 1 per block */

#define	SJC_DIRTY	(1 << 0)
#define SJC_MISSING	(1 << 1)
#define SJC_ONPLATTER	(1 << 2)

#ifdef HAS_TEST_AND_SET

    slock_t		sjc_iolock;		/* transfer in progress */

#endif /* HAS_TEST_AND_SET */

} SJCacheItem;

/* globals used in this file */
SPINLOCK		SJCacheLock;	/* lock for cache metadata */
extern bool		IsPostmaster;	/* is this the postmaster running? */
extern ObjectId		MyDatabaseId;	/* OID of database we have open */
extern Name		MyDatabaseName;	/* name of database we have open */

static File		SJCacheVfd;	/* vfd for cache data file */
static File		SJMetaVfd;	/* vfd for cache metadata file */
static SJCacheHeader	*SJHeader;	/* pointer to cache header in shmem */
static HTAB		*SJCacheHT;	/* pointer to hash table in shmem */
static SJCacheItem	*SJCache;	/* pointer to cache metadata in shmem */

#ifndef	HAS_TEST_AND_SET

/*
 *  If we don't have test-and-set locks, then we need a semaphore for
 *  concurrency control.  This semaphore is in addition to the metadata
 *  lock, SJCacheLock, that we acquire before touching the cache metadata.
 *
 *  This semaphore is used in two ways.  During cache initialization, we
 *  use it to lock out all other backends that want cache access.  During
 *  normal processing, we control access to groups on which IO is in
 *  progress by holding this lock.  When we're done with initialization or
 *  IO, we do enough V's on the semaphore to satisfy all outstanding P's.
 */

static IpcSemaphoreId	SJWaitSemId;	/* wait semaphore */
static long		*SJNWaiting;	/* # procs sleeping on the wait sem */

#endif /* ndef HAS_TEST_AND_SET */

/* static buffer is for data transfer -- SJGRPSIZE blocks + descriptor block */
static char	SJCacheBuf[(BLCKSZ * SJGRPSIZE) + JBBLOCKSZ];
static int	SJBufSize = ((BLCKSZ * SJGRPSIZE) + JBBLOCKSZ);

/* routines declared here */
extern void	sjcacheinit();
extern void	sjwait_init();
extern void	sjunwait_init();
extern void	sjwait_io();
extern void	sjunwait_io();

/* routines declared elsewhere */
extern HTAB	*ShmemInitHash();
extern int	*ShmemInitStruct();
extern int	tag_hash();
extern char	*getenv();

/*
 *  sjinit() -- initialize the Sony jukebox storage manager.
 *
 *	We need to find (or establish) the mag-disk buffer cache metadata
 *	in shared memory and open the cache on mag disk.  If this code is
 *	executed by the postmaster, we'll create (but not populate) the
 *	cache memory.  The first backend to run that touches the cache
 *	initializes it.  All other backends running simultaneously will
 *	only wait for this initialization to complete if they need to get
 *	data out of the cache.  Otherwise, they'll return successfully
 *	immediately after attaching the cache memory, and will let their
 *	older sibling do all the work.
 *
 *	The 'key' argument is the IPC key used in this backend (or postmaster)
 *	for initializing shared memory and semaphores.  Since we need a
 *	wait lock, we need this.
 */

int
sjinit(key)
    IPCKey key;
{
    unsigned int metasize;
    bool metafound;
    HASHCTL info;
    bool initcache;
    char *cacheblk, *cachesave;
    int status;
    char *pghome;
    char path[SJPATHLEN];

    /*
     *  First attach the shared memory block that contains the disk
     *  cache metadata.  At the end of this block in shared memory is
     *  the 
     */

    SpinAcquire(SJCacheLock);

#ifdef HAS_TEST_AND_SET
    metasize = (SJCACHESIZE * sizeof(SJCacheItem)) + sizeof(SJCacheHeader);
#else /* HAS_TEST_AND_SET */
    metasize = (SJCACHESIZE * sizeof(SJCacheItem)) + sizeof(SJCacheHeader)
		+ sizeof(*SJNWaiting);
#endif /* HAS_TEST_AND_SET */
    cachesave = cacheblk = (char *) ShmemInitStruct("Jukebox cache metadata",
						    metasize, &metafound);

    if (cacheblk == (char *) NULL) {
	SpinRelease(SJCacheLock);
	return (SM_FAIL);
    }

    /*
     *  Order of items in shared memory is metadata header, number of
     *  processes sleeping on the wait semaphore (if no test-and-set locks),
     *  and cache entries.
     */

    SJHeader = (SJCacheHeader *) cacheblk;
    cacheblk += sizeof(SJCacheHeader);

#ifndef HAS_TEST_AND_SET
    SJNWaiting = (long *) cacheblk;
    cacheblk += sizeof(long);
#endif /* ndef HAS_TEST_AND_SET */

    SJCache = (SJCacheItem *) cacheblk;

    /*
     *  Now initialize the pointer to the shared memory hash table.
     */

    info.keysize = sizeof(SJCacheTag);
    info.datasize = sizeof(int);
    info.hash = tag_hash;

    SJCacheHT = ShmemInitHash("Jukebox cache hash table",
			      SJCACHESIZE, SJCACHESIZE,
			      &info, (HASH_ELEM|HASH_FUNCTION));

    if (SJCacheHT == (HTAB *) NULL) {
	SpinRelease(SJCacheLock);
	return (SM_FAIL);
    }

#ifndef HAS_TEST_AND_SET
    /*
     *  Finally, we need the wait semaphore if this system does not support
     *  test-and-set locks.
     */

    SJWaitSemId = IpcSemaphoreCreate(IPCKeyGetSJWaitSemaphoreKey(key),
				     1, IPCProtection, 0, &status);
    if (SJWaitSemId < 0) {
	SpinRelease(SJCacheLock);
	return (SM_FAIL);
    }
#endif /* ndef HAS_TEST_AND_SET */

    if (IsPostmaster) {

	if (metafound)
	    elog(FATAL, "sj cache found in shared memory by postmaster!");

	bzero((char *) cachesave, metasize);
	return (SM_SUCCESS);
    }

    /*
     *  Okay, all our shared memory pointers are set up.  If we did not
     *  find the cache metadata entries in shared memory, or if the cache
     *  has not been initialized from disk, initialize it in this backend.
     */

    if (!metafound || !(SJHeader->sjh_flags & (SJH_INITING|SJH_INITED))) {
	initcache = true;
	bzero((char *) cachesave, metasize);
#ifdef HAS_TEST_AND_SET
	S_LOCK(&(SJHeader->sjh_initlock));
#else /* HAS_TEST_AND_SET */
	IpcSemaphoreLock(SJWaitSemId, 0, 1);
	*SJNWaiting = 1;
	SJHeader->sjh_flags = SJH_INITING;
#endif /* HAS_TEST_AND_SET */
    } else {
	initcache = false;
    }

    /* don't need exclusive access anymore */
    SpinRelease(SJCacheLock);

    if ((pghome = getenv("POSTGRESHOME")) == (char *) NULL)
	pghome = "/usr/postgres";

    sprintf(path, "%s/data/%s", pghome, SJCACHENAME);

    SJCacheVfd = PathNameOpenFile(path, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (SJCacheVfd < 0) {
	SJCacheVfd = PathNameOpenFile(path, O_RDWR, 0600);
	if (SJCacheVfd < 0) {

	    /* if we were initializing the metadata, note our surrender */
	    if (!metafound) {
		SJHeader->sjh_flags &= ~SJH_INITING;
		sjunwait_init();
	    }

	    return (SM_FAIL);
	}
    }

    sprintf(path, "%s/data/%s", pghome, SJMETANAME);
    SJMetaVfd = PathNameOpenFile(path, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (SJMetaVfd < 0) {
	SJMetaVfd = PathNameOpenFile(path, O_RDWR, 0600);
	if (SJMetaVfd < 0) {

	    /* if we were initializing the metadata, note our surrender */
	    if (!metafound) {
		SJHeader->sjh_flags &= ~SJH_INITING;
		sjunwait_init();
	    }

	    return (SM_FAIL);
	}
    }

    /*
     *  Finally, if it's our responsibility to initialize the shared-memory
     *  cache metadata, then go do that.  sjcacheinit() will elog(FATAL, ...)
     *  if it can't initialize the cache, so we don't need to worry about
     *  a return value here.
     */

    if (initcache) {
	sjcacheinit();
    }

    return (SM_SUCCESS);
}

void
sjcacheinit()
{
    int nbytes, nread;
    int nentries;
    int nblocks;
    int i;
    SJCacheItem *cur;
    SJHashEntry *result;
    bool found;

    /* sanity check */
    if ((SJHeader->sjh_flags & SJH_INITED)
	|| !(SJHeader->sjh_flags & SJH_INITING)) {
	elog(FATAL, "sj cache header metadata corrupted.");
    }

    /* suck in the metadata */
    nbytes = SJCACHESIZE * sizeof(SJCacheItem);
    nread = FileRead(SJMetaVfd, SJCache, nbytes);

    /* be sure we got an integral number of entries */
    nentries = nread / sizeof(SJCacheItem);
    if ((nentries * sizeof(SJCacheItem)) != nread) {
	SJHeader->sjh_flags &= ~SJH_INITING;
	sjunwait_init();
	elog(FATAL, "sj cache metadata file corrupted.");
    }

    /* add each entry to the hash table, and set up link pointers */
    for (i = 0; i < nentries; i++) {
	cur = &(SJCache[i]);
	result = (SJHashEntry *) hash_search(SJCacheHT, &(cur->sjc_tag),
					     ENTER, &found);

	/*
	 *  If the hash table is corrupted, or the entry is already in the
	 *  table, then we're in trouble and need to surrender.  When we
	 *  release our initialization lock on the cache metadata, someone
	 *  else may come along later and try to reinitialize it.  They'll
	 *  fail, too, since we leave things trashed here.  Rather than try
	 *  to clean up, however, we assume that failing fast is the right
	 *  answer.  Since this is catastrophic, other backends probably
	 *  *should* fail.
	 */

	if (result == (SJHashEntry *) NULL) {
	    SJHeader->sjh_flags &= ~SJH_INITING;
	    sjunwait_init();
	    elog(FATAL, "sj cache hash table corrupted");
	}

	if (found) {
	    SJHeader->sjh_flags &= ~SJH_INITING;
	    sjunwait_init();
	    elog(FATAL, "duplicate group in sj cache file: <%d,%d,%d>",
		 cur->sjc_tag.sjct_dbid, cur->sjc_tag.sjct_relid, 
		 cur->sjc_tag.sjct_base);
	}

	/* store the group number for this key in the hash table */
	result->sjhe_groupno = i;

	/* link up lru list -- no info yet, so just link groups in order */
	cur->sjc_lruprev = i - 1;
	if (i == nentries - 1)
	    cur->sjc_lrunext = -1;
	else
	    cur->sjc_lrunext = i + 1;

	/* not waiting on I/O or anything */
	cur->sjc_gflags = SJG_CLEAR;

#ifdef HAS_TEST_AND_SET
	S_UNLOCK(&(cur->sjc_iolock));
#endif HAS_TEST_AND_SET
    }

    /* set up cache metadata header struct */
    SJHeader->sjh_nentries = nentries;

    if (nentries > 0)
	SJHeader->sjh_lruhead = 0;
    else
	SJHeader->sjh_lruhead = -1;

    SJHeader->sjh_lrutail = nentries - 1;
    SJHeader->sjh_flags = SJH_INITED;
}

/*
 *  sjunwait_init() -- Release initialization lock on the jukebox cache.
 *
 *	When we initialize the cache, we don't keep the cache semaphore
 *	locked.  Instead, we set a flag in the metadata to let other
 *	backends know that we're doing the initialization.  This lets
 *	others start running queries immediately, even if the cache is
 *	not yet populated.  If they want to look something up in the
 *	cache, they'll block on the flag we set, and wait for us to finish.
 *	If they don't need the jukebox, they can run unimpeded.  When we
 *	finish, we call sjunwait_init() to release the initialization lock
 *	that we hold during initialization.
 *
 *	When we do this, either the cache is properly initialized, or
 *	we detected some error we couldn't deal with.  In either case,
 *	we no longer need exclusive access to the cache metadata.
 */

void
sjunwait_init()
{
#ifdef HAS_TEST_AND_SET

    S_UNLOCK(&(SJHeader->sjh_initlock));

#else /* HAS_TEST_AND_SET */

    /* atomically V the semaphore once for every waiting process */
    SpinAcquire(SJCacheLock);
    IpcSemaphoreUnlock(SJWaitSemId, 0, *SJNWaiting);
    *SJNWaiting = 0;
    SpinRelease(SJCacheLock);

#endif /* HAS_TEST_AND_SET */
}

/*
 *  sjunwait_io() -- Release IO lock on the jukebox cache.
 *
 *	While we're doing IO on a particular group in the cache, any other
 *	process that wants to touch that group needs to wait until we're
 *	finished.  If we have TASlocks, then a wait lock appears on the
 *	group entry in the cache metadata.  Otherwise, we use the wait
 *	semaphore in the same way as for initialization, above.
 */

void
sjunwait_io(item)
    SJCacheItem *item;
{
#ifdef HAS_TEST_AND_SET
    S_UNLOCK(&(item->sjc_iolock));
#else /* HAS_TEST_AND_SET */

    /* atomically V the wait semaphore once for each sleeping process */
    SpinAcquire(SJCacheLock);

    if (*SJNWaiting > 0) {
	IpcSemaphoreUnlock(SJWaitSemId, 0, *SJNWaiting);
	*SJNWaiting = 0;
    }

    SpinRelease(SJCacheLock);
#endif /* HAS_TEST_AND_SET */
}

/*
 *  sjwait_init() -- Wait for cache initialization to complete.
 *
 *	This routine is called when we want to access jukebox cache metadata,
 *	but someone else is initializing it.  When we return, the init lock
 *	has been released and we can retry our access.  On entry, we must be
 *	holding the cache metadata lock.
 */

void
sjwait_init()
{
#ifdef HAS_TEST_AND_SET
    SpinRelease(SJCacheLock);
    S_LOCK(&(SJHeader->sjh_initlock));
    S_UNLOCK(&(SJHeader->sjh_initlock));
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    SpinRelease(SJCacheLock);
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */
}

/*
 *  sjwait_io() -- Wait for group IO to complete.
 *
 *	This routine is called when we discover that some other process is
 *	doing IO on a group in the cache that we want to use.  We need to
 *	wait for that IO to complete before we can use the group.  On entry,
 *	we must hold the cache metadata lock.  On return, we don't hold that
 *	lock, and the IO completed.  We can retry our access.
 */

void
sjwait_io(item)
    SJCacheItem *item;
{
#ifdef HAS_TEST_AND_SET
    SpinRelease(SJCacheLock);
    S_LOCK(&(item->sjc_iolock));
    S_UNLOCK(&(item->sjc_iolock));
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    SpinRelease(SJCacheLock);
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */
}

/*
 *  sjshutdown() -- shut down the jukebox storage manager.
 *
 *	We want to close the cache and metadata files, release all our open
 *	jukebox connections, and let the caller know we're done.
 */

int
sjshutdown()
{
    FileClose(SJCacheVfd);
    FileClose(SJMetaVfd);

    return (SM_SUCCESS);
}

/*
 *  sjcreate() -- Create the requested relation on the jukebox.
 *
 *	Creating a new relation requires us to make a new cache group,
 *	fill in the descriptor page, make sure everything is on disk,
 *	and create the new relation file to store the last page of data
 *	on magnetic disk.
 */

int
sjcreate(reln)
    Relation reln;
{
    SJCacheItem *item;
    SJGroupDesc *group;
    SJHashEntry *entry;
    File vfd;
    int grpno;
    bool found;
    int i;
    char path[SJPATHLEN];

    /*
     *  If the cache is in the process of being initialized, then we need
     *  to wait for initialization to complete.  If the cache is not yet
     *  initialized, and no one else is doing it, then we need to initialize
     *  it ourselves.  Sjwait_init() or sj_init() will release the cache
     *  lock for us.
     */

    SpinAcquire(SJCacheLock);
    if (!(SJHeader->sjh_flags & SJH_INITED)) {
	if (SJHeader->sjh_flags & SJH_INITING) {
	    sjwait_init();
	} else {
	    sjinit();
	}
	return (sjcreate(reln));
    }

    /*
     *  By here, cache is initialized and we have exclusive access to
     *  metadata.  Allocate a group in the cache.
     */

    grpno = sjallocgrp();
    item = &SJCache[grpno];

    if (reln->rd_rel->relisshared)
	item->sjc_tag.sjct_dbid = (ObjectId) 0;
    else
	item->sjc_tag.sjct_dbid = MyDatabaseId;

    item->sjc_tag.sjct_relid = (ObjectId) reln->rd_id;
    item->sjc_tag.sjct_base = (BlockNumber) 0;

    entry = (SJHashEntry *) hash_search(SJCacheHT, item, ENTER, &found);

    if (entry == (SJHashEntry *) NULL) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "jukebox cache hash table corrupt.");
    } else if (found) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "Attempt to create existing relation -- impossible");
    }

    entry->sjhe_groupno = grpno;
    item->sjc_gflags = SJG_IOINPROG;
#ifdef HAS_TEST_AND_SET
    S_LOCK(item->sjc_iolock);
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */

    /* okay, cache and group are set up -- safe to release excl lock now */
    SpinRelease(SJCacheLock);

    /* set flags on item, initialize group descriptor block */
    for (i = 0; i < SJGRPSIZE; i++)
	item->sjc_flags[i] = SJC_MISSING;

    /* should be smarter and only bzero what we need to */
    bzero(SJCacheBuf, SJBufSize);

    group = (SJGroupDesc *) (&SJCacheBuf[0]);
    group->sjgd_magic = SJGDMAGIC;
    group->sjgd_version = SJGDVERSION;

    if (reln->rd_rel->relisshared) {
	group->sjgd_dbid = (ObjectId) 0;
    } else {
	strncpy(&(group->sjgd_dbname.data[0]),
		&(MyDatabaseName->data[0]),
		sizeof(NameData));
	group->sjgd_dbid = (ObjectId) MyDatabaseId;
    }

    strncpy(&(group->sjgd_relname.data[0]),
	    &(reln->rd_rel->relname.data[0]),
	    sizeof(NameData));
    group->sjgd_relid = reln->rd_id;
    group->sjgd_relblkno = 0;
    group->sjgd_jboffset = -1;
    group->sjgd_extentsz = -1;
    item->sjc_oid = group->sjgd_groupoid = newoid();

    if (sjwritegrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	return (-1);
    }

    /* can now release i/o lock on the item we just added */
    sjunwait_io(item);

    /* last thing to do is to create the mag-disk file to hold last page */
    if (group->sjgd_dbid == (ObjectId) 0)
	sprintf(path, "../%16s", &(reln->rd_rel->relname.data[0]));
    else
	sprintf(path, "%16s", &(reln->rd_rel->relname.data[0]));

    vfd = FileNameOpenFile(path, O_CREAT|O_RDWR|O_EXCL, 0600);

    return (vfd);
}

/*
 *  sjallocgrp() -- Allocate a new group in the cache for use by some
 *		    relation.
 *
 *	If there are any unused slots in the cache, we just return one
 *	of those.  Otherwise, we need to kick out the least-recently-used
 *	group and make room for another.
 *
 *	On entry, we hold the cache metadata lock.  On exit, we still hold
 *	it.  In between, we may release it in order to do I/O on the cache
 *	group we're kicking out, if indeed we're doing that.
 */

int
sjallocgrp()
{
    int grpno;
    SJCacheItem *item;

    /* see if we can avoid doing any work here */
    if (SJHeader->sjh_nentries < SJCACHESIZE) {
	grpno = SJHeader->sjh_nentries;
	SJHeader->sjh_nentries++;
    } else {
	/* XXX here, need to kick someone out */
	elog(FATAL, "hey mao, your cache appears to be full.");
    }

    item = &SJCache[grpno];

    item->sjc_lruprev = -1;
    item->sjc_lrunext = SJHeader->sjh_lruhead;
    if (SJHeader->sjh_lruhead == -1) {
	SJHeader->sjh_lruhead = grpno;
	SJHeader->sjh_lrutail = grpno;
    } else {
	SJCache[SJHeader->sjh_lruhead].sjc_lruprev = grpno;
	SJHeader->sjh_lruhead = grpno;
    }

    return (grpno);
}

int
sjwritegrp(item, grpno)
    SJCacheItem *item;
    int grpno;
{
    long seekpos;
    long loc;
    int nbytes, i;
    char *buf;

    /* first update the metadata file */
    seekpos = grpno * sizeof(*item);
    if ((loc = FileSeek(SJMetaVfd, seekpos, L_SET)) != seekpos)
	return (SM_FAIL);

    nbytes = sizeof(*item);
    buf = (char *) item;
    while (nbytes > 0) {
	i = FileWrite(SJMetaVfd, buf, nbytes);
	if (i < 0)
	    return (SM_FAIL);
	nbytes -= i;
	buf += i;
    }

    FileSync(SJMetaVfd);

    /* now update the cache file */
    seekpos = grpno * SJBufSize;
    if ((loc = FileSeek(SJCacheVfd, seekpos, L_SET)) != seekpos)
	return (SM_FAIL);

    nbytes = SJBufSize;
    buf = &(SJCacheBuf[0]);
    while (nbytes > 0) {
	i = FileWrite(SJCacheVfd, buf, nbytes);
	if (i < 0)
	    return (SM_FAIL);
	nbytes -= i;
	buf += i;
    }

    FileSync(SJCacheVfd);

    return (SM_SUCCESS);
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

/*
 *  SJShmemSize() -- Declare amount of shared memory we require.
 *
 *	The shared memory initialization code creates a block of shared
 *	memory exactly big enough to hold all the structures it needs to.
 *	This routine declares how much space the Sony jukebox cache will
 *	use.
 */

int
SJShmemSize()
{
    int size;
    int nbuckets;
    int nsegs;

    nbuckets = 1 << my_log2((SJCACHESIZE - 1) / DEF_FFACTOR + 1);
    nsegs = 1 << my_log2((nbuckets - 1) / DEF_SEGSIZE + 1);

    /* size of shared memory binding table */
    size = my_log2(BTABLE_SIZE) + sizeof(HHDR)
            + DEF_SEGSIZE * sizeof(SEGMENT) + BUCKET_ALLOC_INCR *
            (sizeof(BUCKET_INDEX) + BTABLE_KEYSIZE + BTABLE_DATASIZE);

    /* size of cache metadata */
    size += ((SJCACHESIZE + 1) * sizeof(SJCacheItem)) + sizeof(SJCacheHeader);
#ifndef HAS_TEST_AND_SET
    size += sizeof(*SJNWaiting);
#endif /* ndef HAS_TEST_AND_SET */

    /* size of hash table */
    size += my_log2(SJCACHESIZE) + sizeof(HHDR)
          + nsegs * DEF_SEGSIZE * sizeof(SEGMENT)
          + (int)ceil((double)SJCACHESIZE/BUCKET_ALLOC_INCR)*BUCKET_ALLOC_INCR*
             (sizeof(BUCKET_INDEX) + sizeof(SJHashEntry));

    return (size);
}

_sjdump()
{
    int i, j;
    int nentries;
    SJCacheItem *item;

    SpinAcquire(SJCacheLock);

    nentries = SJHeader->sjh_nentries;

    printf("jukebox cache metdata: size %d, %d entries, lru head %d tail %d",
	   SJCACHESIZE, nentries, SJHeader->sjh_lruhead,
	   SJHeader->sjh_lrutail);
    if (SJHeader->sjh_flags & SJH_INITING)
	printf(", INITING");
    if (SJHeader->sjh_flags & SJH_INITED)
	printf(", INITED");
    printf("\n");

    for (i = 0; i < nentries; i++) {
	item = &SJCache[i];
	printf("    [%2d] <%ld,%ld,%ld> next %d prev %d flags %s oid %ld\n",
	       i, item->sjc_tag.sjct_dbid, item->sjc_tag.sjct_relid,
	       item->sjc_tag.sjct_base, item->sjc_lrunext,
	       item->sjc_lruprev,
	       (item->sjc_gflags & SJG_IOINPROG ? "IO_IN_PROG" : "CLEAR"),
	       item->sjc_oid);
	printf("         ");
	for (j = 0; j < SJGRPSIZE; j++) {
	    printf("[%d %c%c%c]", j,
	    	   (item->sjc_flags[j] & SJC_DIRTY ? 'd' : '-'),
	    	   (item->sjc_flags[j] & SJC_MISSING ? 'm' : '-'),
	    	   (item->sjc_flags[j] & SJC_ONPLATTER ? 'o' : '-'));
	}
	printf("\n");
    }

    SpinRelease(SJCacheLock);
}

#else /* SONY_JUKEBOX */

#include "machine.h"
#include "storage/smgr.h"
#include "utils/rel.h"

/*
 *  If there's no sony jukebox, we just use stub routines.
 */

int
sjinit(unused)
    int unused;
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

#endif /* SONY_JUKEBOX */
