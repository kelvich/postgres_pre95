/*
 * bufmgr/internal.h --
 *	Internal definitions.
 *
 * Note:
 *	If BUFFERPAGE0 is defined, then 0 will be used as a
 *	valid buffer page number.
 *
 * Identification:
 *	$Header$
 */

#ifndef	bufmgr_internal
#define bufmgr_internal 1

#include "tmp/postgres.h"
#include "storage/buf.h"
#include "storage/ipci.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "tmp/miscadmin.h"
#include "storage/lmgr.h"
#include "utils/rel.h"
#include "utils/relcache.h"

/* Buf Mgr constants */
#define BLOCK_SIZE		BLCKSZ
extern int Data_Descriptors;
extern int Free_List_Descriptor;
extern int Lookup_List_Descriptor;
extern int Num_Descriptors;

/* Memory Management Constants */
#define BUF_DESC_OFFS	0
#define BUF_BLOCK_OFFS (Num_Descriptors*sizeof(BufferDesc))
#define BUF_END (BUF_BLOCK_OFFS+(NBuffers*BLOCK_SIZE))

#define PTR_ADD(a,b) (char *)(((unsigned) a)+b)

/*
 * Flags for buffer descriptors
 */
#define BM_DIRTY   		0x01
#define BM_PRIVATE 		0x02
#define BM_VALID 		0x04
#define BM_DELETED   		0x08
#define BM_FREE			0x10
#define BM_IO_IN_PROGRESS	0x20
#define BM_IO_ERROR		0x30


#define NEW_BLOCK		P_NEW
#define INVALID_BLOCKNUM	(-2)

typedef bits16 BufFlags;

typedef struct sbufdesc BufferDesc;
typedef struct sbufdesc BufferHdr;
typedef struct buftag BufferTag;
/* long * so alignment will be correct */
typedef long **BufferBlock;


struct buftag{
  LRelId	relId;
  BlockNumber   blockNum;  /* blknum relative to begin of reln */
};
#define CLEAR_BUFFERTAG(a)\
  (a)->relId.dbId = InvalidObjectId; \
  (a)->relId.relId = InvalidObjectId; \
  (a)->blockNum = INVALID_BLOCKNUM;

#define INIT_BUFFERTAG(a,xx_reln,xx_blockNum) \
{ \
  (a)->blockNum = xx_blockNum;\
  (a)->relId = RelationGetLRelId(xx_reln); \
}

#define COPY_BUFFERTAG(a,b)\
{ \
  (a)->blockNum = (b)->blockNum;\
  LRelIdAssign(*(a),*(b));\
}

#define EQUAL_BUFFERTAG(a,b) \
  (((a)->blockNum == (b)->blockNum) &&\
   (OID_Equal((a)->relId.relId,(b)->relId.relId)))


#define BAD_BUFFER_ID(bid) ((bid<1) || (bid>(NBuffers)))
#define INVALID_DESCRIPTOR (-3)
#define INVALID_BUF (-3)
#define BLOCKSZ(bhdr) (BLOCK_SIZE)

/*
 *  bletch hack -- anyplace that we declare space for relation or
 *  database names, we just use '16', not a symbolic constant, to
 *  specify their lengths.  BM_NAMESIZE is the length of these names,
 *  and is used in the buffer manager code.  somebody with lots of
 *  spare time should do this for all the other modules, too.
 */
#define BM_NAMESIZE	16

/*
 *  struct sbufdesc -- shared buffer cache metadata for a single
 *		       shared buffer descriptor.
 *
 *	We keep the name of the database and relation in which this
 *	buffer appears in order to avoid a catalog lookup on cache
 *	flush if we don't have the reldesc in the cache.  It is also
 *	possible that the relation to which this buffer belongs is
 *	not visible to all backends at the time that it gets flushed.
 *	Dbname, relname, dbid, and relid are enough to determine where
 *	to put the buffer, for all storage managers.
 */

struct sbufdesc {
    Buffer		freeNext;	/* link for freelist chain */
    Buffer		freePrev;
    SHMEM_OFFSET	data;		/* pointer to data in buf pool */

    /* tag and id must be together for table lookup to work */
    BufferTag		tag;		/* file/block identifier */
    int			id;		/* maps global desc to local desc */

    BufFlags		flags;    	/* described below */
    int16		bufsmgr;	/* storage manager id for buffer */
    unsigned		refcount;	/* # of times buffer is pinned */

    NameData		sb_dbname;	/* name of db in which buf belongs */
    NameData		sb_relname;	/* name of reln */
#ifdef HAS_TEST_AND_SET
    /* can afford a dedicated lock if test-and-set locks are available */
    slock_t	io_in_progress_lock;
#endif /* HAS_TEST_AND_SET */

    /*
     * I padded this structure to a power of 2 (128 bytes on a MIPS) because
     * BufferDescriptorGetBuffer is called a billion times and it does an
     * C pointer subtraction (i.e., "x - y" -> array index of x relative
     * to y, which is calculated using division by struct size).  Integer
     * ".div" hits you for 35 cycles, as opposed to a 1-cycle "sra" ...
     * this hack cut 10% off of the time to create the Wisconsin database!
     * It eats up more shared memory, of course, but we're (allegedly)
     * going to make some of these types bigger soon anyway... -pma 1/2/93
     */
#if defined(PORTNAME_ultrix4) || \
    defined(PORTNAME_hpux)
    char		sb_pad[60];	/* no slock_t */
#endif /* mips */
#if defined(PORTNAME_sparc)
    char		sb_pad[56];	/* has slock_t */
#endif /* sparc */
#if defined(PORTNAME_alpha)
#if defined(HAS_TEST_AND_SET)
    char		sb_pad[40];	/* has slock_t */
#else /* HAS_TEST_AND_SET */
    char		sb_pad[48];	/* no slock_t */
#endif /* HAS_TEST_AND_SET */
#endif /* sparc */
};

/*
 *  mao tracing buffer allocation
 */

#undef BMTRACE
#ifdef BMTRACE

typedef struct _bmtrace {
    int		bmt_pid;
    long	bmt_buf;
    long	bmt_dbid;
    long	bmt_relid;
    int		bmt_blkno;
    int		bmt_op;

#define BMT_NOTUSED	0
#define BMT_ALLOCFND	1
#define BMT_ALLOCNOTFND	2
#define	BMT_DEALLOC	3

} bmtrace;

#endif /* BMTRACE */


/* 
 * Bufmgr Interface:
 */

/*buf.c*/
extern BufferDesc *BufferAlloc();

/* Internal routines: only called by buf.c */

/*freelist.c*/
extern BufferDesc *GetFreeBuffer();

/*buftable.c*/
extern BufferDesc *BufTableLookup();

extern BufferDesc 	*BufferDescriptors;
extern BufferBlock 	BufferBlocks;

typedef struct Stack {
    long datum;
    struct Stack *next;
} Stack;

#define LRelIdGetRelation(lrel) \
    RelationIdGetRelation(LRelIdGetRelationId(lrel))

#endif	/* !defined(InternalDefined) */
