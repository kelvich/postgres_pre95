/*
 * sinval.h --
 *	POSTGRES shared cache invalidation communication definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	SInvalIncluded	/* Include this file only once */
#define SInvalIncluded	1

#include "tmp/c.h"
#include "storage/ipci.h"
#include "storage/itemptr.h"
#include "storage/backendid.h"

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
InitSharedInvalidationState ARGS((void));

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
    void    (*invalFunction)(),
    void    (*resetFunction)()
));

#endif	/* !defined(SInvalIncluded) */
