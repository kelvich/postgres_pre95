/*======================================================================
 *
 * FILE:
 * stringinfo.c
 *
 * $Header$
 *
 * These are routines that can be used to write informations to a string,
 * without having to worry about string lengths, space allocation etc.
 *
 * Ideally the interface should look like the file i/o interface,
 *======================================================================
 */
#include <strings.h>
#include "nodes/pg_lisp.h"
#include "tmp/stringinfo.h"
#include "utils/log.h"
#include "utils/palloc.h"

/*---------------------------------------------------------------------
 * makeStringInfo
 *
 * Create a StringInfoData & return a pointer to it.
 *
 *---------------------------------------------------------------------
 */
StringInfo
makeStringInfo()
{
    StringInfo res;
    long size;

    res = (StringInfo) palloc(sizeof(StringInfoData));
    if (res == NULL) {
	elog(WARN, "makeStringInfo: Out of memory!");
    }

    size = 100;
    res->data = (String) palloc(size);
    if (res->data == NULL) {
	elog(WARN,
	    "makeStringInfo: Out of memory! (%ld bytes requested)", size);
    }
    res->maxlen = size;
    res->len = 0;
    /*
     * NOTE: we must initialize `res->data' to the empty string because
     * we use 'strcat' in 'appendStringInfo', which of course it always
     * expects a null terminated string.
     */
    res->data[0] = '\0';

    return(res);
}

/*---------------------------------------------------------------------
 * appendStringInfo
 *
 * append to the current 'StringInfo' a new string.
 * If there is not enough space in the current 'data', then reallocate
 * some more...
 *
 * NOTE: if we reallocate space, we pfree the old one!
 *---------------------------------------------------------------------
 */
void
appendStringInfo(str, buffer)
StringInfo str;
char *buffer;
{
    StringInfo res;
    int buflen, newlen;
    String s;

    Assert((str!=NULL));

    /*
     * do we have enough space to append the new string?
     * (don't forget to count the null string terminating char!)
     * If no, then reallocate some more.
     */
    buflen = strlen(buffer);
    if (buflen + str->len >= str->maxlen-1) {
	/*
	 * how much more space to allocate ?
	 * Let's say double the current space...
	 * However we must check if this is enough!
	 */
	newlen = 2 * str->len;
	while (buflen + str->len >= newlen-1) {
	    newlen = 2 * newlen;
	}
	/*
	 * allocate enough space.
	 */
	s = (String) palloc(newlen);
	if (s==NULL) {
	    elog(WARN,
		"appendStringInfo: Out of memory (%d bytes requested)",
		newlen);
	}
	bcopy(str->data, s, str->len+1);
	pfree(str->data);
	str->maxlen = newlen;
	str->data = s;
    }

    /*
     * OK, we have enough space now, append 'buffer' at the
     * end of the string & update the string length.
     * NOTE: this is a text string (i.e. printable characters)
     * so 'strcat' will do the job (no need to use 'bcopy' et all...)
     */
    (void) strcat(str->data, buffer);
    str->len += buflen;
}
