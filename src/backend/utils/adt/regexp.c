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

#ifdef sprite
#include "regexp.h"
#else
#include <regexp.h>
#endif /* sprite */

/*
 *  interface routines called by the function manager
 */

/*
 *  routines that use the regexp stuff
 */
bool
char16regexeq(s, p)
	char *s;
	char *p;
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

	return ((bool) result);
}

bool
char16regexne(s, p)
	char *s;
	char *p;
{
	return (!char16regexeq(s, p));
}

bool
textregexeq(s, p)
	char *s;
	char *p;
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

	return ((bool) result);
}

bool
textregexne(s, p)
	char *s;
	char *p;
{
	return (!textregexeq(s, p));
}
