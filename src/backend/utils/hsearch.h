/*
 *  hsearch.h --
 *	for hashing in the new buffer manager
 *
 *  Identification:
 *  	$Header$
*/

#ifndef HSearchIncluded
#define HSearchIncluded
/*
 * Constants
 */
# define DEF_BUCKET_SIZE	256
# define DEF_BUCKET_SHIFT	8	/* log2(BUCKET) */
# define DEF_SEGSIZE		256
# define DEF_SEGSIZE_SHIFT		8      /* log2(SEGSIZE)	 */
# define DEF_DIRSIZE		256
# define PRIME1			37
# define PRIME2			1048583
# define DEF_FFACTOR		1
# define SPLTMAX		8


/*
 * Hash bucket is actually bigger than this.  Key field can have
 * variable length and a variable length data field follows it.
 */
typedef struct element {
	unsigned int next;	       /* secret from user	 */
	int key;
} ELEMENT;

typedef unsigned int BUCKET_INDEX;
/* segment is an array of bucket pointers  */
typedef BUCKET_INDEX *SEGMENT;
typedef unsigned int SEG_OFFSET;

typedef struct hashhdr {
	int bsize;	/* Bucket/Page Size */
	int bshift;	/* Bucket shift */
	int dsize;	/* Directory Size */
	int ssize;	/* Segment Size */
	int sshift;	/* Segment shift */
	int max_bucket;	/* ID of Maximum bucket in use */
	int high_mask;	/* Mask to modulo into entire table */
	int low_mask;	/* Mask to modulo into lower half of table */
	int ffactor;	/* Fill factor */
	int nkeys;	/* Number of keys in hash table */
	int nsegs;	/* Number of allocated segments */
	int keysize;	/* hash key length in bytes */
	int datasize;	/* elem data length in bytes */
	int max_dsize;  /* 'dsize' limit if directory is fixed size */ 
	BUCKET_INDEX freeBucketIndex;
			/* index of first free bucket */
#ifdef HASH_STATISTICS
	int accesses;
	int collisions;
#endif
} HHDR;

typedef struct htab {
  	HHDR		*hctl;	 /* shared control information */
	int 		(*hash)(); /* Hash Function */
	char *	 	segbase;   /* segment base address for 
				    * calculating pointer values */
	SEG_OFFSET	*dir;	   /* 'directory' of segm starts */
	int *		(*alloc)();/* memory allocator */
	/* int * for alignment reasons */

} HTAB;

typedef struct hashctl {
	int bsize;	/* Bucket Size */
	int ssize;	/* Segment Size */
	int dsize;	/* Dirsize Size */
	int ffactor;	/* Fill factor */
	int (*hash)();	/* Hash Function */
	int keysize;	/* hash key length in bytes */
	int datasize;	/* elem data length in bytes */
	int max_size;  /* limit to dsize if directory size is limited */
	int *segbase;  /* base for calculating bucket + seg ptrs */
	int * (*alloc)(); /* memory allocation function */
	int *dir;	/* directory if allocated already */
	int *hctl;	/* location of header information in shd mem */
} HASHCTL;

/* Flags to indicate action for hctl */
#define HASH_BUCKET	0x001	/* Setting bucket size */
#define HASH_SEGMENT	0x002	/* Setting segment size */
#define HASH_DIRSIZE	0x004	/* Setting directory size */
#define HASH_FFACTOR	0x008	/* Setting fill factor */
#define HASH_FUNCTION	0x010	/* Set user defined hash function */
#define HASH_ELEM	0x020	/* Setting key/data size */
#define HASH_SHARED_MEM 0x040   /* Setting shared mem const */
#define HASH_ATTACH	0x080   /* Do not initialize hctl */
#define HASH_ALLOC	0x100   /* Setting memory allocator */ 


/* seg_alloc assumes that INVALID_INDEX is 0*/
#define INVALID_INDEX 		(0)
#define NO_MAX_DSIZE 		(-1)
/* number of hash buckets allocated at once */
#define BUCKET_ALLOC_INCR	(30)

/* entry points */
HTAB *hash_create();
int *hash_search();
void hash_destroy();
void hash_stats();

/* hash_search operations */
typedef enum { FIND, ENTER, REMOVE } ACTION;
#endif
