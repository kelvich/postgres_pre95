/*=====================================================================
 *
 * FILE:
 * stringfuncs.h
 *
 * $Header$
 *
 * Declarations/definitons for "string" functions.
 *=====================================================================
 */

#ifndef StringInfoIncluded
#define StringInfoIncluded

#include "tmp/c.h"		/* for 'String' */

/*-------------------------
 * StringInfoData holds information about a string.
 * 	'data' is the string.
 *	'len' is the current string length (as returned by 'strlen')
 *	'maxlen' is the size in bytes of 'data', i.e. the maximum string
 *		size (includeing the terminating '\0' char) that we can
 *		currently store in 'data' without having to reallocate
 *		more space.
 */
typedef struct StringInfoData {
    String data;
    int maxlen;
    int len;
} StringInfoData;

typedef StringInfoData *StringInfo;

/*------------------------
 * makeStringInfo
 * create a 'StringInfoData' & return a pointer to it.
 */
extern
StringInfo
makeStringInfo ARGS((
));

/*------------------------
 * appendStringInfo
 * similar to 'strcat' but reallocates more space if necessary...
 */
extern
void
appendStringInfo ARGS((
    StringInfo	str,
    char *	buffer
));

#endif StringInfoIncluded
