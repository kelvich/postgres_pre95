static char *scanner_c = "$Header$";

#include <ctype.h>

/*
 *	Support routines for the scanner.
 *	Includes: comment, character constant,
 *	and string constant scanning.
 */

/*
 *	Scan PL/1 style comment.
 */
scancmnt()
{
	register int c, trail;

	trail = 0;
	for (;;) {
		c = input();
		switch (c) {
		case 0:
			serror("Unterminated comment.");
			return;
		
		case '/':
			if (trail == '*')
				return;
		}
		trail = c;
	}
}

char	delimiter;

/*
 *  Scan a character constant into yytext.
 */
scanchar(buf)
char *buf;
{
	delimiter = '\'';
	scancon(buf, 1);
}

scanstr(buf, len)
char *buf;
int len;
{
	delimiter = '\"';
	scancon(buf, len);
}
scanspecial(buf, len)
char *buf;
int len;
{
	delimiter = '`';
	scancon(buf, len);
}

scanarray(buf, len)
char *buf;
int len;
{
	delimiter = '\}';
	scanarr(buf, len);
}

/*
 * Scan a string.  The leading delimiter (", ') has already been
 * read.  Be sure to gobble up the whole thing, including the
 * trailing delimiter.
 */

scancon(buf, len)
char *buf;
int len;
{
	register char *cp = buf;
	register int c, dc, cspec;

	cspec = 0;
	while ((c = input()) != delimiter) {
		if (cp - buf > len - 1) {
			serror("String/char constant too large");
			cp = buf;
		}
		switch (c) {
		default:
			*cp++ = c;
			break;

		case 0:
		case '\n':
			serror("Unterminated char/string constant");
			goto out;

		case '\\':
			c = input();
			if (c == '\n')
				continue;
			/* *cp++ = '\\'; When _should_ this be done? XXX */
			if (isdigit(c)) {
				dc = 0;
				while (dc++ < 3 && isdigit(c)) {
					*cp++ = c;
					c = input();
				}
				unput(c);
				break;
			}
			if (c != 0) {
				switch (c) {
				case 't': c = '\t'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 'b': c = '\b'; break;
				case 'f': c = '\f'; break;
				default: break;
				}
				*cp++ = c;
			}
			break;
		}
		cspec++;
	}

out:
	*cp = 0;
	return(cspec);
}

/*
 * Scan input for array_in.  The leading delimiter ({) has already been
 * read.  Be sure to gobble up the whole thing, including the
 * trailing delimiter.
 */

scanarr(buf, len)
char *buf;
int len;
{
	register char *cp = buf;
	register int c, c2, dc, cspec, 
		     counter;  /* counts matching '{' and '}'.  */
			       /* stop scanning when unmatched '}' */
			       /* is encounterd. */

	cspec = 0;
	counter = 0;

	while (!(((c = input()) == delimiter) && (counter  == 0))) {
		if ( c == '\{' ) counter++;
		if ( c == '\}' ) counter--;
		if (cp - buf > len - 1) {
			serror("String/char constant too large");
			cp = buf;
		}
		switch (c) {
		default:
			*cp++ = c;
			break;

		case 0:
		case '\n':
			serror("Unterminated char/string constant");
			goto out;

		case '\\':
			c = input();
			if (c == '\n')
				continue;
			/* *cp++ = '\\'; When _should_ this be done? XXX */
			if (isdigit(c)) {
				dc = 0;
				while (dc++ < 3 && isdigit(c2)) {
					*cp++ = c;
					c = input();
				}
				unput(c);
				break;
			}
			if (c != 0) {
				switch (c) {
				case 't': c = '\t'; break;
				case 'n': c = '\n'; break;
				case 'r': c = '\r'; break;
				case 'b': c = '\b'; break;
				case 'f': c = '\f'; break;
				default: *cp++ = '\\'; break;
				}
				*cp++ = c;
			}
			break;
		}
		cspec++;
	}

out:
	*cp = 0;
	return(cspec);
}
