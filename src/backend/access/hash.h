/*
 *  hash.h -- header file for postgres hash access method implementation 
 *  modeled after Margo Seltzer's hash implementation for unix. 
 *
 */


/* 
 * An overflow page is a spare page allocated for storing data whose 
 * bucket doesn't have room to store it. We use overflow pages rather
 * than just splitting the bucket because there is a linear order in
 * the way we split buckets. In other words, if there isn't enough space
 * in the bucket itself, put it in an overflow page. 
 *
 * Overflow page addresses are stored in form: (Splitnumber, Page offset).
 *
 * A splitnumber is the number of the generation where the table doubles
 * in size. The ovflpage's offset within the splitnumber; offsets start
 * at 1. 
 * 
 * We convert the stored bitmap address into a page address with the
 * macro OADDR_OF(S, O) where S is the splitnumber and O is the page 
 * offset. 
 */
typedef uint32 	Bucket;
typedef bits16	OverflowPageAddress;
typedef uint32	SplitNumber;
typedef uint32  PageOffset;

/* A valid overflow address will always have a page offset >= 1 */
#define InvalidOvflAddress	0	
					   
#define SPLITSHIFT	11
#define SPLITMASK	0x7FF
#define SPLITNUM(N)	((SplitNumber)(((u_int)(N)) >> SPLITSHIFT))
#define OPAGENUM(N)	((PageOffset)((N) & SPLITMASK))
#define	OADDR_OF(S,O)	((OverflowPageAddress)((u_int)((u_int)(S) << SPLITSHIFT) + (O)))

#define BUCKET_TO_BLKNO(B) \
	((Bucket) ((B) + ((B) ? metap->SPARES[_hash_log2((B)+1)-1] : 0)) + 1)
#define OADDR_TO_BLKNO(B) 	 \
	((BlockNumber) \
	 (BUCKET_TO_BLKNO ( (1 << SPLITNUM((B))) -1 ) + OPAGENUM((B))));

/* 
 * OVERFLOW_PAGE and BUCKET_PAGE are used to flag which type of
 * page we're looking at. This is necessary information when you're 
 * deleting tuples from a page. If all the tuples are deleted from
 * an overflow page, the overflow is made available to other buckets
 * by calling _hash_freeovflpage(). If all the tuples are deleted from
 * a bucket page, no additional action is necessary.
 */

#define OVERFLOW_PAGE	0x1
#define BUCKET_PAGE	0x2

typedef struct HashPageOpaqueData {
    bits16 hasho_flag;			/* is this page a bucket or ovfl */
    Bucket hasho_bucket;		/* bucket number this pg belongs to */
    OverflowPageAddress hasho_oaddr;	/* ovfl address of this ovfl pg */
    BlockNumber hasho_nextblkno;	/* next ovfl blkno */
    BlockNumber	hasho_prevblkno;	/* previous ovfl (or bucket) blkno */
} HashPageOpaqueData;

typedef HashPageOpaqueData        *HashPageOpaque;

/*
 *  ScanOpaqueData is used to remember which buffers we're currently
 *  examining in the scan.  We keep these buffers locked and pinned and
 *  recorded in the opaque entry of the scan in order to avoid doing a
 *  ReadBuffer() for every tuple in the index.  This avoids semop() calls,
 *  which are expensive.
 */

typedef struct HashScanOpaqueData {
    Buffer      hashso_curbuf;
    Buffer      hashso_mrkbuf;
} HashScanOpaqueData;

typedef HashScanOpaqueData        *HashScanOpaque;

/* 
 * Definitions for metapage.
 */

#define HASH_METAPAGE	0
#define HASH_MAGIC	0x6440640
#define HASH_VERSION	0

/*
 * NCACHED is used to set the array sizeof spares[] & bitmaps[].
 *
 * Spares[] is used to hold the number overflow pages currently
 * allocated at a certain splitpoint. For example, if spares[3] = 7
 * then there are a maximum of 7 ovflpages available at splitpoint 3.
 * The value in spares[] will change as ovflpages are added within
 * a splitpoint. 
 * 
 * Within a splitpoint, one can find which ovflpages are available and
 * which are used by looking at a bitmaps that are stored on the ovfl
 * pages themselves. There is at least one bitmap for every splitpoint's
 * ovflpages. Bitmaps[] contains the ovflpage addresses of the ovflpages 
 * that hold the ovflpage bitmaps. 
 *
 * The reason that the size is restricted to NCACHED (32) is because
 * the bitmaps are 16 bits: upper 5 represent the splitpoint, lower 11
 * indicate the page number within the splitpoint. Since there are 
 * only 5 bits to store the splitpoint, there can only be 32 splitpoints. 
 * Both spares[] and bitmaps[] use splitpoints as there indices, so there
 * can only be 32 of them. 
 */

#define	NCACHED		32	


typedef struct HashMetaPageData {
    uint32	hashm_magic;		/* magic no. for hash tables */
    uint32	hashm_version;		/* version ID */
    uint32	hashm_nkeys;		/* number of keys stored in the table */
    uint32	hashm_ffactor;		/* fill factor */
    uint16	hashm_bsize;		/* bucket size */
    uint16	hashm_bshift;		/* bucket shift */
    uint32 	hashm_maxbucket;	/* ID of maximum bucket in use */
    uint32	hashm_highmask;		/* mask to modulo into entire table */
    uint32	hashm_lowmask;		/* mask to modulo into lower half of table */
    uint32	hashm_ovflpoint;	/* pageno. from which ovflpgs being allocated */
    uint32	hashm_lastfreed;	/* last ovflpage freed */
    uint32	hashm_nmaps;		/* Initial number of bitmaps */
    uint32	hashm_spares[NCACHED];	/* spare pages available at splitpoints */
    bits16	hashm_bitmaps[NCACHED];	/* ovflpage address of ovflpage bitmaps */
    BlockNumber	hashm_mapp[NCACHED];	/* blknumbers of ovfl page maps */
    RegProcedure  hashm_procid;		/* hash procedure id from pg_proc */
} HashMetaPageData;

typedef HashMetaPageData *HashMetaPage;

/* Short hands for accessing structure */
#define BSIZE		hashm_bsize
#define BSHIFT		hashm_bshift
#define OVFL_POINT	hashm_ovflpoint
#define	LAST_FREED	hashm_lastfreed
#define MAX_BUCKET	hashm_maxbucket
#define FFACTOR		hashm_ffactor
#define HIGH_MASK	hashm_highmask
#define LOW_MASK	hashm_lowmask
#define NKEYS		hashm_nkeys
#define SPARES		hashm_spares
#define BITMAPS		hashm_bitmaps
#define VERSION		hashm_version
#define MAGIC		hashm_magic

extern bool	BuildingHash;

/*
 *  HashItems are what we store in the btree. They don't really need to have 
 *  the oids, and those should be taken out. 
 */

typedef struct HashItemData {
        OID                     hash_oid;
        IndexTupleData          hash_itup;
} HashItemData;

typedef HashItemData      *HashItem;

/*
 * Constants
 */
#define DEFAULT_BUCKET_SIZE	BLCKSZ		/* 8 K */ 
#define DEFAULT_BUCKET_SHIFT	13		/* log2(BUCKET) */
#define DEFAULT_FFACTOR		300
#define SPLITMAX		8
#define BYTE_SHIFT		3
#define INT_TO_BYTE		2
#define INT_BYTE_SHIFT		5
#define ALL_SET			((u_int)0xFFFFFFFF)
#define ALL_CLEAR		0

/*
 * The number of bits in an ovflpage bitmap which
 * tells which ovflpages are empty versus in use (NOT the number of
 * bits in an overflow page *address* bitmap). 
 */
#define BITS_PER_MAP	32	/* Number of bits in ovflpage bitmap */

/* Given the address of the beginning of a big map, clear/set the nth bit */
#define CLRBIT(A, N)	((A)[(N)/BITS_PER_MAP] &= ~(1<<((N)%BITS_PER_MAP)))
#define SETBIT(A, N)	((A)[(N)/BITS_PER_MAP] |= (1<<((N)%BITS_PER_MAP)))
#define ISSET(A, N)	((A)[(N)/BITS_PER_MAP] & (1<<((N)%BITS_PER_MAP)))


#define	HASH_READ	0
#define	HASH_WRITE	1

#define HASH_INSERTION	0
#define HASH_DESCENT	1

#define OVFLPAGE	0

/*  
 *  In general, the hash code tries to localize its knowledge about page
 *  layout to a couple of routines.  However, we need a special value to
 *  indicate "no page number" in those places where we expect page numbers.
 */

#define P_NONE		0

/*
 *  Strategy number. There's only one valid strategy for hashing: equality.
 */

#define HTEqualStrategyNumber		1
#define HTMaxStrategyNumber		1

/*
 *  When a new operator class is declared, we require that the user supply
 *  us with an amproc procudure for hashing a key of the new type.
 *  Since we only have one such proc in amproc, it's number 1.
 */

#define HASHPROC	1

/* public routines */
char			*hashgettuple();
InsertIndexResult	hashinsert();

/* private routines */
InsertIndexResult	_hash_doinsert();
InsertIndexResult	_hash_insertonpg();
OffsetIndex		_hash_pgaddtup();
ScanKey			_hash_mkscankey();
void			_hash_search();
void			_hash_relbuf();
Buffer			_hash_getbuf();
void			_hash_wrtbuf();
void			_hash_wrtnorelbuf();
RetrieveIndexResult	_hash_first();
RetrieveIndexResult	_hash_next();
bool			_hash_step();
StrategyNumber		_hash_getstrat();
bool			_hash_invokestrat();
HashItem	       	_hash_formitem();
void			_hash_regscan();
void			_hash_dropscan();
void			_hash_adjscans();
void			_hash_scandel();
bool			_hash_scantouched();
Buffer			_hash_addovflpage();
OverflowPageAddress	_hash_getovfladdr();
void			_hash_expandtable();
uint32			_hash_firstfreebit();
Buffer			_hash_freeovflpage();
int32			_hash_initbitmap();
Page			_hash_chgbufaccess();
void			_hash_splitpage();
void			_hash_squeezebucket();














