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

#include "access/relscan.h"
#include "access/heapam.h"

#include "catalog/pg_platter.h"
#include "catalog/pg_plmap.h"
#include "catalog/pg_proc.h"

#include "storage/sj.h"

RcsId("$Header$");

/* globals used in this file */
SPINLOCK		SJCacheLock;	/* lock for cache metadata */
static ObjectId		MyDatabaseId;	/* OID of database we have open */
static Name		MyDatabaseName;	/* name of database we have open */

static File		SJCacheVfd;	/* vfd for cache data file */
static File		SJMetaVfd;	/* vfd for cache metadata file */
static SJCacheHeader	*SJHeader;	/* pointer to cache header in shmem */
static HTAB		*SJCacheHT;	/* pointer to hash table in shmem */
static SJCacheItem	*SJCache;	/* pointer to cache metadata in shmem */
static SJNBlock		*SJNBlockList;	/* linked list of nblocks by relid */

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

/* used in sj.c, pgjb.c */
int		SJBufSize = ((BLCKSZ * SJGRPSIZE) + JBBLOCKSZ);

/* routines declared in this file */
static void		_sjcacheinit();
static void		_sjwait_init();
static void		_sjunwait_init();
static void		_sjwait_io();
static void		_sjunwait_io();
static void		_sjtouch();
static void		_sjunpin();
static void		_sjregister();
static void		_sjregnblocks();
static int		_sjfindnblocks();
static int		_sjwritegrp();
static int		_sjreadgrp();
static Form_pg_plmap	_sjchoose();
static SJCacheItem	*_sjallocgrp();
static SJCacheItem	*_sjfetchgrp();
static void		_sjdump();

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

    /*
     *  Okay, all our shared memory pointers are set up.  If we did not
     *  find the cache metadata entries in shared memory, or if the cache
     *  has not been initialized from disk, initialize it in this backend.
     */

    if (!metafound || !(SJHeader->sjh_flags & (SJH_INITING|SJH_INITED))) {
	initcache = true;
	bzero((char *) cachesave, metasize);
	SJHeader->sjh_flags = SJH_INITING;
#ifdef HAS_TEST_AND_SET
	S_LOCK(&(SJHeader->sjh_initlock));
#else /* HAS_TEST_AND_SET */
	IpcSemaphoreLock(SJWaitSemId, 0, 1);
	*SJNWaiting = 1;
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
		_sjunwait_init();
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
		_sjunwait_init();
	    }

	    return (SM_FAIL);
	}
    }

    /* haven't computed block counts for any relations yet */
    SJNBlockList = (SJNBlock *) NULL;

    /*
     *  If it's our responsibility to initialize the shared-memory cache
     *  metadata, then go do that.  Sjcacheinit() will elog(FATAL, ...) if
     *  it can't initialize the cache, so we don't need to worry about a
     *  return value here.
     */

    if (initcache) {
	_sjcacheinit();
    }

    /*
     *  Finally, we need to initialize the data structures we use for
     *  communicating with the jukebox.
     */

    if (pgjb_init() == SM_FAIL)
	return (SM_FAIL);

    return (SM_SUCCESS);
}

static void
_sjcacheinit()
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
	_sjunwait_init();
	elog(FATAL, "sj cache metadata file corrupted.");
    }

    /* add each entry to the hash table, and set up link pointers */
    for (i = 0; i < nentries; i++) {
	cur = &(SJCache[i]);
	result = (SJHashEntry *) hash_search(SJCacheHT, &(cur->sjc_tag),
					     HASH_ENTER, &found);

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
	    _sjunwait_init();
	    elog(FATAL, "sj cache hash table corrupted");
	}

	if (found) {
	    SJHeader->sjh_flags &= ~SJH_INITING;
	    _sjunwait_init();
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

	/* no io in progress */
	cur->sjc_gflags &= ~SJC_IOINPROG;
	cur->sjc_refcount = 0;

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
 *  _sjunwait_init() -- Release initialization lock on the jukebox cache.
 *
 *	When we initialize the cache, we don't keep the cache semaphore
 *	locked.  Instead, we set a flag in the metadata to let other
 *	backends know that we're doing the initialization.  This lets
 *	others start running queries immediately, even if the cache is
 *	not yet populated.  If they want to look something up in the
 *	cache, they'll block on the flag we set, and wait for us to finish.
 *	If they don't need the jukebox, they can run unimpeded.  When we
 *	finish, we call _sjunwait_init() to release the initialization lock
 *	that we hold during initialization.
 *
 *	When we do this, either the cache is properly initialized, or
 *	we detected some error we couldn't deal with.  In either case,
 *	we no longer need exclusive access to the cache metadata.
 */

static void
_sjunwait_init()
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
    item->sjc_gflags &= ~SJC_IOINPROG;

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
 *  _sjwait_init() -- Wait for cache initialization to complete.
 *
 *	This routine is called when we want to access jukebox cache metadata,
 *	but someone else is initializing it.  When we return, the init lock
 *	has been released and we can retry our access.  On entry, we must be
 *	holding the cache metadata lock.
 */

static void
_sjwait_init()
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
 *  _sjwait_io() -- Wait for group IO to complete.
 *
 *	This routine is called when we discover that some other process is
 *	doing IO on a group in the cache that we want to use.  We need to
 *	wait for that IO to complete before we can use the group.  On entry,
 *	we must hold the cache metadata lock.  On return, we don't hold that
 *	lock, and the IO completed.  We can retry our access.
 */

static void
_sjwait_io(item)
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
	    _sjwait_init();
	} else {
	    sjinit();
	}
	return (sjcreate(reln));
    }

    /*
     *  By here, cache is initialized and we have exclusive access to
     *  metadata.  Allocate a group in the cache.
     */

    item = _sjallocgrp(&grpno);

    if (reln->rd_rel->relisshared)
	item->sjc_tag.sjct_dbid = (ObjectId) 0;
    else
	item->sjc_tag.sjct_dbid = MyDatabaseId;

    item->sjc_tag.sjct_relid = (ObjectId) reln->rd_id;
    item->sjc_tag.sjct_base = (BlockNumber) 0;

    entry = (SJHashEntry *) hash_search(SJCacheHT, item, HASH_ENTER, &found);

    if (entry == (SJHashEntry *) NULL) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "jukebox cache hash table corrupt.");
    } else if (found) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "Attempt to create existing relation -- impossible");
    }

    entry->sjhe_groupno = grpno;
    item->sjc_gflags |= SJC_IOINPROG;
#ifdef HAS_TEST_AND_SET
    SpinRelease(SJCacheLock);
    S_LOCK(item->sjc_iolock);
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    SpinRelease(SJCacheLock);
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */

    /* set flags on item, initialize group descriptor block */
    item->sjc_gflags = SJC_DIRTY;
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
    item->sjc_oid = group->sjgd_groupoid = newoid();

    /*
     *  Record the presence of the new extent in the system catalogs.  The
     *  plid, jboffset, and extentsz fields are filled in by _sjregister()
     *  or the routines that it calls.  Note that we do not force the new
     *  group descriptor block all the way to the optical platter here.
     *  We do decide where to place it, however, and must go to a fair amount
     *  of trouble elsewhere in the code to avoid allocating the same extent
     *  to a different relation, or block within the same relation.
     */

    _sjregister(item, group);

    /*
     *  Write the new group cache entry to disk.  Sjwritegrp() knows where
     *  the cache buffer begins, and forces out the group descriptor we
     *  just set up.
     */

    if (_sjwritegrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	return (-1);
    }

    _sjregnblocks(reln->rd_id, 0);

    /* can now release i/o lock on the item we just added */
    sjunwait_io(item);

    /* no longer need the reference */
    _sjunpin(item);

    /* last thing to do is to create the mag-disk file to hold last page */
    if (group->sjgd_dbid == (ObjectId) 0)
	strcpy(path, "../");
    else
	path[0] = '\0';

    strncpy(path, &(reln->rd_rel->relname.data[0]), sizeof(NameData));

    vfd = FileNameOpenFile(path, O_CREAT|O_RDWR|O_EXCL, 0600);

    return (vfd);
}

/*
 *  _sjregister() -- Make catalog entry for a new extent
 *
 *	When we create a new jukebox relation, or when we add a new extent
 *	to an existing relation, we need to make the appropriate entry in
 *	pg_plmap().  This routine does that.
 *
 *	On entry, we have item pinned; on exit, it's still pinned, and the
 *	system catalogs have been updated to reflect the presence of the
 *	new extent.
 */

static void
_sjregister(item, group)
    SJCacheItem *item;
    SJGroupDesc *group;
{
    Relation plmap;
    ObjectId plid;
    Form_pg_plmap plmdata;
    HeapTuple plmtup;

    /*
     *  Choose a platter to put the new extent on.  This returns a filled-in
     *  pg_plmap tuple data part to insert into the system catalogs.  The
     *  choose routine also figures out where to place the extent on the
     *  platter.
     *
     *	Sjchoose() palloc's and fills in plmdata; we free it later in this
     *  routine.
     */

    plmdata = _sjchoose(item);

    /* record plid, offset, extent size for caller */
    group->sjgd_plid = plmdata->plid;
    group->sjgd_jboffset = plmdata->ploffset;
    group->sjgd_extentsz = plmdata->plextentsz;

    plmtup = (HeapTuple) heap_addheader(Natts_pg_plmap,
					sizeof(FormData_pg_plmap),
					(char *) plmdata);

    /* clean up the memory that heap_addheader() palloc()'ed for us */
    plmtup->t_oid = InvalidObjectId;
    bzero((char *) &(plmtup->t_chain), sizeof(plmtup->t_chain));

    /* open the relation and lock it */
    plmap = heap_openr(Name_pg_plmap);
    RelationSetLockForWrite(plmap);

    /* insert the new catalog tuple */
    heap_insert(plmap, plmtup, (double *) NULL);

    /* done */
    heap_close(plmap);

    /* be tidy */
    pfree((char *) plmtup);
    pfree((char *) plmdata);
}

/*
 *  _sjchoose() -- Choose a platter to receive a new extent.
 *
 *	For now, this makes a really stupid choice.  Need to think about
 *	the right way to go about this.
 */

static Form_pg_plmap
_sjchoose(item)
    SJCacheItem *item;
{
    Relation plat;
    TupleDescriptor platdesc;
    HeapScanDesc platscan;
    HeapTuple plattup;
    Buffer buf;
    Form_pg_plmap plmdata;
    ObjectId plid;
    Datum d;
    Name platname;
    char *plname;
    bool isnull;

    /* allocate the tuple form */
    plmdata = (Form_pg_plmap) palloc(sizeof(FormData_pg_plmap));
    plname = (char *) palloc(sizeof(NameData) + 1);

    plat = heap_openr(Name_pg_platter);
    platdesc = RelationGetTupleDescriptor(plat);
    platscan = heap_beginscan(plat, false, NowTimeQual, 0, NULL);
    plattup = heap_getnext(platscan, false, &buf);

    if (!HeapTupleIsValid(plattup))
	elog(WARN, "_sjchoose: no platters in pg_plmap");

    /* get platter OID, name */
    plmdata->plid = plattup->t_oid;
    d = (Datum) heap_getattr(plattup, buf, Anum_pg_platter_plname,
			     platdesc, &isnull);
    platname = DatumGetName(d);
    strncpy(plname, &(platname->data[0]), sizeof(NameData));
    plname[sizeof(NameData)] = '\0';

    /* done */
    ReleaseBuffer(buf);
    heap_endscan(platscan);
    heap_close(plat);

    /* init the rest of the fields */
    plmdata->pldbid = item->sjc_tag.sjct_dbid;
    plmdata->plrelid = item->sjc_tag.sjct_relid;
    plmdata->plblkno = item->sjc_tag.sjct_base;
    plmdata->plextentsz = SJEXTENTSZ;
    plmdata->ploffset = pgjb_offset(plname, plmdata->plid, plmdata->plextentsz);

    /* save platter name, id, offset in item */
    bcopy(&(item->sjc_plname.data[0]), plname, sizeof(NameData));
    item->sjc_plid = plmdata->plid;
    item->sjc_jboffset = plmdata->ploffset;

    return (plmdata);
}

/*
 *  _sjallocgrp() -- Allocate a new group in the cache for use by some
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

static SJCacheItem *
_sjallocgrp(grpno)
    int *grpno;
{
    SJCacheItem *item;

    /* see if we can avoid doing any work here */
    if (SJHeader->sjh_nentries < SJCACHESIZE) {
	*grpno = SJHeader->sjh_nentries;
	SJHeader->sjh_nentries++;
    } else {
	/* XXX here, need to kick someone out */
	elog(FATAL, "hey mao, your cache appears to be full.");
    }

    item = &SJCache[*grpno];

    item->sjc_lruprev = -1;
    item->sjc_lrunext = SJHeader->sjh_lruhead;
    if (SJHeader->sjh_lruhead == -1) {
	SJHeader->sjh_lruhead = *grpno;
	SJHeader->sjh_lrutail = *grpno;
    } else {
	SJCache[SJHeader->sjh_lruhead].sjc_lruprev = *grpno;
	SJHeader->sjh_lruhead = *grpno;
    }

    /* bump ref count */
    _sjtouch(item);

    return (item);
}

static SJCacheItem *
_sjfetchgrp(dbid, relid, blkno, grpno)
    ObjectId dbid;
    ObjectId relid;
    int blkno;
    int *grpno;
{
    SJCacheItem *item;
    SJHashEntry *entry;
    bool found;
    SJCacheTag tag;

    SpinAcquire(SJCacheLock);

    tag.sjct_dbid = dbid;
    tag.sjct_relid = relid;
    tag.sjct_base = blkno;

    entry = (SJHashEntry *) hash_search(SJCacheHT, &tag, HASH_FIND, &found);

    if (entry == (SJHashEntry *) NULL) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "_sjfetchgrp: hash table corrupted");
    }

    if (found) {
	*grpno = entry->sjhe_groupno;
	item = &(SJCache[*grpno]);

	if (item->sjc_gflags & SJC_IOINPROG) {
	    _sjwait_io(item);
	    return (_sjfetchgrp(dbid, relid, blkno));
	}

	_sjtouch(item, *grpno);

	SpinRelease(SJCacheLock);
    } else {
	SpinRelease(SJCacheLock);
	elog(FATAL, "_sjfetchgrp: hey mao: can't find <%d,%d,%d>",
		    dbid, relid, blkno);
    }

    return (item);
}

static void
_sjtouch(item, grpno)
    SJCacheItem *item;
    int grpno;
{
    /* first bump the ref count */
    (item->sjc_refcount)++;

    /* now move it to the top of the lru list */
    if (item->sjc_lruprev == -1)
	return;

    if (item->sjc_lrunext == -1)
	SJHeader->sjh_lrutail = item->sjc_lruprev;
    else
	SJCache[item->sjc_lrunext].sjc_lruprev = item->sjc_lruprev;

    SJCache[item->sjc_lruprev].sjc_lrunext = item->sjc_lrunext;

    item->sjc_lruprev = -1;
    item->sjc_lrunext = SJHeader->sjh_lruhead;
    SJHeader->sjh_lruhead = grpno;
}

static void
_sjunpin(item)
    SJCacheItem *item;
{
    SpinAcquire(SJCacheLock);
    if (item->sjc_refcount <= 0)
	elog(FATAL, "_sjunpin: illegal reference count");
    (item->sjc_refcount)--;
    SpinRelease(SJCacheLock);
}

static int
_sjwritegrp(item, grpno)
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

static int
_sjreadgrp(item, grpno)
    SJCacheItem *item;
    int grpno;
{
    long seekpos;
    long loc;
    int nbytes, i;
    char *buf;
    SJGroupDesc *gdesc;

    /* get the group from the cache file */
    seekpos = grpno * SJBufSize;
    if ((loc = FileSeek(SJCacheVfd, seekpos, L_SET)) != seekpos) {
	elog(NOTICE, "_sjreadgrp: cannot seek");
	return (SM_FAIL);
    }

    nbytes = SJBufSize;
    buf = &(SJCacheBuf[0]);
    while (nbytes > 0) {
	i = FileRead(SJCacheVfd, buf, nbytes);
	if (i < 0) {
	    elog(NOTICE, "_sjreadgrp: read failed");
	    return (SM_FAIL);
	}
	nbytes -= i;
	buf += i;
    }

    gdesc = (SJGroupDesc *) &(SJCacheBuf[0]);

    if (gdesc->sjgd_magic != SJGDMAGIC
	|| gdesc->sjgd_version != SJGDVERSION
	|| gdesc->sjgd_groupoid != item->sjc_oid) {
	elog(NOTICE, "_sjreadgrp: trashed cache");
	return (SM_FAIL);
    }

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
    SJCacheItem *item;
    SJHashEntry *entry;
    SJCacheTag tag;
    int grpno;
    int nblocks;
    int base;
    int offset;
    int blkno;
    bool found;

    RelationSetLockForExtend(reln);
    nblocks = sjnblocks(reln);
    base = nblocks / SJGRPSIZE;

    if (reln->rd_rel->relisshared)
	tag.sjct_dbid = (ObjectId) 0;
    else
	tag.sjct_dbid = MyDatabaseId;

    tag.sjct_relid = reln->rd_id;
    tag.sjct_base = base;

    SpinAcquire(SJCacheLock);

    entry = (SJHashEntry *) hash_search(SJCacheHT, &tag, HASH_FIND, &found);

    if (entry == (SJHashEntry *) NULL) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "sjextend: cache hash table corrupted");
    }

    if (!found) {
	SpinRelease(SJCacheLock);
	elog(WARN, "sjextend:  hey mao:  your group is missing.");
    }

    grpno = entry->sjhe_groupno;
    item = &SJCache[grpno];

    _sjtouch(item, grpno);

    for (blkno = 0; blkno < SJGRPSIZE; blkno++) {
	if (item->sjc_flags[blkno] & SJC_MISSING) {
	    item->sjc_flags[blkno] &= ~SJC_MISSING;
	    item->sjc_flags[blkno] |= SJC_DIRTY;
	    break;
	}
    }

    if (blkno == SJGRPSIZE) {
	SpinRelease(SJCacheLock);
	elog(WARN, "sjextend:  hey mao:  no missing blocks to extend");
    }

    item->sjc_gflags |= SJC_IOINPROG;

#ifdef HAS_TEST_AND_SET
    SpinRelease(SJCacheLock);
    S_LOCK(item->sjc_iolock);
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    SpinRelease(SJCacheLock);
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */

    if (_sjreadgrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	return (SM_FAIL);
    }

    offset = ((blkno % SJGRPSIZE) * BLCKSZ) + JBBLOCKSZ;
    bcopy(buffer, &(SJCacheBuf[offset]), BLCKSZ);

    if (_sjwritegrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	return (SM_FAIL);
    }

    sjunwait_io(item);
    _sjunpin(item);

    _sjregnblocks(reln->rd_id, ++nblocks);

    return (SM_SUCCESS);
}

int
sjopen(reln)
    Relation reln;
{
    char *path;
    int fd;
    extern char *relpath();

    path = relpath(&(reln->rd_rel->relname.data[0]));

    fd = FileNameOpenFile(path, O_RDWR, 0666);

    return (fd);
}

int
sjclose(reln)
    Relation reln;
{
    FileClose(reln->rd_fd);

    return (SM_SUCCESS);
}

int
sjread(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    SJCacheItem *item;
    ObjectId reldbid;
    int offset;
    int grpno;

    if (reln->rd_rel->relisshared)
	reldbid = (ObjectId) 0;
    else
	reldbid = MyDatabaseId;

    item = _sjfetchgrp(reldbid, reln->rd_id, blocknum / SJGRPSIZE, &grpno);

    /* shd expand _sjfetchgrp() inline to avoid extra semop()s */
    SpinAcquire(SJCacheLock);

    item->sjc_gflags |= SJC_IOINPROG;

#ifdef HAS_TEST_AND_SET
    SpinRelease(SJCacheLock);
    S_LOCK(item->sjc_iolock);
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    SpinRelease(SJCacheLock);
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */

    if (_sjreadgrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	return (SM_FAIL);
    }

    offset = ((blocknum % SJGRPSIZE) * BLCKSZ) + JBBLOCKSZ;
    bcopy(&(SJCacheBuf[offset]), buffer, BLCKSZ);

    sjunwait_io(item);
    _sjunpin(item);

    return (SM_SUCCESS);
}

int
sjwrite(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    SJCacheItem *item;
    ObjectId reldbid;
    int offset;
    int grpno;
    int which;

    if (reln->rd_rel->relisshared)
	reldbid = (ObjectId) 0;
    else
	reldbid = MyDatabaseId;

    item = _sjfetchgrp(reldbid, reln->rd_id, blocknum / SJGRPSIZE, &grpno);

    /* shd expand _sjfetchgrp() inline to avoid extra semop()s */
    SpinAcquire(SJCacheLock);

    which = blocknum % SJGRPSIZE;

    if (item->sjc_flags[which] & SJC_ONPLATTER) {
	SpinRelease(SJCacheLock);
	_sjunpin(item);
	elog(WARN, "sjwrite: optical platters are write-once, cannot rewrite");
    }

    item->sjc_flags[which] &= ~SJC_MISSING;
    item->sjc_flags[which] |= SJC_DIRTY;
    item->sjc_gflags |= SJC_IOINPROG;

#ifdef HAS_TEST_AND_SET
    SpinRelease(SJCacheLock);
    S_LOCK(item->sjc_iolock);
#else /* HAS_TEST_AND_SET */
    (*SJNWaiting)++;
    SpinRelease(SJCacheLock);
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */

    if (_sjreadgrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    offset = (which * BLCKSZ) + JBBLOCKSZ;
    bcopy(buffer, &(SJCacheBuf[offset]), BLCKSZ);

    if (_sjwritegrp(item, grpno) == SM_FAIL) {
	sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    sjunwait_io(item);
    _sjunpin(item);

    return (SM_SUCCESS);
}

int
sjflush(reln, blocknum, buffer)
    Relation reln;
    BlockNumber blocknum;
    char *buffer;
{
    return (sjwrite(reln, blocknum, buffer));
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

/*
 *  sjnblocks() -- Return the number of blocks that appear in this relation.
 *
 *	This is an unbelievably expensive operation.  We should cache this
 *	number in shared memory once we compute it.
 */

int
sjnblocks(reln)
    Relation reln;
{
    Relation plmap;
    TupleDescriptor plmdesc;
    HeapScanDesc plmscan;
    HeapTuple plmtup;
    Buffer buf;
    ObjectId reldbid;
    Datum d;
    Boolean n;
    int32 v;
    int32 maxblkno;
    int i;
    int grpno;
    SJCacheItem *item;
    ScanKeyEntryData plmkey[2];

    /* see if we've already figured this out */
    if ((maxblkno = _sjfindnblocks(reln->rd_id)) >= 0)
	return (maxblkno);

    if (reln->rd_rel->relisshared)
	reldbid = (ObjectId) 0;
    else
	reldbid = MyDatabaseId;

    ScanKeyEntryInitialize(&plmkey[0], 0x0, Anum_pg_plmap_pldbid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(reldbid));

    ScanKeyEntryInitialize(&plmkey[1], 0x0, Anum_pg_plmap_plrelid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(reln->rd_id));

    plmap = heap_openr(Name_pg_plmap);
    plmdesc = RelationGetTupleDescriptor(plmap);
    plmscan = heap_beginscan(plmap, false, NowTimeQual, 2, &plmkey[0]);

    maxblkno = 0;

    /*
     *  Find the highest-numbered group in the relation by scanning
     *  pg_plmap.
     */

    while (HeapTupleIsValid(plmtup = heap_getnext(plmscan, false,
						  (Buffer *) NULL))) {
	d = (Datum) heap_getattr(plmtup, InvalidBuffer, Anum_pg_plmap_plblkno,
				 plmdesc, &n);
	v = DatumGetInt32(d);
	if (v > maxblkno)
	    maxblkno = v;
    }

    heap_endscan(plmscan);
    heap_close(plmap);

    /*
     *  Get the highest-numbered group, and count the number of blocks
     *  that are actually present in the group.
     */

    item = _sjfetchgrp(reldbid, reln->rd_id, maxblkno, &grpno);

    for (i = 0; i < SJGRPSIZE; i++) {
	if (item->sjc_flags[i] & SJC_MISSING)
	    break;
    }

    /* don't need the reference anymore */
    _sjunpin(item);

    /* adjust the count of blocks and remember it for next time */
    maxblkno += i;
    _sjregnblocks(reln->rd_id, maxblkno);

    return(maxblkno);
}

/*
 *  _sjfindnblocks() -- Find a precomputed block count for the given relid.
 *
 *	We should really do something smarter here.
 */

static int
_sjfindnblocks(relid)
    ObjectId relid;
{
    SJNBlock *l;

    l = SJNBlockList;

    while (l != (SJNBlock *) NULL) {
	if (l->sjnb_relid == relid)
	    return (l->sjnb_nblocks);

	l = l->sjnb_next;
    }

    return (-1);
}

/*
 *  _sjregnblocks() -- Remember the count of blocks for this relid.
 *
 *	Should really do something smarter here.
 */

static void
_sjregnblocks(relid, nblocks)
    ObjectId relid;
    int nblocks;
{
    SJNBlock *l;

    l = SJNBlockList;

    /* overwrite old value, if one exists */
    while (l != (SJNBlock *) NULL) {

	if (l->sjnb_relid == relid) {
	    l->sjnb_nblocks = nblocks;
	    return;
	}

	l = l->sjnb_next;
    }

    /* otherwise, allocate new slot and write new value */
    l = (SJNBlock *) palloc(sizeof(SJNBlock));
    l->sjnb_relid = relid;
    l->sjnb_nblocks = nblocks;
    l->sjnb_next = SJNBlockList;
    SJNBlockList = l;
}
int
sjcommit()
{
    /* XXX should free the list, but it's in the wrong mcxt */
    SJNBlockList = (SJNBlock *) NULL;

    return (SM_SUCCESS);
}

int
sjabort()
{
    /* XXX should free the list, but it's in the wrong mcxt */
    SJNBlockList = (SJNBlock *) NULL;

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

    /* size of cache metadata */
    size = ((SJCACHESIZE + 1) * sizeof(SJCacheItem)) + sizeof(SJCacheHeader);
#ifndef HAS_TEST_AND_SET
    size += sizeof(*SJNWaiting);
#endif /* ndef HAS_TEST_AND_SET */

    /* size of hash table */
    nbuckets = 1 << (int)my_log2((SJCACHESIZE - 1) / DEF_FFACTOR + 1);
    nsegs = 1 << (int)my_log2((nbuckets - 1) / DEF_SEGSIZE + 1);
    size += my_log2(SJCACHESIZE) + sizeof(HHDR)
          + nsegs * DEF_SEGSIZE * sizeof(SEGMENT)
          + (int)ceil((double)SJCACHESIZE/BUCKET_ALLOC_INCR)*BUCKET_ALLOC_INCR*
             (sizeof(BUCKET_INDEX) + sizeof(SJHashEntry));

    /* count shared memory required for jukebox state */
    size += JBShmemSize();

    return (size);
}

/*
 *  sjmaxseg() -- Find highest segment number occupied by platter id plid
 *		  in the on-disk cache.
 *
 *	This routine is called from _pgjb_findoffset().  On entry here,
 *	we hold JBSpinLock, but not SJCacheLock.  We do something a little
 *	dangerous here; we trust the group descriptor metadata that is in
 *	shared memory to reflect accurately the state of the actual cache
 *	file.  This isn't so bad; if there's an inconsistency, there are
 *	exactly two possibilities:
 *
 *		+  There was a crash between metadata and cache update,
 *		   and we'll figure that out later;
 *
 *		+  Some other backend has IO_IN_PROG set on the group we
 *		   are examining, and we need to look at the group desc
 *		   on disk in order to find out if the group is on plid.
 *
 *	The second case basically means that we wind up holding SJCacheLock
 *	during a disk io, but that's a sufficiently rare event that we don't
 *	care.  I can't think of any cleaner way to do this, anyway.
 *
 *	We return the address of the first block of the highest-numbered
 *	extent that we have cached for plid.  If we have none cached, we
 *	return InvalidBlockNumber.
 */

BlockNumber
sjmaxseg(plid)
    ObjectId plid;
{
    int i;
    long seekpos, loc;
    int nbytes;
    BlockNumber last;
    SJGroupDesc *group;

    /* XXX hold the lock for a walk of the entire cache */
    SpinAcquire(SJCacheLock);

    last = InvalidBlockNumber;
    group = (SJGroupDesc *) &(SJCacheBuf[0]);

    for (i = 0; i < SJHeader->sjh_nentries; i++) {

	/* if IO_IN_PROG is set, we need to look at the group desc on disk */
	if (SJCache[i].sjc_gflags & SJC_IOINPROG) {
	    seekpos = i * SJBufSize;
	    if ((loc = FileSeek(SJCacheVfd, seekpos, L_SET)) != seekpos) {
		SpinRelease(SJCacheLock);
		elog(NOTICE, "sjmaxseg: cannot seek");
		return (-1);
	    }

	    nbytes = FileRead(SJCacheVfd, (char *) group, sizeof(SJGroupDesc));
	    if (nbytes != sizeof(SJGroupDesc)) {
		SpinRelease(SJCacheLock);
		elog(NOTICE, "sjmaxseg: read of group desc %d failed", i);
		return (-1);
	    }

	    /* sanity checks */
	    if (group->sjgd_magic != SJGDMAGIC
		|| group->sjgd_version != SJGDVERSION) {
		elog(FATAL, "sjmaxseg: cache file corrupt.");
	    }

	    if (group->sjgd_plid == plid) {
		if (group->sjgd_jboffset > last)
		    last = group->sjgd_jboffset;
	    }
	} else {
	    if (SJCache[i].sjc_plid == plid) {
		if (SJCache[i].sjc_jboffset > last)
		    last = SJCache[i].sjc_jboffset;
	    }
	}
    }

    SpinRelease(SJCacheLock);

    return (last);
}

static void
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
	printf("    [%2d] <%ld,%ld,%ld> %d@%d next %d prev %d flags %s oid %ld\n",
	       i, item->sjc_tag.sjct_dbid, item->sjc_tag.sjct_relid,
	       item->sjc_tag.sjct_base, item->sjc_plid, item->sjc_jboffset,
	       item->sjc_lrunext, item->sjc_lruprev,
	       (item->sjc_gflags & SJC_IOINPROG ? "IO_IN_PROG" : "CLEAR"),
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

#endif /* SONY_JUKEBOX */
