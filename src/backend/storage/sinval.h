/*
 * sinval.h --
 *	POSTGRES shared cache invalidation communication definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SInvalIncluded	/* Include this file only once */
#define SInvalIncluded	1


#ifndef C_H
#include "c.h"
#endif

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
