/*
 *  regexp.c -- regular expression handling code.
 *
 *	$Header$
 */
#include "tmp/postgres.h"	/* postgres system include file */
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

	if (!s || !p)
		return FALSE;

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
	struct varlena *s;
	struct varlena *p;
{
	char *expbuf, *endbuf;
	char *sbuf, *pbuf;
	int result;

	if (!s || !p)
		return FALSE;

	/* ---------------
	 * text is a varlena, not a string so we have to make 
	 * a string from the vl_data field of the struct. 
	 * jeff 13 July 1991
	 * ---------------
	 */
	
	/* palloc the length of the text + the null character */
	sbuf = (char *) palloc(s->vl_len - sizeof(int32) + 1);
	pbuf = (char *) palloc(p->vl_len - sizeof(int32) + 1);

	expbuf = (char *) palloc(EXPBUFSZ);
	endbuf = expbuf + (EXPBUFSZ - 1);

	bcopy(s->vl_dat, sbuf, s->vl_len - sizeof(int32));
	bcopy(p->vl_dat, pbuf, p->vl_len - sizeof(int32));
	*(sbuf + s->vl_len - sizeof(int32)) = (char)NULL;
	*(pbuf + p->vl_len - sizeof(int32)) = (char)NULL;


	/* compile the re */
	(void) compile(pbuf, expbuf, endbuf, NULL);

	/* do the regexp matching */
	result = step(sbuf, expbuf);

	pfree(expbuf);
	pfree(sbuf);
	pfree(pbuf);

	return ((bool) result);
}

bool
textregexne(s, p)
	char *s;
	char *p;
{
	return (!textregexeq(s, p));
}
