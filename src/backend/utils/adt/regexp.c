/*
 *  regexp.c -- regular expression handling code.
 *
 *	$Header$
 */
#include "tmp/c.h"		/* postgres system include file */
#include "utils/log.h"		/* for logging postgres errors */

/*
 *  macros to support the regexp(3) library calls
 */
#define INIT		register char *p = instring;
#define GETC()		(*p++)
#define PEEKC()		*(p+1)
#define	UNGETC(c)	(*--p = (c))
#define	RETURN(v)	return(v)
#define	ERROR(val)	elog(WARN, "regexp library reports error %d", (val));

#define	EXPBUFSZ	256
#define	PCHARLEN	16

#include <regexp.h>

/*
 *  routines that use the regexp stuff
 */
bool
char16regexeq(p, s)
	char *p;
	char *s;
{
	char *expbuf, *endbuf;
	char *sterm, *pterm;
	int result;

	expbuf = (char *) palloc(EXPBUFSZ);
	endbuf = expbuf + (EXPBUFSZ - 1);

	/* be sure the strings are null-terminated */
	sterm = (char *) palloc(PCHARLEN + 1);
	bzero(sterm, PCHARLEN + 1);
	strncpy(sterm, s, PCHARLEN);
	pterm = (char *) palloc(PCHARLEN + 1);
	bzero(pterm, PCHARLEN + 1);
	strncpy(pterm, p, PCHARLEN);

	/* compile the re */
	(void) compile(pterm, expbuf, endbuf, NULL);

	/* do the regexp matching */
	result = step(sterm, expbuf);

	pfree(expbuf);
	pfree(sterm);
	pfree(pterm);

	if (result)
		return (true);
	else
		return (false);
}

bool
textregexeq(p, s)
	char *p;
	char *s;
{
	char *expbuf, *endbuf;
	int result;

	expbuf = (char *) palloc(EXPBUFSZ);
	endbuf = expbuf + (EXPBUFSZ - 1);

	/* compile the re */
	(void) compile(p, expbuf, endbuf, NULL);

	/* do the regexp matching */
	result = step(s, expbuf);

	pfree(expbuf);

	if (result)
		return (true);
	else
		return (false);
}
