/* ----------------------------------------------------------------
 *   FILE
 *	indexnode.h
 *
 *   DESCRIPTION
 *	prototypes for indexnode.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef indexnodeIncluded		/* include this file only once */
#define indexnodeIncluded	1

extern LispValue find_relation_indices ARGS((Rel rel));
extern LispValue find_secondary_index ARGS((bool notfirst, ObjectId relid));

#endif /* indexnodeIncluded */
