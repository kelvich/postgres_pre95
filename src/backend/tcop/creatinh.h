/*
 * creatinh.h --
 *	POSTGRES create/destroy relation with inheritance utility definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef CreatinhHIncluded
#define CreatinhHIncluded 1	/* include once only */

/*
 * DefineRelation --
 *	Creates a new relation.
 */
extern
void
DefineRelation ARGS((
	char		*relationName,
	LispValue	parameters,
	LispValue	schema
));

/*
 * RemoveRelation --
 *	Deletes a new relation.
 *
 * Exceptions:
 *	BadArg if name is invalid.
 *
 * Note:
 *	If the relation has indices defined on it, then the index relations
 * themselves will be destroyed, too.
 */
extern
void
RemoveRelation ARGS((
	Name	name
));

#endif CreatinhHIncluded
