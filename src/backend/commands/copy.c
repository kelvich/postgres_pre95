/*
 * copy.c --
 *	the POSTGRES copy command
 *
 * Notes:
 *	createdomains() is incredibly ugly
 *	efficiency - too many fread()s and fwrite()s
 */

#include <stdio.h>
#include <pwd.h>
#include <strings.h>
#include <varargs.h>
#include <sys/file.h>
#include <sys/param.h>

#include "cat.h"
#include "catalog.h"	/* for struct type */
#include "fd.h"
#include "fmgr.h"
#include "heapam.h"
#include "log.h"
#include "syscache.h"
#include "tqual.h"

#include "copy.h"

RcsId("$Header$");

#define	NON_NULL_ATT	(' ')
#define	NULL_ATT	('n')
#define	DUMMY_ATT	((AttributeNumber) 0)
#define	CHARN_PAD_CHAR	(' ')

#define	B_TRUE			((Boolean) 1)
#define	B_FALSE			((Boolean) 0)
#define	BooleanIsTrue(BOOLEAN)	((BOOLEAN) == B_TRUE)
#define	BooleanIsFalse(BOOLEAN)	((BOOLEAN) == B_FALSE)
#define	BooleanIsValid(BOOLEAN)	((BOOLEAN) == B_TRUE || (BOOLEAN) == B_FALSE)

#define	DelimIsValid(DELIM)	((DELIM) >= 0)
#define	AttrIsVarLen(ATTR)	((ATTR)->attlen < 0)
#define	DomainIsVarLen(DOMAIN) 	((DOMAIN)->domlen < 0)
#define	DomainIsValid(DOMAIN)	PointerIsValid(DOMAIN)

/* Internal routines */
extern Domain		createdomainsAlloc();
extern			createdomainsIsCharType();
extern AttributeNumber	createdomainsMatchAttName();
extern Boolean		copyWriteString();
extern Boolean		copyWriteExternal();
extern Boolean		copyWriteChar0();
extern Boolean		copyWriteCharN();
extern Boolean		copyWriteBinary();
extern			copyReadDummy();
extern char		*copyReadExternal();
extern char		*copyReadChar0();
extern char		*copyReadCharN();
extern char		*copyReadBinary();
extern char		*copyAlloc();
extern 			copyCleanup();

/* #define COPYDEBUG	/* for my own use */


/*
 *	createdomains
 *
 *	Expects:
 *		(char *) relation_name,
 *		(int) isbinary
 *		(int) nonulls
 *		(int) isfrom
 *		(int) number_of_domains,
 * 		{ (char *) domain_name, (char *) domain_type, (char) delim }
 *
 *	Returns:
 *		array of domain descriptors
 */
/*VARARGS*/
Domain
createdomains(va_alist)
	va_dcl
{
	va_list			pvar;
	register		i;
	char			*relname;	/* char16 relation name */
	Boolean			isbinary;	/* all-binary file format? */
	Boolean			nonulls;	/* INGRES-style binary fmt */
	Boolean			isfrom;		/* copy direction */
	int			ndoms;		/* # of argument domains */
	int			*ndomains;	/* final # of domains */
	Relation		rdesc;
	AttributeNumber		relnatts;
	TupleDescriptor		rdatt;
	Boolean			att_defined;
	Domain			doms;
	char			*domname;	/* not necessarily the name */
	char			*domtype;	/* not necessarily the type */
	char			domdelim;	/* not necessarily the delim */
	AttributeNumber		attnum;
	HeapTuple		typtup;
	TypeTupleForm		tp;

	va_start(pvar);
	relname = va_arg(pvar, char *);
	isbinary = va_arg(pvar, int);
	nonulls = va_arg(pvar, int);
	isfrom = va_arg(pvar, int);
	ndoms = va_arg(pvar, int);
	ndomains = va_arg(pvar, int *);
	if (!PointerIsValid(relname) ||
	    !BooleanIsValid(isbinary) ||
	    !BooleanIsValid(nonulls) ||
	    !BooleanIsValid(isfrom) ||
	    ndoms < 0) {
		elog(WARN, "createdomains: Bad arguments: \"%s\" %d %d %d %d",
		     relname, ndoms, isbinary, nonulls, isfrom);
		return((Domain) NULL);
	}

	/*
	 * We need the information in a Relation structure, so open the
	 * relation.  The relation MUST already exist.
	 */
	rdesc = RelationNameOpenHeapRelation(relname);
	if (!RelationIsValid(rdesc)) {
		elog(WARN, "createdomains: Can't open relation \"%s\"",
		     relname);
		return((Domain) NULL);
	}
	rdatt = RelationGetTupleDescriptor(rdesc);
	relnatts = (RelationGetRelationTupleForm(rdesc))->relnatts;

	/* If no domains are specified, then range over the whole relation */
	if (ndoms == 0) {
		ndoms = (int) relnatts;
		att_defined = B_FALSE;
	} else
		att_defined = B_TRUE;

	doms = createdomainsAlloc(ndoms);

	/*
	 * Allocate and initialize each domain element and attribute,
	 * then load it with the appropriate values.
	 */
	for (i = 0; i < ndoms; ++i) {

		if (BooleanIsTrue(nonulls))
			DomainSetNoNulls(&doms[i]);

		/*
		 * Determine the domain name, type, and delimitor.
		 * If a domain is defined and it exists in the relation
		 * (e.g. it isn't a dummy), then the atts entry is that
		 * of the attribute it names.
		 */
		if (att_defined) {
			domname = va_arg(pvar, char *);
			domtype = va_arg(pvar, char *);
			domdelim = va_arg(pvar, int);
			attnum = createdomainsMatchAttName(rdatt->data,
							   relnatts,
							   (Name) domname);
		} else {
			domname = NULL;
			domtype = NULL;
			domdelim = NO_DELIM;
			attnum = (AttributeNumber) (i + 1);
		}

		/*
		 * Handle dummy strings (to be written to the file).
		 */
		if (domtype == STRING_TYPE) {
			if (BooleanIsTrue(isfrom)) {
				copyCleanup(rdesc, (FILE *) NULL, doms);
				elog(WARN,
				     "createdomains: No strings in 'from'");
				return((Domain) NULL);
			}
			DomainSetString(&doms[i]);
			doms[i].domlen = domdelim;
			doms[i].string = domname;
#ifdef COPYDEBUG
			printf("%d: string, value=%s\n", i, domname);
#endif
			continue;
		}

		/*
		 * Determine the size of the domain, as specified by 'domtype'
		 * (Even dummies have to have a type of some sort).
		 * XXX The default output type is text.
		 */
		if (!PointerIsValid(domtype))
			domtype = "text";
		typtup = SearchSysCacheTuple(TYPNAME,
					     domtype, NULL, NULL, NULL);
		if (!HeapTupleIsValid(typtup)) {
			copyCleanup(rdesc, (FILE *) NULL, doms);
			elog(WARN, "createdomains: Unknown type %s", domtype);
			return((Domain) NULL);
		}
		doms[i].domlen = ((TypeTupleForm) GETSTRUCT(typtup))->typlen;
		if (createdomainsIsCharType((Name) domtype))
			DomainSetExternal(&doms[i]);

		/*
		 * Deal with dummy domains.
		 */
		if (attnum == DUMMY_ATT) {
			if (BooleanIsFalse(isfrom)) {
				copyCleanup(rdesc, (FILE *) NULL, doms);
				elog(WARN,
				     "createdomains: No dummies in 'to'");
				return((Domain) NULL);
			}
			DomainSetDummy(&doms[i]);
			doms[i].delim = domdelim;
			if (DelimIsValid(domdelim))
				DomainSetDelimited(&doms[i]);
#ifdef COPYDEBUG
			printf("%d: dummy, type=%s\n", i, domtype);
#endif
			continue;
		}

		/*
		 * At this point, the domain must be a normal attribute.
		 */
		typtup = SearchSysCacheTuple(TYPOID,
					     (char *)
					     rdatt->data[attnum-1]->atttypid,
					     NULL, NULL, NULL);
		if (!HeapTupleIsValid(typtup)) {
			copyCleanup(rdesc, (FILE *) NULL, doms);
			elog(WARN, "createdomains: Unknown attribute type %d",
			     rdatt->data[attnum]->atttypid);
			return((Domain) NULL);
		}
		tp = (TypeTupleForm) GETSTRUCT(typtup);
		doms[i].typoutput = tp->typoutput;
		doms[i].typinput = tp->typinput;
		if (doms[i].domlen == 0)
			doms[i].domlen = tp->typlen;
		doms[i].attnum = attnum;
		DomainSetAttribute(&doms[i]);

		if (BooleanIsTrue(isbinary) ||
		    !DomainIsVarLen(&doms[i]))
			continue;
		if ((DomainIsDummy(&doms[i]) || DomainIsExternal(&doms[i])) &&
		    DelimIsValid(domdelim)) {
			DomainSetDelimited(&doms[i]);
			doms[i].delim = domdelim;
		} else if (DomainIsExternal(&doms[i])) {
			DomainSetDelimited(&doms[i]);
			doms[i].delim = (i == ndoms - 1) ? '\n' : '\t';
		}
	}
	va_end(pvar);
	*ndomains = ndoms;
#ifdef COPYDEBUG
	print_domains(ndoms, doms);
#endif
	return(doms);
}

Domain
createdomainsAlloc(ndoms)
	int	ndoms;
{
	register	i;
	Domain		doms;

	doms = (Domain) palloc(sizeof(DomainData) * ndoms);
	for (i = 0; i < ndoms; ++i) {
		doms[i].domnum = i;
		doms[i].attnum = DUMMY_ATT;
		doms[i].domlen = 0;
		doms[i].domtype = 0;
		doms[i].delim = NO_DELIM;
		doms[i].string = NULL;
		doms[i].typoutput = doms[i].typinput = (ObjectId) 0;
	}
	return(doms);
}

int
createdomainsIsCharType(typename)
	Name	typename;
{
	/* XXX This will get more complicated when we have character arrays */
	static NameData	textname = { "text" };

	return(!strncmp(typename->data, textname.data, sizeof(NameData)));
}

AttributeNumber
createdomainsMatchAttName(atts, natts, attname)
	AttributeTupleForm	atts[];
	AttributeNumber		natts;
	Name			attname;
{
	register	i;

	for (i = 0; i < (int) natts; ++i)
		if (!strncmp(attname->data, atts[i]->attname.data,
			     sizeof(NameData)))
			return(i + 1);
	return(DUMMY_ATT);
}

/*
 *	copyrel
 *
 */
copyrel(relname, isfrom, filename, maprelname, ndoms, doms)
	char		*relname;
	Boolean		isfrom;
	char		*filename;
	char		*maprelname;
	int		ndoms;
	DomainData	doms[];
{
	Relation	rdesc = (Relation) NULL;
	Relation	maprdesc = (Relation) NULL;
	FILE		*fp = (FILE *) NULL;

	if (!PointerIsValid(relname) ||
	    !BooleanIsValid(isfrom) ||
	    !PointerIsValid(filename) ||
	    ndoms <= 0 ||
	    !DomainIsValid(doms)) {
		copyCleanup(rdesc, fp, doms);
		elog(WARN, "copyrel: Bad arg(s) - \"%s\" %d \"%s\" %d %x",
		     relname, isfrom, filename, ndoms, doms);
		return;
	}

	/*
	 * XXX Security - should check:
	 *	Source/target relation retrieve/update permission
	 *	Catalog 'usecatupd' permission
	 */

	rdesc = RelationNameOpenHeapRelation(relname);
	if (!RelationIsValid(rdesc)) {
		copyCleanup(rdesc, fp, doms);
		elog(WARN, "copyrel: Can't open relation %s", relname);
		return;
	}

	if (PointerIsValid(maprelname)) {
		maprdesc = RelationNameOpenHeapRelation(maprelname);
		if (!RelationIsValid(maprdesc)) {
			copyCleanup(rdesc, fp, doms);
			elog(WARN, "copyrel: Can't open map relation %s",
			     maprelname);
			return;
		}
	}

	AllocateFile();
	fp = fopen(filename, BooleanIsFalse(isfrom) ? "w" : "r");
	if (fp == (FILE *) NULL) {
		perror("copyrel");
		copyCleanup(rdesc, fp, doms);
		elog(WARN, "copyrel: Could not open file %s in %c mode",
		     filename, BooleanIsFalse(isfrom) ? 'w' : 'r');
		return;
	}

	startmmgr(M_DYNAMIC);
	if (BooleanIsFalse(isfrom))
		copyWrite(rdesc, maprdesc, fp, ndoms, doms);
	else
		copyRead(rdesc, maprdesc, fp, ndoms, doms);
	endmmgr(NULL);

	copyCleanup(rdesc, fp, doms);

	if (RelationIsValid(maprdesc))
		RelationCloseHeapRelation(maprdesc);
}

/*
 *	copyWrite
 *
 *	Write a relation into a UNIX file.
 */
/*ARGSUSED*/
copyWrite(rdesc, maprdesc, fp, ndoms, doms)
	Relation	rdesc;		/* relation to write from */
	Relation	maprdesc;	/* relation to write map info to */
	FILE		*fp;		/* file to write to */
	int		ndoms;		/* number of domains */
	DomainData 	doms[];		/* domain descriptors */
{
	register 		i;
	AttributeTupleForm	*atts;
	HeapScan		relscan;
	Boolean			ioerr = B_FALSE;
	HeapTuple		htp;
	Buffer			bufp;		/* buffer page of tuple */
	char			*attr;		/* result from amgetattr */
	Boolean			isnull;		/* whether attr is null */

	atts = (RelationGetTupleDescriptor(rdesc))->data;

	relscan = RelationBeginHeapScan(rdesc, 0, NowTimeQual, (unsigned) 0,
					(ScanKey) NULL);

	while (BooleanIsFalse(ioerr) &&
	       HeapTupleIsValid(htp =
				HeapScanGetNextTuple(relscan, 0, &bufp))) {
#ifdef COPYDEBUG
		printf("writing");
#endif
		for (i = 0; i < ndoms; ++i) {
			if (DomainIsString(&doms[i])) {
				ioerr = copyWriteString(fp, &doms[i]);
				continue;
			}
			if (!DomainIsAttribute(&doms[i])) {
				elog(WARN,
				     "copyWrite: Bad domain, domtype=0x%x\n",
				     doms[i].domtype);
				ioerr = B_TRUE;
				break;
			}
			attr = amgetattr(htp, bufp, (int) doms[i].attnum,
					 atts, &isnull);
			if (DomainIsExternal(&doms[i]))
				ioerr = copyWriteExternal(attr, isnull, rdesc,
							  fp, &doms[i]);
			else 
				ioerr = copyWriteBinary(attr, isnull, rdesc,
							fp, &doms[i]);
		} /* attribute */
#ifdef COPYDEBUG
		printf("\n");
#endif
		if (BooleanIsTrue(ioerr)) {
			elog(WARN, "copyWrite: Bailing out!\n");
			break;
		}

		/* XXX Write the mapping information here */
	} /* tuple */

	if (!HeapScanIsValid(relscan))
		HeapScanEnd(relscan);
}

/*
 *	copyWriteString
 *
 *	Write a dummy string to a file.
 */
Boolean
copyWriteString(fp, dom)
	FILE	*fp;
	Domain	dom;
{
	int	wbytes;

	wbytes = fwrite(dom->string, sizeof(char), dom->domlen, fp);
	if (wbytes != dom->domlen) {
		elog(WARN, "copyWriteString: fwrite() error, dom%d",
		     dom->domnum);
		return(B_TRUE);
	}
#ifdef COPYDEBUG
	printf(", dom%d=\"%s\"",
	       dom->domnum,
	       PointerIsValid(dom->string) ? dom->string : "NULL");
#endif
	return(B_FALSE);
}

/*
 *	copyWriteExternal
 *
 *	Write an attribute to a file as an ASCII character string containing
 *	the attribute value in external format.
 *	If the attribute is variable length, it will always be delimited.
 *	If it is not, dom->domlen characters are written (including padding,
 *	if necessary).
 */
/*ARGSUSED*/
Boolean
copyWriteExternal(attr, isnull, rdesc, fp, dom)
	char		*attr;		/* attribute to write */
	Boolean		isnull;		/* whether 'attr' is null or not */
	Relation	rdesc;		/* relation to write from */
	FILE		*fp;		/* file to write to */
	Domain	 	dom;		/* domain descriptor */
{
	char		*out;			/* output string from fmgr */
	int		outlen;			/* length of out */
	Boolean		ioerr = B_FALSE;

	outlen = -1;
	out = NULL;
	if (BooleanIsFalse(isnull)) {
		out = fmgr(dom->typoutput, attr);
		if (!PointerIsValid(out)) {
			elog(WARN,
			     "copyWriteExternal: fmgr() error in calling %d",
			     dom->typoutput);
			return(B_TRUE);
		} else
			outlen = strlen(out);
	}
	if (outlen >= 0)
		if (DomainIsVarLen(dom))
			ioerr = copyWriteChar0(out, isnull, outlen, fp, dom);
		else
			ioerr = copyWriteCharN(out, isnull, outlen, fp, dom);
#ifdef COPYDEBUG
	printf(", dom%d=\"%s\"",
	       dom->domnum, PointerIsValid(out) ? out : "NULL");
#endif
	return(ioerr);
}

/*
 *	copyWriteChar0
 *
 *	Write an attribute to a file as a delimited ASCII string containing
 *	the attribute value in external format.
 */
Boolean
copyWriteChar0(chars, isnull, nchars, fp, dom)
	char		*chars;
	Boolean		isnull;
	int		nchars;
	FILE		*fp;
	Domain		dom;
{
	register	i, j, k;
	Boolean		ioerr = B_FALSE;
	static char	buf[2 * BUFSIZ + 1];	/* fixed-length buffer */
	int		writelen;

	/*
	 * If the attribute is null, simply print a zero-length record.
	 *	XXX "" ends up as a NULL text value
	 */
	if (BooleanIsTrue(isnull)) {
		if (fwrite(&dom->delim, sizeof(char), 1, fp) != 1) {
			elog(WARN, "copyWriteChar0: fwrite() error on dom%d[]",
			     dom->domnum);
			ioerr = B_TRUE;
		}
		return(ioerr);
	}

	/*
	 * Copy 'chars' into 'buf', escaping instances of the delim character
	 * which are found within 'chars' (at worst, every character in each
	 * BUFSIZ-byte fragment of 'chars' will need escaping, thus
	 * doubling the string length) and then write 'buf'.
	 */
	for (i = 0; i < nchars; i += writelen) {
		writelen = Min(nchars - i, BUFSIZ);
		for (j = i, k = 0; j < writelen; ++j, ++k) {
			if (chars[j] == '\\' || chars[j] == dom->delim)
				buf[k++] = '\\';
			buf[k] = chars[j];
		}
		if (i + writelen >= nchars)	/* finished! */
			buf[k++] = dom->delim;
		if (fwrite(buf, sizeof(char), k, fp) != k) {
			elog(WARN,
			     "copyWriteChar0: fwrite() error on dom%d[%d+%d]",
			     dom->domnum, i, k);
			ioerr = B_TRUE;
			break;
		}
	}
	return(ioerr);
}

/*
 *	copyWriteCharN
 *
 *	Write an attribute to a file as a fixed-length ASCII string containing
 *	the attribute value in external format.
 *	If the string is too long, it is truncated; if it is too short,
 *	it is right-padded with CHARN_PAD_CHAR.
 */
Boolean
copyWriteCharN(chars, isnull, nchars, fp, dom)
	char		*chars;
	Boolean		isnull;
	int		nchars;
	FILE		*fp;
	Domain		dom;
{
	register	i;
	Boolean		ioerr = B_FALSE;
	int		padlen, writelen;
	static char	padbuf[BUFSIZ];
	static Boolean	padinit = B_FALSE;

	if (BooleanIsFalse(padinit)) {
		for (i = 0; i < BUFSIZ; ++i)
			padbuf[i] = CHARN_PAD_CHAR;
		padinit = B_TRUE;
	}
	if (BooleanIsTrue(isnull)) {
		padlen = dom->domlen;
	} else {
		padlen = dom->domlen - nchars;
		if (padlen < 0) {	/* requires truncation, not padding */
			if (fwrite(chars, sizeof(char), dom->domlen, fp)
			    != dom->domlen) {
				elog(WARN,
				     "copyWriteCharN: fwrite() error on dom%d",
				     dom->domnum);
				ioerr = B_TRUE;
			}
			return(ioerr);
		}
		if (fwrite(chars, sizeof(char), nchars, fp) != nchars) {
			elog(WARN,
			     "copyWriteCharN: fwrite() error on dom%d",
			     dom->domnum);
			return(ioerr);
		}
	}
	for (i = 0; i < padlen; i += writelen) {
		writelen = Min(padlen, BUFSIZ);
		if (fwrite(padbuf, sizeof(char), writelen, fp) != writelen) {
			elog(WARN, "copyWriteCharN: fwrite error on dom%d",
			     dom->domnum);
			ioerr = B_TRUE;
			break;
		}
	}
	return(ioerr);
}

/*
 *	copyWriteBinary
 *
 *	Write an attribute to a file as raw host-order bytes.  Unless the
 *	NONULLS flag has been set, fixed-length data is preceded by the length
 *	in bytes, written as a host-order int4 (nulls are assigned a length
 *	of 0); variable-length fields are always preceded by their length.
 */
Boolean
copyWriteBinary(attr, isnull, rdesc, fp, dom)
	char		*attr;		/* attribute to write */
	Boolean		isnull;		/* whether attr is null or not */
	Relation	rdesc;		/* relation to write from */
	FILE		*fp;		/* file to write to */
	Domain		dom;		/* domain descriptor */
{
	AttributeTupleForm	att;
	int			attrlen;
	Boolean			ioerr = B_FALSE;
	int			wbytes;
	char			*out;

	att = (RelationGetTupleDescriptor(rdesc))->data[dom->attnum - 1];

	/*
	 * In standard POSTGRES mode, precede all attributes with their length
	 * (written as a long integer).  In NONULLS mode, only variable-length
	 * attributes have a preceding length word.
	 * XXX A length of zero is assumed to mean a null attribute.
	 *     This is wasteful, but how else can null values be represented?
	 */
	if (BooleanIsTrue(isnull))
		attrlen = 0;
	else if (AttrIsVarLen(att))
		attrlen = PSIZE(attr);
	else
		attrlen = att->attlen;
	if (!DomainIsNoNulls(dom) || AttrIsVarLen(att))
		if (fwrite((char *) &attrlen, 1, sizeof(long), fp) != 1) {
			elog(WARN,
			     "copyWriteBinary: fwrite(%d) error, dom%d length",
			     attrlen, dom->domnum);
			return(B_TRUE);
		}

	out = BooleanIsTrue(att->attbyval) ? (char *) &attr : attr;
	wbytes = fwrite(out, sizeof(char), attrlen, fp);
	if (wbytes != attrlen) {
		elog(WARN,
		     "copyWriteBinary: fwrite() error, dom%d[%d/%d]",
		     dom->domnum, wbytes, attrlen);
		ioerr = B_TRUE;
	}
#ifdef COPYDEBUG
	if (BooleanIsTrue(att->attbyval))
		printf(", dom%d=%x", dom->domnum, attr);
	else
		printf(", dom%d=<%x>", dom->domnum, attr);
#endif
	return(ioerr);
}

/*
 *	copyRead
 *
 *	Read a relation from a text file.
 */
/*ARGSUSED*/
copyRead(rdesc, maprdesc, fp, ndoms, doms)
	Relation	rdesc;		/* relation to write to */
	Relation	maprdesc;	/* relation to write map info to */
	FILE		*fp;		/* file to read from */
	int		ndoms;		/* number of domains */
	DomainData	doms[];		/* domain descriptor */
{
	register		i, attindex;
	int			relnatts;
	char			*p;
	char			*nullmap;
	char			**vals;
	TupleDescriptor		rdatt;
	HeapTuple 		tup;
	Boolean			isnull, eof;
	HeapTuple		formtuple();	/* XXX */

	/*
	 * Allocate a pointer vector and a bytemap of domains for formtuple()
	 * and initialize them.
	 */
	relnatts = (int) (RelationGetRelationTupleForm(rdesc))->relnatts;
	vals = (char **) palloc(sizeof(char *) * relnatts);
	nullmap = palloc(relnatts);

	rdatt = RelationGetTupleDescriptor(rdesc);

	/* Read in a file, a tuple at a time. */
	for (eof = B_FALSE; BooleanIsFalse(eof); ) {

		for (i = 0; i < relnatts; ++i) {
			vals[i] = NULL;
			nullmap[i] = NULL_ATT;
		}

		/* Read in a tuple, an attribute at a time. */
#ifdef COPYDEBUG
		printf("reading");
#endif
		for (i = 0; i < ndoms && BooleanIsFalse(eof); ++i)  {
			if (DomainIsDummy(&doms[i])) {
				copyReadDummy(fp, &doms[i], &isnull, &eof);
				if (BooleanIsTrue(eof))
					break;
				else
					continue;
			}

			attindex = doms[i].attnum - 1;
			if (!DomainIsAttribute(&doms[i])) {
				elog(WARN, "copyRead: Invalid domain %x",
				     doms[i].domtype);
				goto exit_copyRead;
			}
			if (DomainIsExternal(&doms[i]))
				p = copyReadExternal(fp, &doms[i],
						     &isnull, &eof);
			else
				p = copyReadBinary(fp, &doms[i],
						   &isnull, &eof);
			if (BooleanIsTrue(eof))
				break;
			if (BooleanIsFalse(isnull)) {
				nullmap[attindex] = NON_NULL_ATT;
				if (DomainIsExternal(&doms[i]))
					vals[attindex] =
						fmgr(doms[i].typinput, p);
				else
					vals[attindex] = p;
			}
#ifdef COPYDEBUG			
			printf(", att%d=\"%s\"/%x[%c]",
			       i, p, vals[attindex], nullmap[attindex]);
#endif
		} /* attribute */
#ifdef COPYDEBUG
		printf("\n");
#endif
		/*
		 * This should only happen on EOF or system failure, so we
		 * will halt the read completely.
		 */
		if (i != ndoms) {
			if (i != 0)
				elog(DEBUG, "copyRead: Stopped at dom%d", i);
			break;
		}

		/*
		 * Form a tuple using the master atts (i.e., actual
		 * atts in relation) and data array filled above, then
		 * insert the newly formed tuple into the relation.
		 *
		 * XXX Ignore locks for now.
		 */
		tup = formtuple(relnatts, rdatt->data, vals, nullmap);
		(void) RelationInsertHeapTuple(rdesc, tup, (double *) NULL);
#ifdef SHORTOFMEMORY
		pfree((char *) tup);
		for (i = 0; i < relnatts; ++i) {
			attindex = doms[i].attnum - 1;
			if (nullmap[attindex] == NON_NULL_ATT &&
			    BooleanIsFalse(rdatt->data[doms[i].attnum]->attbyval))
				pfree(vals[attindex]);
		}
#endif
	} /* tuple */

 exit_copyRead:
	if (PointerIsValid(nullmap))
		pfree(nullmap);
	if (PointerIsValid(vals))
		pfree((char *) vals);
}

/*
 *	copyReadDummy
 *
 *	Returns nothing.
 */
copyReadDummy(fp, dom, isnull, eof)
	FILE		*fp;		/* file to read from */
	Domain 		dom;		/* domain descriptor */
	Boolean		*isnull;	/* return value: is the attr null? */
	Boolean		*eof;		/* return value: have we read EOF? */
{
	char	*p;

	if (DomainIsVarLen(dom)) {
		if (DomainIsDelimited(dom)) {
			(void) copyReadChar0(fp, dom, isnull, eof);
		} else {
			p = copyReadBinary(fp, dom, isnull, eof);
			if (PointerIsValid(p))
				pfree(p);
		}
	} else
		(void) fseek(fp, (long) dom->domlen, 1);
}

/*
 *	copyReadExternal
 *
 *	Returns a pointer to a static character buffer containing the external
 *	format of a data type.
 *
 *	Return variables:
 *		BooleanIsTrue(isnull) 	iff no valid attribute was read
 *		BooleanIsTrue(eof)	iff EOF or a fatal error occurred
 */
char *
copyReadExternal(fp, dom, isnull, eof)
	FILE		*fp;		/* file to read from */
	Domain	 	dom;		/* domain descriptor */
	Boolean		*isnull;	/* return value: is the attr null? */
	Boolean		*eof;		/* return value: have we read EOF? */
{
	char	*inbuf;

	*eof = B_TRUE;
	*isnull = B_TRUE;

	if (DomainIsDelimited(dom))
		inbuf = copyReadChar0(fp, dom, isnull, eof);
	else
		inbuf = copyReadCharN(fp, dom, isnull, eof);
	return(inbuf);
}

char *
copyReadCharN(fp, dom, isnull, eof)
	FILE		*fp;		/* file to read from */
	Domain	 	dom;		/* domain descriptor */
	Boolean		*isnull;	/* return value: is the attr null? */
	Boolean		*eof;		/* return value: have we read EOF? */
{
	register	rbytes;
	unsigned	morebuf;
	static char	*inbuf = NULL;
	static unsigned	inbuflen = 0;

	morebuf = dom->domlen - inbuflen + 1;
	if (morebuf > 0) {
		inbuf = copyAlloc(inbuf, &inbuflen, morebuf);
		if (!PointerIsValid(inbuf)) {
			elog(WARN, "copyReadExternal: copyAlloc() failure");
			return(NULL);
		}
	}
	rbytes = fread(inbuf, sizeof(char), dom->domlen, fp);
	if (rbytes != dom->domlen) {
		elog(WARN, "copyReadExternal: fread() error, dom%d[%d/%d]",
		     rbytes, dom->domlen, dom->domnum);
		return(NULL);
	}
	*eof = B_FALSE;
	*isnull = B_FALSE;
	inbuf[rbytes] = '\0';
	return(inbuf);
}

char *
copyReadChar0(fp, dom, isnull, eof)
	FILE		*fp;		/* file to read from */
	Domain	 	dom;		/* domain descriptor */
	Boolean		*isnull;	/* return value: is the attr null? */
	Boolean		*eof;		/* return value: have we read EOF? */
{
	register	c, rbytes;
	Boolean		escaped;
	static char	*inbuf = NULL;
	static unsigned	inbuflen = 0;

	rbytes = 0;
	escaped = B_FALSE;
	/* Read in an attribute, a character at a time. */
	while ((c = getc(fp)) != EOF &&
	       (c != dom->delim || BooleanIsTrue(escaped))) {
		if (rbytes >= inbuflen) {
			inbuf = copyAlloc(inbuf, &inbuflen, BUFSIZ);
			if (!PointerIsValid(inbuf)) {
				elog(WARN,
				     "copyReadExternal: copyAlloc() failure");
				return(NULL);
			}
		}
		if (c != '\\' || BooleanIsTrue(escaped)) {
			inbuf[rbytes++] = c;
			escaped = B_FALSE;
		} else
			escaped = B_TRUE;
	}
	*eof = (Boolean) (c == EOF);
	if (rbytes <= 0)		/* XXX "" is read as a NULL field */
		return(NULL);
	*isnull = B_FALSE;
	inbuf[rbytes] = '\0';
	return(inbuf);
}

/*
 *	copyReadBinary
 *
 *	Returns a pointer to a palloc'ed character buffer, suitable for
 *	direct use in a fmgr() call.
 *
 *	Return variables:
 *		BooleanIsTrue(isnull) 	iff no valid attribute was read
 *		BooleanIsTrue(eof)	iff EOF or a fatal error occurred
 */
char *
copyReadBinary(fp, dom, isnull, eof)
	FILE		*fp;		/* file to read from */
	Domain	 	dom;		/* domain descriptor */
	Boolean		*isnull;	/* return value: is the attr null? */
	Boolean		*eof;		/* return value: have we read EOF? */
{
	int		rbytes;
	uint32		attrlen;
	char	 	*inbuf;

	*isnull = B_TRUE;
	*eof = B_FALSE;

	/*
	 * Attributes are preceded by their length in bytes unless the
	 * NONULLS flag is set.
	 * Allocate at least this much space to hold the attribute.
	 */
	if (!DomainIsNoNulls(dom) || DomainIsVarLen(dom)) {
		rbytes = fread((char *) &attrlen, sizeof(long), 1, fp);
		if (rbytes != 1) {
			if (rbytes == 0)
				*eof = B_TRUE;
			else
				elog(WARN,
				     "copyReadBinary: fread() error, attrlen");
			return(NULL);
		}
	} else
		attrlen = dom->domlen;
	if (attrlen <= 0) {
		if (attrlen < 0)
			elog(WARN, "copyReadBinary: Bad vl_len %d", attrlen);
		return(NULL);
	}
	inbuf = palloc((int) attrlen);

	rbytes = fread(inbuf, sizeof(char), (int) attrlen, fp);
	if (rbytes != (int) attrlen) {
		if (rbytes == 0)
			*eof = B_TRUE;
		else
			elog(WARN,
			     "copyReadBinary: fread() error, read %d/%d",
			     rbytes, attrlen);
		pfree(inbuf);
		return(NULL);
	}
	*isnull = B_FALSE;
	return(inbuf);
}

/*
 *	copyCleanup
 *
 *	Deallocate the argument objects.
 */
int
copyCleanup(rdesc, fp, doms)
	Relation	rdesc;
	FILE 		*fp;
	DomainData	doms[];
{
	if (RelationIsValid(rdesc))
		RelationCloseHeapRelation(rdesc);
	if (fp != (FILE *) NULL) {
		FreeFile();		/* XXX race condition */
		if (fclose(fp) == EOF) {
			elog(WARN, "copyCleanup: Can't close file");
		}
	}
	if (DomainIsValid(doms))
		pfree((char *) doms);
}

/*
 *	copyAlloc
 *
 *	Grab at least 'more' more bytes for a static buffer.
 */
char *
copyAlloc(p, plen, more)
	char		*p;
	unsigned	*plen;
	unsigned	more;
{
	char		*tmpp;
	unsigned 	newlen;
	extern char 	*malloc();

	if (PointerIsValid(p)) {
		newlen = *plen + Max(more, BUFSIZ);
		p = repalloc(p,newlen);
	} else {
		newlen = Max(more, BUFSIZ);
		p = palloc(newlen);
	}
	if (!PointerIsValid(p)) {
		elog(WARN, "copyAlloc: malloc() failure");
		newlen = 0;
	}
	*plen = newlen;
	return(p);
}

/*
 *	*** DEBUGGING CODE ***
 */

/*
 *	print_domains
 */
print_domains(ndoms, doms)
	int		ndoms;
	DomainData	doms[];
{
	int	i;

	for (i = 0; i < ndoms; ++i)
		printf("doms[%d] = %d %d %d 0x%x %d %s %d %d\n",
		       i,
		       doms[i].domnum,
		       doms[i].attnum,
		       doms[i].domlen,
		       doms[i].domtype,
		       doms[i].delim,
		       doms[i].string ? doms[i].string : "NULL",
		       doms[i].typoutput,
		       doms[i].typinput);
}
