/* ----------------------------------------------------------------
 *   FILE
 *	fastpath.h
 *
 *   DESCRIPTION
 *
 *   NOTES
 *	This information pulled out of tcop/fastpath.c and put
 *	here so that the PQfn() in be-pqexec.c could access it.
 *	-cim 2/26/91
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef FastpathIncluded 	/* include this file only once. */
#define FastpathIncluded 	1

/* ----------------
 *	fastpath #defines
 * ----------------
 */
#define FUNCTION_BY_NAME 	(-1)
#define MAX_FUNC_NAME_LENGTH	80
#define VAR_LENGTH_RESULT 	(-1)
#define VAR_LENGTH_ARG 		(-5)
#define MAX_STRING_LENGTH 	100
#define PORTAL_RESULT 		(-2)
#define PASS_BY_REF 		(1)
#define PASS_BY_VALUE 		(0)

#endif FastpathIncluded
