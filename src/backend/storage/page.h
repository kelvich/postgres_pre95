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

#ifndef C_H
#include "c.h"
#endif

typedef Pointer	Page;

/*
 * PageIsValid --
 *	True iff page is valid.
 */
extern
bool
PageIsValid ARGS((
	Page	page
));

#define PAGE_SYMBOLS \
	SymbolDecl(PageIsValid, "_PageIsValid")

#endif	/* !defined(PageIncluded) */
