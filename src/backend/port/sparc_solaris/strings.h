/* ----------------------------------------------------------------
 *   FILE
 *	strings.h
 *
 *   DESCRIPTION
 *	prototypes for BSD string functions.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef stringsIncluded		/* include this file only once */
#define stringsIncluded	1

#include <string.h>

#define	index(s,c)	strchr(s,c)
#define	rindex(s,c)	strrchr(s,c)

#endif /* stringsIncluded */
