/*
 * sinval.c --
 *  POSTGRES shared cache invalidation communication code.
 */

/* #define INVALIDDEBUG	1 */

#include "tmp/postgres.h"

#include "storage/sinval.h"
#include "storage/sinvaladt.h"
#include "storage/plm.h"
#include "utils/log.h"

RcsId("$Header$");

#ifdef TEST 	    	    /* XXX REMOVE for INTEGRATION  */
void
PRT(data)
int  data;
{ printf(">>Cache Id %3d written.\n", data);
}
#endif	    	    	    /* XXX REMOVE for INTEGRATION  */



static SharedInvalid	Invalid = NULL;
extern SISeg		*shmInvalBuffer;/* the shared buffer segment, set by*/
    	    	    	    	    	/*   SISegmentAttach()	    	    */
extern BackendId	MyBackendId;
extern BackendTag	MyBackendTag;

extern LockTableId  SILockTableId;  /*identifies the lock table	    	*/
extern LRelId  	    SIRelId;	    /* identification of the buffer 	*/
extern ObjectId		SIDummyOid;
extern TransactionId	SIXid;	    /* identification of the buffer */

/****************************************************************************/
/*  CreateSharedInvalidationState(key)   Create a buffer segment    	    */
/*  	    	    	    	    	    	    	    	    	    */
/*  should be called only by the POSTMASTER 	    	    	    	    */
/****************************************************************************/
void
CreateSharedInvalidationState(key)
	IPCKey	key;
{
	int	status;

    /* REMOVED
    SISyncKill(IPCKeyGetSIBufferMemorySemaphoreKey(key));
    SISyncInit(IPCKeyGetSIBufferMemorySemaphoreKey(key));
    */
    
    status = SISegmentInit(true, IPCKeyGetSIBufferMemoryBlock(key));
    
    if (status == -1) {
    	elog(FATAL, "CreateSharedInvalidationState: failed segment init");
    }
}
/****************************************************************************/
/*  AttachSharedInvalidationState(key)   Attach a buffer segment    	    */
/*  	    	    	    	    	    	    	    	    	    */
/*  should be called only by the POSTMASTER 	    	    	    	    */
/****************************************************************************/
void
AttachSharedInvalidationState(key)
	IPCKey	key;
{
	int	status;

    if (key == PrivateIPCKey) {
          CreateSharedInvalidationState(key);
          return;
        }
    /* attach the semaphores and segment    	    	*/
    /* REMOVED
    SISyncInit(IPCKeyGetSIBufferMemorySemaphoreKey(key));
    */

    status = SISegmentInit(false, IPCKeyGetSIBufferMemoryBlock(key));
    
    if (status == -1) {
    	elog(FATAL, "AttachSharedInvalidationState: failed segment init");
    }
}

void
InitSharedInvalidationState()
{
	SIBackendInit(shmInvalBuffer);
}

/****************************************************************************/
/*  RegisterSharedInvalid(cacheId, hashIndex, pointer)      	    	    */
/*  	    	    	    	    	    	    	    	    	    */
/*  register a message in the buffer	    	    	    	    	    */
/*  should be called by a backend   	    	    	    	    	    */
/****************************************************************************/
void
RegisterSharedInvalid(cacheId, hashIndex, pointer)
    int	    cacheId;	/* XXX */
    Index   	hashIndex;
    ItemPointer	pointer;
{
    SharedInvalid   newInvalid;
    int	    	    status;

/*
 * This code has been hacked to accept two types of messages.  This might
 * be treated more generally in the future.
 *
 * (1)
 *	cacheId= system cache id
 *	hashIndex= system cache hash index for a (possibly) cached tuple
 *	pointer= pointer of (possibly) cached tuple
 *
 * (2)
 *	cacheId= special non-syscache id
 *	hashIndex= object id contained in (possibly) cached relation descriptor
 *	pointer= null
 */
/*Assert(IndexIsValid(hashIndex) && IndexIsInBounds(hashIndex, NCCBUCK));*/
/*Assert(ItemPointerIsValid(pointer));*/

#ifdef	INVALIDDEBUG
	elog(DEBUG, "RegisterSharedInvalid(%d, %d, 0x%x) called", cacheId,
		hashIndex, pointer);
#endif

    newInvalid = new(SharedInvalidData);
    if (!PointerIsValid(newInvalid)) {
    	elog(WARN, "malloc failed");
    }
    newInvalid->cacheId = cacheId;
    newInvalid->hashIndex = hashIndex;

	if (ItemPointerIsValid(pointer)) {
		ItemPointerCopy(pointer, &newInvalid->pointerData);
	} else {
		ItemPointerSetInvalid(&newInvalid->pointerData);
	}
   
    /* try to write to the buffer */
    /* WRITE LOCK buffer */
    status = LMLock(SILockTableId, LockWait, &SIRelId, &SIDummyOid, &SIDummyOid,
    	    	SIXid, MultiLevelLockRequest_WriteRelation);
    if (status == L_ERROR) {
    	    elog(FATAL, "RegisterSharedInvalid: Could not lock buffer segment");
    }	    	    
    if (!SISetDataEntry(shmInvalBuffer, *newInvalid)) {
    	/* buffer full */
    	/* release a message, mark process cache states to be invalid */
    	SISetProcStateInvalid(shmInvalBuffer);
    	if (!SIDelDataEntry(shmInvalBuffer)) {
    	    /* inconsistent buffer state */
    	    /* Attention: BUFFER IS WRITE LOCKED while exiting !!!!!! XXX   */
    	    /*	    	  This should really NOT happen	    	    */ 
    	    elog(FATAL, "RegisterSharedInvalid: inconsistent buffer state");
    	}
#ifdef	EBUG
    	elog(NOTICE, "RegisterSharedInvalid: buffer segment full, message removed; causes state resets.");
#endif
    	/* write again */
    	(void) SISetDataEntry(shmInvalBuffer, *newInvalid);
    }
#ifdef TEST
    PRT(cacheId); /* just for debuging */
#endif
    /* release WRITE LOCK from buffer */
    status = LMLockReleaseAll(SILockTableId, SIXid);
    if (status == L_ERROR) {
    	    elog(FATAL, "RegisterSharedInvalid: Could not unlock buffer segment");
    }	    	    
}
/****************************************************************************/
/*  InvalidateSharedInvalid(invalFunction, resetFunction)    	    	    */
/*  	    	    	    	    	    	    	    	    	    */
/*  invalidate a message in the buffer	 (read and clean up)	    	    */
/*  should be called by a backend   	    	    	    	    	    */
/****************************************************************************/
void
InvalidateSharedInvalid(invalFunction, resetFunction)
    void    	(*invalFunction)();
    void    	(*resetFunction)();
{
    SharedInvalid   temporaryInvalid;
    int	    	    status;
    
    /* READ LOCK buffer */
    status = LMLock(SILockTableId, LockWait, &SIRelId, &SIDummyOid, &SIDummyOid,
    	    	    SIXid, MultiLevelLockRequest_ReadRelation);
    if (status == L_ERROR) {
    	    elog(FATAL, "InvalidateSharedInvalid: Could not lock buffer segment");
    }
    
    SIReadEntryData(shmInvalBuffer, MyBackendId, 
    	    	    invalFunction, resetFunction);  
    	    	     
    /* UNLOCK buffer */
    status = LMLockReleaseAll(SILockTableId, SIXid);
    if (status == L_ERROR) {
    	    elog(FATAL, "InvalidateSharedInvalid: Could not unlock buffer segment");
    }
    
    /* WRITE LOCK buffer */
    status = LMLock(SILockTableId, LockWait, &SIRelId, &SIDummyOid, &SIDummyOid,
    	    	SIXid, MultiLevelLockRequest_WriteRelation);
    if (status == L_ERROR) {
    	    elog(FATAL, "RegisterSharedInvalid: Could not lock buffer segment");
    }
    
    SIDelExpiredDataEntries(shmInvalBuffer);
    /*UNLOCK buffer */
    status = LMLockReleaseAll(SILockTableId, SIXid);
    if (status == L_ERROR) {
    	    elog(FATAL, "InvalidateSharedInvalid: Could not unlock buffer segment");
    }
}
