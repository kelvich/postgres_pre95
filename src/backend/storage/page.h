/*
 * page.h --
 *	POSTGRES buffer page abstraction definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PageIncluded	/* Include this file only once. */
#define PageIncluded	1

#ifndef C_H
#include "c.h"
#endif

typedef Pointer	Page;

/*
 * PageIsValid --
 *	True iff the page is valid.
 */
extern
bool
PageSizeIsValid ARGS((
	Page	page
));

#endif	/* !defined(PageIncluded) */
