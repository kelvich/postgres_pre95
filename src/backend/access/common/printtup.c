/*
 * printtup.c --
 *	Routines to print out tuples to the standard output.
 */

#include <sys/file.h>
#include <stdio.h>
#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/heapam.h"
#include "access/htup.h"
#include "access/skey.h"
#include "storage/buf.h"
#include "utils/memutils.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"


static ObjectId	typtoout();

/*
 *	convtypeinfo
 *
 *	Converts an old-style typeinfo (struct attribute array) into
 *	the new-style typeinfo (struct attribute (*) array).
 *
 *	XXX temporary, but still called by the executor.
 */
struct attribute **
convtypeinfo(natts, att)
	int 			natts;
	struct attribute	*att;
{
	struct	attribute *ap, **rpp, **retatt;

	rpp = retatt = (struct attribute **) 
	    palloc(natts * sizeof(*rpp) + natts * sizeof(**rpp));
	MemoryCopy(rpp + natts, att, natts * sizeof(**rpp));

	/* ----------------
	 *	REMOVED:
	 *
	 * pfree(att);	
	 *
	 *	executor now does the freeing.. -cim 8/8/89
	 * ----------------
	 */
	ap = (struct attribute *)(rpp + natts);
	while (--natts >= 0)
		*rpp++ = ap++;
	return(retatt);
}

printtup(tuple, typeinfo)
	HeapTuple		tuple;
	struct attribute 	*typeinfo[];
{
	int		i, j, k;
	char		*outputstr, *attr;
	Boolean		isnull;
	ObjectId	typoutput;

	putnchar("D", 1);
	j = 0;
	k = 1 << 7;
	for (i = 0; i < tuple->t_natts; ) {
		attr = amgetattr(tuple, InvalidBuffer, ++i, typeinfo, &isnull);
		if (!isnull)
			j |= k;
		k >>= 1;
		if (!(i & 7)) {
			putint(j, 1);
			j = 0;
			k = 1 << 7;
		}
	}
	if (i & 7)
		putint(j, 1);
	
/*			XXX no longer needed???
	{
		char 	*s = tuple->t_bits;

		for (i = USEDBITMAPLEN(tuple); --i >= 0; )
			putc(reversebitmapchar(*s++), Pfout);
	}
*/
	for (i = 0; i < tuple->t_natts; ++i) {
		attr = amgetattr(tuple, InvalidBuffer, i+1, typeinfo, &isnull);
		typoutput = typtoout((ObjectId) typeinfo[i]->atttypid);
		if (!isnull && ObjectIdIsValid(typoutput)) {
			outputstr = fmgr(typoutput, attr);
			putint(strlen(outputstr)+4, 4);
			putnchar(outputstr, strlen(outputstr));
			pfree(outputstr);
		}
	}
}

#ifdef BOGUSROUTINES	/* XXX no longer seem to be called */
int
reversebitmapchar(c)
	int	c;
{
	int	i, rev, bit;
	
	rev = 0;
	bit = 01;
	for (i = 8; --i >= 0; ) {
		rev <<= 1;
		if (c & bit)
			rev |= 01;
		bit <<= 1;
	}
	return(rev);
}

dumptup(tuple, typeinfo)
	HeapTuple		tuple;
	struct attribute 	*typeinfo[];
{
	int		i;
	char		*attr, *s;
	Boolean		isnull;

	putnchar("D", 1);
	s = tuple->t_bits;
	for (i = USEDBITMAPLEN(tuple); --i >= 0; )
		putint(*s++, 1);
	for (i = 1; i <= tuple->t_natts; ++i) {
		attr = amgetattr(tuple, InvalidBuffer, i, typeinfo, &isnull);
		if (!isnull) {
			/*
			 * The following if statement should be replaced
			 * to use the 'send' procedure of a type if such an
			 * item exists.
			 */
			if (typeinfo[i-1]->attlen < 0) {
				putint((int) VARSIZE(attr), 4);
				putnchar(VARDATA(attr), (int) VARSIZE(attr));
			} else
				putnchar(attr, typeinfo[i-1]->attlen);
		}
	}
}
#endif	/* BOGUSROUTINES */

initport(name, natts, attinfo)
	char			*name;
	int			natts;
	struct attribute	*attinfo[];
{
	register int	i;

	putnchar("P", 1);
	putint(0, 4);
	putstr(name);
	putnchar("T", 1);
	putint(natts, 2);
	for (i = 0; i < natts; ++i) {
		putstr(attinfo[i]->attname);	/* if 16 char name oops.. */
		putint((int) attinfo[i]->atttypid, 4);
		putint(attinfo[i]->attlen, 2);
	}
}

void
printatt(attributeId, attributeP, value)
	unsigned		attributeId;
	struct attribute	*attributeP;
	char			*value;
{
	printf("\t%2d: %s%s%s%s\t(typeid = %lu, len = %d, byval = %c)\n",
	       attributeId,
	       attributeP->attname,
	       value != NULL ? " = \"" : "",
	       value != NULL ? value : "",
	       value != NULL ? "\"" : "",
	       attributeP->atttypid,
	       attributeP->attlen,
	       attributeP->attbyval ? 't' : 'f');
}

showatts(name, natts, attinfo)
	char			*name;
	int			natts;
	struct attribute	*attinfo[];
{
	register int	i;
	
	puts(name);
	for (i = 0; i < natts; ++i)
		printatt((unsigned) i+1, attinfo[i], (char *) NULL);
	printf("\t----\n");
}

debugtup(tuple, typeinfo)
	HeapTuple		tuple;
	struct attribute 	*typeinfo[];
{
	register int	i;
	char		*attr, *value;
	Boolean		isnull;
	ObjectId	typoutput;

	for (i = 0; i < tuple->t_natts; ++i) {
		attr = amgetattr(tuple, InvalidBuffer, i+1, typeinfo, &isnull);
		typoutput = typtoout((ObjectId) typeinfo[i]->atttypid);
		if (!isnull && ObjectIdIsValid(typoutput)) {
			value = fmgr(typoutput, attr);
			printatt((unsigned) i+1, typeinfo[i], value);
			pfree(value);
		}
	}
	printf("\t----\n");
}

static ObjectId
typtoout(type)
	ObjectId	type;
{
	HeapTuple	typeTuple;

	typeTuple = SearchSysCacheTuple(TYPOID,
					(char *) type,
					(char *) NULL,
					(char *) NULL,
					(char *) NULL);
	if (HeapTupleIsValid(typeTuple))
		return((ObjectId)
       ((TypeTupleForm) GETSTRUCT(typeTuple))->typoutput);
	elog(WARN, "typtoout: Cache lookup of type %d failed", type);
	return(InvalidObjectId);
}
