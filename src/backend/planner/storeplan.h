/* ----------------------------------------------------------------
 *   FILE
 *	storeplan.h
 *
 *   DESCRIPTION
 *	prototypes for storeplan.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef storeplanIncluded		/* include this file only once */
#define storeplanIncluded	1

extern LispValue StringToPlan ARGS((char *string));
extern LispValue StringToPlanWithParams ARGS((char *string, ParamListInfo *paramListP));
extern char *PlanToString ARGS((LispValue l));

#endif /* storeplanIncluded */
