/* ----------------------------------------------------------------
 *   FILE
 *	module.h
 *
 *   DESCRIPTION
 *	this file contains general "module" stuff  that used to be
 *	spread out between the following files:
 *
 *	enbl.h			module enable stuff
 *	trace.h			module trace stuff (now gone)
 *
 *   NOTES
 *	some more stuff may be moved here.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#ifndef ModuleHIncluded
#define ModuleHIncluded 1	/* include once only */

#include "tmp/c.h"
#include "tmp/simplelists.h"

/* ----------------
 *	enbl.h
 * ----------------
 */
/*
 * enbl.h --
 *	POSTGRES module enable and disable support code.
 *
 * Identification:
 *	$Header$
 */

#ifndef	EnblIncluded		/* Include this file only once */
#define EnblIncluded	1

#define ENBL_H	"$Header$"

/*
 * BypassEnable --
 *	False iff enable/disable processing is required given on and "*countP."
 *
 * Note:
 *	As a side-effect, *countP is modified.  It should be 0 initially.
 *
 * Exceptions:
 *	BadState if called with pointer to value 0 and false.
 *	BadArg if "countP" is invalid pointer.
 *	BadArg if on is invalid.
 */
extern
bool
BypassEnable ARGS((
	Count	*enableCountInOutP,
	bool	on
));

#endif	/* !defined(EnblIncluded) */

/* ----------------
 *	end of module.h
 * ----------------
 */
#endif ModuleHIncluded
