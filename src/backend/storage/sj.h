/*
 *  sj.h -- Private header file for the Sony jukebox storage manager.
 *
 *	This file is only used if you're at Berkeley, and SONY_JUKEBOX
 *	is defined.
 *
 *	$Header$
 */

#ifdef SONY_JUKEBOX

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
#define	SJBLOCKNAME	"_sj_nblocks_"	/* relative to $POSTGRESHOME/data */

/* bogus macros */
#define	RelationSetLockForExtend(r)

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
    ObjectId	sjgd_plid;
    long	sjgd_relblkno;
    long	sjgd_jboffset;
    long	sjgd_extentsz;
    ObjectId	sjgd_groupoid;
} SJGroupDesc;

#define SJGDMAGIC	0x060362
#define	SJGDVERSION	0
#define JBBLOCKSZ	1024

/* size of SJCacheBuf */
#define SJBUFSIZE	((BLCKSZ * SJGRPSIZE) + JBBLOCKSZ)

/* # of jb blocks in extent */
#define SJEXTENTSZ	(SJBUFSIZE / JBBLOCKSZ)

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
    int			sjh_freehead;
    int			sjh_freetail;
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
 *	We keep a free list of groups to which no references exist.  We
 *	allocate groups off this list on demand.  In general, references
 *	to groups in the cache are very short-lived; we never return pointers
 *	into private structures outside of the code that manages the cache.
 *	The free list is maintained in LRU order, and the least-recently-
 *	used group is allocated first.
 *
 *	Groups on the jukebox include one page (the first) that describes the
 *	group, including its dbid, relid, dbname, relname, and extent size.
 *	This page also includes the OID described above.
 */

typedef struct SJCacheItem {
    SJCacheTag		sjc_tag;		/* dbid, relid, group triple */
    int			sjc_freeprev;		/* free list pointer */
    int			sjc_freenext;		/* free list pointer */
    int			sjc_refcount;		/* number of active refs */
    ObjectId		sjc_oid;		/* OID of group */
    ObjectId		sjc_plid;		/* platter OID for group */
    NameData		sjc_plname;		/* platter name for group */
    int			sjc_jboffset;		/* offset of first block */

    uint8		sjc_gflags;		/* flags for entire group */

    uint8		sjc_flags[SJGRPSIZE];	/* flag bytes, 1 per block */

#define SJC_CLEAR	(uint8) 0x0
#define	SJC_DIRTY	(1 << 0)
#define SJC_MISSING	(1 << 1)
#define SJC_ONPLATTER	(1 << 2)
#define	SJC_IOINPROG	(1 << 7)

#ifdef HAS_TEST_AND_SET

    slock_t		sjc_iolock;		/* transfer in progress */

#endif /* HAS_TEST_AND_SET */

} SJCacheItem;

#endif /* SONY_JUKEBOX */
