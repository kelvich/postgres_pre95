/* ----------------------------------------------------------------
 *   FILE
 *	planner.h
 *
 *   DESCRIPTION
 *	prototypes for planner.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef plannerIncluded		/* include this file only once */
#define plannerIncluded	1

#include "planner/internal.h"

extern Plan planner ARGS((LispValue parse));
extern Plan make_sortplan ARGS((List tlist, List sortkeys, List sortops, Plan plannode));
extern Plan init_query_planner ARGS((LispValue root, LispValue tlist, LispValue qual));
extern Existential make_existential ARGS((Plan left, Plan right));
extern void pg_checkretval ARGS((ObjectId rettype, List parselist));

#endif /* plannerIncluded */
