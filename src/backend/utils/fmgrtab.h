/*----------------------------------------------------------------
 *   FILE
 *	fmgrtab.h
 *
 *   DESCRIPTION
 *
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 *----------------------------------------------------------------
 */

#ifndef FmgrtabIncluded
#define FmgrtabIncluded 1	/* Include this file only once */

#include "tmp/postgres.h"	/* for ObjectId */
#include "fmgr.h"

typedef struct {
	ObjectId	proid;
	uint16		nargs;
	func_ptr	func;
} FmgrCall;

extern FmgrCall	*fmgr_isbuiltin ARGS((ObjectId id));

#endif /* FmgrtabIncluded */
