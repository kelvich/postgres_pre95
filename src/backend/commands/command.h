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

#include "tmp/c.h"
#include "nodes/pg_lisp.h"

/*
 * CreateQueryDesc --
 *	Returns a query descriptor created from a parse and plan.
 *
 * Note:
 *	This definition belongs closer to the executor code.
 */
extern
List
CreateQueryDesc ARGS((
	List	parse,
	List	plan
));

/*
 * ValidateParse --
 *	Returns normally iff parser output is *syntatically* valid.
 *
 * Exceptions:
 *	.
 */
extern
void
ValidateParse ARGS((
	List	parse
));

/*
 * ValidateParsedQuery --
 *	Returns normally iff parsed query command is *syntatically* valid.
 *
 * Exceptions:
 *	.
 */
extern
void
ValidateParsedQuery ARGS((
	List	parse
));

/*
 * ProcessQuery --
 *	.
 *
 * Exceptions:
 *	.
 */
extern
void
ProcessQuery ARGS((
	LispValue	parse,
	Plan		plan
));

/*
 * ValidateUtility --
 *	Returns normally iff parsed utility command is *syntatically* valid.
 *
 * Exceptions:
 *	.
 */
extern
void
ValidateUtility ARGS((
	int		commandTag,
	LispValue	args
));

/*
 * ProcessUtility --
 *	.
 *
 * Exceptions:
 *	.
 */
extern
void
ProcessUtility ARGS((
	int		commandTag,
	LispValue	args
));

/*
 * EndCommand --
 *	Informs the frontend that command has finished.
 *
 * Exceptions:
 *	BadArg if invalid commandId.
 *
 * Note:
 *	The commandId is a free-format identification string which some
 * frontends (e.g, terminal monitors) may be interested in seeing.  There
 * should be a standard described in the protocol documentation.
 */
extern
void
EndCommand ARGS((
	String	commandId
));

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
	Count	count
));

/*
 * PerformPortalClose --
 *	Performs the POSTQUEL function CLOSE.
 */
extern
void
PerformPortalClose ARGS((
	String	name		/* XXX PortalName */
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

/*
 * PerformRelationFilter --
 *	Performs the POSTQUEL function COPY.
 */
extern
void
PerformRelationFilter ARGS((
	Name	relationName,
	bool	isBinary,
	bool	noNulls,
	bool	isFrom,
	String	fileName,
	Name	mapName,
	List	domains
));

#endif	/* !defined(CommandIncluded) */
