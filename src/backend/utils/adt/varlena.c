/*
 * varlena.c --
 * 	Functions for the variable-length built-in types.
 */

#include "c.h"

RcsId("$Header$");

#include <ctype.h>
#include <strings.h>

#include "palloc.h"
#include "postgres.h"	/* XXX for varlena */


	    /* ========== USER I/O ROUTINES ========== */


#define	VAL(CH)		((CH) - '0')
#define	DIG(VAL)	((VAL) + '0')

/*
 *	byteain		- converts from printable representation of byte array
 *
 *	Non-printable characters must be passed as '\nnn' (octal) and are
 *	converted to internal form.  '\' must be passed as '\\'.
 *	Returns NULL if bad form.
 *
 *	BUGS:
 *		The input is scaned twice.
 *		The error checking of input is minimal.
 */
struct varlena *
byteain(text)
	char	*text;
{
	register char	*tp;
	register char	*rp;
	register int	byte;
	struct varlena	*result;

	if (text == NULL)
		return(NULL);
	for (byte = 0, tp = text; *tp != '\0'; byte++)
		if (*tp++ == '\\')
			if (*tp == '\\')
				tp++;
			else if (!isdigit(*tp++) ||
				 !isdigit(*tp++) ||
				 !isdigit(*tp++))
				return(NULL);
	tp = text;
	byte += sizeof(int32);					/* varlena? */
	result = (struct varlena *) palloc(byte);
	result->vl_len = byte;					/* varlena? */
	rp = result->vl_dat;
	while (*tp != '\0')
		if (*tp != '\\' || *++tp == '\\')
			*rp++ = *tp++;
		else {
			byte = VAL(*tp++);
			byte <<= 3;
			byte += VAL(*tp++);
			byte <<= 3;
			*rp++ = byte + VAL(*tp++);
		}
	return(result);
}

/*
 * Shoves a bunch of memory pointed at by bytes into varlena.
 * BUGS:  Extremely unportable as things shoved can be string
 * representations of structs, etc.
 */

struct varlena *
shove_bytes(text, len)

unsigned char *text;
int len;

{
	struct varlena *result;

	result = (struct varlena *) palloc(len + sizeof(int32));
	result->vl_len = len;
	bcopy(text + sizeof(int32), result->vl_dat, len - sizeof(int32));
	return(result);
}



/*
 *	byteaout	- converts to printable representation of byte array
 *
 *	Non-printable characters are inserted as '\nnn' (octal) and '\' as
 *	'\\'.
 *
 *	NULL vlena should be an error--returning string with NULL for now.
 */
char *
byteaout(vlena)
	struct varlena	*vlena;
{
	register char	*vp;
	register char	*rp;
	register int	val;		/* holds unprintable chars */
	int		i;
	int		len;
	static	char	*result;

	if (vlena == NULL) {
		result = palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}
	vp = vlena->vl_dat;
	len = 1;		/* empty string has 1 char */
	for (i = vlena->vl_len - sizeof(int32); i != 0; i--)	/* varlena? */
		if (*vp == '\\')
			len += 2;
		else if (isascii(*vp) && isprint(*vp))
			len++;
		else
			len += 4;
	rp = result = palloc(len);
	vp = vlena->vl_dat;
	for (i = vlena->vl_len - sizeof(int32); i != 0; i--)	/* varlena? */
		if (*vp == '\\') {
			*vp++;
			*rp++ = '\\';
			*rp++ = '\\';
		} else if (isascii(*vp) && isprint(*vp))
			*rp++ = *vp++;
		else {
			val = *vp++;
			*rp = '\\';
			rp += 3;
			*rp-- = DIG(val & 07);
			val >>= 3;
			*rp-- = DIG(val & 07);
			val >>= 3;
			*rp = DIG(val & 03);
			rp += 3;
		}
	*rp = '\0';
	return(result);
}

/*
 *	textin		- converts "..." to internal representation
 */
struct varlena *
textin(text)
	char 	*text;
{
	register struct varlena	*result;
	register int		len;
	extern			bcopy();

	if (text == NULL)
		return(NULL);
	len = strlen(text) + sizeof(int32);			/* varlena? */
	result = (struct varlena *) palloc(len);
	result->vl_len = len;
	bcopy(text, result->vl_dat, len - sizeof(int32));	/* varlena? */
	return(result);
}

/*
 *	textout		- converts internal representation to "..."
 */
char *
textout(vlena)
	struct varlena	*vlena;
{
	register int	len;
	char		*result;
	extern		bcopy();

	if (vlena == NULL) {
		result = palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}
	len = vlena->vl_len - sizeof(int32);			/* varlena? */
	result = palloc(len + 1);
	bcopy(vlena->vl_dat, result, len);
	result[len] = '\0';
	return(result);
}


	     /* ========== PUBLIC ROUTINES ========== */


/*
 *	texteq		- returns 1 iff arguments are equal
 *	textne		- returns 1 iff arguments are not equal
 */
int32
texteq(arg1, arg2)
	struct varlena	*arg1, *arg2;
{
	register int	len;
	register char	*a1p, *a2p;

	if (arg1 == NULL || arg2 == NULL)
		return((int32) NULL);
	if ((len = arg1->vl_len) != arg2->vl_len)
		return((int32) 0);
	a1p = arg1->vl_dat;
	a2p = arg2->vl_dat;
	len -= sizeof(int32);					/* varlena? */
	while (len-- != 0)
		if (*a1p++ != *a2p++)
			return((int32) 0);
	return((int32) 1);
}

int32
textne(arg1, arg2)
	struct varlena	*arg1, *arg2;
{
	return((int32) !texteq(arg1, arg2));
}


	     /* ========== PRIVATE ROUTINES ========== */


			     /* (none) */
