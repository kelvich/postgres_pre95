/*
 * dynahash.c -- dynamic hashing
 *
 * Identification:
 *	$Header$
 *
 * Dynamic hashing, after CACM April 1988 pp 446-457, by Per-Ake Larson.
 * Coded into C, with minor code improvements, and with hsearch(3) interface,
 * by ejp@ausmelb.oz, Jul 26, 1988: 13:16;
 * also, hcreate/hdestroy routines added to simulate hsearch(3).
 *
 * These routines simulate hsearch(3) and family, with the important
 * difference that the hash table is dynamic - can grow indefinitely
 * beyond its original size (as supplied to hcreate()).
 *
 * Performance appears to be comparable to that of hsearch(3).
 * The 'source-code' options referred to in hsearch(3)'s 'man' page
 * are not implemented; otherwise functionality is identical.
 *
 * Compilation controls:
 * DEBUG controls some informative traces, mainly for debugging.
 * HASH_STATISTICS causes HashAccesses and HashCollisions to be maintained;
 * when combined with HASH_DEBUG, these are displayed by hdestroy().
 *
 * Problems & fixes to ejp@ausmelb.oz. WARNING: relies on pre-processor
 * concatenation property, in probably unnecessary code 'optimisation'.
 *
 * Modified margo@postgres.berkeley.edu February 1990
 * 	added multiple table interface
 * Modified by sullivan@postgres.berkeley.edu April 1990
 *      changed ctl structure for shared memory
 */
/*
    RCS INFO
    $Header$
    $Log$
    Revision 1.17  1994/02/11 21:35:44  aoki
    hash_seq never, ever could have possibly worked for the case
    where you were traversing the table and removing the current
    element at the same time.  no wonder mao couldn't get
    RelationFlushIndexes to work (see relcache.c)

 * Revision 1.16  1994/02/07  11:49:13  aoki
 * proto fixes
 *
 * Revision 1.15  1993/08/14  11:37:37  aoki
 * conditionalize mem cxt stuff so that we can use this in the frontend
 *
 * Revision 1.14  1993/08/10  01:46:51  marc
 * alpha port
 *
 * Revision 1.13  1992/08/18  18:27:16  mer
 * allow sequential walk of a hash table (better protocol for hash_seq)
 *
 * Revision 1.12  1992/03/30  00:10:43  mer
 * cut down on calls to hash functions by saving some state
 * .,
 *
 * Revision 1.11  1992/03/05  23:25:29  mer
 * feeble attempt at speed up
 *
 * Revision 1.10  91/11/12  20:20:29  mer
 * prototyping changes
 * 
 * Revision 1.9  1991/09/06  12:30:29  hong
 * added a new function, hash_seq(), which sequentially scans a hash table
 *
 * Revision 1.8  91/08/12  20:54:11  mao
 * make macro name unique
 * 
 * Revision 1.7  1991/08/06  10:44:13  mer
 * fix compile bug
 *
 * Revision 1.6  91/07/31  23:00:03  mer
 * fixes for expanding tables
 * 
 * Revision 1.5  91/07/29  16:54:33  mer
 * cleanup
 * 
 * Revision 1.4  91/04/01  08:49:32  hong
 * for debugging memory leaks.
 * 
 * Revision 1.3  91/03/07  21:56:57  kemnitz
 * Fixed log2() problem.
 * 
 * Revision 1.2  91/01/19  14:31:31  cimarron
 * made some corrections to memory allocation routines --
 * added a DynaHashCxt so that allocations not associated with
 * the caches clutter up the cache context and changed MEM_ALLOC
 * and MEM_FREE macros appropriately.  Note: the old code explicitly
 * allocated things in the CacheCxt but then used pfree() -- this
 * is an error unless you are in the CacheCxt when you call pfree()
 * because it uses the current memory context..  
 * 
 * Revision 1.1  91/01/18  22:32:39  hong
 * Initial revision
 * 
 * Revision 3.1  90/02/23  15:02:24  margo
 * New version -- tests hash interface.  Turns off debug statements but still 
 * collects hash statistics.
 * 
 * Revision 2.1  90/02/23  14:47:25  margo
 * Update revision number to mark completed revision
 * 
 * Revision 1.2  90/02/09  14:21:19  margo
 * My first rev:  multiple tables, user interface allows you to specify own hash
 * function and own table sizes.  Includes both hash and memory hash functions.
 * 
*/

# include	<stdio.h>
# include	<sys/types.h>
# include	<string.h>
# include	"tmp/postgres.h"
# include	"utils/hsearch.h"
#ifndef FRONTEND
# include	"nodes/mnodes.h"
# include	"utils/mcxt.h"
#endif /* !FRONTEND */
# include	"utils/palloc.h"
# include	"utils/log.h"

/*
 * Fast arithmetic, relying on powers of 2,
 * and on pre-processor concatenation property
 */

# define MOD(x,y)		((x) & ((y)-1))

/*
 * external routines
 */

/*
 * Private function prototypes
 */
extern long *DynaHashAlloc ARGS((unsigned int size));
extern void DynaHashFree ARGS((Pointer ptr));
extern int hash_clear ARGS((HTAB *hashp));
extern uint32 call_hash ARGS((HTAB *hashp, char *k, int len));
extern SEG_OFFSET seg_alloc ARGS((HTAB *hashp));
extern int bucket_alloc ARGS((HTAB *hashp));
extern int my_log2 ARGS((long num));

typedef long * ((*dhalloc_ptr)());

#ifndef FRONTEND
/* ----------------
 * memory allocation routines
 *
 * for postgres: all hash elements have to be in
 * the global cache context.  Otherwise the postgres
 * garbage collector is going to corrupt them. -wei
 *
 * ??? the "cache" memory context is intended to store only
 *     system cache information.  The user of the hashing
 *     routines should specify which context to use or we
 *     should create a separate memory context for these
 *     hash routines.  For now I have modified this code to
 *     do the latter -cim 1/19/91
 * ----------------
 */
GlobalMemory DynaHashCxt = (GlobalMemory) NULL;

long *
DynaHashAlloc(size)
    unsigned int size;
{
    if (! DynaHashCxt)
	DynaHashCxt = CreateGlobalMemory("DynaHash");

    return (long  *)
	MemoryContextAlloc((MemoryContext)DynaHashCxt, size);
}

void
DynaHashFree(ptr)
    Pointer ptr;
{
    MemoryContextFree((MemoryContext)DynaHashCxt, ptr);
}

#define MEM_ALLOC	DynaHashAlloc
#define MEM_FREE	DynaHashFree

#else /* FRONTEND */

#define	MEM_ALLOC	palloc
#define	MEM_FREE	pfree

#endif /* FRONTEND */

/* ----------------
 * Internal routines
 * ----------------
 */

static int expand_table();
static int hdefault();
static int init_htab();


/*
 * pointer access macros.  Shared memory implementation cannot
 * store pointers in the hash table data structures because 
 * pointer values will be different in different address spaces.
 * these macros convert offsets to pointers and pointers to offsets.
 * Shared memory need not be contiguous, but all addresses must be
 * calculated relative to some offset (segbase).
 */

#define GET_SEG(hp,seg_num)\
  (SEGMENT) (((unsigned long) (hp)->segbase) + (hp)->dir[seg_num])

#define GET_BUCKET(hp,bucket_offs)\
  (ELEMENT *) (((unsigned long) (hp)->segbase) + bucket_offs)

#define MAKE_HASHOFFSET(hp,ptr)\
  ( ((unsigned long) ptr) - ((unsigned long) (hp)->segbase) )

# if HASH_STATISTICS
static long hash_accesses, hash_collisions, hash_expansions;
# endif

/************************** CREATE ROUTINES **********************/

HTAB *
hash_create( nelem, info, flags )
int	nelem;
HASHCTL *info;
int	flags;
{
	register HHDR *	hctl;
	HTAB * 		hashp;
	

	hashp = (HTAB *) MEM_ALLOC((unsigned long) sizeof(HTAB));
	bzero(hashp,sizeof(HTAB));

	if ( flags & HASH_FUNCTION ) {
	  hashp->hash    = info->hash;
	} else {
	  /* default */
	  hashp->hash	 = string_hash;
	}

	if ( flags & HASH_SHARED_MEM )  {
	  /* ctl structure is preallocated for shared memory tables */

	  hashp->hctl     = (HHDR *) info->hctl;
	  hashp->segbase  = (char *) info->segbase; 
	  hashp->alloc    = info->alloc;
	  hashp->dir 	  = (SEG_OFFSET *)info->dir;

	  /* hash table already exists, we're just attaching to it */
	  if (flags & HASH_ATTACH) {
	    return(hashp);
	  }

	} else {
	  /* setup hash table defaults */

	  hashp->alloc	  = (dhalloc_ptr) MEM_ALLOC;
	  hashp->dir	  = NULL;
	  hashp->segbase  = NULL;

	}

	if (! hashp->hctl) {
	  hashp->hctl = (HHDR *) hashp->alloc((unsigned long)sizeof(HHDR));
	  if (! hashp->hctl) {
	    return(0);
	  }
	}
	  
	if ( !hdefault(hashp) ) return(0);
	hctl = hashp->hctl;
#ifdef HASH_STATISTICS
	hctl->accesses = hctl->collisions = 0;
#endif

	if ( flags & HASH_BUCKET )   {
	  hctl->bsize   = info->bsize;
	  hctl->bshift  = my_log2(info->bsize);
	}
	if ( flags & HASH_SEGMENT )  {
	  hctl->ssize   = info->ssize;
	  hctl->sshift  = my_log2(info->ssize);
	}
	if ( flags & HASH_FFACTOR )  {
	  hctl->ffactor = info->ffactor;
	}

	/*
	 * SHM hash tables have fixed maximum size (allocate
	 * a maximal sized directory).
	 */
	if ( flags & HASH_DIRSIZE )  {
	  hctl->max_dsize = my_log2(info->max_size);
	  hctl->dsize     = my_log2(info->dsize);
	}
	/* hash table now allocates space for key and data
	 * but you have to say how much space to allocate 
	 */
	if ( flags & HASH_ELEM ) {
	  hctl->keysize    = info->keysize; 
	  hctl->datasize   = info->datasize;
	}
	if ( flags & HASH_ALLOC )  {
	  hashp->alloc = info->alloc;
	}

	if ( init_htab (hashp, nelem ) ) {
	  hash_destroy(hashp);
	  return(0);
	}
	return(hashp);
}

/*
	Allocate and initialize an HTAB structure 
*/
static int
hdefault(hashp)
HTAB *	hashp;
{
  HHDR	*hctl;

  bzero(hashp->hctl,sizeof(HHDR));

  hctl = hashp->hctl;
  hctl->bsize    = DEF_BUCKET_SIZE;
  hctl->bshift   = DEF_BUCKET_SHIFT;
  hctl->ssize    = DEF_SEGSIZE;
  hctl->sshift   = DEF_SEGSIZE_SHIFT;
  hctl->dsize    = DEF_DIRSIZE;
  hctl->ffactor  = DEF_FFACTOR;
  hctl->nkeys    = 0;
  hctl->nsegs    = 0;

  /* I added these MS. */

  /* default memory allocation for hash buckets */
  hctl->keysize	 = sizeof(char *);
  hctl->datasize = sizeof(char *);

  /* table has no fixed maximum size */
  hctl->max_dsize = NO_MAX_DSIZE;

  /* garbage collection for HASH_REMOVE */
  hctl->freeBucketIndex = INVALID_INDEX;

  return(1);
}


static int
init_htab (hashp, nelem )
HTAB *	hashp;
int	nelem;
{
  register SEG_OFFSET	*segp;
  register int nbuckets;
  register int nsegs;
  int	l2;
  HHDR	*hctl;

  hctl = hashp->hctl;
 /*
  * Divide number of elements by the fill factor and determine a desired
  * number of buckets.  Allocate space for the next greater power of
  * two number of buckets
  */
  nelem = (nelem - 1) / hctl->ffactor + 1;

  l2 = my_log2(nelem);
  nbuckets = 1 << l2;

  hctl->max_bucket = hctl->low_mask = nbuckets - 1;
  hctl->high_mask = (nbuckets << 1) - 1;

  nsegs = (nbuckets - 1) / hctl->ssize + 1;
  nsegs = 1 << my_log2(nsegs);

  if ( nsegs > hctl->dsize ) {
    hctl->dsize  = nsegs;
  }

  /* Use two low order bits of points ???? */
  /*
    if ( !(hctl->mem = bit_alloc ( nbuckets )) ) return(-1);
    if ( !(hctl->mod = bit_alloc ( nbuckets )) ) return(-1);
   */

  /* allocate a directory */
  if (!(hashp->dir)) {
    hashp->dir = 
      (SEG_OFFSET *)hashp->alloc(hctl->dsize * sizeof(SEG_OFFSET));
    if (! hashp->dir)
      return(-1);
  }

  /* Allocate initial segments */
  for (segp = hashp->dir; hctl->nsegs < nsegs; hctl->nsegs++, segp++ ) {
    *segp = seg_alloc(hashp);
    if ( *segp == NULL ) {
      hash_destroy(hashp);
      return (0);
    }
  }

# if HASH_DEBUG
  fprintf(stderr, "%s\n%s%x\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%d\n%s%x\n%s%x\n%s%d\n%s%d\n",
		"init_htab:",
		"TABLE POINTER   ", hashp,
		"BUCKET SIZE     ", hctl->bsize,
		"BUCKET SHIFT    ", hctl->bshift,
		"DIRECTORY SIZE  ", hctl->dsize,
		"SEGMENT SIZE    ", hctl->ssize,
		"SEGMENT SHIFT   ", hctl->sshift,
		"FILL FACTOR     ", hctl->ffactor,
		"MAX BUCKET      ", hctl->max_bucket,
		"HIGH MASK       ", hctl->high_mask,
		"LOW  MASK       ", hctl->low_mask,
		"NSEGS           ", hctl->nsegs,
		"NKEYS           ", hctl->nkeys );
# endif
	return (0);
}

/********************** DESTROY ROUTINES ************************/

hash_clear( hashp )
HTAB 	*hashp;
{
  elog(NOTICE,"hash_clear not implemented\n");
}


void
hash_destroy ( hashp )
HTAB	*hashp;
{
  /* cannot destroy a shared memory hash table */
  Assert(! hashp->segbase);

  if (hashp != NULL) {
    register SEG_OFFSET 	segNum;
    SEGMENT		segp;
    int			nsegs = hashp->hctl->nsegs;
    int 		j;
    BUCKET_INDEX	*elp,p,q;
    ELEMENT		*curr;

    for (segNum =  0;  nsegs > 0; nsegs--, segNum++) {

      segp = GET_SEG(hashp,segNum);
      for (j = 0, elp = segp; j < hashp->hctl->ssize; j++, elp++) {
	for ( p = *elp; p != INVALID_INDEX; p = q ){
	  curr = GET_BUCKET(hashp,p);
	  q = curr->next;
	  MEM_FREE((char *) curr);
	}
      }
      free((char *)segp);
    }
    (void) MEM_FREE( (char *) hashp->dir);
    (void) MEM_FREE( (char *) hashp->hctl);
    hash_stats("destroy",hashp);
    (void) MEM_FREE( (char *) hashp);
  }
}

void
hash_stats(where,hashp)
char *where;
HTAB *hashp;
{
# if HASH_STATISTICS

    fprintf(stderr,"%s: this HTAB -- accesses %ld collisions %ld\n",
		where,hashp->hctl->accesses,hashp->hctl->collisions);
	
    fprintf(stderr,"hash_stats: keys %ld keysize %ld maxp %d segmentcount %d\n",
	    hashp->hctl->nkeys, hashp->hctl->keysize,
	    hashp->hctl->max_bucket, hashp->hctl->nsegs);
    fprintf(stderr,"%s: total accesses %ld total collisions %ld\n",
	    where, hash_accesses, hash_collisions);
    fprintf(stderr,"hash_stats: total expansions %ld\n",
	    hash_expansions);

# endif

}

/*******************************SEARCH ROUTINES *****************************/

uint32
call_hash ( hashp, k, len )
HTAB	*hashp;
char    *k;
int     len;
{
        long     hash_val, bucket;
	HHDR	*hctl;

	hctl = hashp->hctl;
        hash_val = hashp->hash(k, len);

        bucket = hash_val & hctl->high_mask;
        if ( bucket > hctl->max_bucket ) {
            bucket = bucket & hctl->low_mask;
        }

        return(bucket);
}

/*
 * hash_search -- look up key in table and perform action
 *
 * action is one of HASH_FIND/HASH_ENTER/HASH_REMOVE
 *
 * RETURNS: NULL if table is corrupted, a pointer to the element
 * 	found/removed/entered if applicable, TRUE otherwise.
 *	foundPtr is TRUE if we found an element in the table 
 *	(FALSE if we entered one).
 */
long *
hash_search(hashp, keyPtr, action, foundPtr)
HTAB		*hashp;
char		*keyPtr;
HASHACTION	action;		/*
				 * HASH_FIND / HASH_ENTER / HASH_REMOVE
				 * HASH_FIND_SAVE / HASH_REMOVE_SAVED
				 */
Boolean	*foundPtr;
{
	uint32 bucket;
	long segment_num;
	long segment_ndx;
	SEGMENT segp;
	register ELEMENT *curr;
	HHDR	*hctl;
	BUCKET_INDEX	currIndex;
	BUCKET_INDEX	*prevIndexPtr;
	char *		destAddr;
	static struct State {
		ELEMENT      *currElem;
		BUCKET_INDEX currIndex;
		BUCKET_INDEX *prevIndex;
	} saveState;

	Assert((hashp && keyPtr));
	Assert((action == HASH_FIND) || (action == HASH_REMOVE) || (action == HASH_ENTER) || (action == HASH_FIND_SAVE) || (action == HASH_REMOVE_SAVED));

	hctl = hashp->hctl;

# if HASH_STATISTICS
	hash_accesses++;
	hashp->hctl->accesses++;
# endif
	if (action == HASH_REMOVE_SAVED)
	{
	    curr = saveState.currElem;
	    currIndex = saveState.currIndex;
	    prevIndexPtr = saveState.prevIndex;
	    /*
	     * Try to catch subsequent errors
	     */
	    Assert(saveState.currElem && !(saveState.currElem = 0));
	}
	else
	{
	    bucket = call_hash(hashp, keyPtr, hctl->keysize);
	    segment_num = bucket >> hctl->sshift;
	    segment_ndx = bucket & ( hctl->ssize - 1 );

	    segp = GET_SEG(hashp,segment_num);

	    Assert(segp);

	    prevIndexPtr = &segp[segment_ndx];
	    currIndex = *prevIndexPtr;
 /*
  * Follow collision chain
  */
	    for (curr = NULL;currIndex != INVALID_INDEX;) {
	  /* coerce bucket index into a pointer */
	      curr = GET_BUCKET(hashp,currIndex);

	      if (! bcmp((char *)&(curr->key), keyPtr, hctl->keysize)) {
	        break;
	      } 
	      prevIndexPtr = &(curr->next);
	      currIndex = *prevIndexPtr;
# if HASH_STATISTICS
	      hash_collisions++;
	      hashp->hctl->collisions++;
# endif
	    }
	}

	/*
	 *  if we found an entry or if we weren't trying
	 * to insert, we're done now.
	 */
	*foundPtr = (Boolean) (currIndex != INVALID_INDEX);
	switch (action) {
	case HASH_ENTER:
	  if (currIndex != INVALID_INDEX)
	    return(&(curr->key));
	  break;
	case HASH_REMOVE:
	case HASH_REMOVE_SAVED:
	  if (currIndex != INVALID_INDEX) {
	    Assert(hctl->nkeys > 0);
	    hctl->nkeys--;

	    /* add the bucket to the freelist for this table.  */
	    *prevIndexPtr = curr->next;
	    curr->next = hctl->freeBucketIndex;
	    hctl->freeBucketIndex = currIndex;

	    /* better hope the caller is synchronizing access to
	     * this element, because someone else is going to reuse
	     * it the next time something is added to the table
	     */
	    return (&(curr->key));
	  }
	  return((long *) TRUE);
	case HASH_FIND:
	  if (currIndex != INVALID_INDEX)
	    return(&(curr->key));
	  return((long *)TRUE);
	case HASH_FIND_SAVE:
	  if (currIndex != INVALID_INDEX)
	  {
	      saveState.currElem = curr;
	      saveState.prevIndex = prevIndexPtr;
	      saveState.currIndex = currIndex;
	      return(&(curr->key));
	  }
	  return((long *)TRUE);
	default:
	  /* can't get here */
	  return (NULL);
	}

	/* 
	    If we got here, then we didn't find the element and
	    we have to insert it into the hash table
	*/
	Assert(currIndex == INVALID_INDEX);

	/* get the next free bucket */
	currIndex = hctl->freeBucketIndex;
	if (currIndex == INVALID_INDEX) {

	  /* no free elements.  allocate another chunk of buckets */
	  if (! bucket_alloc(hashp)) {
	    return(NULL);
	  }
	  currIndex = hctl->freeBucketIndex;
	}
	Assert(currIndex != INVALID_INDEX);

	curr = GET_BUCKET(hashp,currIndex);
	hctl->freeBucketIndex = curr->next;

	  /* link into chain */
	*prevIndexPtr = currIndex;	

 	 /* copy key and data */
	destAddr = (char *) &(curr->key);
	bcopy(keyPtr,destAddr,hctl->keysize);
	curr->next = INVALID_INDEX;

	/* let the caller initialize the data field after 
	 * hash_search returns.
	 */
/*	bcopy(keyPtr,destAddr,hctl->keysize+hctl->datasize);*/

        /*
         * Check if it is time to split the segment
         */
	if (++hctl->nkeys / (hctl->max_bucket+1) > hctl->ffactor) {
/*
	  fprintf(stderr,"expanding on '%s'\n",keyPtr);
	  hash_stats("expanded table",hashp);
*/
	  if (! expand_table(hashp))
	    return(NULL);
	}
	return (&(curr->key));
}

/*
 * hash_seq -- sequentially search through hash table and return
 *             all the elements one by one, return NULL on error and
 *	       return TRUE in the end.
 *
 */
long *
hash_seq(hashp)
HTAB		*hashp;
{
    static uint32 curBucket = 0;
    static BUCKET_INDEX curIndex;
    ELEMENT *curElem;
    long segment_num;
    long segment_ndx;
    SEGMENT segp;
    HHDR *hctl;

    if (hashp == NULL)
    {
	/*
	 * reset static state
	 */
	curBucket = 0;
	curIndex = INVALID_INDEX;
	return((long *) NULL);
    }

    hctl = hashp->hctl;
    while (curBucket <= hctl->max_bucket) {
	if (curIndex != INVALID_INDEX) {
	    curElem = GET_BUCKET(hashp, curIndex);
	    curIndex = curElem->next;
	    if (curIndex == INVALID_INDEX)	/* end of this bucket */
		++curBucket;
	    return(&(curElem->key));
	}

	/*
	 * initialize the search within this bucket.
	 */
	segment_num = curBucket >> hctl->sshift;
	segment_ndx = curBucket & ( hctl->ssize - 1 );
	
	/*
	 * first find the right segment in the table directory.
	 */
	segp = GET_SEG(hashp, segment_num);
	if (segp == NULL)
	    /* this is probably an error */
	    return((long *) NULL);
	
	/*
	 * now find the right index into the segment for the first
	 * item in this bucket's chain.  if the bucket is not empty
	 * (its entry in the dir is valid), we know this must
	 * correspond to a valid element and not a freed element
	 * because it came out of the directory of valid stuff.  if
	 * there are elements in the bucket chains that point to the
	 * freelist we're in big trouble.
	 */
	curIndex = segp[segment_ndx];

	if (curIndex == INVALID_INDEX)		/* empty bucket */
	    ++curBucket;
    }

    return((long *) TRUE);			/* out of buckets */
}


/********************************* UTILITIES ************************/
static int
expand_table(hashp)
HTAB *	hashp;
{
  	HHDR	*hctl;
	SEGMENT	old_seg,new_seg;
	long	old_bucket, new_bucket;
	long	new_segnum, new_segndx;
	long	old_segnum, old_segndx;
	long	dirsize;
	ELEMENT	*chain;
	BUCKET_INDEX *old,*newbi;
	register BUCKET_INDEX chainIndex,nextIndex;

#ifdef HASH_STATISTICS
	hash_expansions++;
#endif

	hctl = hashp->hctl;
	new_bucket = ++hctl->max_bucket;
	old_bucket = (hctl->max_bucket & hctl->low_mask);

	new_segnum = new_bucket >> hctl->sshift;
	new_segndx = MOD ( new_bucket, hctl->ssize );

	if ( new_segnum >= hctl->nsegs ) {
	  
	  /* Allocate new segment if necessary */
	  if (new_segnum >= hctl->dsize) {
	    (void) dir_realloc(hashp);
	  }
	  if (! (hashp->dir[new_segnum] = seg_alloc(hashp))) {
	    return (0);
	  }
	  hctl->nsegs++;
	}


	if ( new_bucket > hctl->high_mask ) {
	    /* Starting a new doubling */
	    hctl->low_mask = hctl->high_mask;
	    hctl->high_mask = new_bucket | hctl->low_mask;
	}

	/*
	 * Relocate records to the new bucket
	 */
	old_segnum = old_bucket >> hctl->sshift;
	old_segndx = MOD(old_bucket, hctl->ssize);

        old_seg = GET_SEG(hashp,old_segnum);
        new_seg = GET_SEG(hashp,new_segnum);

	old = &old_seg[old_segndx];
	newbi = &new_seg[new_segndx];
	for (chainIndex = *old; 
	     chainIndex != INVALID_INDEX;
	     chainIndex = nextIndex){

	  chain = GET_BUCKET(hashp,chainIndex);
	  nextIndex = chain->next;
	  if ( call_hash(hashp,
			 (char *)&(chain->key),
			 hctl->keysize) == old_bucket ) {
		*old = chainIndex;
		old = &chain->next;
	  } else {
		*newbi = chainIndex;
		newbi = &chain->next;
	  }
	    chain->next = INVALID_INDEX;
	}
	return (1);
}


static int
dir_realloc ( hashp )
HTAB *	hashp;
{
	register char	*p;
	char	**p_ptr;
	long	old_dirsize;
	long	new_dirsize;


	if (hashp->hctl->max_dsize != NO_MAX_DSIZE) 
	  return (0);

	/* Reallocate directory */
	old_dirsize = hashp->hctl->dsize * sizeof ( SEGMENT * );
	new_dirsize = old_dirsize << 1;

	p_ptr = (char **) hashp->dir;
	p = (char *) hashp->alloc((unsigned long) new_dirsize );
	if (p != NULL) {
	  bcopy ( *p_ptr, p, old_dirsize );
	  bzero ( *p_ptr + old_dirsize, new_dirsize-old_dirsize );
	  (void) free( (char *)*p_ptr);
	  *p_ptr = p;
	  hashp->hctl->dsize = new_dirsize;
	  return(1);
	} 
	return (0);

}


SEG_OFFSET
seg_alloc(hashp)
HTAB * hashp;
{
  SEGMENT segp;
  SEG_OFFSET segOffset;
  

  segp = (SEGMENT) hashp->alloc((unsigned long) 
	    sizeof(SEGMENT)*hashp->hctl->ssize);

  if (! segp) {
    return(0);
  }

  bzero((char *)segp,
	(long) sizeof(SEGMENT)*hashp->hctl->ssize);

  segOffset = MAKE_HASHOFFSET(hashp,segp);
  return(segOffset);
}

/*
 * allocate some new buckets and link them into the free list
 */
bucket_alloc(hashp)
HTAB *hashp;
{
  int i;
  ELEMENT *tmpBucket;
  long bucketSize;
  BUCKET_INDEX tmpIndex,lastIndex;

  bucketSize = 
    sizeof(BUCKET_INDEX) + hashp->hctl->keysize + hashp->hctl->datasize;

  /* make sure its aligned correctly */
  bucketSize += sizeof(long *) - (bucketSize % sizeof(long *));

  /* tmpIndex is the shmem offset into the first bucket of
   * the array.
   */
  tmpBucket = (ELEMENT *)
    hashp->alloc((unsigned long) BUCKET_ALLOC_INCR*bucketSize);

  if (! tmpBucket) {
    return(0);
  }

  tmpIndex = MAKE_HASHOFFSET(hashp,tmpBucket);

  /* set the freebucket list to point to the first bucket */
  lastIndex = hashp->hctl->freeBucketIndex;
  hashp->hctl->freeBucketIndex = tmpIndex;

  /* initialize each bucket to point to the one behind it */
  for (i=0;i<(BUCKET_ALLOC_INCR-1);i++) {
    tmpBucket = GET_BUCKET(hashp,tmpIndex);
    tmpIndex += bucketSize;
    tmpBucket->next = tmpIndex;
  }
  
  /* the last bucket points to the old freelist head (which is
   * probably invalid or we wouldnt be here) 
   */
  tmpBucket->next = lastIndex;

  return(1);
}

/* calculate the log base 2 of num */
my_log2( num )
long	num;
{
    int		i = 1;
    int		limit;

    for ( i = 0, limit = 1; limit < num; limit = 2 * limit, i++ );
    return (i);
}
