
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
 * sinval.h --
 *	POSTGRES shared cache invalidation communication definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SInvalIncluded	/* Include this file only once */
#define SInvalIncluded	1


#include "c.h"

#include "ipci.h"
#include "itemptr.h"

typedef int16	BackendId;	/* unique currently active backend identifier */

#define InvalidBackendId	(-1)

typedef int32	BackendTag;	/* unique backend identifier */

#define InvalidBackendTag	(-1)

extern BackendId	MyBackendId;	/* backend id of this backend */
extern BackendTag	MyBackendTag;	/* backend tag of this backend */

/*
 * GenerateMyBackendId --
 */
extern
void
GenerateMyBackendId ARGS((
	void
));

/*
 * CreateSharedInvalidationState --
 *	Creates the shared cache invalidation state.
 */
void
CreateSharedInvalidationState ARGS((
	IPCKey	key
));

/*
 * AttachSharedInvalidationState --
 *	Attaches the shared cache invalidation state.
 */
void
AttachSharedInvalidationState ARGS((
	IPCKey	key
));

/*
 * InitSharedInvalidationState --
 *	Initializes the backend state for processing.
 */
void
InitSharedInvalidationState ARGS((
	IPCKey	key
));

/*
 * RegisterSharedInvalid --
 *  Returns a new local cache invalidation state containing a new entry.
 *
 * Note:
 *  Assumes hash index is valid.
 *  Assumes item pointer is valid.
 */
extern
void
RegisterSharedInvalid ARGS((
    int	    cacheId,	/* XXX */
    Index   	hashIndex,
    ItemPointer	pointer
));

/*
 * InvalidateSharedInvalid --
 *  Processes all entries in a shared cache invalidation state.
 */
extern
void
InvalidateSharedInvalid ARGS((
    void    (*function)(
    	int 	cacheId,
    	Index	    hashIndex,
    	ItemPointer pointer
    )
));

#endif	/* !defined(SInvalIncluded) */
