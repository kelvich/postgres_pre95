/*
 * page.h --
 *	POSTGRES buffer page abstraction definitions.
 */

#ifndef	PageIncluded		/* Include this file only once */
#define PageIncluded	1

/*
 * Identification:
 */
#define PAGE_H	"$Header$"

#include "tmp/c.h"

typedef Pointer	Page;

/*
 * PageIsValid --
 *	True iff page is valid.
 */
#define	PageIsValid(page) PointerIsValid(page)

#endif	/* !defined(PageIncluded) */
