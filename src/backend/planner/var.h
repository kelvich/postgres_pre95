/* ----------------------------------------------------------------
 *   FILE
 *	var.h
 *
 *   DESCRIPTION
 *	prototypes for var.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef varIncluded		/* include this file only once */
#define varIncluded	1

extern LispValue pull_agg_clause ARGS((LispValue clause));
extern LispValue pull_varnos ARGS((LispValue me));
extern LispValue pull_var_clause ARGS((LispValue clause));
extern bool var_equal ARGS((LispValue var1, LispValue var2));
extern int numlevels ARGS((Var var));
extern ObjectId var_getrelid ARGS((Var var));

#endif /* varIncluded */
