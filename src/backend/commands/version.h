/*
 *  version.h
 * 
 *  Header file for versions.
 *
 *  $Header$
 */

#ifndef VersionIncluded          /* Include this file only once */
#define VersionIncluded  1
#endif

#include "name.h"


/*
 *  Creates a version.
 */
extern 
void
CreateVersion ARGS(( 
		    Name  name,
		    Name  bname));

/*
 *  Creates the deltas.
 */
extern
void
VersionCreate ARGS((
		    Name  vname,
		    Name  bname));


/*
 *  Returns a list of attributes for the given relation.
 */
extern 
LispValue
GetAttrList ARGS(( Name bname));

/*
 * Rule governing the append semantics for versions.
 */
extern
void
VersionAppend ARGS((
		    Name  vname,
		    Name  bname));

/*
 * Rule governing the retrieval semantics for versions.
 */
extern
void
VersionRetrieve ARGS((
		    Name  vname,
		    Name  bname));

/*
 * Rule governing the delete semantics for versions.
 */
extern
void
VersionDelete ARGS((
		    Name  vname,
		    Name  bname));

/*
 * Rule governing the update semantics for versions.
 */
extern
void
VersionReplace ARGS((
		    Name  vname,
		    Name  bname));
