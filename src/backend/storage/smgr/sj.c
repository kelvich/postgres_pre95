/*
 *  sj.c -- sony jukebox storage manager.
 *
 *	This code manages relations that reside on the sony write-once
 *	optical disk jukebox.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#ifdef SONY_JUKEBOX

#include <sys/file.h>
#include <math.h>
#include "machine.h"

#include "tmp/miscadmin.h"

#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/smgr.h"
#include "storage/shmem.h"
#include "storage/spin.h"

#include "utils/hsearch.h"
#include "utils/rel.h"
#include "utils/log.h"

#include "access/htup.h"
#include "access/relscan.h"
#include "access/heapam.h"

#include "catalog/pg_platter.h"
#include "catalog/pg_plmap.h"
#include "catalog/pg_proc.h"

#include "storage/sj.h"

RcsId("$Header$");

/* globals used in this file */
SPINLOCK		SJCacheLock;	/* lock for cache metadata */
extern ObjectId		MyDatabaseId;	/* OID of database we have open */
extern Name		MyDatabaseName;	/* name of database we have open */

static File		SJCacheVfd;	/* vfd for cache data file */
static File		SJMetaVfd;	/* vfd for cache metadata file */
static File		SJBlockVfd;	/* vfd for nblocks file */
static SJCacheHeader	*SJHeader;	/* pointer to cache header in shmem */
static HTAB		*SJCacheHT;	/* pointer to hash table in shmem */
static SJCacheItem	*SJCache;	/* pointer to cache metadata in shmem */
static SJCacheTag	*SJNBlockCache;	/* pointer to nblock cache */

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

/* static buffer is for data transfer */
static char	SJCacheBuf[SJBUFSIZE];

/*
 *  When we have to do IO on a group, we avoid holding an exclusive lock on
 *  the cache metadata for the duration of the operation.  We do this by
 *  setting a finer-granularity lock on the group itself.  How we do this
 *  depends on whether we have test-and-set locks or not.  If so, it's
 *  easy; we set the TASlock on the item itself.  Otherwise, we use the
 *  'wait' semaphore described above.
 */

#ifdef HAS_TEST_AND_SET
#define SET_IO_LOCK(item) \
    item->sjc_gflags |= SJC_IOINPROG; \
    SpinRelease(SJCacheLock); \
    S_LOCK(&(item->sjc_iolock));
#else /* HAS_TEST_AND_SET */
#define SET_IO_LOCK(item) \
    item->sjc_gflags |= SJC_IOINPROG; \
    (*SJNWaiting)++; \
    SpinRelease(SJCacheLock); \
    IpcSemaphoreLock(SJWaitSemId, 0, 1);
#endif /* HAS_TEST_AND_SET */

#define GROUPNO(item)	(((char *) item) - ((char *) &(SJCache[0])))/sizeof(SJCacheItem)

/* globals defined elsewhere */
extern char		*DataDir;

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
static void		_sjnewextent();
static void		_sjrdextent();
static void		_sjdirtylast();
static int		_sjfindnblocks();
static int		_sjwritegrp();
static int		_sjreadgrp();
static int		_sjgroupvrfy();
static Form_pg_plmap	_sjchoose();
static SJCacheItem	*_sjallocgrp();
static SJCacheItem	*_sjfetchgrp();
static SJHashEntry	*_sjhashop();
static int		_sjgetgrp();
static void		_sjdump();

/* routines declared elsewhere */
extern HTAB		*ShmemInitHash();
extern int		*ShmemInitStruct();
extern Relation		RelationIdGetRelation();
extern BlockNumber	pgjb_offset();
extern bool		pgjb_freespc();

/*
 *  sjinit() -- initialize the Sony jukebox storage manager.
 *
 *	We need to find (or establish) the mag-disk buffer cache metadata
 *	in shared memory and open the cache on mag disk.  The first backend
 *	to run that touches the cache initializes it.  All other backends
 *	running simultaneously will only wait for this initialization to
 *	complete if they need to get data out of the cache.  Otherwise,
 *	they'll return successfully immediately after attaching the cache
 *	memory, and will let their older sibling do all the work.
 */

int
sjinit()
{
    unsigned int metasize;
    bool metafound;
    HASHCTL info;
    bool initcache;
    char *cacheblk, *cachesave;
    int status;
    char *path;

    /*
     *  First attach the shared memory block that contains the disk
     *  cache metadata.  At the end of this block in shared memory is
     *  the hash table we use to do fast lookup on groups in the cache.
     */

    SpinAcquire(SJCacheLock);

#ifdef HAS_TEST_AND_SET
    metasize = (SJCACHESIZE * sizeof(SJCacheItem)) + sizeof(SJCacheHeader)
		+ (SJNBLKSIZE * sizeof(SJCacheTag));
#else /* HAS_TEST_AND_SET */
    metasize = (SJCACHESIZE * sizeof(SJCacheItem)) + sizeof(SJCacheHeader)
		+ (SJNBLKSIZE * sizeof(SJCacheTag)) + sizeof(*SJNWaiting);
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
     *  nblock cache, and jukebox cache entries.
     */

    SJHeader = (SJCacheHeader *) cacheblk;
    cacheblk += sizeof(SJCacheHeader);

#ifndef HAS_TEST_AND_SET
    SJNWaiting = (long *) cacheblk;
    cacheblk += sizeof(long);
#endif /* ndef HAS_TEST_AND_SET */

    SJNBlockCache = (SJCacheTag *) cacheblk;
    cacheblk += SJNBLKSIZE * sizeof(SJCacheTag);

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

    path = (char *) palloc(strlen(DataDir) + strlen(SJCACHENAME) + 2);
    sprintf(path, "%s/%s", DataDir, SJCACHENAME);

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

    pfree(path);

    path = (char *) palloc(strlen(DataDir) + strlen(SJMETANAME) + 2);
    sprintf(path, "%s/%s", DataDir, SJMETANAME);

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

    pfree(path);

    path = (char *) palloc(strlen(DataDir) + strlen(SJBLOCKNAME) + 2);
    sprintf(path, "%s/%s", DataDir, SJBLOCKNAME);

    SJBlockVfd = PathNameOpenFile(path, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (SJBlockVfd < 0) {
	SJBlockVfd = PathNameOpenFile(path, O_RDWR, 0600);
	if (SJBlockVfd < 0) {

	    /* if we were initializing the metadata, note our surrender */
	    if (!metafound) {
		SJHeader->sjh_flags &= ~SJH_INITING;
		_sjunwait_init();
	    }

	    return (SM_FAIL);
	}
    }

    pfree(path);

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
    nread = FileRead(SJMetaVfd, (char *) SJCache, nbytes);

    /* be sure we got an integral number of entries */
    nentries = nread / sizeof(SJCacheItem);
    if ((nentries * sizeof(SJCacheItem)) != nread) {
	SJHeader->sjh_flags &= ~SJH_INITING;
	_sjunwait_init();
	elog(FATAL, "sj cache metadata file corrupted.");
    }

    /*
     *  Clear out the nblock cache
     */
    bzero((char *) SJNBlockCache, SJNBLKSIZE * sizeof(SJCacheTag));

    /* add every group that appears in the cache to the hash table */
    for (i = 0; i < nentries; i++) {
	cur = &(SJCache[i]);
	result = _sjhashop(&(cur->sjc_tag), HASH_ENTER, &found);

	/* store the group number for this key in the hash table */
	result->sjhe_groupno = i;

	/* no io in progress */
	cur->sjc_gflags &= ~SJC_IOINPROG;
	cur->sjc_refcount = 0;

#ifdef HAS_TEST_AND_SET
	S_UNLOCK(&(cur->sjc_iolock));
#endif HAS_TEST_AND_SET
    }

    /*
     *  Now construct the LRU list (free list).  Extents will be nominated
     *  for reuse in this order.  Since we have no usage information, we
     *  adopt the following policy:  any extents not yet allocated in the
     *  cache are come first in the list, in order.  These are followed by
     *  the allocated extents, in order.  The free list head is the first
     *  unallocated extent, and its tail is the last allocated one.  This
     *  list is doubly-linked and is not circular.
     */

    if (nentries == SJCACHESIZE || nentries == 0) {
	cur = &(SJCache[i]);
	cur->sjc_freeprev = i - 1;

	if (i == SJCACHESIZE - 1) {
	    cur->sjc_freenext = -1;
	} else {
	    cur->sjc_freenext = i + 1;
	}

	/* list head, tail pointers */
	SJHeader->sjh_freehead = 0;
	SJHeader->sjh_freetail = SJCACHESIZE - 1;
    } else {
	for (i = 0; i < nentries; i++) {
	    cur = &(SJCache[i]);

	    if (i == 0)
		cur->sjc_freeprev = SJCACHESIZE - 1;
	    else
		cur->sjc_freeprev = i - 1;

	    if (i == nentries - 1)
		cur->sjc_freenext = -1;
	    else
		cur->sjc_freenext = i + 1;
	}

	for (i = nentries; i < SJCACHESIZE; i++) {
	    cur = &(SJCache[i]);

	    /* mark this as unused by setting oid to invalid object id */
	    cur->sjc_oid = InvalidObjectId;

	    if (i == nentries)
		cur->sjc_freeprev = -1;
	    else
		cur->sjc_freeprev = i - 1;

	    if (i == SJCACHESIZE - 1)
		cur->sjc_freenext = 0;
	    else
		cur->sjc_freenext = i + 1;
	}

	/* list head, tail pointers */
	SJHeader->sjh_freehead = nentries;
	SJHeader->sjh_freetail = nentries - 1;
    }

    /* set up cache metadata header struct */
    SJHeader->sjh_nentries = 0;
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
 *  _sjunwait_io() -- Release IO lock on the jukebox cache.
 *
 *	While we're doing IO on a particular group in the cache, any other
 *	process that wants to touch that group needs to wait until we're
 *	finished.  If we have TASlocks, then a wait lock appears on the
 *	group entry in the cache metadata.  Otherwise, we use the wait
 *	semaphore in the same way as for initialization, above.
 */

static void
_sjunwait_io(item)
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
    SJCacheTag tag;

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
    SpinRelease(SJCacheLock);

    /*
     *  By here, cache is initialized.  We are aggressively lazy, and
     *  will not allocate an initial extent for this relation until it's
     *  actually used.  We just register an initial block count of zero.
     */

    if (reln->rd_rel->relisshared)
	tag.sjct_dbid = (ObjectId) 0;
    else
	tag.sjct_dbid = MyDatabaseId;

    tag.sjct_relid = reln->rd_id;
    tag.sjct_base = (BlockNumber) 0;

    _sjregnblocks(&tag);

    /* return a fake file descriptor */
    return (0);
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
 *	Allocation strategy is:
 *
 *	  + For the first extent of a new relation, put it on the first
 *	    with room for a new relation.  The policy for allocating new
 *	    relations to a platter is implemented by pgjb_freespc().
 *
 *	  + For second and subsequent extents of an existing relation:
 *
 *	    -  If there's a platter holding another extent for this
 *	       relation, and that platter has room for this extent,
 *	       allocate it there.  NOTE:  this is true in the current
 *	       implementation, but it's a side effect of the way in which
 *	       we scan for free space on platters (we consider platters
 *	       in the same order every time we look).
 *
 *	    -  Otherwise, allocate the extent on the first platter with
 *	       space for a new extent.
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
    bool done;
    int alloctype;

    /* allocate the tuple form */
    plmdata = (Form_pg_plmap) palloc(sizeof(FormData_pg_plmap));
    plname = (char *) palloc(sizeof(NameData) + 1);

    plat = heap_openr(Name_pg_platter);

    /*
     *  We do short-term (non-two-phase) locking on the platter relation
     *  in order to guarantee serial allocations.
     */

    RelationSetLockForWrite(plat);

    platdesc = RelationGetTupleDescriptor(plat);
    platscan = heap_beginscan(plat, false, NowTimeQual, 0, NULL);

    /* figure out if this is a new or an old relation allocation */
    alloctype = (item->sjc_tag.sjct_base > 0 ? SJOLDRELN : SJNEWRELN);

    /* find a qualifying tuple in pg_platter */
    plattup = heap_getnext(platscan, false, &buf);
    if (!HeapTupleIsValid(plattup))
	elog(WARN, "_sjchoose: no platters in pg_plmap");

    done = false;
    do {
	/* get platter OID, name */
	plid = plmdata->plid = plattup->t_oid;
	d = (Datum) heap_getattr(plattup, buf, Anum_pg_platter_plname,
				 platdesc, &isnull);
	platname = DatumGetName(d);
	strncpy(plname, &(platname->data[0]), sizeof(NameData));
	plname[sizeof(NameData)] = '\0';

	done = pgjb_freespc(plname, plid, alloctype);

	/* done with this tuple */
	ReleaseBuffer(buf);

	/* next tuple */
	if (!done) {
	    plattup = heap_getnext(platscan, false, &buf);
	    if (!HeapTupleIsValid(plattup))
		elog(WARN, "_sjchoose: no space on platters in pg_plmap");
	}
    } while (!done);

    /* init the rest of the fields */
    plmdata->pldbid = item->sjc_tag.sjct_dbid;
    plmdata->plrelid = item->sjc_tag.sjct_relid;
    plmdata->plblkno = item->sjc_tag.sjct_base;
    plmdata->plextentsz = SJEXTENTSZ;
    plmdata->ploffset = pgjb_offset(plname, plmdata->plid, plmdata->plextentsz);

    /* no longer need an exclusive lock for the allocation */
    RelationUnsetLockForWrite(plat);

    heap_endscan(platscan);
    heap_close(plat);

    /* save platter name, id, offset in item */
    bcopy(plname, &(item->sjc_plname.data[0]), sizeof(NameData));
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
 *	group we're kicking out, if we have to do that.
 */

static SJCacheItem *
_sjallocgrp(grpno)
    int *grpno;
{
    SJCacheItem *item;

    /* free list had better not be empty */
    if (SJHeader->sjh_nentries == SJCACHESIZE)
	elog(FATAL, "_sjallocgrp:  no groups on free list!");

    /*
     *  Get a new group off the free list.  As a side effect, _sjgetgrp()
     *  bumps the ref count on the group for us.
     */

    *grpno = _sjgetgrp();

    item = &SJCache[*grpno];

    return (item);
}

/*
 *  _sjgetgrp() -- Get a group off the free list.
 *
 *	This routine returns the least-recently used group on the free list
 *	to the caller.  If necessary, the (old) contents of the group are
 *	forced to the platter.  On entry, we hold the cache metadata lock.
 *	We release it and mark IOINPROG on the group if we need to do any
 *	io.  We reacquire the lock before returning.
 *
 *	We know that there's something on the free list when we call this
 *	routine.
 *
 *	There's an interesting problem with write-once media that we have
 *	to deal with here.  It is possible in postgres for a half-full
 *	buffer to be flushed to stable storage, then to be reloaded into
 *	the buffer cache, filled completely, and for a new page to be
 *	allocated before the old page is flushed again.  If this happens
 *	to us, it's possible for the half-full page to get flushed all the
 *	way through to an optical disk platter, where it can never be
 *	overwritten.
 *
 *	In order to deal with this, we probe the buffer manager for all
 *	dirty blocks it has that live on an extent before we flush the
 *	extent to permanent storage.
 */

static int
_sjgetgrp()
{
    SJCacheItem *item;
    int grpno;
    int where;
    long loc;
    bool found;
    int grpoffset;
    BlockNumber nblocks;
    Relation reln;
    bool dirty;
    int i;
    int offset;
    ObjectId dbid;
    ObjectId relid;
    BlockNumber base;

    /* pull the least-recently-used group off the free list */
    grpno = SJHeader->sjh_freehead;
    item = &(SJCache[grpno]);
    _sjtouch(item);

    /* if it was previously a valid group, remove it from the hash table */
    if (item->sjc_oid != InvalidObjectId)
	_sjhashop(&(item->sjc_tag), HASH_REMOVE, &found);

    /*
     *  See if we need to flush the group to the jukebox.  If we're working
     *  with an entirely new item (the corresponding cache slot is empty),
     *  dbid == relid == base == 0, so we can ignore the flags.  Otherwise,
     *  we check every flags entry in the group descriptor to see if anyone
     *  wants to get flushed.
     */

    dirty = false;
    if (item->sjc_tag.sjct_dbid != 0 || item->sjc_tag.sjct_relid != 0) {
	if (MUST_FLUSH(item->sjc_gflags)) {
	    dirty = true;
	} else {
	    for (i = 0; i < SJGRPSIZE; i++) {
		if (MUST_FLUSH(item->sjc_flags[i])) {
		    dirty = true;
		    break;
		}
	    }
	}
    }

    if (!dirty)
	return (grpno);

    /*
     *  By here, we need to force the group to stable storage outside the
     *  cache.  Mark IOINPROG on the group (in fact, this shouldn't matter,
     *  since no one should be able to get at it -- we just got it off the
     *  free list and removed its hash table entry), release our exclusive
     *  lock, and write it out.
     */

    SET_IO_LOCK(item);

    if (_sjreadgrp(item, grpno) == SM_FAIL) {
	_sjunwait_io(item);
	elog(FATAL, "_sjgetgrp:  cannot read group %d", grpno);
    }

    /*
     *  Probe the buffer manager for dirty blocks that belong in this
     *  extent.  The buffer manager will copy them into the space we
     *  pass in, and will mark them clean in the buffer cache.
     */

    dbid = item->sjc_tag.sjct_dbid;
    relid = item->sjc_tag.sjct_relid;
    base = item->sjc_tag.sjct_base;

    for (i = 0; i < SJGRPSIZE; i++) {
	if (MUST_FLUSH(item->sjc_flags[i])) {
	    offset = (i * BLCKSZ) + JBBLOCKSZ;
	    DirtyBufferCopy(dbid, relid, base + i, &(SJCacheBuf[offset]));
	}
    }

    nblocks = _sjfindnblocks(&(item->sjc_tag));

    if (pgjb_wrtextent(item, nblocks, &(SJCacheBuf[0])) == SM_FAIL) {
	_sjunwait_io(item);
	elog(FATAL, "_sjfree:  cannot free group.");
    }

    _sjunwait_io(item);

    /* give us back our exclusive lock */
    SpinAcquire(SJCacheLock);

    return (grpno);
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

    entry = _sjhashop(&tag, HASH_FIND, &found);

    if (found) {
	*grpno = entry->sjhe_groupno;
	item = &(SJCache[*grpno]);

	if (item->sjc_gflags & SJC_IOINPROG) {
	    _sjwait_io(item);
	    return (_sjfetchgrp(dbid, relid, blkno, grpno));
	}

	_sjtouch(item);

	SpinRelease(SJCacheLock);
    } else {
	item = _sjallocgrp(grpno);

	/*
	 *  Possible race condition:  someone else instantiated the extent
	 *  we want while we were off allocating a group for it.  If that
	 *  happened, we want to put our just-allocated group back on the
	 *  free list for someone else to use.
	 */

	entry = _sjhashop(&tag, HASH_FIND, &found);
	if (found) {
	    /*
	     *  Put the just-allocated group back on the free list.  This
	     *  requires us to reenter it into the hash table if it refers
	     *  to actual data.  We only want to do this if we got a different
	     *  free group from the other process.
	     */

	    if (entry->sjhe_groupno != *grpno) {
		if (item->sjc_oid != InvalidObjectId)
		    (void) _sjhashop(&(item->sjc_tag), HASH_ENTER, &found);
		_sjunpin(item);
	    }

	    item = &(SJCache[entry->sjhe_groupno]);

	    /* if io in progress, wait for it to complete and try again */
	    if (item->sjc_gflags & SJC_IOINPROG) {
		_sjunpin(item);
		_sjwait_io(item);
		return (_sjfetchgrp(dbid, relid, blkno));
	    }

	    SpinRelease(SJCacheLock);
	} else {

	    /* okay, we need to read the extent from a platter */
	    bcopy((char *) &tag, (char *) &(item->sjc_tag), sizeof(tag));
	    entry = _sjhashop(&tag, HASH_ENTER, &found);
	    entry->sjhe_groupno = *grpno;

	    SET_IO_LOCK(item);

	    /* read the extent off the optical platter */
	    _sjrdextent(item);

	    /* update the magnetic disk cache */
	    _sjwritegrp(item, *grpno);

	    /* done, release IO lock */
	    _sjunwait_io(item);
	}
    }

    return (item);
}

/*
 *  _sjrdextent() -- Read an extent from an optical platter.
 *
 *	This routine prepares the SJCacheItem group for the pgjb_rdextent()
 *	routine to work with, and passes it along.  We don't have exclusive
 *	access to the cache metadata on entry, but we do have the IOINPROGRESS
 *	bit set on the item we're working with, so on one else will screw
 *	around with it.
 */

static void
_sjrdextent(item)
    SJCacheItem *item;
{
    Relation reln;
    HeapScanDesc hscan;
    HeapTuple htup;
    TupleDescriptor tupdesc;
    Datum d;
    Boolean n;
    Name plname;
    ScanKeyEntryData skey[3];

    /* first get platter id and offset from pg_plmap */
    reln = heap_openr(Name_pg_plmap);
    tupdesc = RelationGetTupleDescriptor(reln);
    ScanKeyEntryInitialize(&skey[0], 0x0, Anum_pg_plmap_pldbid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(item->sjc_tag.sjct_dbid));
    ScanKeyEntryInitialize(&skey[1], 0x0, Anum_pg_plmap_plrelid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(item->sjc_tag.sjct_relid));
    ScanKeyEntryInitialize(&skey[2], 0x0, Anum_pg_plmap_plblkno,
			   Integer32EqualRegProcedure,
			   Int32GetDatum(item->sjc_tag.sjct_base));
    hscan = heap_beginscan(reln, false, NowTimeQual, 3, &skey[0]);

    /*
     *  if there is no matching entry in the platter map, then we're
     *  asking for an extent that has not yet been allocated.  in this
     *  case, we return a zero-filled extent.  this happens, for example,
     *  when we try to read the initial block of a relation before one
     *  has been written.
     */

    if (!HeapTupleIsValid(htup = heap_getnext(hscan, false, (Buffer *) NULL))) {
	heap_endscan(hscan);
	heap_close(reln);
	bzero(&(SJCacheBuf[0]), SJBUFSIZE);
	return;
    }

    d = (Datum) heap_getattr(htup, InvalidBuffer, Anum_pg_plmap_plid,
			     tupdesc, &n);
    item->sjc_plid = DatumGetObjectId(d);
    d = (Datum) heap_getattr(htup, InvalidBuffer, Anum_pg_plmap_ploffset,
			     tupdesc, &n);
    item->sjc_jboffset = DatumGetInt32(d);

    heap_endscan(hscan);
    heap_close(reln);

    /* now figure out the platter's name from pg_platter */
    reln = heap_openr(Name_pg_platter);
    tupdesc = RelationGetTupleDescriptor(reln);
    ScanKeyEntryInitialize(&skey[0], 0x0, ObjectIdAttributeNumber,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(item->sjc_plid));
    hscan = heap_beginscan(reln, false, NowTimeQual, 1, &skey[0]);

    if (!HeapTupleIsValid(htup = heap_getnext(hscan, false, (Buffer *) NULL))) {
	_sjunwait_io(item);
	elog(WARN, "_sjrdextent: cannot find platter oid %d", item->sjc_plid);
    }

    d = (Datum) heap_getattr(htup, InvalidBuffer, Anum_pg_platter_plname,
			     tupdesc, &n);
    plname = DatumGetName(d);
    bcopy(&(plname->data[0]), &(item->sjc_plname.data[0]), sizeof(NameData));

    heap_endscan(hscan);
    heap_close(reln);

    /*
     *  Okay, by here, we have all the fields in item filled in except for
     *  sjc_oid, sjc_gflags, and sjc_flags[].  Those are all filled in by
     *  pgjb_rdextent(), so we call that routine to do the work.
     */

    if (pgjb_rdextent(item, &SJCacheBuf[0]) == SM_FAIL) {
	_sjunwait_io(item);
	elog(WARN, "read of extent <%d,%d,%d> from platter %d failed",
		   item->sjc_tag.sjct_dbid, item->sjc_tag.sjct_relid,
		   item->sjc_tag.sjct_base, item->sjc_plid);
    }
}

/*
 *  _sjtouch() -- Increment reference count on the supplied item.
 *
 *	If this is the first reference to the item, we remove it from the
 *	free list.  On entry and exit, we hold SJCacheLock.  If we pulled
 *	the item off the free list, we adjust SJHeader->sjh_nentries.
 */

static void
_sjtouch(item)
    SJCacheItem *item;
{
    /*
     *  Bump the reference count to this group.  If it's the first
     *  reference, pull the group off the free list.
     */

    if (++(item->sjc_refcount) == 1) {

	/* if at the start of the free list, adjust 'head' pointer */
	if (item->sjc_freeprev != -1)
	    SJCache[item->sjc_freeprev].sjc_freenext = item->sjc_freenext;
	else
	    SJHeader->sjh_freehead = item->sjc_freenext;

	/* if at the end of the free list, adjust 'tail' pointer */
	if (item->sjc_freenext != -1)
	    SJCache[item->sjc_freenext].sjc_freeprev = item->sjc_freeprev;
	else
	    SJHeader->sjh_freetail = item->sjc_freeprev;

	/* disconnect from free list */
	item->sjc_freeprev = item->sjc_freenext = -1;

	/* keep track of number of groups allocated */
	(SJHeader->sjh_nentries)++;
    }
}

/*
 *  _sjunpin() -- Decrement reference count on the supplied item.
 *
 *	If we are releasing the last reference to the supplied item, we put
 *	it back on the free list.  On entry and exit, we do not hold the
 *	cache lock.  We must acquire it in order to carry out the requested
 *	release.
 */

static void
_sjunpin(item)
    SJCacheItem *item;
{
    int grpno;

    /* exclusive access */
    SpinAcquire(SJCacheLock);

    /* item had better be pinned */
    if (item->sjc_refcount <= 0)
	elog(FATAL, "_sjunpin: illegal reference count");

    /*
     *  Unpin the item.  If this is the last reference, put the item at the
     *  end of the free list.  Implemenation note:  if SJHeader->sjh_freehead
     *  is -1, then the list is empty, and SJHeader->sjh_freetail is also -1.
     */

    if (--(item->sjc_refcount) == 0) {

	grpno = GROUPNO(item);

	if (SJHeader->sjh_freehead == -1) {
	    SJHeader->sjh_freehead = grpno;
	} else {
	    item->sjc_freeprev = SJHeader->sjh_freetail;
	    SJCache[SJHeader->sjh_freetail].sjc_freenext = grpno;
	}

	/* put item at end of free list */
	SJHeader->sjh_freetail = grpno;
	(SJHeader->sjh_nentries)--;
    }

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
    seekpos = grpno * SJBUFSIZE;
    if ((loc = FileSeek(SJCacheVfd, seekpos, L_SET)) != seekpos)
	return (SM_FAIL);

    nbytes = SJBUFSIZE;
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

/*
 *  sjextend() -- extend a relation by one block.
 */

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
    bool found;
    int grpoffset;
    long seekpos;

    RelationSetLockForExtend(reln);
    nblocks = sjnblocks(reln);
    base = (nblocks / SJGRPSIZE) * SJGRPSIZE;

    SpinAcquire(SJCacheLock);

    /*
     *  If the highest extent is full, we need to allocate a new group in
     *  the cache.  As a side effect, _sjnewextent will release SJCacheLock.
     *  We need to reacquire it immediately afterwards.
     */

    if ((nblocks % SJGRPSIZE) == 0) {
	_sjnewextent(reln, base);
	SpinAcquire(SJCacheLock);
    }

    if (reln->rd_rel->relisshared)
	tag.sjct_dbid = (ObjectId) 0;
    else
	tag.sjct_dbid = MyDatabaseId;

    tag.sjct_relid = reln->rd_id;
    tag.sjct_base = base;

    entry = _sjhashop(&tag, HASH_FIND, &found);

    if (!found) {
	SpinRelease(SJCacheLock);
	elog(WARN, "sjextend:  hey mao:  your group is missing.");
    }

    /* find the item and block in the item to write */
    grpno = entry->sjhe_groupno;
    item = &SJCache[grpno];
    grpoffset = nblocks % SJGRPSIZE;

    /*
     *  Okay, allocate the next block in this extent by marking it 'not
     *  missing'.  Once we've done this, we must hold the extend lock
     *  until end of transaction, since the number of allocated blocks no
     *  longer matches the block count visible to other backends.
     */

    if (!(item->sjc_flags[grpoffset] & SJC_MISSING)) {
	SpinRelease(SJCacheLock);
	elog(WARN, "sjextend: cache botch: next block in group present");
    } else {
	item->sjc_flags[grpoffset] &= ~SJC_MISSING;
    }

    _sjtouch(item);

    SET_IO_LOCK(item);

    /* page is allocated */
    item->sjc_flags[grpoffset] = SJC_CLEAR;

    /* verify group descriptor data in the cache file */
    if (_sjgroupvrfy(item, grpno) == SM_FAIL) {
	_sjunpin(item);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    /* write the page */
    seekpos = (grpno * SJBUFSIZE) + ((nblocks % SJGRPSIZE) * BLCKSZ)
	      + JBBLOCKSZ;
    if (FileSeek(SJCacheVfd, seekpos, L_SET) != seekpos) {
	elog(NOTICE, "sjextend: failed to seek to buffer lock (%d)", seekpos);
	_sjunpin(item);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    if (FileWrite(SJCacheVfd, buffer, BLCKSZ) != BLCKSZ) {
	elog(NOTICE, "sjextend: can't write page %d", nblocks);
	_sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    /* write the updated cache metadata entry */
    seekpos = grpno * sizeof(*item);

    if (FileSeek(SJMetaVfd, seekpos, L_SET) != seekpos) {
	elog(NOTICE, "sjextend: seek to %d on metadata file failed", seekpos);
	_sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    if (FileWrite(SJMetaVfd, (char *) item, sizeof(*item)) < 0) {
	elog(NOTICE, "sjextend: write of metadata file failed");
	_sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    /* success */
    _sjunwait_io(item);
    _sjunpin(item);

    tag.sjct_base = ++nblocks;
    _sjregnblocks(&tag);

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
    seekpos = grpno * SJBUFSIZE;
    if ((loc = FileSeek(SJCacheVfd, seekpos, L_SET)) != seekpos) {
	elog(NOTICE, "_sjreadgrp: cannot seek");
	return (SM_FAIL);
    }

    nbytes = SJBUFSIZE;
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
    /*
     *  Even though we can't really unlink the relation, we return
     *  success here, so that users can destroy tables (remove the
     *  pg_class pointers to them).
     */

    return (SM_SUCCESS);
}

/*
 *  _sjnewextent() -- Add a new extent to a relation in the jukebox cache.
 */

static void
_sjnewextent(reln, base)
    Relation reln;
    BlockNumber base;
{
    SJHashEntry *entry;
    SJGroupDesc *group;
    SJCacheItem *item;
    bool found;
    int grpno;
    int i;

    item = _sjallocgrp(&grpno);

    if (reln->rd_rel->relisshared)
	item->sjc_tag.sjct_dbid = (ObjectId) 0;
    else
	item->sjc_tag.sjct_dbid = MyDatabaseId;

    item->sjc_tag.sjct_relid = (ObjectId) reln->rd_id;
    item->sjc_tag.sjct_base = base;

    entry = _sjhashop(&(item->sjc_tag), HASH_ENTER, &found);

    entry->sjhe_groupno = grpno;

    SET_IO_LOCK(item);

    /* set flags on item, initialize group descriptor block */
    item->sjc_gflags = SJC_CLEAR;
    for (i = 0; i < SJGRPSIZE; i++)
	item->sjc_flags[i] = SJC_MISSING;

    /* should be smarter and only bzero what we need to */
    bzero(SJCacheBuf, SJBUFSIZE);

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
    group->sjgd_relblkno = base;
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
	_sjunwait_io(item);
	elog(FATAL, "_sjnewextent: cannot write new extent to disk");
    }

    _sjregnblocks(&(item->sjc_tag));

    /* can now release i/o lock on the item we just added */
    _sjunwait_io(item);

    /* no longer need the reference */
    _sjunpin(item);
}

/*
 *  _sjhashop() -- Do lookup, insertion, or deletion on the metadata hash
 *		   table in shared memory.
 *
 *	We don't worry about the number of entries in the hash table here;
 *	that's handled at a higher level (_sjallocgrp and _sjgetgrp).  We
 *	hold SJCacheLock on entry.
 */

static SJHashEntry *
_sjhashop(tagP, op, foundP)
    SJCacheTag *tagP;
    HASHACTION op;
    bool *foundP;
{
    SJHashEntry *entry;

    entry = (SJHashEntry *) hash_search(SJCacheHT, (char *) tagP, op, foundP);

    if (entry == (SJHashEntry *) NULL) {
	SpinRelease(SJCacheLock);
	elog(FATAL, "_sjhashop: hash table corrupt.");
    }

    if (*foundP) {
	if (op == HASH_ENTER) {
	    SpinRelease(SJCacheLock);
	    elog(WARN, "_sjhashop: cannot enter <%d,%d,%d>: already exists",
		 tagP->sjct_dbid, tagP->sjct_relid, tagP->sjct_base);
	}
    } else {
	if (op == HASH_REMOVE) {
	    SpinRelease(SJCacheLock);
	    elog(WARN, "_sjhashop: cannot delete <%d,%d,%d>: missing",
		 tagP->sjct_dbid, tagP->sjct_relid, tagP->sjct_base);
	}
    }

    return (entry);
}

int
sjopen(reln)
    Relation reln;
{
    /* return a fake fd */
    return (0);
}

int
sjclose(reln)
    Relation reln;
{
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
    BlockNumber base;
    int offset;
    int grpno;
    long seekpos;

    /* fake successful read on non-existent data */
    if (sjnblocks(reln) <= blocknum) {
	bzero(buffer, BLCKSZ);
	return (SM_SUCCESS);
    }

    if (reln->rd_rel->relisshared)
	reldbid = (ObjectId) 0;
    else
	reldbid = MyDatabaseId;

    base = (blocknum / SJGRPSIZE) * SJGRPSIZE;

    item = _sjfetchgrp(reldbid, reln->rd_id, base, &grpno);

    /* shd expand _sjfetchgrp() inline to avoid extra semop()s */
    SpinAcquire(SJCacheLock);

    SET_IO_LOCK(item);

    /* First read and verify the group descriptor metadata */
    if (_sjgroupvrfy(item, grpno) == SM_FAIL) {
	_sjunpin(item);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    /* By here, group descriptor metadata is okay */
    seekpos = (grpno * SJBUFSIZE) + ((blocknum % SJGRPSIZE) * BLCKSZ)
	      + JBBLOCKSZ;
    if (FileSeek(SJCacheVfd, seekpos, L_SET) != seekpos) {
	elog(NOTICE, "sjread: failed to seek to buffer lock (%d)", seekpos);
	_sjunpin(item);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    /* read the requested page */
    if (FileRead(SJCacheVfd, buffer, BLCKSZ) != BLCKSZ) {
	elog(NOTICE, "sjread: can't read page %d", blocknum);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    _sjunwait_io(item);
    _sjunpin(item);

    return (SM_SUCCESS);
}

static int
_sjgroupvrfy(item, grpno)
    SJCacheItem *item;
    int grpno;
{
    long seekpos;
    SJGroupDesc gdesc;

    seekpos = SJBUFSIZE * grpno;
    if (FileSeek(SJCacheVfd, seekpos, L_SET) != seekpos) {
	elog(NOTICE, "sjgroupvrfy: Cannot seek to %d on sj cache file",
		     seekpos);
	return (SM_FAIL);
    }

    if (FileRead(SJCacheVfd, (char *) &gdesc, sizeof(gdesc)) < 0) {
	elog(NOTICE, "sjgroupvrfy: Cannot read group desc from sj cache file");
	return (SM_FAIL);
    }

    if (gdesc.sjgd_magic != SJGDMAGIC
	|| gdesc.sjgd_version != SJGDVERSION
	|| gdesc.sjgd_groupoid != item->sjc_oid) {

	elog(NOTICE, "sjgroupvrfy: trashed cache");
	return (SM_FAIL);
    }

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
    BlockNumber base;
    int offset;
    int grpno;
    int which;
    long seekpos;

    if (reln->rd_rel->relisshared)
	reldbid = (ObjectId) 0;
    else
	reldbid = MyDatabaseId;

    base = (blocknum / SJGRPSIZE) * SJGRPSIZE;

    item = _sjfetchgrp(reldbid, reln->rd_id, base, &grpno);

    /* shd expand _sjfetchgrp() inline to avoid extra semop()s */
    SpinAcquire(SJCacheLock);

    which = blocknum % SJGRPSIZE;

    if (item->sjc_flags[which] & SJC_ONPLATTER) {
	SpinRelease(SJCacheLock);
	_sjunpin(item);
	elog(WARN, "sjwrite: optical platters are write-once, cannot rewrite");
    }

    SET_IO_LOCK(item);

    item->sjc_flags[which] &= ~SJC_MISSING;

    /* verify group descriptor data in the cache file */
    if (_sjgroupvrfy(item, grpno) == SM_FAIL) {
	_sjunpin(item);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    /* write the page */
    seekpos = (grpno * SJBUFSIZE) + ((blocknum % SJGRPSIZE) * BLCKSZ)
	      + JBBLOCKSZ;
    if (FileSeek(SJCacheVfd, seekpos, L_SET) != seekpos) {
	elog(NOTICE, "sjwrite: failed to seek to buffer lock (%d)", seekpos);
	_sjunpin(item);
	_sjunwait_io(item);
	return (SM_FAIL);
    }

    if (FileWrite(SJCacheVfd, buffer, BLCKSZ) != BLCKSZ) {
	elog(NOTICE, "sjwrite: can't read page %d", blocknum);
	_sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    /* write the updated cache metadata entry */
    seekpos = grpno * sizeof(*item);

    if (FileSeek(SJMetaVfd, seekpos, L_SET) != seekpos) {
	elog(NOTICE, "sjwrite: seek to %d on metadata file failed", seekpos);
	_sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    if (FileWrite(SJMetaVfd, (char *) item, sizeof(*item)) < 0) {
	elog(NOTICE, "sjwrite: write of metadata file failed");
	_sjunwait_io(item);
	_sjunpin(item);
	return (SM_FAIL);
    }

    _sjunwait_io(item);
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
 *	Rather than compute this by walking through pg_plmap and fetching
 *	groups off of platters, we store the number of blocks currently
 *	allocated to a relation in a special Unix file.
 */

int
sjnblocks(reln)
    Relation reln;
{
    SJCacheTag tag;
    int nblocks;

    if (reln->rd_rel->relisshared)
	tag.sjct_dbid = (ObjectId) 0;
    else
	tag.sjct_dbid = MyDatabaseId;

    tag.sjct_relid = reln->rd_id;

    tag.sjct_base = (BlockNumber) _sjfindnblocks(&tag);

    return ((int) (tag.sjct_base));
}

/*
 *  _sjfindnblocks() -- Find block count for the (dbid,relid) pair.
 */

static int
_sjfindnblocks(tag)
    SJCacheTag *tag;
{
    int nbytes;
    int i;
    SJCacheTag *cachetag;
    SJCacheTag mytag;

    cachetag = SJNBlockCache;
    i = 0;
    while (i < SJNBLKSIZE && cachetag->sjct_relid != (ObjectId) 0) {
	if (cachetag->sjct_dbid == tag->sjct_dbid
	    && cachetag->sjct_relid == tag->sjct_relid) {
	    return (cachetag->sjct_base);
	}
	i++;
	cachetag++;
    }

    if (FileSeek(SJBlockVfd, 0L, L_SET) != 0) {
	elog(FATAL, "_sjfindnblocks: cannot seek to zero on block count file");
    }

    while ((nbytes = FileRead(SJBlockVfd, (char *)&mytag, sizeof(mytag))) > 0) {
	if (mytag.sjct_dbid == tag->sjct_dbid
	    && mytag.sjct_relid == tag->sjct_relid) {

	    if (i == SJNBLKSIZE) {
		/* fast pseudo-random function */
		i = mytag.sjct_relid % SJNBLKSIZE;
		cachetag = &(SJNBlockCache[i]);
	    }

	    /* save cache tag */
	    cachetag->sjct_dbid = mytag.sjct_dbid;
	    cachetag->sjct_relid = mytag.sjct_relid;
	    cachetag->sjct_base = mytag.sjct_base;

	    return (mytag.sjct_base);
	}
    }

    elog(FATAL, "_sjfindnblocks: cannot get block count for <%d,%d>",
		tag->sjct_dbid, tag->sjct_relid);
}

/*
 *  _sjregnblocks() -- Remember the count of blocks for this relid.
 */

static void
_sjregnblocks(tag)
    SJCacheTag *tag;
{
    int loc;
    int i;
    SJCacheTag *cachetag;
    SJCacheTag mytag;

    cachetag = SJNBlockCache;
    i = 0;
    while (i < SJNBLKSIZE && cachetag->sjct_relid != (ObjectId) 0) {
	if (cachetag->sjct_dbid == tag->sjct_dbid
	    && cachetag->sjct_relid == tag->sjct_relid)
	    break;

	i++;
	cachetag++;
    }

    if (i == SJNBLKSIZE) {
	i = tag->sjct_relid % SJNBLKSIZE;
	cachetag = &(SJNBlockCache[i]);
    }

    cachetag->sjct_dbid = tag->sjct_dbid;
    cachetag->sjct_relid = tag->sjct_relid;
    cachetag->sjct_base = tag->sjct_base;

    /* update block count file */
    if (FileSeek(SJBlockVfd, 0L, L_SET) < 0) {
	elog(FATAL, "_sjregnblocks: cannot seek to zero on block count file");
    }

    loc = 0;
    mytag.sjct_base = tag->sjct_base;

    /* overwrite existing entry, if any */
    while (FileRead(SJBlockVfd, (char *) &mytag, sizeof(mytag)) > 0) {
	if (mytag.sjct_dbid == tag->sjct_dbid
	    && mytag.sjct_relid == tag->sjct_relid) {
	    if (FileSeek(SJBlockVfd, (loc * sizeof(SJCacheTag)), L_SET) < 0)
		elog(FATAL, "_sjregnblocks: cannot seek to loc");
	    if (FileWrite(SJBlockVfd, (char *) tag, sizeof(*tag)) < 0)
		elog(FATAL, "_sjregnblocks: cannot write nblocks");
	    return;
	}
	loc++;
    }

    /* new relation -- write at end of file */
    if (FileWrite(SJBlockVfd, (char *) tag, sizeof(*tag)) < 0)
	elog(FATAL, "_sjregnblocks: cannot write nblocks for new reln");
}
int
sjcommit()
{
    FileSync(SJMetaVfd);
    FileSync(SJCacheVfd);
    FileSync(SJBlockVfd);

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
    int tmp;

    /* size of cache metadata */
    size = ((SJCACHESIZE + 1) * sizeof(SJCacheItem)) + sizeof(SJCacheHeader);
#ifndef HAS_TEST_AND_SET
    size += sizeof(*SJNWaiting);
#endif /* ndef HAS_TEST_AND_SET */

    /* size of hash table */
    nbuckets = 1 << (int)my_log2((SJCACHESIZE - 1) / DEF_FFACTOR + 1);
    nsegs = 1 << (int)my_log2((nbuckets - 1) / DEF_SEGSIZE + 1);
    size += my_log2(SJCACHESIZE) + sizeof(HHDR);
    size += nsegs * DEF_SEGSIZE * sizeof(SEGMENT);
    tmp = (int)ceil((double)SJCACHESIZE/BUCKET_ALLOC_INCR);
    size += tmp * BUCKET_ALLOC_INCR *
            (sizeof(BUCKET_INDEX) + sizeof(SJHashEntry));

    /* nblock cache */
    size += SJNBLKSIZE * sizeof(SJCacheTag);

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

    /*
     *  Walk backwards along the free list.  If we ever hit an unallocated
     *  block, we can stop searching.  Otherwise, we'll hit the head of the
     *  list when freeprev == -1.
     */

    for (i = SJHeader->sjh_freetail;
	 i != -1 && SJCache[i].sjc_oid != InvalidObjectId;
	 i = SJCache[i].sjc_freeprev) {

	/* if IO_IN_PROG is set, we need to look at the group desc on disk */
	if (SJCache[i].sjc_gflags & SJC_IOINPROG) {
	    seekpos = i * SJBUFSIZE;
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
		if (group->sjgd_jboffset > last || last == InvalidBlockNumber)
		    last = group->sjgd_jboffset;
	    }
	} else {
	    if (SJCache[i].sjc_plid == plid) {
		if (SJCache[i].sjc_jboffset > last
		    || last == InvalidBlockNumber) {

		    last = SJCache[i].sjc_jboffset;
		}
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

    printf("jukebox cache metdata: size %d, %d entries, free head %d tail %d",
	   SJCACHESIZE, nentries, SJHeader->sjh_freehead,
	   SJHeader->sjh_freetail);
    if (SJHeader->sjh_flags & SJH_INITING)
	printf(", INITING");
    if (SJHeader->sjh_flags & SJH_INITED)
	printf(", INITED");
    printf("\n");

    for (i = 0; i < SJCACHESIZE; i++) {
	item = &SJCache[i];
	printf("    [%2d] <%ld,%ld,%ld> %d@%d next %d prev %d flags %s oid %ld\n",
	       i, item->sjc_tag.sjct_dbid, item->sjc_tag.sjct_relid,
	       item->sjc_tag.sjct_base, item->sjc_plid, item->sjc_jboffset,
	       item->sjc_freenext, item->sjc_freeprev,
	       (item->sjc_gflags & SJC_IOINPROG ? "IO_IN_PROG" : "CLEAR"),
	       item->sjc_oid);
	printf("         ");
	for (j = 0; j < SJGRPSIZE; j++) {
	    printf("[%d %c%c]", j,
	    	   (item->sjc_flags[j] & SJC_MISSING ? 'm' : '-'),
	    	   (item->sjc_flags[j] & SJC_ONPLATTER ? 'o' : '-'));
	}
	printf("\n");
    }

    SpinRelease(SJCacheLock);
}

/*
 *  SJInitSemaphore() -- Initialize the 'wait' semaphore for jukebox cache
 *			 pages.
 *
 *	We only do this if we don't have test-and-set locks.
 */

SJInitSemaphore(key)
    IPCKey key;
{
#ifndef HAS_TEST_AND_SET
    int status;

    SJWaitSemId = IpcSemaphoreCreate(IPCKeyGetSJWaitSemaphoreKey(key),
				     1, IPCProtection, 0, &status);
    if (SJWaitSemId < 0) {
	elog(FATAL, "cannot create/attach jukebox semaphore");
    }
#else /* ndef HAS_TEST_AND_SET */
    return;
#endif /* ndef HAS_TEST_AND_SET */
}

#endif /* SONY_JUKEBOX */
