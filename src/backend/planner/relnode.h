/* ----------------------------------------------------------------
 *   FILE
 *	relnode.h
 *
 *   DESCRIPTION
 *	prototypes for relnode.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef relnodeIncluded		/* include this file only once */
#define relnodeIncluded	1

extern Rel get_rel ARGS((LispValue relid));
extern Rel rel_member ARGS((List relid, List rels));

#endif /* relnodeIncluded */
