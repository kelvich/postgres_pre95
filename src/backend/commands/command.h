/*
 * command.h --
 *	POSTGRES utility command definitions.
 */

#ifndef	CommandIncluded		/* Include this file only once */
#define CommandIncluded	1

/*
 * Identification:
 */
#define COMMAND_H	"$Header$"

/*
 * PortalCleanup --
 *	Cleans up the query state of the portal.
 *
 * Exceptions:
 *	BadArg if portal invalid.
 */
extern
void
PortalCleanup ARGS((
	Portal	portal
));

/*
 * PerformPortalFetch --
 *	Performs the POSTQUEL function FETCH.  Fetches count (or all if 0)
 * tuples in portal with name in the forward direction iff goForward.
 *
 * Exceptions:
 *	BadArg if forward invalid.
 *	"WARN" if portal not found.
 */
extern
void
PerformPortalFetch ARGS((
	String	name,		/* XXX PortalName */
	bool	goForward,
	Count	count,
	String tag,
        CommandDest dest
));

/*
 * PerformPortalClose --
 *	Performs the POSTQUEL function CLOSE.
 */
extern
void
PerformPortalClose ARGS((
	String	name,		/* XXX PortalName */
	CommandDest dest		 
));

/*
 * PerformAddAttribute --
 *	Performs the POSTQUEL function ADD.
 */
extern
void
PerformAddAttribute ARGS((
	Name	relationName,
	List	schema
));

#endif	/* !defined(CommandIncluded) */
