/*
 *  jbconn.c -- Manage Sony jukebox connections for sj storage manager.
 *
 *	This file is only included in the compiled version of Postgres
 *	if SONY_JUKEBOX is defined, which, in turn, should only be true
 *	if you are using the Sony WORM optical disk jukebox at Berkeley.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#ifdef SONY_JUKEBOX

RcsId("$Header$");

#include "machine.h"

#include "storage/block.h"
#include "storage/ipc.h"
#include "storage/ipci.h"
#include "storage/smgr.h"
#include "storage/shmem.h"
#include "storage/spin.h"
#include "storage/sj.h"

#include "utils/hsearch.h"
#include "utils/log.h"

#include "storage/jbstruct.h"
#include "storage/jblib.h"

/*
 *  JBHashEntry -- In shared memory, we maintain a hash table with the number
 *  of blocks allocated to a platter, keyed by the platter's OID in the
 *  pg_platter catalog.
 */

typedef struct JBHashEntry {
    ObjectId	jbhe_oid;
    BlockNumber	jbhe_nblocks;
} JBHashEntry;

/*
 *  In order to avoid dying if hermes is down, we postpone establishing
 *  a connection to the jukebox until we absolutely have to do so.  This
 *  macro is invoked at the top of the public interface routines that
 *  actually do any jukebox operations.
 */

#define VRFY_CONNECT()	if (!JBConnected) _pgjb_connect()

#define JBCACHESIZE	100			/* one entry per platter */
#define PGJBFORMAT	"POSTGRES_FMT"		/* format string for jb_open */

/*
 *  JBPlatDesc -- Description of an open platter in the jukebox.
 *
 *	We keep these in private memory, in a hash table.  Every time we
 *	open a new platter, we create a new platter descriptor and add it
 *	to the table.  Once we open a platter, we keep it open until the
 *	backend terminates.  The autochanger at the other end of the
 *	jukebox connection will shuffle platters in and out of drives for
 *	us.
 */

typedef struct JBPlatDesc {
    ObjectId	jbpd_plid;
    JBPLATTER	*jbpd_platter;
} JBPlatDesc;

#define	JBPH_STARTSIZE	10

/* globals defined here */
static int		*JBNEntries;
static HTAB		*JBHash;
static bool		JBConnected = false;
static HTAB		*JBPlatHash;

SPINLOCK		JBSpinLock;

/* routines declared here */
extern int		pgjb_init();
extern BlockNumber	pgjb_offset();
extern int		pgjb_wrtextent();
extern int		pgjb_rdextent();
extern int		JBShmemSize();
static void		_pgjb_connect();
static JBPlatDesc	*_pgjb_getplatdesc();
static JBHashEntry	*_pgjb_hashget();
static BlockNumber	_pgjb_findoffset();

/* routines declared elsewhere */
extern HTAB		*ShmemInitHash();
extern int		*ShmemInitStruct();
extern int		tag_hash();
extern BlockNumber	sjmaxseg();
extern int		mylog2();

/*
 *  pgjb_init() -- Initialize data structures used to communicate with the
 *		   Sony jukebox under POSTGRES.
 *
 *	This routine is called from sjinit().  We use some structures in
 *	shared memory, and some in private memory, to do this.  Shared
 *	memory stores a hash table of the highest block number allocated
 *	on a given platter so far; we use this to allocate new extents
 *	to platters.  In private memory, we keep a record of what connections
 *	we currently have open to the jukebox.
 */
 
int
pgjb_init()
{
    bool found;
    HASHCTL info;

    /* exclusive access required */
    SpinAcquire(JBSpinLock);

    /*
     *  Get the shared memory block (actually, the shared memory integer)
     *  that tells us how full the hash table is.
     */

    JBNEntries = ShmemInitStruct("Jukebox connection metadata",
				 sizeof(*JBNEntries), &found);

    if (JBNEntries == (int *) NULL) {
	SpinRelease(JBSpinLock);
	return (SM_FAIL);
    }

    /* init it if we need to */
    if (!found)
	*JBNEntries = 0;

    /*
     *  Get the shared memory hash table that maps platter OIDs to next free
     *  block.  Hash table entries are SJHashEntry structures.
     */

    info.keysize = sizeof(ObjectId);
    info.datasize = sizeof(BlockNumber);
    info.hash = tag_hash;

    JBHash = ShmemInitHash("Jukebox platter map", JBCACHESIZE, JBCACHESIZE,
			   &info, (HASH_ELEM|HASH_FUNCTION));

    if (JBHash == (HTAB *) NULL) {
	SpinRelease(JBSpinLock);
	return (SM_FAIL);
    }

    /* done with shared initialization */
    SpinRelease(JBSpinLock);

    /*
     *  Now initialize data structures in private memory that we use for
     *  jukebox connections.  We don't establish a connection to the
     *  jukebox until we actually need to use it.
     */

    bzero(&info, sizeof(info));
    info.keysize = sizeof(ObjectId);
    info.datasize = sizeof(JBPLATTER *);
    info.hash = tag_hash;

    JBPlatHash = hash_create(JBPH_STARTSIZE, &info, (HASH_ELEM|HASH_FUNCTION));

    if (JBPlatHash == (HTAB *) NULL)
	return (SM_FAIL);

    return (SM_SUCCESS);
}

/*
 *  _pgjb_connect() -- Establish a connection to the jukebox.
 *
 *	We postpone doing this for as long as possible.  Whenever we try
 *	to operate on a platter, we check to see if we've already opened
 *	a connection.  If not, we call this routine.
 *
 *	On success, we just return.  On failure, we elog(WARN, ...), which
 *	aborts the transaction.  Should we elog(FATAL, ...) instead?
 */

static void
_pgjb_connect()
{
    if (JB_INIT() < 0)
	elog(WARN, "cannot connect to jukebox server.");

    JBConnected = true;
}

/*
 *  pgjb_offset() -- Find offset of first free extent on platter.
 *
 *	If we're lucky, we'll have this in the shared memory hash table.
 *	If we're not lucky, we have to visit the cache and the platter
 *	in order to figure out where the first free block is.
 *
 *	On entry into this routine, we hold no locks.  We acquire the
 *	jukebox lock in order to query the hash table.  For now, we wind
 *	up holding this (exclusive) lock during IO, if we wind up needing
 *	to compute the first free block number.  This is VERY slow and
 *	needs to be fixed.
 */

BlockNumber
pgjb_offset(plname, plid, extentsz)
    char *plname;
    ObjectId plid;
    int extentsz;
{
    JBHashEntry *entry;
    BlockNumber offset;
    bool found;

    /* be sure we have a connection */
    VRFY_CONNECT();

    SpinAcquire(JBSpinLock);

    /* get the entry for plid from the shared hash table */
    entry = _pgjb_hashget(plid);

    /*
     *  If we haven't yet computed the first free block offset for this
     *  platter, we need to do that.
     */

    if (entry->jbhe_nblocks == InvalidBlockNumber) {
	offset = _pgjb_findoffset(plname, plid, extentsz);

	if (offset == InvalidBlockNumber) {
	    SpinRelease(JBSpinLock);
	    elog(FATAL, "pgjb_offset:  cannot find first free block <%s,%d>",
			plname, plid);
	}
    } else {
	offset = entry->jbhe_nblocks;
    }

    /* update shared memory state to reflect the allocation */
    entry->jbhe_nblocks = offset + extentsz;

    SpinRelease(JBSpinLock);

    return (offset);
}

/*
 *  _pgjb_hashget() -- Find a shared memory hash table record by platter id.
 *
 *	If the requested platter id is in the shared cache, we return a
 *	pointer to its hash table entry.  If it's not there yet, we enter
 *	it and return a pointer to the new entry.  We're careful not to
 *	exceed the capacity of the hash table.
 *
 *	On entry and exit, we hold the jukebox spin lock.
 */

static JBHashEntry *
_pgjb_hashget(plid)
    ObjectId plid;
{
    JBHashEntry *entry;
    bool found;

    entry = (JBHashEntry *) hash_search(JBHash, &plid, HASH_FIND, &found);

    if (entry == (JBHashEntry *) NULL) {
	SpinRelease(JBSpinLock);
	elog(FATAL, "_pgjb_hashget: shared hash table corrupt on FIND");
    }

    if (found)
	return (entry);

    if (*JBNEntries == JBCACHESIZE) {
	SpinRelease(JBSpinLock);
	elog(WARN, "_pgjb_hashget: cannot enter %d: shared hash table full",
		   plid);
    }

    /* entering a new plid */
    (*JBNEntries)++;

    entry = (JBHashEntry *) hash_search(JBHash, &plid, HASH_ENTER, &found);

    if (entry == (JBHashEntry *) NULL) {
	SpinRelease(JBSpinLock);
	elog(FATAL, "_pgjb_hashget: shared hash table corrupt on ENTER");
    }

    /* have not yet computed first free block for this entry */
    entry->jbhe_nblocks = InvalidBlockNumber;

    return (entry);
}

/*
 *  _pgjb_findoffset() -- Find offset of first free extent on this platter.
 *
 *	This is an extremely expensive call; we work hard to make it as
 *	seldom as possible.
 *
 *	The basic idea is to find the last occupied segment, and to add
 *	extentsz blocks to that to get the offset of the first free block.
 *	In order to find the last occupied segment, we must consult both
 *	the shared cache on magnetic disk, and the platter itself.  We first
 *	find the highest occupied segment we know about in the magnetic disk
 *	cache.  We begin scanning for an unoccupied segment on the platter
 *	from there.
 *
 *	We hold the jukebox spin lock throughout, and also wind up acquiring
 *	the sony jukebox cache lock during our scan of the magnetic disk
 *	cache.  This is a lot of locks held for a long time.  We ought to do
 *	something smarter.
 */

static BlockNumber
_pgjb_findoffset(plname, plid, extentsz)
    char *plname;
    ObjectId plid;
    int extentsz;
{
    BlockNumber last;
    BlockNumber extentno;
    long blkno;
    JBPlatDesc *jbp;

    /* check the mag disk cache for highest-numbered segment */
    last = sjmaxseg(plid);

    if ((jbp = _pgjb_getplatdesc(plname, plid)) == (JBPlatDesc *) NULL)
	return (InvalidBlockNumber);

    /*
     *  Starting at the first extent after the last known allocated extent,
     *  search for a free extent on the platter.  We must start at an integral
     *  multiple of extentsz blocks on the platter, which is why the else
     *  case of the condition below is doing so much math.
     */

    if (last == InvalidBlockNumber) {
	last = 0;
    } else {
	extentno = last / extentsz;

	if (extentno * extentsz != last)
	    extentno++;

	last = extentno * extentsz;
    }

    do {
	/* see if block 'last' is written */
	blkno = jb_scanw(jbp->jbpd_platter, last, 1);

	/* if so, skipt to next extent */
	if (blkno >= 0)
	    last += extentsz;
    } while (blkno >= 0);

    /* XXX should use symbolic constant */
    if (blkno != -2L) {
	elog(NOTICE, "_pgjb_findoffset: scanw failed on <%s,%d>: %ld",
		     plname, plid, blkno);
    }

    return (last);
}

/*
 *  _pgjb_getplatdesc() -- Get platter descriptor from private hash table.
 *
 *	This routine enters the platter by plid if necessary, and returns
 *	a pointer to a JBPlatDesc structure containing an open JBPLATTER
 *	record.
 */

static JBPlatDesc *
_pgjb_getplatdesc(plname, plid)
    char *plname;
    ObjectId plid;
{
    JBPlatDesc *jbp;
    bool found;

    jbp = (JBPlatDesc *) hash_search(JBPlatHash, &plid, HASH_ENTER, &found);

    if (jbp == (JBPlatDesc *) NULL) {
	elog(NOTICE, "_pgjb_getplatdesc: private hash table corrupt");
	return ((JBPlatDesc *) NULL);
    }

    if (!found) {
	jbp->jbpd_platter = jb_open(plname, PGJBFORMAT, JB_RDWR);
	if (jbp->jbpd_platter == (JBPLATTER *) NULL) {
	    elog(NOTICE, "_pgjb_getplatdesc: cannot open <%s,%d>",
			 plname, plid);
	    return ((JBPlatDesc *) NULL);
	}
    }

    return (jbp);
}

/*
 *  pgjb_wrtextent() -- Write an extent to the jukebox.
 *
 *	This routine takes a pointer to the SJCacheBuf buffer from sj.c,
 *	and a pointer to the SJ cache item that describes it.  Item includes
 *	a description of the write that needs to be done.  As a side effect,
 *	this routine modifies flags in item to reflect the write.
 */

int
pgjb_wrtextent(item, buf)
    SJCacheItem *item;
    char *buf;
{
    SJGroupDesc *group;
    JBPlatDesc *jbp;
    int i, low, high;
    int status;
    char *plname;

    /* be sure we have a connection */
    VRFY_CONNECT();

    plname = (char *) palloc(sizeof(NameData) + 1);
    strncpy(plname, &(item->sjc_plname.data[0]), sizeof(NameData));
    plname[sizeof(NameData)] = '\0';

    SpinAcquire(JBSpinLock);
    jbp = _pgjb_getplatdesc(item->sjc_plid, plname);
    SpinRelease(JBSpinLock);

    pfree(plname);

    if (jbp == (JBPlatDesc *) NULL) {
	elog(NOTICE, "pgjb_wrtextent: cannot get platter <%s,%d>",
		     plname, item->sjc_plid);
	return (SM_FAIL);
    }

    group = (SJGroupDesc *) buf;

    if (item->sjc_gflags & SJC_DIRTY) {
	item->sjc_gflags &= ~SJC_DIRTY;
	item->sjc_gflags |= SJC_ONPLATTER;
	low = 0;
	high = 0;
    } else {
	low = -1;
    }

    /*
     *  Block zero in the buffer is the group descriptor; this block is of
     *  size JBBLOCKSZ.  There are SJGRPSIZE blocks of size BLCKSZ that
     *  follow.  We do some hocus-pocus for each group to locate the first
     *  and last blocks of size JBBLOCKSZ in the buffer at which we have
     *  data that needs to be written.
     *
     *	We batch these writes up, and submit a single request for as many
     *  adjacent blocks as we can.
     */

    for (i = 0; i < SJGRPSIZE; i++) {
	if (item->sjc_flags[i] & SJC_DIRTY) {
	    if (low == -1)
		low = ((BLCKSZ / JBBLOCKSZ) * i) + 1;

	    high = ((BLCKSZ / JBBLOCKSZ) * (i + 1));

	    item->sjc_flags[i] &= ~SJC_DIRTY;
	    item->sjc_flags[i] |= SJC_ONPLATTER;
	} else {
	    if (low != -1) {
		/* got some blocks -- write them */
		status = jb_write(jbp->jbpd_platter,
				  &(buf[low * JBBLOCKSZ]),
				  group->sjgd_jboffset + low,
				  (high - low) + 1);

		if (status < 0)
		    return (SM_FAIL);

		low = -1;
	    }
	}
    }

    return (SM_SUCCESS);
}

/*
 *  pgjb_rdextent() -- Read an extent off of a platter.
 *
 *	This routine takes an SJCacheItem pointer and a pointer to the
 *	char buffer from sj.c, just like pgjb_wrtextent().  We read in
 *	the desired extent, setting flags in the cache item structure
 *	as appropriate.
 *
 *	Due to a design problem in the Sony jukebox driver and library
 *	code, if the entire extent has not been written to disk (which
 *	may happen, for example, when we kick out the highest extent of
 *	any given relation), our request to read the extent in a single
 *	call will fail.  When it fails, it won't return any data.  In
 *	this case, we have to issue lots of single-block reads in order
 *	to figure out which blocks in the extent are actually present,
 *	and which are not.
 */

int
pgjb_rdextent(item, buf)
    SJCacheItem *item;
    char *buf;
{
    JBPlatDesc *jbp;
    SJGroupDesc *group;
    char *plname;
    int i;
    int status;

    /* be sure we have a connection */
    VRFY_CONNECT();

    plname = (char *) palloc(sizeof(NameData) + 1);
    strncpy(plname, &(item->sjc_plname.data[0]), sizeof(NameData));
    plname[sizeof(NameData)] = '\0';

    SpinAcquire(JBSpinLock);
    jbp = _pgjb_getplatdesc(item->sjc_plid, plname);
    SpinRelease(JBSpinLock);

    pfree(plname);

    if (jbp == (JBPlatDesc *) NULL) {
	elog(NOTICE, "pgjb_rdextent: cannot get platter <%s,%d>",
		     plname, item->sjc_plid);
	return (SM_FAIL);
    }

    status = jb_read(jbp->jbpd_platter, buf, item->sjc_jboffset, SJEXTENTSZ);

    /*
     *  If we failed to read the whole extent, then we don't know what's
     *  out there, and we need to read one block at a time.  This is tedious.
     */

    if (status < 0) {
	for (i = 0; i <= SJGRPSIZE; i++) {
	    status = jb_read(jbp->jbpd_platter,
			     &(buf[i * JBBLOCKSZ]),
			     item->sjc_jboffset + i, 1);
	    if (status < 0) {
		if (i == 0) {
		    /* block zero is the group descriptor, has to be there */
		    elog(NOTICE, "pgjb_rdextent: groupdesc missing <%d>@%d",
				 item->sjc_plid, item->sjc_jboffset);
		    return (SM_FAIL);
		} else {
		    item->sjc_flags[i - 1] = SJC_MISSING;
		}
	    } else {
		if (i == 0)
		    item->sjc_gflags |= SJC_ONPLATTER;
		else
		    item->sjc_flags[i - 1] = SJC_ONPLATTER;
	    }
	}
    } else {
	/* the entire extent is on the platter */
	item->sjc_gflags |= SJC_ONPLATTER;
	for (i = 0; i < SJGRPSIZE; i++)
	    item->sjc_flags[i] = SJC_ONPLATTER;
    }

    /* record OID of group on platter in item */
    group = (SJGroupDesc *) buf;
    item->sjc_oid = group->sjgd_groupoid;

    /* sanity check */
    if (group->sjgd_magic != SJGDMAGIC || group->sjgd_version != SJGDVERSION)
	return (SM_FAIL);

    return (SM_SUCCESS);
}

/*
 *  JBShmemSize() -- return amount of shared memory required for jukebox
 *		     connection state.
 */

int
JBShmemSize()
{
    int size;
    int nsegs;
    int nbuckets;

    /* size of hash table */
    nbuckets = 1 << my_log2((JBCACHESIZE - 1) / DEF_FFACTOR + 1);
    nsegs = 1 << my_log2((nbuckets - 1) / DEF_SEGSIZE + 1);
    size = my_log2(JBCACHESIZE) + sizeof(HHDR)
	   + nsegs * DEF_SEGSIZE * sizeof(SEGMENT)
           + (int)ceil((double)JBCACHESIZE/BUCKET_ALLOC_INCR)*BUCKET_ALLOC_INCR*
              (sizeof(BUCKET_INDEX) + sizeof(JBHashEntry));

    /* size of integer telling us how full hash table is */
    size += sizeof(*JBNEntries);

    return (size);
}

#endif /* SONY_JUKEBOX */
