/* ----------------------------------------------------------------
 *   FILE
 *	pfrag.h
 *
 *   DESCRIPTION
 *	prototypes for pfrag.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef pfragIncluded		/* include this file only once */
#define pfragIncluded	1

extern Fragment InitialPlanFragments ARGS((List parsetree, Plan plan));
extern void OptimizeAndExecuteFragments ARGS((List fragmentlist, CommandDest destination));

#endif /* pfragIncluded */
