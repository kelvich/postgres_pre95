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

#define	index(s,c)	strchr((s),(c))
#define	rindex(s,c)	strrchr((s),(c))
#define bcopy(s,d,len)	memmove((d),(s),(size_t)(len))
#define bzero(b,len)    memset((b),0,(size_t)(len))
#define bcmp(s,d,len)	memcmp((s),(d),(size_t)(len))

#endif /* stringsIncluded */
