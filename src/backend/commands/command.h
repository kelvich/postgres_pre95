/* ----------------------------------------------------------------
 *   FILE
 *	command.h
 *
 *   DESCRIPTION
 *	prototypes for command.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef commandIncluded		/* include this file only once */
#define commandIncluded	1

/*
 * PortalCleanup --
 *	Cleans up the query state of the portal.
 *
 * Exceptions:
 *	BadArg if portal invalid.
 */

/*
 * PerformPortalFetch --
 *	Performs the POSTQUEL function FETCH.  Fetches count (or all if 0)
 * tuples in portal with name in the forward direction iff goForward.
 *
 * Exceptions:
 *	BadArg if forward invalid.
 *	"WARN" if portal not found.
 */

/*
 * PerformPortalClose --
 *	Performs the POSTQUEL function CLOSE.
 */

/*
 * PerformAddAttribute --
 *	Performs the POSTQUEL function ADD.
 */

extern void PortalCleanup ARGS((Portal portal));
extern void PerformPortalFetch ARGS((String name, bool forward, Count count, String tag, CommandDest dest));
extern void PerformPortalClose ARGS((String name, CommandDest dest));
extern void PerformAddAttribute ARGS((Name relationName, Name userName, List schema));

#endif /* commandIncluded */
