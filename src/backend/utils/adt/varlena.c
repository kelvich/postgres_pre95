/*
 * varlena.c --
 * 	Functions for the variable-length built-in types.
 *
 * $Header$
 */

#include <ctype.h>
#include <strings.h>

#include "tmp/postgres.h"
#include "utils/log.h"
#include "utils/palloc.h"
#include "utils/builtins.h"
struct varlena *shove_bytes ARGS((unsigned char *stuff , int len ));

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
byteain(inputText)
	char	*inputText;
{
	register char	*tp;
	register char	*rp;
	register int	byte;
	struct varlena	*result;

	if (inputText == NULL)
		return(NULL);
	for (byte = 0, tp = inputText; *tp != '\0'; byte++)
		if (*tp++ == '\\')
			if (*tp == '\\')
				tp++;
			else if (!isdigit(*tp++) ||
				 !isdigit(*tp++) ||
				 !isdigit(*tp++))
				return(NULL);
	tp = inputText;
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
shove_bytes(stuff, len)

unsigned char *stuff;
int len;

{
	struct varlena *result;

	result = (struct varlena *) palloc(len + sizeof(int32));
	result->vl_len = len;
	bcopy(stuff + sizeof(int32), result->vl_dat, len - sizeof(int32));
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
		result = (char *) palloc(2);
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
	rp = result = (char *) palloc(len);
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
textin(inputText)
	char 	*inputText;
{
	register struct varlena	*result;
	register int		len;
	extern			bcopy();

	if (inputText == NULL)
		return(NULL);
	len = strlen(inputText) + sizeof(int32);		/* varlena? */
	result = (struct varlena *) palloc(len);
	result->vl_len = len;
	bcopy(inputText, result->vl_dat, len - sizeof(int32));	/* varlena? */
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
		result = (char *) palloc(2);
		result[0] = '-';
		result[1] = '\0';
		return(result);
	}
	len = vlena->vl_len - sizeof(int32);			/* varlena? */
	result = (char *) palloc(len + 1);
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

int32
text_lt(arg1, arg2)
	struct varlena *arg1, *arg2;
{
	int len;

	if (arg1 == NULL && arg2 != NULL)
		return((int32) 1);
	if (arg2 == NULL)
		return((int32) 0);
	if ((len = arg1->vl_len) < arg2->vl_len)
		len = arg2->vl_len;
	len -= sizeof(int32);					/* varlena? */

	if (strncmp(arg1->vl_dat, arg2->vl_dat, len) < 0)
		return ((int32) 1);
	return ((int32) 0);
}

int32
text_le(arg1, arg2)
	struct varlena *arg1, *arg2;
{
	int len;

	if (arg1 == NULL && arg2 != NULL)
		return((int32) 1);
	if (arg2 == NULL)
		return((int32) 0);
	if ((len = arg1->vl_len) < arg2->vl_len)
		len = arg2->vl_len;
	len -= sizeof(int32);					/* varlena? */

	if (strncmp(arg1->vl_dat, arg2->vl_dat, len) <= 0)
		return ((int32) 1);
	return ((int32) 0);
}

int32
text_gt(arg1, arg2)
	struct varlena *arg1, *arg2;
{
	int len;

	if (arg1 == NULL && arg2 != NULL)
		return((int32) 0);
	if (arg2 == NULL)
		return((int32) 1);
	if ((len = arg1->vl_len) < arg2->vl_len)
		len = arg2->vl_len;
	len -= sizeof(int32);					/* varlena? */

	if (strncmp(arg1->vl_dat, arg2->vl_dat, len) > 0)
		return ((int32) 1);
	return ((int32) 0);
}

int32
text_ge(arg1, arg2)
	struct varlena *arg1, *arg2;
{
	int alen, blen;
	int res;
	int len;
	char *ap, *bp;

	if (arg1 == NULL && arg2 != NULL)
		return((int32) 0);
	if (arg2 == NULL)
		return((int32) 1);
	if ((alen = len = arg1->vl_len) < (blen = arg2->vl_len))
		len = blen;
	len -= sizeof(int32);					/* varlena? */

	ap = (char *) VARDATA(arg1);
	bp = (char *) VARDATA(arg2);
	res = strncmp(ap, bp, len);

	if (res < 0 || (res == 0 && alen <= blen))
		return ((int32) 1);

	return ((int32) 0);
}

/*-------------------------------------------------------------
 * byteaGetSize
 *
 * get the number of bytes contained in an instance of type 'bytea'
 *-------------------------------------------------------------
 */
int32
byteaGetSize(v)
struct varlena *v;
{
    register int len;

    len = v->vl_len - sizeof(v->vl_len);

    return(len);
}

/*-------------------------------------------------------------
 * byteaGetByte
 *
 * this routine treats "bytea" as an array of bytes.
 * It returns the Nth byte (a number between 0 and 255) or
 * it dies if the length of this array is less than n.
 *-------------------------------------------------------------
 */
int32
byteaGetByte(v, n)
struct varlena *v;
int32 n;
{
    int len;
    int byte;

    len = byteaGetSize(v);

    if (n>=len) {
	elog(WARN, "byteaGetByte: index (=%d) out of range [0..%d]",
	    n,len-1);
    }

    byte = (unsigned char) (v->vl_dat[n]);

    return((int32) byte);
}

/*-------------------------------------------------------------
 * byteaGetBit
 *
 * This routine treats a "bytea" type like an array of bits.
 * It returns the value of the Nth bit (0 or 1).
 * If 'n' is out of range, it dies!
 *
 *-------------------------------------------------------------
 */
int32
byteaGetBit(v, n)
struct varlena *v;
int32 n;
{
    int len;
    int byteNo, bitNo;
    int byte;

    byteNo = n/8;
    bitNo = n%8;

    byte = byteaGetByte(v, byteNo);

    if (byte & (1<<bitNo)) {
	return((int32)1);
    } else {
	return((int32)0);
    }
}
/*-------------------------------------------------------------
 * byteaSetByte
 *
 * Given an instance of type 'bytea' creates a new one with
 * the Nth byte set to the given value.
 *
 *-------------------------------------------------------------
 */

struct varlena *
byteaSetByte(v, n, newByte)
struct varlena *v;
int32 n;
int32 newByte;
{
    int len;
    struct varlena *res;
    int i;

    len = byteaGetSize(v);

    if (n>=len) {
	elog(WARN,
	    "byteaSetByte: index (=%d) out of range [0..%d]",
	    n, len-1);
    }

    /*
     * Make a copy of the original varlena.
     */
    res = (struct varlena *) palloc(VARSIZE(v));
    if (res==NULL) {
	elog(WARN, "byteaSetByte: Out of memory (%d bytes requested)",
	    VARSIZE(v));
    }
    bcopy((char *)v, (char *)res, VARSIZE(v));
    
    /*
     *  Now set the byte.
     */
    res->vl_dat[n] = newByte;

    return(res);
}

/*-------------------------------------------------------------
 * byteaSetBit
 *
 * Given an instance of type 'bytea' creates a new one with
 * the Nth bit set to the given value.
 *
 *-------------------------------------------------------------
 */

struct varlena *
byteaSetBit(v, n, newBit)
struct varlena *v;
int32 n;
int32 newBit;
{
    struct varlena *res;
    int oldByte, newByte;
    int byteNo, bitNo;

    /*
     * sanity check!
     */
    if (newBit != 0 && newBit != 1) {
	elog(WARN, "byteaSetByte: new bit must be 0 or 1");
    }

    /*
     * get the byte where the bit we want is stored.
     */
    byteNo = n / 8;
    bitNo = n % 8;
    oldByte = byteaGetByte(v, byteNo);

    /*
     * calculate the new value for that byte
     */
    if (newBit == 0) {
	newByte = oldByte & (~(1<<bitNo));
    } else {
	newByte = oldByte | (1<<bitNo);
    }

    /*
     * NOTE: 'byteaSetByte' creates a copy of 'v' & sets the byte.
     */
    res = byteaSetByte(v, byteNo, newByte);

    return(res);
}
