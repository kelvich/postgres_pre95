/* ----------------------------------------------------------------
 *   FILE
 *	creatinh.h
 *
 *   DESCRIPTION
 *	prototypes for creatinh.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef creatinhIncluded		/* include this file only once */
#define creatinhIncluded	1

/*
 * DefineRelation --
 *	Creates a new relation.
 */

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
extern void DefineRelation ARGS((char *relname, LispValue parameters, LispValue schema));
extern void RemoveRelation ARGS((Name name));
extern List lispCopyList ARGS((List list));

#endif /* creatinhIncluded */
