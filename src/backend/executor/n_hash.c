/* ----------------------------------------------------------------
 *   FILE
 *	hash.c
 *	
 *   DESCRIPTION
 *	Routines to hash relations for hashjoin
 *
 *   INTERFACE ROUTINES
 *     	ExecHash	- generate an in-memory hash table of the relation 
 *     	ExecInitHash	- initialize node and subnodes..
 *     	ExecEndHash	- shutdown node and subnodes
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include <math.h>
#include <sys/file.h>
#include "storage/ipci.h"
#include "storage/bufmgr.h"	/* for BLCKSZ */
#include "tcop/slaves.h"
#include "executor/executor.h"

 RcsId("$Header$");


extern int NBuffers;

/* ----------------------------------------------------------------
 *   	ExecHash
 *
 *	build hash table for hashjoin, all do partitioning if more
 *	than one batches are required.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot
ExecHash(node)
Hash node;
{
    EState	  estate;
    HashState	  hashstate;
    Plan 	  outerNode;
    Var	  	  hashkey;
    HashJoinTable hashtable;
    HeapTuple     heapTuple;
    TupleTableSlot slot;
    ExprContext	  econtext;

    int		  nbatch;
    File	  *batches;
    RelativeAddr  *batchPos;
    int		  *batchSizes;
    char	  *tempname;
    int		  i;
    RelativeAddr  *innerbatchNames;
    
    /* ----------------
     *	get state info from node
     * ----------------
     */
    
    hashstate =   get_hashstate(node);
    estate =      (EState) get_state((Plan) node);
    outerNode =   get_outerPlan((Plan) node);

    if (!IsMaster && ParallelExecutorEnabled()) {
	IpcMemoryId  shmid;
	IpcMemoryKey hashtablekey;
	int	     hashtablesize;
	int	     tmpfd;

	hashtablekey = get_hashtablekey(node);
	hashtablesize = get_hashtablesize(node);
	
	/* ----------------
	 *  in Sequent version, shared memory is implemented by 
	 *  memory mapped files, it takes one file descriptor.
	 *  we may have to free one for this.
	 * ----------------
	 */
	closeOneVfd();
        shmid = IpcMemoryCreateWithoutOnExit(hashtablekey,
				            hashtablesize,
				            HASH_PERMISSION);
        hashtable = (HashJoinTable) IpcMemoryAttach(shmid);
	set_hashtable(node, hashtable);
    }
    
    hashtable =	get_hashtable(node);
    if (hashtable == NULL)
	elog(WARN, "ExecHash: hash table is NULL.");
    
    nbatch = hashtable->nbatch;
    
    if (nbatch > 0) {  /* if needs hash partition */
	innerbatchNames = (RelativeAddr *) ABSADDR(hashtable->innerbatchNames);

	/* --------------
	 *  allocate space for the file descriptors of batch files
	 *  then open the batch files in the current processes.
	 * --------------
	 */
	batches = (File*)palloc(nbatch * sizeof(File));
	for (i=0; i<nbatch; i++) {
	    batches[i] = FileNameOpenFile(ABSADDR(innerbatchNames[i]),
					 O_CREAT | O_RDWR, 0600);
	}
	set_hashBatches(hashstate, batches);
        batchPos = (RelativeAddr*) ABSADDR(hashtable->innerbatchPos);
        batchSizes = (int*) ABSADDR(hashtable->innerbatchSizes);
    }

    /* ----------------
     *	set expression context
     * ----------------
     */
    hashkey = get_hashkey(node);
    econtext = get_cs_ExprContext((CommonState) hashstate);

    /* ----------------
     *	get tuple and insert into the hash table
     * ----------------
     */
    for (;;) {
	slot = ExecProcNode(outerNode);
	if (TupIsNull((Pointer)slot))
	    break;
	
	set_ecxt_innertuple(econtext, slot);
	ExecHashTableInsert(hashtable, econtext, hashkey, 
			    get_hashBatches(hashstate));

	ExecClearTuple((Pointer) slot);
    }
    
    /*
     * end of build phase, flush all the last pages of the batches.
     */
    for (i=0; i<nbatch; i++) {
	if (FileSeek(batches[i], 0L, L_XTND) < 0)
	    perror("FileSeek");
	if (FileWrite(batches[i],ABSADDR(hashtable->batch)+i*BLCKSZ,BLCKSZ) < 0)
	    perror("FileWrite");
	NDirectFileWrite++;
   }
    
    /* ---------------------
     *  Return the slot so that we have the tuple descriptor 
     *  when we need to save/restore them.  -Jeff 11 July 1991
     * ---------------------
     */
    return slot;
}
 
/* ----------------------------------------------------------------
 *   	ExecInitHash
 *
 *   	Init routine for Hash node
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List
ExecInitHash(node, estate, parent)
    Hash 	node;
    EState 	estate;
    Plan	parent;
{
    HashState		hashstate;
    Plan		outerPlan;
    
    SO1_printf("ExecInitHash: %s\n",
	       "initializing hash node");
    
    /* ----------------
     *  assign the node's execution state
     * ----------------
     */
    set_state((Plan) node,  (EStatePtr)estate);
    
    /* ----------------
     * create state structure
     * ----------------
     */
    hashstate = MakeHashState(NULL);
    set_hashstate(node, hashstate);
    set_hashBatches(hashstate, NULL);
    
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, (BaseNode) hashstate, parent);
    ExecAssignDebugHooks((Plan) node, (BaseNode) hashstate);
    ExecAssignExprContext(estate, (CommonState) hashstate);
    
    /* ----------------
     * initialize our result slot
     * ----------------
     */
    ExecInitResultTupleSlot(estate, (CommonState) hashstate);
    
    /* ----------------
     * initializes child nodes
     * ----------------
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecInitNode(outerPlan, estate, (Plan) node);
    
    /* ----------------
     * 	initialize tuple type. no need to initialize projection
     *  info because this node doesn't do projections
     * ----------------
     */
    ExecAssignResultTypeFromOuterPlan((Plan) node, (CommonState) hashstate);
    set_cs_ProjInfo((CommonState) hashstate, NULL);

    return
	LispTrue;
}
 
/* ---------------------------------------------------------------
 *   	ExecEndHash
 *
 *	clean up routine for Hash node
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndHash(node)
    Hash node;
{
    HashState		hashstate;
    Plan		outerPlan;
    File		*batches;
    
    /* ----------------
     *	get info from the hash state 
     * ----------------
     */
    hashstate = get_hashstate(node);
    batches = get_hashBatches(hashstate);
    if (batches != NULL)
       pfree(batches);

    /* ----------------
     *	free projection info.  no need to free result type info
     *  because that came from the outer plan...
     * ----------------
     */
    ExecFreeProjectionInfo((CommonState) hashstate);
    
    /* ----------------
     *	shut down the subplan
     * ----------------
     */
    outerPlan = get_outerPlan((Plan) node);
    ExecEndNode(outerPlan);
} 

RelativeAddr
hashTableAlloc(size, hashtable)
int size;
HashJoinTable hashtable;
{
    RelativeAddr p;
    p = hashtable->top;
    hashtable->top += size;
    return p;
}

/* ----------------------------------------------------------------
 *	ExecHashTableCreate
 *
 *	create a hashtable in shared memory for hashjoin.
 * ----------------------------------------------------------------
 */
#define NTUP_PER_BUCKET		10
#define FUDGE_FAC		1.5

HashJoinTable
ExecHashTableCreate(node)
    Plan node;
{
    Plan	  outerNode;
    int		  nbatch;
    int 	  ntuples;
    int 	  tupsize;
    IpcMemoryId   shmid;
    HashJoinTable hashtable;
    HashBucket 	  bucket;
    int 	  nbuckets;
    int		  totalbuckets;
    int 	  bucketsize;
    int 	  i;
    int		  tmpfd;
#ifdef sequent
    slock_t       *batchLock;
#endif
    RelativeAddr  *outerbatchNames;
    RelativeAddr  *outerbatchPos;
    RelativeAddr  *innerbatchNames;
    RelativeAddr  *innerbatchPos;
    int		  *innerbatchSizes;
    RelativeAddr  tempname;

    /*
     * determine number of batches for the hashjoin
     */
    nbatch = ExecHashPartition((Hash) node);
    if (nbatch < 0)
       elog(WARN, "not enough memory for hashjoin.");
    /* ----------------
     *	get information about the size of the relation
     * ----------------
     */
    outerNode = get_outerPlan(node);
    ntuples = get_plan_size(outerNode);
    if (ntuples <= 0)
	ntuples = 100;  /* XXX just a hack */
    tupsize = get_plan_width(outerNode) + sizeof(HeapTupleData);

    /*
     * totalbuckets is the total number of hash buckets needed for
     * the entire relation
     */
    totalbuckets = ceil((double)ntuples/NTUP_PER_BUCKET);
    bucketsize = NTUP_PER_BUCKET * tupsize + sizeof(*bucket);
    
    /*
     * nbuckets is the number of hash buckets for the first pass
     * of hybrid hashjoin
     */
    nbuckets = (NBuffers - nbatch) * BLCKSZ / (bucketsize * FUDGE_FAC);
    if (totalbuckets < nbuckets)
       totalbuckets = nbuckets;
    if (nbatch == 0)
       nbuckets = totalbuckets;
#ifdef HJDEBUG
    printf("nbatch = %d, totalbuckets = %d, nbuckets = %d\n", nbatch, totalbuckets, nbuckets);
#endif

#ifdef sequent
    /* ----------------
     *	in Sequent version, shared memory is implemented by 
     *  memory mapped files, it takes one file descriptor.
     *  we may have to free one for this.
     * ----------------
     */
    closeOneVfd();
    if (ParallelExecutorEnabled()) {
       IpcMemoryKey hashtablekey;
       int	    hashtablesize;
       hashtablekey = get_hashtablekey((Hash)node);
       hashtablesize = (NBuffers+SlaveLocalInfoD.nparallel)*BLCKSZ;
       shmid = IpcMemoryCreateWithoutOnExit(hashtablekey,
				            hashtablesize,
				            HASH_PERMISSION);
       set_hashtablesize((Hash)node, hashtablesize);
      }
    else
       shmid = IpcMemoryCreateWithoutOnExit(PrivateIPCKey,
				     (NBuffers+1)*BLCKSZ,HASH_PERMISSION);
    hashtable = (HashJoinTable) IpcMemoryAttach(shmid);
#else /* sequent */
    /* ----------------
     *  in non-parallel machines, we don't need to put the hash table
     *  in the shared memory.  We just malloc it.
     * ----------------
     */
    hashtable = (HashJoinTable)malloc((NBuffers+1)*BLCKSZ);
#endif /* sequent */

    if (hashtable == NULL) {
	elog(WARN, "not enough memory for hashjoin.");
      }
    /* ----------------
     *	initialize the hash table header
     * ----------------
     */
    hashtable->nbuckets = nbuckets;
    hashtable->totalbuckets = totalbuckets;
    hashtable->bucketsize = bucketsize;
    hashtable->shmid = shmid;
    hashtable->top = sizeof(HashTableData);
#ifdef sequent
    S_INIT_LOCK(&(hashtable->overflowlock));
#endif
    hashtable->bottom = NBuffers * BLCKSZ;
    /*
     *  hashtable->readbuf has to be long aligned!!!
     */
    hashtable->readbuf = hashtable->bottom;
    hashtable->nbatch = nbatch;
    hashtable->curbatch = 0;
    hashtable->pcount = hashtable->nprocess = SlaveLocalInfoD.nparallel;
#ifdef sequent
    S_INIT_BARRIER(&(hashtable->batchBarrier), hashtable->nprocess);
    S_INIT_LOCK(&(hashtable->tableLock));
#endif
    if (nbatch > 0) {
#ifdef sequent
	/* ---------------
	 *  allocate and initialize locks for each batch
	 * ---------------
	 */
	batchLock = (slock_t*)ABSADDR(
		       hashTableAlloc(nbatch * sizeof(slock_t), hashtable));
	for (i=0; i<nbatch; i++) 
	    S_INIT_LOCK(&(batchLock[i]));
	hashtable->batchLock = RELADDR(batchLock);
#endif
	/* ---------------
	 *  allocate and initialize the outer batches
	 * ---------------
	 */
	outerbatchNames = (RelativeAddr*)ABSADDR(
		hashTableAlloc(nbatch * sizeof(RelativeAddr), hashtable));
	outerbatchPos = (RelativeAddr*)ABSADDR(
		    hashTableAlloc(nbatch * sizeof(RelativeAddr), hashtable));
	for (i=0; i<nbatch; i++) {
	    tempname = hashTableAlloc(12, hashtable);
	    mk_hj_temp(ABSADDR(tempname));
	    outerbatchNames[i] = tempname;
	    outerbatchPos[i] = -1;
	  }
	hashtable->outerbatchNames = RELADDR(outerbatchNames);
	hashtable->outerbatchPos = RELADDR(outerbatchPos);
	/* ---------------
	 *  allocate and initialize the inner batches
	 * ---------------
	 */
	innerbatchNames = (RelativeAddr*)ABSADDR(
		   hashTableAlloc(nbatch * sizeof(RelativeAddr), hashtable));
	innerbatchPos = (RelativeAddr*)ABSADDR(
		   hashTableAlloc(nbatch * sizeof(RelativeAddr), hashtable));
	innerbatchSizes = (int*)ABSADDR(
			     hashTableAlloc(nbatch * sizeof(int), hashtable));
	for (i=0; i<nbatch; i++) {
	    tempname = hashTableAlloc(12, hashtable);
	    mk_hj_temp(ABSADDR(tempname));
	    innerbatchNames[i] = tempname;
	    innerbatchPos[i] = -1;
	    innerbatchSizes[i] = 0;
	  }
	hashtable->innerbatchNames = RELADDR(innerbatchNames);
	hashtable->innerbatchPos = RELADDR(innerbatchPos);
	hashtable->innerbatchSizes = RELADDR(innerbatchSizes);
    }
    else {
	hashtable->outerbatchNames = (RelativeAddr)NULL;
	hashtable->outerbatchPos = (RelativeAddr)NULL;
	hashtable->innerbatchNames = (RelativeAddr)NULL;
	hashtable->innerbatchPos = (RelativeAddr)NULL;
	hashtable->innerbatchSizes = (RelativeAddr)NULL;
    }

    hashtable->batch = (RelativeAddr)LONGALIGN(hashtable->top + 
					       bucketsize * nbuckets);
    hashtable->overflownext=hashtable->batch + nbatch * BLCKSZ;
    /* ----------------
     *	initialize each hash bucket
     * ----------------
     */
    bucket = (HashBucket)ABSADDR(hashtable->top);
    for (i=0; i<nbuckets; i++) {
	bucket->top = RELADDR((char*)bucket + sizeof(*bucket));
	bucket->bottom = bucket->top;
	bucket->firstotuple = bucket->lastotuple = -1;
#ifdef sequent
	S_INIT_LOCK(&(bucket->bucketlock));
#endif
	bucket = (HashBucket)((char*)bucket + bucketsize);
    }
    return(hashtable);
}

/* ----------------------------------------------------------------
 *	ExecHashTableInsert
 *
 *	insert a tuple into the hash table depending on the hash value
 *	it may just go to a tmp file for other batches
 * ----------------------------------------------------------------
 */
void
ExecHashTableInsert(hashtable, econtext, hashkey, batches)
    HashJoinTable hashtable;
    ExprContext   econtext;
    Var 	  hashkey;
    File	  *batches;
{
    TupleTableSlot	slot;
    HeapTuple 		heapTuple;
    Const 		hashvalue;
    HashBucket 		bucket;
    int			bucketno;
    int			nbatch;
    int			batchno;
    char		*buffer;
    RelativeAddr	*batchPos;
    int			*batchSizes;
#ifdef sequent
    slock_t		*batchLock;
#endif
    char		*pos;

    nbatch = 		hashtable->nbatch;
    batchPos = 		(RelativeAddr*)ABSADDR(hashtable->innerbatchPos);
    batchSizes = 	(int*)ABSADDR(hashtable->innerbatchSizes);
#ifdef sequent
    batchLock = 	(slock_t*)ABSADDR(hashtable->batchLock);
#endif

    slot = get_ecxt_innertuple(econtext);
    heapTuple = (HeapTuple) CAR(slot);
    
#ifdef HJDEBUG
    printf("Inserting ");
#endif
    
    bucketno = ExecHashGetBucket(hashtable, econtext, hashkey);

    /* ----------------
     *	decide whether to put the tuple in the hash table or a tmp file
     * ----------------
     */
    if (bucketno < hashtable->nbuckets) {
	/* ---------------
	 *  put the tuple in hash table
	 * ---------------
	 */
	bucket = (HashBucket)
	    (ABSADDR(hashtable->top) + bucketno * hashtable->bucketsize);
#ifdef sequent
	/* --------------
	 *  lock the hash bucket
	 * ---------------
	 */
        if (ParallelExecutorEnabled())
	   S_LOCK(&(bucket->bucketlock));
#endif
	if ((char*)LONGALIGN(ABSADDR(bucket->bottom))
	     -(char*)bucket+heapTuple->t_len > hashtable->bucketsize)
	    ExecHashOverflowInsert(hashtable, bucket, heapTuple);
	else {
	    bcopy(heapTuple,(char*)LONGALIGN(ABSADDR(bucket->bottom)),
		  heapTuple->t_len); 
	    bucket->bottom = 
		((RelativeAddr)LONGALIGN(bucket->bottom) + heapTuple->t_len);
	}
#ifdef sequent
	/* --------------
	 *  insertion done, unlock the hash bucket
	 * --------------
	 */
        if (ParallelExecutorEnabled())
	   S_UNLOCK(&(bucket->bucketlock));
#endif
    }
    else {
	/* -----------------
	 * put the tuple into a tmp file for other batches
	 * -----------------
	 */
	batchno = (float)(bucketno - hashtable->nbuckets)/
		  (float)(hashtable->totalbuckets - hashtable->nbuckets)
		  * nbatch;
	buffer = ABSADDR(hashtable->batch) + batchno * BLCKSZ;
#ifdef sequent
	/* ----------------
	 *  lock the batch
	 * ----------------
	 */
	if (ParallelExecutorEnabled())
	    S_LOCK(&(batchLock[batchno]));
#endif
	batchSizes[batchno]++;
	pos= (char *)
	    ExecHashJoinSaveTuple(heapTuple,
				  buffer,
				  batches[batchno],
				  ABSADDR(batchPos[batchno]));
	batchPos[batchno] = RELADDR(pos);
#ifdef sequent
	/* -----------------
	 *  unlock the batch
	 * -----------------
	 */
	if (ParallelExecutorEnabled())
	    S_UNLOCK(&(batchLock[batchno]));
#endif
    }
}

/* ----------------------------------------------------------------
 *	ExecHashTableDestroy
 *
 *	destroy a hash table
 * ----------------------------------------------------------------
 */
void
ExecHashTableDestroy(hashtable)
    HashJoinTable hashtable;
{
#ifdef sequent
    IPCPrivateMemoryKill(0, hashtable->shmid);
#else /* sequent */
    free((char *)hashtable);
#endif /* sequent */
}

/* ----------------------------------------------------------------
 *	ExecHashGetBucket
 *
 *	Get the hash value for a tuple
 * ----------------------------------------------------------------
 */
int
ExecHashGetBucket(hashtable, econtext, hashkey)
    HashJoinTable hashtable;
    ExprContext   econtext;
    Var 	  hashkey;
{
    int 	bucketno;
    HashBucket 	bucket;
    Const 	hashvalue;
    Datum 	keyval;
	extern Boolean execConstByVal;
	extern int execConstLen;
	Boolean isNull;


    /* ----------------
     *	Get the join attribute value of the tuple
     * ----------------
     */
    keyval = ExecEvalVar(hashkey, econtext, &isNull);
    
    /* ------------------
     *  compute the hash function
     * ------------------
     */
    if (execConstByVal)
        bucketno =
	    hashFunc((char *) &keyval, execConstLen) % hashtable->totalbuckets;
    else
        bucketno =
	    hashFunc((char *) keyval, execConstLen) % hashtable->totalbuckets;
#ifdef HJDEBUG
    if (bucketno >= hashtable->nbuckets)
       printf("hash(%d) = %d SAVED\n", keyval, bucketno);
    else
       printf("hash(%d) = %d\n", keyval, bucketno);
#endif
 
    return(bucketno);
}

/* ----------------------------------------------------------------
 *	ExecHashOverflowInsert
 *
 *	insert into the overflow area of a hash bucket
 * ----------------------------------------------------------------
 */
void
ExecHashOverflowInsert(hashtable, bucket, heapTuple)
    HashJoinTable hashtable;
    HashBucket 	  bucket;
    HeapTuple 	  heapTuple;
{
    OverflowTuple otuple;
    RelativeAddr  newend;
    OverflowTuple firstotuple;
    OverflowTuple lastotuple;

    firstotuple = (OverflowTuple)ABSADDR(bucket->firstotuple);
    lastotuple = (OverflowTuple)ABSADDR(bucket->lastotuple);
#ifdef sequent
    if (ParallelExecutorEnabled()) {
        /* -----------------
         *  Here we assume that the current bucket lock has already
	 *  been acquried.  But we still need to lock the overflow lock.
	 *  This could be a potential bottleneck.
	 * -----------------
	 */
	S_LOCK(&(hashtable->overflowlock));
       }
#endif
    /* ----------------
     *	see if we run out of overflow space
     * ----------------
     */
    newend = (RelativeAddr)LONGALIGN(hashtable->overflownext + sizeof(*otuple))
	     + heapTuple->t_len ;
    if (newend > hashtable->bottom) {
#ifdef sequent
	elog(WARN, "hash table out of memory.");
#else
	elog(NOTICE, "hash table out of memory. expanding.");
	/* ------------------
	 * XXX this is a temporary hack
	 * eventually, recursive hash partitioning will be 
	 * implemented
	 * ------------------
	 */
	hashtable->readbuf = hashtable->bottom = 2 * hashtable->bottom;
	hashtable = (HashJoinTable)realloc(hashtable, hashtable->bottom+BLCKSZ);
	if (hashtable == NULL) {
	    perror("realloc");
	    elog(WARN, "can't expand hashtable.");
	  }
#endif
    }
    
    /* ----------------
     *	establish the overflow chain
     * ----------------
     */
    otuple = (OverflowTuple)ABSADDR(hashtable->overflownext);
    hashtable->overflownext = newend;
#ifdef sequent
    /* ------------------
     *  unlock the overflow chain
     * ------------------
     */
    if (ParallelExecutorEnabled())
	S_UNLOCK(&(hashtable->overflowlock));
#endif
    if (firstotuple == NULL)
	bucket->firstotuple = bucket->lastotuple = RELADDR(otuple);
    else {
	lastotuple->next = RELADDR(otuple);
	bucket->lastotuple = RELADDR(otuple);
     }
    
    /* ----------------
     *	copy the tuple into the overflow area
     * ----------------
     */
    otuple->next = -1;
    otuple->tuple = RELADDR(LONGALIGN(((char*)otuple + sizeof(*otuple))));
    bcopy(heapTuple, ABSADDR(otuple->tuple), heapTuple->t_len);
}

/* ----------------------------------------------------------------
 *	ExecScanHashBucket
 *
 *	scan a hash bucket of matches
 * ----------------------------------------------------------------
 */
HeapTuple
ExecScanHashBucket(hjstate, bucket, curtuple, hjclauses, econtext)
    HashJoinState 	hjstate;
    HashBucket 		bucket;
    HeapTuple 		curtuple;
    List 		hjclauses;
    ExprContext 	econtext;
{
    HeapTuple 		heapTuple;
    bool 		qualResult;
    OverflowTuple 	otuple = NULL;
    OverflowTuple 	curotuple;
    TupleTableSlot	inntuple;
    OverflowTuple	firstotuple;
    OverflowTuple	lastotuple;
    HashJoinTable	hashtable;

    hashtable = get_hj_HashTable(hjstate);
    firstotuple = (OverflowTuple)ABSADDR(bucket->firstotuple);
    lastotuple = (OverflowTuple)ABSADDR(bucket->lastotuple);
    
    /* ----------------
     *	search the hash bucket
     * ----------------
     */
    if (curtuple == NULL || curtuple < (HeapTuple)ABSADDR(bucket->bottom)) {
        if (curtuple == NULL)
	    heapTuple = (HeapTuple)
		LONGALIGN(ABSADDR(bucket->top));
	else 
	    heapTuple = (HeapTuple)
		LONGALIGN(((char*)curtuple+curtuple->t_len));
	
        while (heapTuple < (HeapTuple)ABSADDR(bucket->bottom)) {
	
	    inntuple = (TupleTableSlot)
		ExecStoreTuple((Pointer) heapTuple,	/* tuple to store */
			       (Pointer) get_hj_HashTupleSlot(hjstate),
			       /* slot */
			       InvalidBuffer,	/* tuple has no buffer */
			       false);		/* do not pfree this tuple */
	    
	    set_ecxt_innertuple(econtext, inntuple);
	    qualResult = ExecQual(hjclauses, econtext);
	    
	    if (qualResult)
		return heapTuple;
	   
	    heapTuple = (HeapTuple)
		LONGALIGN(((char*)heapTuple+heapTuple->t_len));
	}
	
	if (firstotuple == NULL)
	    return NULL;
	otuple = firstotuple;
    }

    /* ----------------
     *	search the overflow area of the hash bucket
     * ----------------
     */
    if (otuple == NULL) {
	curotuple = get_hj_CurOTuple(hjstate);
	otuple = (OverflowTuple)ABSADDR(curotuple->next);
    }

    while (otuple != NULL) {
	heapTuple = (HeapTuple)ABSADDR(otuple->tuple);
	
	inntuple = (TupleTableSlot)
	    ExecStoreTuple((Pointer) heapTuple,	  /* tuple to store */
			   (Pointer) get_hj_HashTupleSlot(hjstate), /* slot */
			   InvalidBuffer, /* SP?? this tuple has no buffer */
			   false);	  /* do not pfree this tuple */

	set_ecxt_innertuple(econtext, inntuple);
	qualResult = ExecQual(hjclauses, econtext);
	
	if (qualResult) {
	    set_hj_CurOTuple(hjstate, otuple);
	    return heapTuple;
	}
	 
	otuple = (OverflowTuple)ABSADDR(otuple->next);
    }
    
    /* ----------------
     *	no match
     * ----------------
     */
    return NULL;
}

/* ----------------------------------------------------------------
 *	hashFunc
 *
 *	the hash function, copied from Margo
 * ----------------------------------------------------------------
 */

int
hashFunc(key,len)
    char *key;
    int len;
{
    register unsigned int h;
    register int l;
    register unsigned char *k;

    /*
     * If this is a variable length type, then 'k' points
     * to a "struct varlena" and len == -1.
     * NOTE:
     * VARSIZE returns the "real" data length plus the sizeof the
     * "vl_len" attribute of varlena (the length information).
     * 'k' points to the beginning of the varlena struct, so
     * we have to use "VARDATA" to find the beginning of the "real"
     * data.
     */
    if (len == -1) {
	l = VARSIZE(key) - sizeof(long);	/* 'varlena.vl_len' is a long*/
	k = (unsigned char*) VARDATA(key);
    } else {
	l = len;
	k = (unsigned char *) key;
    }

    h = 0;
    
    /*
     * Convert string to integer
     */
    while (l--) h = h * PRIME1 ^ (*k++);
    h %= PRIME2;

    return (h);
}

/* ----------------------------------------------------------------
 *	ExecHashPartition
 *
 *	determine the number of batches needed for a hashjoin
 * ----------------------------------------------------------------
 */

int
ExecHashPartition(node)
Hash node;
{
    Plan	outerNode;
    int		b;
    int		pages;
    int		ntuples;
    int		tupsize;

    /*
     * get size information for plan node
     */
    outerNode = get_outerPlan((Plan) node);
    ntuples = get_plan_size(outerNode);
    tupsize = get_plan_width(outerNode) + sizeof(HeapTupleData);
    pages = ceil((double)ntuples * tupsize * FUDGE_FAC / BLCKSZ);

    /*
     * if amount of buffer space below hashjoin threshold,
     * return negative
     */
    if (ceil(sqrt((double)pages)) > NBuffers)
       return -1;
    if (pages <= NBuffers)
       b = 0;  /* fit in memory, no partitioning */
    else
       b = ceil((double)(pages - NBuffers)/(double)(NBuffers - 1));

    return b;
}

/* ----------------------------------------------------------------
 *	ExecHashTableReset
 *
 *	reset hash table header for new batch
 * ----------------------------------------------------------------
 */

void
ExecHashTableReset(hashtable, ntuples)
HashJoinTable hashtable;
int ntuples;
{
    int i;
    HashBucket bucket;

    hashtable->nbuckets = hashtable->totalbuckets
    = ceil((double)ntuples/NTUP_PER_BUCKET);

    hashtable->overflownext = hashtable->top + hashtable->bucketsize *
			      hashtable->nbuckets;

    bucket = (HashBucket)ABSADDR(hashtable->top);
    for (i=0; i<hashtable->nbuckets; i++) {
       bucket->top = RELADDR((char*)bucket + sizeof(*bucket));
       bucket->bottom = bucket->top;
       bucket->firstotuple = bucket->lastotuple = -1;
#ifdef sequent
        S_INIT_LOCK(&(bucket->bucketlock));
#endif
       bucket = (HashBucket)((char*)bucket + hashtable->bucketsize);
    }
   hashtable->pcount = hashtable->nprocess;
}

static int hjtmpcnt = 0;

void
mk_hj_temp(tempname)
char *tempname;
{
    sprintf(tempname, "HJ%d.%d", getpid(), hjtmpcnt);
    hjtmpcnt = (hjtmpcnt + 1) % 1000;
}

print_buckets(hashtable)
HashJoinTable hashtable;
{
   int i;
   HashBucket bucket;
   bucket = (HashBucket)ABSADDR(hashtable->top);
   for (i=0; i<hashtable->nbuckets; i++) {
      printf("bucket%d = (top = %d, bottom = %d, firstotuple = %d, lastotuple = %d)\n", i, bucket->top, bucket->bottom, bucket->firstotuple, bucket->lastotuple);
      bucket = (HashBucket)((char*)bucket + hashtable->bucketsize);
    }
}
