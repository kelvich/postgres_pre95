/* ----------------------------------------------------------------
 *   FILE
 *	hashjoin.c
 *	
 *   DESCRIPTION
 *	Routines to handle hash join nodes
 *
 *   INTERFACE ROUTINES
 *     	ExecHashJoin
 *     	ExecInitHashJoin
 *     	ExecEndHashJoin
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include <sys/file.h>

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) execdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h
 *	6) externs.h comes last
 * ----------------
 */
#include "executor/execdebug.h"

#include "planner/clauses.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"

#include "executor/execdefs.h"
#include "executor/execmisc.h"

#include "executor/externs.h"
#include "executor/x_hashjoin.h"

#include "storage/bufmgr.h"	/* for BLCKSZ */
#include "tcop/slaves.h"

/* ----------------------------------------------------------------
 *   	ExecHashJoin
 *
 *	This function implements the Hybrid Hashjoin algorithm.
 *	recursive partitioning remains to be added.
 *	Note: the relation we build hash table on is the inner
 *	      the other one is outer.
 * ----------------------------------------------------------------
 */

/**** xxref:
 *           ExecProcNode
 ****/
TupleTableSlot				/* return: a tuple or LispNil */
ExecHashJoin(node)
    HashJoin node;			/* the hash join node */
{
    HashJoinState	hjstate;
    HashState		hashstate;
    EState		estate;
    Plan 	  	outerNode;
    Plan		hashNode;
    List		hjclauses;
    List		clause;
    List		qual;
    ScanDirection 	dir;
    TupleTableSlot	inntuple;
    Var			outerVar;
    ExprContext		econtext;

    HashJoinTable	hashtable;
    int			bucketno;
    HashBucket		bucket;
    HeapTuple		curtuple;

    bool		qualResult;

    TupleDescriptor	tupType;
    Pointer		tupValue;
    List		targetList;
    int			len;
    
    TupleTableSlot	outerTupleSlot;
    TupleTableSlot	innerTupleSlot;
    int			nbatch;
    int			curbatch;
    File		*outerbatches;
    RelativeAddr	*outerbatchNames;
    File		*innerbatches;
    RelativeAddr	*innerbatchNames;
    RelativeAddr	*outerbatchPos;
    Var			innerhashkey;
    int			batch;
    int			batchno;
    char		*buffer;
    int			i;
    char		*tempname;
    bool		hashPhaseDone;
    char		*pos;
#ifdef sequent
    slock_t		*batchLock;
#endif

    /* ----------------
     *	get information from HashJoin node
     * ----------------
     */
    hjstate =   	get_hashjoinstate(node);
    hjclauses = 	get_hashclauses(node);
    clause =		CAR(hjclauses);
    estate = 		(EState)get_state(node);
    qual = 		get_qpqual(node);
    hashNode = 		get_innerPlan(node);
    outerNode = 	get_outerPlan(node);
    hashPhaseDone = 	get_hashdone(node);

    dir =   	  	get_es_direction(estate);

    /* -----------------
     * get information from HashJoin state
     * -----------------
     */
    hashtable = 	get_hj_HashTable(hjstate);
    bucket = 		get_hj_CurBucket(hjstate);
    curtuple =		get_hj_CurTuple(hjstate);
    
    /* --------------------
     * initialize expression context
     * --------------------
     */
    econtext = 		get_cs_ExprContext(hjstate);

    /* ----------------
     *	if this is the first call, build the hash table for inner relation
     * ----------------
     */
    if (!hashPhaseDone) {  /* if the hash phase not completed */
	hashtable = get_hashjointable(node);
        if (hashtable == NULL) { /* if the hash table has not been created */
	    /* ----------------
	     * create the hash table
	     * ----------------
	     */
	    hashtable = ExecHashTableCreate(hashNode);
	    set_hj_HashTable(hjstate, hashtable);
	    innerhashkey = get_hashkey(hashNode);
	    set_hj_InnerHashKey(hjstate, innerhashkey);

	    /* ----------------
	     * execute the Hash node, to build the hash table 
	     * ----------------
	     */
	    set_hashtable(hashNode, hashtable);
	    innerTupleSlot = ExecProcNode(hashNode);
	}
	bucket = NULL;
	curtuple = NULL;
	curbatch = 0;
	set_hashdone(node, true);
    }
    else if (hashtable == NULL && !IsMaster && ParallelExecutorEnabled()) {
	IpcMemoryId  shmid;
	IpcMemoryKey hashjointablekey;
	int          hashjointablesize;

	hashjointablekey = get_hashjointablekey(node);
	hashjointablesize = get_hashjointablesize(node);
	/* ----------------
	 *      in Sequent version, shared memory is implemented by
	 *  memory mapped files, it takes one file descriptor.
	 *  we may have to free one for this.
	 * ----------------
	 */
	closeOneVfd();
	shmid = IpcMemoryCreateWithoutOnExit(hashjointablekey,
					    hashjointablesize,
					    HASH_PERMISSION);
	hashtable = (HashJoinTable) IpcMemoryAttach(shmid);
	set_hashjointable(node, hashtable);
	set_hj_HashTable(hjstate, hashtable);
	set_hj_HashTableShmId(hjstate, shmid);
    }
    nbatch = hashtable->nbatch;
    outerbatches = get_hj_OuterBatches(hjstate);
    if (nbatch > 0 && outerbatches == NULL) {  /* if needs hash partition */
	/* -----------------
	 *  allocate space for file descriptors of outer batch files
	 *  then open the batch files in the current process
	 * -----------------
	 */
	innerhashkey = get_hashkey(hashNode);
	set_hj_InnerHashKey(hjstate, innerhashkey);
        outerbatchNames = (RelativeAddr*)
	    ABSADDR(hashtable->outerbatchNames);
	outerbatches = (File*)
	    palloc(nbatch * sizeof(File));
	for (i=0; i<nbatch; i++) {
	    outerbatches[i] = FileNameOpenFile(
				  ABSADDR(outerbatchNames[i]), 
				  O_CREAT | O_RDWR, 0600);
	}
	set_hj_OuterBatches(hjstate, outerbatches);
	if (ParallelExecutorEnabled()) {
            innerbatchNames = (RelativeAddr*)
		               ABSADDR(hashtable->innerbatchNames);
	    innerbatches = (File*)palloc(nbatch * sizeof(File));
	    for (i=0; i<nbatch; i++) {
		innerbatches[i] = FileNameOpenFile(
				      ABSADDR(innerbatchNames[i]),
				      O_CREAT | O_RDWR, 0600);
	    }
	    set_hj_InnerBatches(hjstate, innerbatches);
	}
	else {
	    /* ------------------
	     *  get the inner batch file descriptors from the
	     *  hash node
	     * ------------------
	     */
	    set_hj_InnerBatches(hjstate, 
				get_hashBatches(get_hashstate(hashNode)));
	}
    }
    outerbatchPos = (RelativeAddr*)ABSADDR(hashtable->outerbatchPos);
    curbatch = hashtable->curbatch;
#ifdef sequent
    batchLock = (slock_t*)ABSADDR(hashtable->batchLock);
#endif
    outerbatchNames = (RelativeAddr*)ABSADDR(hashtable->outerbatchNames);
	
    /* ----------------
     *	Now get an outer tuple and probe into the hash table for matches
     * ----------------
     */
    outerTupleSlot = 	get_cs_OuterTupleSlot(hjstate);
    outerVar =   	get_leftop(clause);
    
    bucketno = -1;  /* if bucketno remains -1, means use old outer tuple */
    if (TupIsNull(outerTupleSlot)) {
	/*
	 * if the current outer tuple is nil, get a new one
	 */
	outerTupleSlot = (TupleTableSlot)
	    ExecHashJoinOuterGetTuple(outerNode, hjstate);
	
	while (curbatch <= nbatch && TupIsNull(outerTupleSlot)) {
	/*
	 * if the current batch runs out, switch to new batch
	 */
	    curbatch = ExecHashJoinNewBatch(hjstate);
	    if (curbatch > nbatch) {
	    /*
	     * when the last batch runs out, clean up
	     */
#ifdef sequent
	    /* ---------------
	     *  we want to make sure that only the last process does
	     *  the cleanup.
	     * ---------------
	     */
		if (ParallelExecutorEnabled()) {
		    S_LOCK(&(hashtable->tableLock));
		    if (--(hashtable->pcount) > 0) {
		       S_UNLOCK(&(hashtable->tableLock));
		       return NULL;
		      }
		    S_UNLOCK(&(hashtable->tableLock));
		  }
#endif
		if (!IsMaster && ParallelExecutorEnabled()) {
		    /* ----------------
		     *  set the shmid to the one of the current process
		     * ----------------
		     */
		    hashtable->shmid = get_hj_HashTableShmId(hjstate);
		   }
		ExecHashTableDestroy(hashtable);
		set_hj_HashTable(hjstate, NULL);
		return NULL;
	      }
	    else
	      outerTupleSlot = (TupleTableSlot)
		  ExecHashJoinOuterGetTuple(outerNode, hjstate);
	  }
	/*
	 * now we get an outer tuple, find the corresponding bucket for
	 * this tuple from the hash table
	 */
	set_ecxt_outertuple(econtext, outerTupleSlot);
	
#ifdef HJDEBUG
	printf("Probing ");
#endif
	bucketno = ExecHashGetBucket(hashtable, econtext, outerVar);
	bucket=(HashBucket)(ABSADDR(hashtable->top) 
			    + bucketno * hashtable->bucketsize);
    }
    
    for (;;) {
    /* ----------------
     *	Now we've got an outer tuple and the corresponding hash bucket,
     *  but this tuple may not belong to the current batch.
     * ----------------
     */
	if (curbatch == 0 && bucketno != -1)  /* if this is the first pass */
	   batch = ExecHashJoinGetBatch(bucketno, hashtable, nbatch);
	else
	   batch = 0;
	if (batch > 0) {
	     /*
	      * if the current outer tuple does not belong to
	      * the current batch, save to the tmp file for
	      * the corresponding batch.
	      */
	     buffer = ABSADDR(hashtable->batch) + (batch - 1) * BLCKSZ;
	     batchno = batch - 1;
#ifdef sequent
	     /* ---------------
	      *  lock the batch to write
	      * ---------------
	      */
	     if (ParallelExecutorEnabled())
		 S_LOCK(&(batchLock[batchno]));
#endif
	     pos  = ExecHashJoinSaveTuple(SlotContents(outerTupleSlot), 
					  buffer,
				     	  outerbatches[batchno],
				     	  ABSADDR(outerbatchPos[batchno]));
	     
	     outerbatchPos[batchno] = RELADDR(pos);
#ifdef sequent
	     /* ---------------
	      *  unlock the batch to write
	      * ---------------
	      */
	     if (ParallelExecutorEnabled())
		 S_UNLOCK(&(batchLock[batchno]));
#endif
	  }
	else if (bucket != NULL) {
	    do {
		/*
		 * scan the hash bucket for matches
		 */
		curtuple = ExecScanHashBucket(hjstate,
					      bucket,
					      curtuple,
					      hjclauses,
					      econtext);
		
		if (curtuple != NULL) {
		    /*
		     * we've got a match, but still need to test qpqual
		     */
                    inntuple = (TupleTableSlot)
			ExecStoreTuple(curtuple, 
                                       get_hj_HashTupleSlot(hjstate),
				       InvalidBuffer,
                                       false); /* don't pfree this tuple */
		    
		    set_ecxt_innertuple(econtext, inntuple);

		    /* ----------------
		     * test to see if we pass the qualification
		     * ----------------
		     */
		    qualResult = ExecQual(qual, econtext);
		    
		    /* ----------------
		     * if we pass the qual, then save state for next call and
		     * have ExecProject form the projection, store it
		     * in the tuple table, and return the slot.
		     * ----------------
		     */
		    if (qualResult) {
			ProjectionInfo	projInfo;
			
			set_hj_CurBucket(hjstate, bucket);
			set_hj_CurTuple(hjstate, curtuple);
			hashtable->curbatch = curbatch;
			set_cs_OuterTupleSlot(hjstate, outerTupleSlot);
			
			projInfo = get_cs_ProjInfo(hjstate);
			return
			    ExecProject(projInfo);
		    }
		}
	    }
	    while (curtuple != NULL);
	}

	/* ----------------
	 *   Now the current outer tuple has run out of matches,
	 *   so we free it and get a new outer tuple.
	 * ----------------
	 */
	outerTupleSlot = (TupleTableSlot)
	    ExecHashJoinOuterGetTuple(outerNode, hjstate);
	
	while (curbatch <= nbatch && TupIsNull(outerTupleSlot)) {
	/*
	 * if the current batch runs out, switch to new batch
	 */
	   curbatch = ExecHashJoinNewBatch(hjstate);
	   if (curbatch > nbatch) {
	    /*
	     * when the last batch runs out, clean up
	     */
#ifdef sequent
	    /* ---------------
	     *  we want to make sure that only the last process does
	     *  the cleanup.
	     * ---------------
	     */
		if (ParallelExecutorEnabled()) {
		    S_LOCK(&(hashtable->tableLock));
		    if (--(hashtable->pcount) > 0) {
		       S_UNLOCK(&(hashtable->tableLock));
		       return NULL;
		      }
		    S_UNLOCK(&(hashtable->tableLock));
		  }
#endif
		if (!IsMaster && ParallelExecutorEnabled()) {
		    /* ----------------
		     *  set the shmid to the one of the current process
		     * ----------------
		     */
		    hashtable->shmid = get_hj_HashTableShmId(hjstate);
		   }
		ExecHashTableDestroy(hashtable);
		set_hj_HashTable(hjstate, NULL);
		return NULL;
	     }
	   else
	      outerTupleSlot = (TupleTableSlot)
		  ExecHashJoinOuterGetTuple(outerNode, hjstate);
	  }
	
	/* ----------------
	 *   Now get the corresponding hash bucket for the new
	 *   outer tuple.
	 * ----------------
	 */
	set_ecxt_outertuple(econtext, outerTupleSlot);
#ifdef HJDEBUG
	printf("Probing ");
#endif
	bucketno = ExecHashGetBucket(hashtable, econtext, outerVar);
	bucket=(HashBucket)(ABSADDR(hashtable->top) 
			    + bucketno * hashtable->bucketsize);
	curtuple = NULL;
    }
}
 
/* ----------------------------------------------------------------
 *   	ExecInitHashJoin
 *
 *	Init routine for HashJoin node.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecInitNode
 ****/
List	/* return: initialization status */
ExecInitHashJoin(node, estate, parent)
    HashJoin 	node;
    EState 	estate;
    Plan	parent;
{
    HashJoinState	hjstate;
    List		targetList;
    int			len;
    TupleDescriptor	tupType;
    Pointer	        tupValue;
    ParamListInfo       paraminfo;
    ExprContext	        econtext;
    int			baseid;
    
    Plan 	  	outerNode;
    Plan		hashNode;
    
    /* ----------------
     *  assign the node's execution state
     * ----------------
     */
    set_state(node, estate);
    
    /* ----------------
     * create state structure
     * ----------------
     */
    hjstate = MakeHashJoinState();
    set_hashjoinstate(node, hjstate);
        
    /* ----------------
     *  Miscellanious initialization
     *
     *	     +	assign node's base_id
     *       +	assign debugging hooks and
     *       +	create expression context for node
     * ----------------
     */
    ExecAssignNodeBaseInfo(estate, hjstate, parent);
    ExecAssignDebugHooks(node, hjstate);
    ExecAssignExprContext(estate, hjstate);

    /* ----------------
     *	tuple table initialization
     * ----------------
     */
    ExecInitResultTupleSlot(estate, hjstate);    
    ExecInitOuterTupleSlot(estate, hjstate);    
    
    /* ----------------
     * initializes child nodes
     * ----------------
     */
    outerNode = get_outerPlan(node);
    hashNode  = get_innerPlan(node);
    
    ExecInitNode(outerNode, estate, node);
    ExecInitNode(hashNode,  estate, node);
    
    /* ----------------
     *	now for some voodoo.  our temporary tuple slot
     *  is actually the result tuple slot of the Hash node
     *  (which is our inner plan).  we do this because Hash
     *  nodes don't return tuples via ExecProcNode() -- instead
     *  the hash join node uses ExecScanHashBucket() to get
     *  at the contents of the hash table.  -cim 6/9/91
     * ----------------
     */
    {
	HashState      hashstate  = get_hashstate(hashNode);
	TupleTableSlot slot 	  = get_cs_ResultTupleSlot(hashstate);
	set_hj_HashTupleSlot(hjstate, slot);
    }
    (void)ExecSetSlotDescriptor(get_hj_OuterTupleSlot(hjstate),
				ExecGetTupType(outerNode));
			      
    /* ----------------
     * 	initialize tuple type and projection info
     * ----------------
     */
    ExecAssignResultTypeFromTL(node, hjstate);
    ExecAssignProjectionInfo(node, hjstate);

    /* ----------------
     *	XXX comment me
     * ----------------
     */
    if (IsMaster && ParallelExecutorEnabled())
        set_hj_HashTable(hjstate, get_hashjointable(node));
    else
        set_hj_HashTable(hjstate, NULL);
    
    set_hashdone(node, false);
    
    set_hj_HashTableShmId(hjstate, 0);
    set_hj_CurBucket(hjstate, NULL);
    set_hj_CurTuple(hjstate, NULL);
    set_hj_InnerHashKey(hjstate, NULL);
    set_hj_OuterBatches(hjstate, NULL);
    set_hj_InnerBatches(hjstate, NULL);
    set_hj_OuterReadPos(hjstate, NULL);
    set_hj_OuterReadBlk(hjstate, 0);
    
    set_cs_OuterTupleSlot(hjstate, LispNil);

    /* ----------------
     *  return true
     * ----------------
     */
    return
	LispTrue;
}
 
/* ----------------------------------------------------------------
 *   	ExecEndHashJoin
 *
 *   	clean up routine for HashJoin node
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEndNode
 ****/
void
ExecEndHashJoin(node)
    HashJoin node;
{
    HashJoinState   hjstate;
    
    /* ----------------
     *	get info from the HashJoin state 
     * ----------------
     */
    hjstate = get_hashjoinstate(node);

    /* ----------------
     *	Free the projection info and the scan attribute info
     *
     *  Note: we don't ExecFreeResultType(hjstate) 
     *        because the rule manager depends on the tupType
     *	      returned by ExecMain().  So for now, this
     *	      is freed at end-transaction time.  -cim 6/2/91     
     * ----------------
     */    
    ExecFreeProjectionInfo(hjstate);

    /* ----------------
     * clean up subtrees 
     * ----------------
     */
    ExecEndNode(get_outerPlan(node));
    ExecEndNode(get_innerPlan(node));

    /* ----------------
     *  clean out the tuple table
     * ----------------
     */
    ExecClearTuple(get_cs_ResultTupleSlot(hjstate));
    ExecClearTuple(get_hj_OuterTupleSlot(hjstate));
    ExecClearTuple(get_hj_HashTupleSlot(hjstate));

} 

/* ----------------------------------------------------------------
 *   	ExecHashJoinOuterGetTuple
 *
 *   	get the next outer tuple for hashjoin: either by
 *	executing a plan node as in the first pass, or from
 *	the tmp files for the hashjoin batches.
 * ----------------------------------------------------------------
 */
 
TupleTableSlot
ExecHashJoinOuterGetTuple(node, hjstate)
    Plan node;
    HashJoinState hjstate;
{
    TupleTableSlot	slot;
    HashJoinTable	hashtable;
    int			curbatch;
    File 		*outerbatches;
    char 		*outerreadPos;
    int 		batchno;
    char 		*outerreadBuf;
    int 		outerreadBlk;

    hashtable = get_hj_HashTable(hjstate);
    curbatch = hashtable->curbatch;

    if (curbatch == 0) {  /* if it is the first pass */
	slot = ExecProcNode(node);
	return slot;
    }
    
    /*
     * otherwise, read from the tmp files
     */
    outerbatches = get_hj_OuterBatches(hjstate);
    outerreadPos = get_hj_OuterReadPos(hjstate);
    outerreadBlk = get_hj_OuterReadBlk(hjstate);
    if (ParallelExecutorEnabled())
       outerreadBuf = ABSADDR(hashtable->readbuf) + 
		      SlaveInfoP[MyPid].groupPid * BLCKSZ; 
    else
       outerreadBuf = ABSADDR(hashtable->readbuf); 
    batchno = curbatch - 1;
    
   slot = ExecHashJoinGetSavedTuple(hjstate,
				    outerreadBuf,
				    outerbatches[batchno],
				    get_hj_OuterTupleSlot(hjstate),
				    &outerreadBlk,
				    &outerreadPos);
    
    set_hj_OuterReadPos(hjstate, outerreadPos);
    set_hj_OuterReadBlk(hjstate, outerreadBlk);
    
    return slot;
}

/* ----------------------------------------------------------------
 *   	ExecHashJoinGetSavedTuple
 *
 *   	read the next tuple from a tmp file using a certain buffer
 * ----------------------------------------------------------------
 */
 
TupleTableSlot
ExecHashJoinGetSavedTuple(hjstate, buffer, file, tupleSlot, block, position)
    HashJoinState hjstate;    
    char 	  *buffer;
    File 	  file;
    Pointer	  tupleSlot;
    int 	  *block;       /* return parameter */
    char 	  **position;  /* return parameter */
{
    char 	*bufstart;
    char 	*bufend;
    int	 	cc;
    HeapTuple 	heapTuple;
    HashJoinTable hashtable;

    hashtable = get_hj_HashTable(hjstate);
    bufend = buffer + *(long*)buffer;
    bufstart = (char*)(buffer + sizeof(long));
    if ((*position == NULL) || (*position >= bufend)) {
	if (*position == NULL)
	    if (ParallelExecutorEnabled())
		(*block) = SlaveInfoP[MyPid].groupPid;
	    else
	        (*block) = 0;
	else
	    if (ParallelExecutorEnabled())
		(*block) += hashtable->nprocess;
	    else
	        (*block)++;
	FileSeek(file, *block * BLCKSZ, L_SET);
	cc = FileRead(file, buffer, BLCKSZ);
	NDirectFileRead++;
	if (cc < 0)
	    perror("FileRead");
	if (cc == 0)  /* end of file */
	    return NULL;
	else
	    (*position) = bufstart;
    }
    heapTuple = (HeapTuple) (*position);
    (*position) = (char*)LONGALIGN(*position + heapTuple->t_len);

    return (TupleTableSlot)
	ExecStoreTuple(heapTuple,
		       tupleSlot,
		       InvalidBuffer,
		       false);
}

/* ----------------------------------------------------------------
 *   	ExecHashJoinNewBatch
 *
 *   	switch to a new hashjoin batch
 * ----------------------------------------------------------------
 */
 
int
ExecHashJoinNewBatch(hjstate)
    HashJoinState hjstate;
{
    File 		*innerBatches;
    File 		*outerBatches;
    int 		*innerBatchSizes;
    Var 		innerhashkey;
    HashJoinTable 	hashtable;
    int 		nbatch;
    char 		*readPos;
    int 		readBlk;
    char 		*readBuf;
    TupleTableSlot 	slot;
    ExprContext 	econtext;
    int 		i;
    int 		cc;
    int			newbatch;

    hashtable = get_hj_HashTable(hjstate);
    outerBatches = get_hj_OuterBatches(hjstate);
    innerBatches = get_hj_InnerBatches(hjstate);
    nbatch = hashtable->nbatch;
    newbatch = hashtable->curbatch + 1;
#ifdef sequent
    /* -----------------
     *  We want to make sure that only the last process does
     *  the cleanup and switching to the new batch  and all
     *  the other processes have to wait until the entire batch
     *  has been finished.  This takes a counter, hashtable->pcount
     *  and a barrier, hashtable->batchBarrier to achieve
     * -----------------
     */
    if (ParallelExecutorEnabled())
        S_LOCK(&(hashtable->tableLock));
#endif
    if (ParallelExecutorEnabled() && --(hashtable->pcount) > 0) {
#ifdef sequent
	  /* ----------------
	   *  if it is not the last process, wait on the barrier
	   * ----------------
	   */
	  S_UNLOCK(&(hashtable->tableLock));
          S_WAIT_BARRIER(&(hashtable->batchBarrier));
#endif
	  if (newbatch > nbatch)
	      return newbatch;
       }
    else {
	/* ------------------
	 *  this is the last process, so it will do the cleanup and
	 *  batch-switching.
	 * ------------------
	 */
	if (newbatch == 1) {
	    /* 
	     * if it is end of the first pass, flush all the last pages for
	     * the batches.
	     */
	    outerBatches = get_hj_OuterBatches(hjstate);
	    for (i=0; i<nbatch; i++) {
		cc = FileSeek(outerBatches[i], 0L, L_XTND);
		if (cc < 0)
		    perror("FileSeek");
		cc = FileWrite(outerBatches[i],
			       ABSADDR(hashtable->batch) + i * BLCKSZ, BLCKSZ);
		NDirectFileWrite++;
		if (cc < 0)
		    perror("FileWrite");
	    }
	}
	if (newbatch > 1) {
	    /*
	     * remove the previous outer batch
	     */
	    FileUnlink(outerBatches[newbatch - 2]);
	}
	/*
	 * rebuild the hash table for the new inner batch
	 */
	innerBatchSizes = (int*)ABSADDR(hashtable->innerbatchSizes);
	/* --------------
	 *  skip over empty inner batches
	 * --------------
	 */
	while (newbatch <= nbatch && innerBatchSizes[newbatch - 1] == 0) {
	   FileUnlink(outerBatches[newbatch-1]);
	   FileUnlink(innerBatches[newbatch-1]);
	   newbatch++;
	  }
	if (newbatch > nbatch) {
	   hashtable->pcount = hashtable->nprocess;
#ifdef sequent
	   S_UNLOCK(&(hashtable->tableLock));
           S_WAIT_BARRIER(&(hashtable->batchBarrier));
#endif
	   return newbatch;
	 }
	ExecHashTableReset(hashtable, innerBatchSizes[newbatch - 1]);

#ifdef sequent
	/* -------------------
	 *  batch change completed
	 *  release the barrier, then reset the barrier for the next batch
	 * -------------------
	 */
        if (ParallelExecutorEnabled()) {
	   S_WAIT_BARRIER(&(hashtable->batchBarrier));
	   S_INIT_BARRIER(&(hashtable->batchBarrier), hashtable->nprocess);
	   S_UNLOCK(&(hashtable->tableLock));
	  }
#endif
    }
    econtext = get_cs_ExprContext(hjstate);
    innerhashkey = get_hj_InnerHashKey(hjstate);
    readPos = NULL;
    readBlk = 0;
    if (ParallelExecutorEnabled())
       /* ----------------------
	*  build the hash table of the new batch in parallel
	* ----------------------
	*/
       readBuf = ABSADDR(hashtable->readbuf) + 
		 SlaveInfoP[MyPid].groupPid * BLCKSZ;
    else
       readBuf = ABSADDR(hashtable->readbuf);

    while ((slot = ExecHashJoinGetSavedTuple(hjstate,
					    readBuf, 
					    innerBatches[newbatch-1],
					    get_hj_HashTupleSlot(hjstate),
					    &readBlk,
					    &readPos))
	   && ! TupIsNull(slot)) {
	set_ecxt_innertuple(econtext, slot);
	ExecHashTableInsert(hashtable, econtext, innerhashkey);
    }
    
#ifdef sequent
    /* ---------------
     *  we want to make sure that the processes proceed to the probe
     *  phase after all the processes finish the build phase
     * ---------------
     */
    if (ParallelExecutorEnabled())
	S_LOCK(&(hashtable->tableLock));
#endif
    if (ParallelExecutorEnabled() && --(hashtable->pcount) > 0) {
#ifdef sequent
	/* ----------------
	 *  if not the last process, wait on the barrier
	 * ----------------
	 */
	S_UNLOCK(&(hashtable->tableLock));
	S_WAIT_BARRIER(&(hashtable->batchBarrier));
#endif
       }
    else {
	/* -----------------
	 *  only the last process comes to this branch
	 *  now all the processes have finished the build phase
	 * ----------------
	 */

	/*
	 * after we build the hash table, the inner batch is no longer needed
	 */
	FileUnlink(innerBatches[newbatch - 1]);
	set_hj_OuterReadPos(hjstate, NULL);
	hashtable->pcount = hashtable->nprocess;
#ifdef sequent
	/* -----------------
	 *  release the barrier, then reset it for the next batch
	 * -----------------
	 */
	if (ParallelExecutorEnabled()) {
	    S_WAIT_BARRIER(&(hashtable->batchBarrier));
	    S_INIT_BARRIER(&(hashtable->batchBarrier), hashtable->nprocess);
	    S_UNLOCK(&(hashtable->tableLock));
	  }
#endif
       }
    hashtable->curbatch = newbatch;
    return newbatch;
}

/* ----------------------------------------------------------------
 *   	ExecHashJoinGetBatch
 *
 *   	determine the batch number for a bucketno
 *      +----------------+-------+-------+ ... +-------+
 *	0             nbuckets                       totalbuckets
 * batch         0           1       2     ...
 * ----------------------------------------------------------------
 */
 
int
ExecHashJoinGetBatch(bucketno, hashtable, nbatch)
int bucketno;
HashJoinTable hashtable;
int nbatch;
{
    int b;
    if (bucketno < hashtable->nbuckets || nbatch == 0)
       return 0;
    
    b = (float)(bucketno - hashtable->nbuckets) /
	(float)(hashtable->totalbuckets - hashtable->nbuckets) *
	nbatch;
    return b+1;
}

/* ----------------------------------------------------------------
 *   	ExecHashJoinSaveTuple
 *
 *   	save a tuple to a tmp file using a buffer.
 *	the first few bytes in a page is an offset to the end
 *	of the page.
 * ----------------------------------------------------------------
 */
 
char *
ExecHashJoinSaveTuple(heapTuple, buffer, file, position)
    HeapTuple	heapTuple;
    char 	*buffer;
    File 	file;
    char 	*position;
{
    long	*pageend;
    char	*pagestart;
    char	*pagebound;
    int		cc;

    pageend = (long*)buffer;
    pagestart = (char*)(buffer + sizeof(long));
    pagebound = buffer + BLCKSZ;
    if (position == NULL)
       position = pagestart;
    
    if (position + heapTuple->t_len >= pagebound) {
       cc = FileSeek(file, 0L, L_XTND);
       if (cc < 0)
	  perror("FileSeek");
       cc = FileWrite(file, buffer, BLCKSZ);
       NDirectFileWrite++;
       if (cc < 0)
	  perror("FileWrite");
       position = pagestart;
       *pageend = 0;
      }
    bcopy(heapTuple, position, heapTuple->t_len);
    position = (char*)LONGALIGN(position + heapTuple->t_len);
    *pageend = position - buffer;
    
    return position;
}
