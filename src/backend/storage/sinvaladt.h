
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * sinvaladt.h --
 *  POSTGRES shared cache invalidation segment definitions.
 *
 * Identification:
 * $Header$
 */

#include "postgres.h"	/* XXX */

#include "c.h"

#include "ipc.h"
#include "itemptr.h"
#include "sinval.h"
 
/*
 * The structure of the shared cache invaidation segment
 *
 */
/*
A------------- Header info --------------
    criticalSectionSemaphoreId
    generalSemaphoreId
    startEntrySection   (offset a)
    endEntrySection     (offset a + b)
    startFreeSpace      (offset relative to B)
    startEntryChain     (offset relatiev to B)
    endEntryChain       (offset relative to B)
    numEntries
    maxNumEntries
    procState[MaxBackendId] --> limit
				resetState (bool)
a				tag (POSTID)
B------------- Start entry section -------
    SISegEntry  --> entryData --> ... (see  SharedInvalidData!)
                    free  (bool)
                    next  (offset to next entry in chain )
b     .... (dynamically growing down)
C----------------End shared segment -------  

*/

/* Parameters (configurable)  *******************************************/
#define MaxBackendId 32      	    /* maximum number of backends   	*/
#define MAXNUMMESSAGES 1000 	    /* maximum number of messages in seg*/


#define	InvalidOffset	1000000000  /* a invalid offset  (End of chain)	*/

typedef struct ProcState {
    	int 	limit;      	/* the number of read messages	    	*/
    	bool 	resetState; 	/* true, if backend has to reset its state */
	int	tag;		/* special tag, recieved from the postmaster */
} ProcState;


typedef struct SISeg {
    IpcSemaphoreId  	criticalSectionSemaphoreId; /* semaphore id     */
    IpcSemaphoreId  	generalSemaphoreId; 	    /* semaphore id     */
    Offset      startEntrySection;  	/* (offset a)	    	    	*/
    Offset      endEntrySection;    	/* (offset a + b)   	    	*/
    Offset      startFreeSpace;	    	/* (offset relative to B)   	*/
    Offset      startEntryChain;    	/* (offset relative to B)   	*/
    Offset      endEntryChain;          /* (offset relative to B)   	*/
    int         numEntries;
    int         maxNumEntries;
    ProcState   procState[MaxBackendId]; /* reflects the invalidation state */
    /* here starts the entry section, controlled by offsets */
} SISeg;
#define SizeSISeg     sizeof(SISeg)

typedef struct SharedInvalidData {
    int	    	    	cacheId;    /* XXX */
    Index   	    	hashIndex;
    ItemPointerData 	pointerData;
} SharedInvalidData;

typedef SharedInvalidData   *SharedInvalid;


typedef struct SISegEntry {
    SharedInvalidData	entryData;  	    	    /* the message data */
    bool                free;	    	    	    /* entry free? */
    Offset  	    	next;	    	    	    /* offset to next entry*/
} SISegEntry;

#define SizeOfOneSISegEntry   sizeof(SISegEntry)
    
typedef struct SISegOffsets {
    Offset  startSegment;   	    	/* always 0 (for now) */
    Offset  offsetToFirstEntry;         /* A + a = B */
    Offset  offsetToEndOfSegemnt;       /* A + a + b */
} SISegOffsets;


/****************************************************************************/
/* synchronization of the shared buffer access	    	    	    	    */
/*    access to the buffer is synchronized by the lock manager !!   	    */
/****************************************************************************/

#define SI_LockStartValue  255
#define SI_SharedLock     (-1)
#define SI_ExclusiveLock  (-255)
