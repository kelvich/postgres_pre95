/* ----------------------------------------------------------------
 *   FILE
 * 	sinvaladt.c
 *	
 *   DESCRIPTION
 *	POSTGRES shared cache invalidation segment definitions.
 *
 *   INTERFACE ROUTINES
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

/* ----------------
 *	include files
 *
 *	XXX yucky order dependency
 * ----------------
 */

#include "storage/ipci.h"
#include "storage/ipc.h"
#include "storage/sinvaladt.h"
#include "storage/lmgr.h"
#include "utils/log.h"

/* ----------------
 *	global variable notes
 *
 *	SharedInvalidationSemaphore
 *
 *	shmInvalBuffer
 *		the shared buffer segment, set by SISegmentAttach()
 *
 *	MyBackendId
 *		might be removed later, used only for
 * 		debugging in debug routines (end of file)
 *
 *	SIDbId
 *		identification of buffer (disappears)
 *
 *	SIRelId		\ 
 *	SIDummyOid	 \  identification of buffer
 *	SIXidData	 /
 *	SIXid		/
 *
 * ----------------
 */
#ifdef HAS_TEST_AND_SET
int SharedInvalidationLockId;
#else
IpcSemaphoreId	SharedInvalidationSemaphore;
#endif
SISeg		*shmInvalBuffer;	
extern BackendId MyBackendId;

OID 		SIDbId = 0;	    
LRelId  	SIRelId;    	    
OID 		SIDummyOid = InvalidObjectId;
TransactionIdData SIXidData;	    
TransactionId	SIXid = &SIXidData; 

/* ----------------
 *	declarations
 * ----------------
 */

/*
 * SISetActiveProcess --
 */
extern
BackendId
SIAssignBackendId ARGS((
	SISeg 		*segInOutP,
	BackendTag	tag
));

/*
 * CleanupInvalidationState --
 * Note:
 *	This is a temporary hack.  ExitBackend should call this instead
 *	of exit (via on_exitpg).
 */
extern
void
CleanupInvalidationState ARGS((
	int		status,	/* XXX */
	SISeg		*segInOutP
));

/************************************************************************/
/* SISetActiveProcess(segP, backendId)	set the backend status active	*/
/*  	should be called only by the postmaster when creating a backend	*/
/************************************************************************/
/* XXX I suspect that the segP parameter is extraneous. -hirohama */
void
SISetActiveProcess(segInOutP, backendId)
    SISeg 	*segInOutP;
    BackendId 	backendId;
{
    /* mark all messages as read */

    /* Assert(segP->procState[backendId - 1].tag == MyBackendTag); */

    segInOutP->procState[backendId - 1].resetState = false;
    segInOutP->procState[backendId - 1].limit = SIGetNumEntries(segInOutP);
}

/****************************************************************************/
/* SIBackendInit()  initializes a backend to operate on the buffer  	    */
/****************************************************************************/
void
SIBackendInit(segInOutP)
    SISeg 		*segInOutP;
{
    LRelId  	    	    LtCreateRelId();
    TransactionId           LMITransactionIdCopy();
    
    Assert(MyBackendTag > 0);

    MyBackendId = SIAssignBackendId(segInOutP, MyBackendTag);

#ifdef	INVALIDDEBUG
    elog(DEBUG, "SIBackendInit: backend tag %d; backend id %d.",
	 MyBackendTag, MyBackendId);
#endif	INVALIDDEBUG

    SISetActiveProcess(segInOutP, MyBackendId);
    on_exitpg(CleanupInvalidationState, segInOutP);
}

/* ----------------
 *	SIAssignBackendId
 * ----------------
 */
BackendId
SIAssignBackendId(segInOutP, backendTag)
    SISeg		*segInOutP;
    BackendTag	backendTag;
{
    Index		index;
    ProcState	*stateP;

    stateP = NULL;

    for (index = 0; index < MaxBackendId; index += 1) {
	if (segInOutP->procState[index].tag == InvalidBackendTag ||
	    segInOutP->procState[index].tag == backendTag)
	    {
		stateP = &segInOutP->procState[index];
		break;
	    }

	if (!PointerIsValid(stateP) ||
	    (segInOutP->procState[index].resetState &&
	     (!stateP->resetState ||
	      stateP->tag < backendTag)) ||
	    (!stateP->resetState &&
	     (segInOutP->procState[index].limit <
	      stateP->limit ||
	      stateP->tag < backendTag)))
	    {
		stateP = &segInOutP->procState[index];
	    }
    }

    /* verify that all "procState" entries checked for matching tags */

    for (index += 1; index < MaxBackendId; index += 1) {
	if (segInOutP->procState[index].tag == backendTag) {
	    elog (FATAL, "SIAssignBackendId: tag %d found twice",
		  backendTag);
	}
    }

    if (stateP->tag != InvalidBackendTag) {
	if (stateP->tag == backendTag) {
	    elog(NOTICE, "SIAssignBackendId: reusing tag %d",
		 backendTag);
	} else {
	    elog(NOTICE, "SIAssignBackendId: discarding tag %d",
		 stateP->tag);
	}
    }

    stateP->tag = backendTag;

    return (1 + stateP - &segInOutP->procState[0]);
}


/************************************************************************/
/* The following function should be called only by the postmaster !!    */
/************************************************************************/

/************************************************************************/
/* SISetDeadProcess(segP, backendId)  set the backend status DEAD   	*/
/*  	should be called only by the postmaster when a backend died 	*/
/************************************************************************/
void
SISetDeadProcess(segP, backendId)
    SISeg 	*segP;
    int 	backendId;
{
    /* XXX call me.... */

    segP->procState[backendId - 1].resetState = false;
    segP->procState[backendId - 1].limit = -1;
    segP->procState[backendId - 1].tag = InvalidBackendTag;
}

/* ----------------
 *	CleanupInvalidationState
 * ----------------
 */
void
CleanupInvalidationState(status, segInOutP)
    int		status;		/* XXX */
    SISeg	*segInOutP;	/* XXX style */
{
    Assert(PointerIsValid(segInOutP));

    SISetDeadProcess(segInOutP, MyBackendId);
}


/************************************************************************/
/* SIComputeSize()  - retuns the size of a buffer segment   	    	*/
/************************************************************************/
SISegOffsets *
SIComputeSize(segSize)
    int *segSize;
{
    int      	 A, B, a, b, totalSize;
    SISegOffsets *oP;

    A = 0;
    a = SizeSISeg;  	/* offset to first data entry */
    b = SizeOfOneSISegEntry * MAXNUMMESSAGES;
    B = A + a + b;
    totalSize = B - A;
    *segSize = totalSize;
    
    oP = new(SISegOffsets);
    oP->startSegment = A;
    oP->offsetToFirstEntry = a; /* relatiove to A */
    oP->offsetToEndOfSegemnt = totalSize; /* relative to A */
    return(oP);
}


/************************************************************************/
/* SIGetSemaphoreId - returns the general semaphore Id	    	    	*/
/************************************************************************/
SIGetSemaphoreId(segP)
    SISeg *segP;
{
    return(segP->generalSemaphoreId);
}


/************************************************************************/
/* SISetSemaphoreId 	- sets the general semaphore Id	    	    	*/
/************************************************************************/
void
SISetSemaphoreId(segP, id)
    SISeg *segP;
    IpcSemaphoreId id;
{
    segP->generalSemaphoreId = id;
}


/************************************************************************/
/* SISetStartEntrySection(segP, offset)     - sets the offset		*/
/************************************************************************/
void
SISetStartEntrySection(segP, offset)
    SISeg *segP;
    Offset offset;
{
    segP->startEntrySection = offset;
}

/************************************************************************/
/* SIGetStartEntrySection(segP)     - returnss the offset   		*/
/************************************************************************/
Offset
SIGetStartEntrySection(segP)
SISeg *segP;
{
    return(segP->startEntrySection);
}


/************************************************************************/
/* SISetEndEntrySection(segP, offset) 	- sets the offset   		*/
/************************************************************************/
void
SISetEndEntrySection(segP, offset)
    SISeg *segP;
    Offset offset;
{
    segP->endEntrySection = offset;
}

/************************************************************************/
/* SIGetEndEntrySection(segP) 	- returnss the offset	    	    	*/
/************************************************************************/
Offset
SIGetEndEntrySection(segP)
    SISeg *segP;
{
    return(segP->endEntrySection);
}

/************************************************************************/
/* SISetEndEntryChain(segP, offset) 	- sets the offset   	    	*/
/************************************************************************/
void
SISetEndEntryChain(segP, offset)
    SISeg *segP;
    Offset offset;
{
    segP->endEntryChain = offset;
}

/************************************************************************/
/* SIGetEndEntryChain(segP) 	- returnss the offset	    	    	*/
/************************************************************************/
Offset
SIGetEndEntryChain(segP)
    SISeg *segP;
{
    return(segP->endEntryChain);
}

/************************************************************************/
/* SISetStartEntryChain(segP, offset) 	- sets the offset   	    	*/
/************************************************************************/
void
SISetStartEntryChain(segP, offset)
    SISeg *segP;
    Offset offset;
{
    segP->startEntryChain = offset;
}

/************************************************************************/
/* SIGetStartEntryChain(segP) 	- returns  the offset	    	    	*/
/************************************************************************/
Offset
SIGetStartEntryChain(segP)
    SISeg *segP;
{
    return(segP->startEntryChain);
}

/************************************************************************/
/* SISetNumEntries(segP, num)	sets the current nuber of entries   	*/
/************************************************************************/
bool
SISetNumEntries(segP, num)
    SISeg *segP;
    int   num;
{
    if ( num <= MAXNUMMESSAGES) {
        segP->numEntries =  num;
        return(true);
    } else {
        return(false);  /* table full */
    }    
}

/************************************************************************/
/* SIGetNumEntries(segP)    - returns the current nuber of entries  	*/
/************************************************************************/
int
SIGetNumEntries(segP)
    SISeg *segP;
{
    return(segP->numEntries);
}


/************************************************************************/
/* SISetMaxNumEntries(segP, num)    sets the maximal number of entries	*/
/************************************************************************/
bool
SISetMaxNumEntries(segP, num)
    SISeg *segP;
    int   num;
{
    if ( num <= MAXNUMMESSAGES) {
        segP->maxNumEntries =  num;
        return(true);
    } else {
        return(false);  /* wrong number */
    }   
}


/************************************************************************/
/* SIGetMaxNumEntries(segP) 	returns the maximal number of entries	*/
/************************************************************************/
int
SIGetMaxNumEntries(segP)
    SISeg *segP;
{
    return(segP->maxNumEntries);
}

/************************************************************************/
/* SIGetProcStateLimit(segP, i)	returns the limit of read messages  	*/
/************************************************************************/
int
SIGetProcStateLimit(segP, i) 
    SISeg 	*segP;
    int 	i;
{
    return(segP->procState[i].limit);
}

/************************************************************************/
/* SIGetProcStateResetState(segP, i)  returns the limit of read messages*/
/************************************************************************/
bool
SIGetProcStateResetState(segP, i) 
    SISeg 	*segP;
    int 	i;
{
    return((bool) segP->procState[i].resetState);
}

/************************************************************************/
/* SIIncNumEntries(segP, num)	increments the current nuber of entries	*/
/************************************************************************/
bool
SIIncNumEntries(segP, num)
    SISeg *segP;
    int   num;
{
    if ((segP->numEntries + num) <= MAXNUMMESSAGES) {
        segP->numEntries = segP->numEntries + num;
        return(true);
    } else {
        return(false);  /* table full */
    }   
}

/************************************************************************/
/* SIDecNumEntries(segP, num)	decrements the current nuber of entries	*/
/************************************************************************/
bool
SIDecNumEntries(segP, num)
    SISeg *segP;
    int   num;
{
    if ((segP->numEntries - num) >=  0) {
        segP->numEntries = segP->numEntries - num;
        return(true);
    } else {
        return(false);  /* not enough entries in table */
    }   
}

/************************************************************************/
/* SISetStartFreeSpace(segP, offset)  - sets the offset	    	    	*/
/************************************************************************/
void
SISetStartFreeSpace(segP, offset)
    SISeg *segP;
    Offset offset;
{
    segP->startFreeSpace = offset;
}

/************************************************************************/
/* SIGetStartFreeSpace(segP)  - returns the offset  	    	    	*/
/************************************************************************/
Offset
SIGetStartFreeSpace(segP)
    SISeg *segP;
{
    return(segP->startFreeSpace);
}



/************************************************************************/
/* SIGetFirstDataEntry(segP)  returns first data entry	    	    	*/
/************************************************************************/
SISegEntry *
SIGetFirstDataEntry(segP)
    SISeg *segP;
{
    SISegEntry  *eP;
    Offset      startChain;
    
    startChain = SIGetStartEntryChain(segP);
    
    if (startChain == InvalidOffset)
    	return(NULL);
    	 
    eP = (SISegEntry  *) ((Pointer) segP + 
                           SIGetStartEntrySection(segP) +
                           startChain );
    return(eP);
}


/************************************************************************/
/* SIGetLastDataEntry(segP)  returns last data entry in the chain   	*/
/************************************************************************/
SISegEntry *
SIGetLastDataEntry(segP)
    SISeg *segP;
{
    SISegEntry  *eP;
    Offset      endChain;
    
    endChain = SIGetEndEntryChain(segP);
    
    if (endChain == InvalidOffset)
    	return(NULL);
    	 
    eP = (SISegEntry  *) ((Pointer) segP + 
                           SIGetStartEntrySection(segP) +
                           endChain );
    return(eP);
}

/************************************************************************/
/* SIGetNextDataEntry(segP, offset)  returns next data entry	    	*/
/************************************************************************/
SISegEntry *
SIGetNextDataEntry(segP, offset)
    SISeg 	    *segP;
    Offset       offset;
{
    SISegEntry  *eP;
    
    if (offset == InvalidOffset)
    	return(NULL);
    	
    eP = (SISegEntry  *) ((Pointer) segP +
                          SIGetStartEntrySection(segP) + 
                          offset);
    return(eP);
}


/************************************************************************/
/* SIGetNthDataEntry(segP, n)	returns the n-th data entry in chain	*/
/************************************************************************/
SISegEntry *
SIGetNthDataEntry(segP, n)
    SISeg 	*segP;
    int 	n;      /* must range from 1 to MaxMessages */
{
    SISegEntry  *eP;
    int	    	i;
    
    if (n <= 0) return(NULL);
    
    eP = SIGetFirstDataEntry(segP);
    for (i = 1; i < n; i++) {
    	/* skip one and get the next	*/
    	eP = SIGetNextDataEntry(segP, eP->next);
    }
    
    return(eP);
}

/************************************************************************/
/* SIEntryOffset(segP, entryP)   returns the offset for an pointer  	*/
/************************************************************************/
Offset
SIEntryOffset(segP, entryP)
    SISeg  	*segP;
    SISegEntry *entryP;
{
    /* relative to B !! */
    return ((Offset) ((Pointer) entryP -
                      (Pointer) segP - 
                      SIGetStartEntrySection(segP) ));
}


/************************************************************************/
/* SISetDataEntry(segP, data)  - sets a message in the segemnt	    	*/
/************************************************************************/
bool
SISetDataEntry(segP, data)
    SISeg *segP;
    SharedInvalidData  data;
{
    Offset  	    offsetToNewData;
    SISegEntry 	    *eP, *lastP;
    bool    	    SISegFull();
    Offset  	    SIEntryOffset();
    Offset  	    SIGetStartFreeSpace();
    SISegEntry 	    *SIGetFirstDataEntry();
    SISegEntry 	    *SIGetNextDataEntry();
    SISegEntry 	    *SIGetLastDataEntry();

    if (!SIIncNumEntries(segP, 1)) 
      return(false);  /* no space */
    
    /* get a free entry */
    offsetToNewData = SIGetStartFreeSpace(segP);
    eP = SIGetNextDataEntry(segP, offsetToNewData); /* it's a free one */
    SISetStartFreeSpace(segP, eP->next);
    /* fill it up */
    eP->entryData = data;
    eP->isfree = false;
    eP->next = InvalidOffset;
    
    /* handle insertion point at the end of the chain !!*/
    lastP = SIGetLastDataEntry(segP);
    if (lastP == NULL) {
    	/* there is no chain, insert the first entry */
    	SISetStartEntryChain(segP, SIEntryOffset(segP, eP));
    } else {
    	/* there is a last entry in the chain */
    	lastP->next = SIEntryOffset(segP, eP);
    }
    SISetEndEntryChain(segP, SIEntryOffset(segP, eP));
    return(true);
}


/************************************************************************/
/* SIDecProcLimit(segP, num)  decrements all process limits 	    	*/
/************************************************************************/
void
SIDecProcLimit(segP, num)
    SISeg 	*segP;
    int 	num;
{
    int i;
    for (i=0; i < MaxBackendId; i++) {
    	/* decrement only, if there is a limit > 0  */
    	if (segP->procState[i].limit > 0) {
    	    segP->procState[i].limit = segP->procState[i].limit - num;
    	    if (segP->procState[i].limit < 0) {
    	    	/* limit was not high enough, reset to zero */
    	    	/* negative means it's a dead backend	    */
    	    	segP->procState[i].limit = 0;
    	    }
    	}
    }
}
 

/************************************************************************/
/* SIDelDataEntry(segP)	    - free the FIRST entry   	    	    	*/
/************************************************************************/
bool
SIDelDataEntry(segP)
    SISeg 	*segP;
{
    SISegEntry 	    *eP, *e1P;
    SISegEntry 	    *SIGetFirstDataEntry();

    if (!SIDecNumEntries(segP, 1))  {
    	/* no entries in buffer */
    	return(false);
    }
    
    e1P = SIGetFirstDataEntry(segP);
    SISetStartEntryChain(segP, e1P->next);
    if (SIGetStartEntryChain(segP) == InvalidOffset) {
    	/* it was the last entry */
    	SISetEndEntryChain(segP, InvalidOffset);
    }
    /* free the entry */
    e1P->isfree = true;
    e1P->next = SIGetStartFreeSpace(segP);
    SISetStartFreeSpace(segP, SIEntryOffset(segP, e1P));
    SIDecProcLimit(segP, 1);
    return(true); 
}

    

/************************************************************************/
/* SISetProcStateInvalid(segP)	checks and marks a backends state as 	*/
/*  	    	    	    	    invalid 	    	    	    	*/
/************************************************************************/
void
SISetProcStateInvalid(segP)
    SISeg 	*segP;
{
    int i;

    for (i=0; i < MaxBackendId; i++) {
    	if (segP->procState[i].limit == 0) {
    	    /* backend i didn't read any message    	    	    	*/
    	    segP->procState[i].resetState = true;
    	    /*XXX signal backend that it has to reset its internal cache ? */
    	}
    }
}

/************************************************************************/
/* SIReadEntryData(segP, backendId, function)	    	    	    	*/
/*  	    	    	- marks messages to be read by id   	    	*/
/*  	    	          and executes function	    	    	    	*/
/************************************************************************/
void
SIReadEntryData(segP, backendId, invalFunction, resetFunction)
    SISeg   *segP;
    int     backendId;
    void    (*invalFunction)();
    void    (*resetFunction)();
{
    int i;
    SISegEntry *data;

    Assert(segP->procState[backendId - 1].tag == MyBackendTag);

    if (!segP->procState[backendId - 1].resetState) {
    	/* invalidate data, but only those, you have not seen yet !!*/
    	/* therefore skip read messages */
    	i = 0;
    	data = SIGetNthDataEntry(segP, 
    	    	    	    	 SIGetProcStateLimit(segP, backendId - 1) + 1);
    	while (data != NULL) {
    	    i++;
    	    segP->procState[backendId - 1].limit++;  /* one more message read */
    	    invalFunction(data->entryData.cacheId, 
    	         data->entryData.hashIndex,
    	         &data->entryData.pointerData);
    	    data = SIGetNextDataEntry(segP, data->next);
    	}
    	/* SIDelExpiredDataEntries(segP); */
    } else {
    	/*backend must not read messages, its own state has to be reset	    */
    	elog(NOTICE, "SIMarkEntryData: cache state reset");
        resetFunction(); /* XXXX call it here, parameters? */

		/* new valid state--mark all messages "read" */
		segP->procState[backendId - 1].resetState = false;
		segP->procState[backendId - 1].limit = SIGetNumEntries(segP);
    }
    /* check whether we can remove dead messages    	    	    	    */
    if (i > MAXNUMMESSAGES) {
    	elog(FATAL, "SIReadEntryData: Invalid segment state");
    }
}

/************************************************************************/
/* SIDelExpiredDataEntries  (segP)  - removes irrelevant messages   	*/
/************************************************************************/
void
SIDelExpiredDataEntries(segP)
    SISeg 	*segP;
{
    int   min, i, h;
    
    min = 9999999;
    for (i = 0; i < MaxBackendId; i++) {
    	h = SIGetProcStateLimit(segP, i);
    	if (h >= 0)  { /* backend active */
    	    if (h < min ) min = h;
    	}
    }
    if (min != 9999999) {
    	/* we can remove min messages */
    	for (i = 1; i <= min; i++) {
    	    /* this  adjusts also the state limits!*/
    	    if (!SIDelDataEntry(segP)) { 
            	elog(FATAL, "SIDelExpiredDataEntries: Invalid segment state");
    	    }
    	}
    }
}



/************************************************************************/
/* SISegInit(segP)  - initializes the segment	    	    	    	*/
/************************************************************************/
void
SISegInit(segP)
    SISeg *segP;
{
    SISegOffsets    *oP;
    SISegOffsets    *SIComputeSize();
    int	    	    segSize, i;
    SISegEntry      *eP;
    
    oP = SIComputeSize(&segSize);
    /* set sempahore ids in the segment */
    /* XXX */
    SISetStartEntrySection(segP, oP->offsetToFirstEntry);
    SISetEndEntrySection(segP, oP->offsetToEndOfSegemnt);
    SISetStartFreeSpace(segP, 0);
    SISetStartEntryChain(segP, InvalidOffset);
    SISetEndEntryChain(segP, InvalidOffset);
    (void) SISetNumEntries(segP, 0);
    (void) SISetMaxNumEntries(segP, MAXNUMMESSAGES);
    for (i = 0; i < MaxBackendId; i++) {
    	segP->procState[i].limit = -1; 	    /* no backend active  !!*/
    	segP->procState[i].resetState = false;
		segP->procState[i].tag = InvalidBackendTag;
    }
    /* construct a chain of free entries    	    	    	    */
    for (i = 1; i < MAXNUMMESSAGES; i++)  {
    	eP = (SISegEntry  *) ((Pointer) segP +
    	                     SIGetStartEntrySection(segP) +
    	                     (i - 1) * sizeof(SISegEntry));
    	eP->isfree = true;
    	eP->next = i * sizeof(SISegEntry); /* relative to B */
    }
    /* handle the last free entry separate  	    	    	    */
    eP = (SISegEntry  *) ((Pointer) segP +
    	                     SIGetStartEntrySection(segP) +
    	                     (MAXNUMMESSAGES - 1) * sizeof(SISegEntry));
    eP->isfree = true;
    eP->next = InvalidOffset;  /* it's the end of the chain !! */
    	                     
}
   


/************************************************************************/
/* SISegmentKill(key)   - kill any segment                              */
/************************************************************************/
void
SISegmentKill(key)
    int key;                    /* the corresponding key for the segment    */
{   
    IpcMemoryKill(key);
}	


/************************************************************************/
/* SISegmentGet(key, size)  - get a shared segment of size <size>       */
/*                returns a segment id                                  */
/************************************************************************/
IpcMemoryId
SISegmentGet(key, size, create)
    int     key;                /* the corresponding key for the segment    */
    int     size;               /* size of segment in bytes                 */
    bool    create;
{
    IpcMemoryId   shmid;

    if (create) {
	shmid = IpcMemoryCreate(key, size, IPCProtection);
    } else {
	shmid = IpcMemoryIdGet(key, size);
    }
    return(shmid);
}

/************************************************************************/
/* SISegmentAttach(shmid)   - attach a shared segment with id shmid     */
/************************************************************************/
void
SISegmentAttach(shmid)
    IpcMemoryId   shmid;
{
    shmInvalBuffer = (struct SISeg *) IpcMemoryAttach(shmid);
    if (shmInvalBuffer == IpcMemAttachFailed) {   
	/* XXX use validity function */
	elog(NOTICE, "SISegmentAttach: Could not attach segment");
	elog(FATAL, "SISegmentAttach: %m");
    }
}


/************************************************************************/
/* SISegmentInit(killExistingSegment, key)  initialize segment	    	*/
/************************************************************************/
int
SISegmentInit(killExistingSegment, key)
    bool    killExistingSegment;
    IPCKey  key;
{ 
    int     	    	status, segSize;
    IpcMemoryId	    	shmId;
    bool    	    	create;
    
    if (killExistingSegment) {
        /* Kill existing segment */
        /* set semaphore */
    	SISegmentKill(key);
    	
        /* Get a shared segment */
         
        (void) SIComputeSize(&segSize);
        create = true;
        shmId = SISegmentGet(key,segSize, create);
        if (shmId < 0) {
            perror("SISegmentGet: failed");
            return(-1);                                     /* an error */
        }

        /* Attach the shared cache invalidation  segment */
        /* sets the global variable shmInvalBuffer */
        SISegmentAttach(shmId);

        /* Init shared memory table */
        SISegInit(shmInvalBuffer);  
    } else {
    	/* use an existing segment */
    	create = false;
    	shmId = SISegmentGet(key, 0, create);
    	if (shmId < 0) {
    	    perror("SISegmentGet: getting an existent segment failed");
    	    return(-1);	    	    	    	    	    /* an error */
    	}
    	/* Attach the shared cache invalidation segment */
      	SISegmentAttach(shmId);
    }
    return(1);
}


/* synchronization of the shared buffer access	    	    */

void
SISyncInit(key)
    IPCKey	key;
{
#ifdef HAS_TEST_AND_SET
    SharedInvalidationLockId = (int)SINVALLOCKID;  /* a fixed lock */
#else /* HAS_TEST_AND_SET */
    int status;
    SharedInvalidationSemaphore =
    	IpcSemaphoreCreate(key, 1, IPCProtection, SI_LockStartValue,
    	                   &status);
    if(0) { /* XXX use validity function and check status */
    	elog(FATAL, "SISynchInit: %m");
    }
#endif /* HAS_TEST_AND_SET */
}

void
SISyncKill(key)
    IPCKey	key;
{
#ifndef HAS_TEST_AND_SET
    IpcSemaphoreKill(key);
#endif
}


void
SIReadLock()
{
#ifdef HAS_TEST_AND_SET
    SharedLock(SharedInvalidationLockId);
#else /* HAS_TEST_AND_SET */
    IpcSemaphoreLock ( SharedInvalidationSemaphore ,
		       0 , 
		       SI_SharedLock );
#endif /* HAS_TEST_AND_SET */
}


void
SIWriteLock()
{
#ifdef HAS_TEST_AND_SET
    ExclusiveLock(SharedInvalidationLockId);
#else /* HAS_TEST_AND_SET */
    IpcSemaphoreLock ( SharedInvalidationSemaphore,
		       0 ,
		      SI_ExclusiveLock );
#endif /* HAS_TEST_AND_SET */
}


void
SIReadUnlock()
{
#ifdef HAS_TEST_AND_SET
    SharedUnlock(SharedInvalidationLockId);
#else /* HAS_TEST_AND_SET */
    IpcSemaphoreLock ( SharedInvalidationSemaphore,
		       0,
		       (- SI_SharedLock) );
#endif /* HAS_TEST_AND_SET */
}


void
SIWriteUnlock()
{
#ifdef HAS_TEST_AND_SET
    ExclusiveUnlock(SharedInvalidationLockId);
#else /* HAS_TEST_AND_SET */
    IpcSemaphoreLock ( SharedInvalidationSemaphore,
		       0,
		       ( - SI_ExclusiveLock ) );
#endif /* HAS_TEST_AND_SET */
}



#ifdef _FOO_BAR_BAZ_
/************************************************************************
* debug routines
*
* This code tries to use the old lmgr setup and should not be called 
* unless you are willing to set up a spin lock for it as was done above
* to handle shared invaladation processing.
*************************************************************************/
void
SIBufferImage()
{
    int i, status;
    SISegEntry 	*SIGetFirstDataEntry();
    SISegEntry 	*eP;
    SISeg   	*segP;
    
    /* READ LOCK buffer */
    status = LMLock(SILockTableId, LockWait, &SIRelId, &SIDummyOid, &SIDummyOid,
    	    	    SIXid, MultiLevelLockRequest_ReadRelation);
    if (status == L_ERROR) {
    	    elog(FATAL, "InvalidateSharedInvalid: Could not lock buffer segment");
    }
    
    segP = shmInvalBuffer;
    fprintf(stderr, "\nDEBUG: current segment IMAGE");
    fprintf(stderr, "\n startEntrySection: %d", SIGetStartEntrySection(segP));
    fprintf(stderr, "\n endEntrySection: %d", SIGetEndEntrySection(segP));
    fprintf(stderr, "\n startFreeSpace: %d", SIGetStartFreeSpace(segP));
    fprintf(stderr, "\n startEntryChain: %d", SIGetStartEntryChain(segP));
    fprintf(stderr, "\n endEntryChain: %d", SIGetEndEntryChain(segP));
    fprintf(stderr, "\n numEntries: %d", SIGetNumEntries(segP));
    fprintf(stderr, "\n maxNumEntries: %d", SIGetMaxNumEntries(segP));
    for (i = 0; i < MaxBackendId; i++) {
    	fprintf(stderr, "\n   Backend[%2d] - limit: %3d  reset: %s", 
    	    	    i + 1,
    	    	    SIGetProcStateLimit(segP, i),
    	    	    SIGetProcStateResetState(segP, i) ? "true" : "false");
    }
    fprintf(stderr, "\n Message entries:");
    eP = SIGetFirstDataEntry(segP);
    while (eP != NULL) {
    	fprintf(stderr, "\n  cacheId: %-7d", eP->entryData.cacheId);
    	fprintf(stderr, "   free: %s", eP->isfree ? "true" : "false");
    	fprintf(stderr, "   my offset: %-4d", SIEntryOffset(segP, eP));
    	fprintf(stderr, "   next: %-4d", eP->next);
    	eP = SIGetNextDataEntry(segP, eP->next);
    }
    fprintf(stderr, "\n No further entries");
    
    fprintf(stderr, "\n No. of free entries: %d", 
    	    	    (SIGetMaxNumEntries(segP) - SIGetNumEntries(segP)));
/*  fprintf(stderr, "\n Free space chain:");
    eP = SIGetNextDataEntry(segP, SIGetStartFreeSpace(segP));
    while (eP != NULL) {
    	fprintf(stderr, "\n  free: %s", eP->isfree ? "true" : "false");
    	fprintf(stderr, "   my offset: %-4d", SIEntryOffset(segP, eP));
    	fprintf(stderr, "   next: %-4d", eP->next);
    	eP = SIGetNextDataEntry(segP, eP->next);
    }
    fprintf(stderr, "\n No further entries");
*/  
    fprintf(stderr, "\n");
    
    /* UNLOCK buffer */
    status = LMLockReleaseAll(SILockTableId, SIXid);
    if (status == L_ERROR) {
    	    elog(FATAL, "InvalidateSharedInvalid: Could not unlock buffer segment");
    }
}
#endif _FOO_BAR_BAZ_

/****************************************************************************/
/*  Invalidation functions for testing cache invalidation   	    	    */
/****************************************************************************/
void
SIinvalFunc(data)
    int data;
{   
    printf(" Backend: %d -- invalidating data (cacheId) %d\n",
    	    MyBackendId, data);
}

void
SIresetFunc()
{   
    printf("\n Backend: %d -- state reset done",
    	    MyBackendId);
}
