/* ----------------------------------------------------------------
 *   FILE
 *	paramutils.h
 *
 *   DESCRIPTION
 *	prototypes for paramutils.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef paramutilsIncluded		/* include this file only once */
#define paramutilsIncluded	1

extern LispValue find_parameters ARGS((LispValue plan));
extern LispValue find_all_parameters ARGS((LispValue tree));
extern LispValue substitute_parameters ARGS((LispValue plan, LispValue params));
extern LispValue assoc_params ARGS((LispValue plan, LispValue param_alist));

#endif /* paramutilsIncluded */
