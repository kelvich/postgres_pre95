/*
 * copy.h --
 * 	Definitions for using the POSTGRES copy command.
 *	This isn't in h/ since it isn't called by other POSTGRES routines.
 *
 * Identification:
 *	$Header$
 */

#ifndef CopyIncluded
#define	CopyIncluded	1

#include "postgres.h"

/*
 *	Copy domain
 *
 *------------------------------------------------------------------
 * Arguments to createdomains():
 *	DOMNAME			DOMTYPE			DOMDELIM
 *------------------------------------------------------------------
 *	attribute name		attribute type		no 
 *	attribute name	 	"text" or NULL		optional
 *	attribute name	 	"char[N]"		no
 *	non-attribute (dummy)	?			optional **
 *	string			-1			no
 *
 *------------------------------------------------------------------
 * As stored:
 *	TYPE				DELIM?
 *------------------------------------------------------------------
 *	relation attribute, binary	no
 *	relation attribute, text	yes
 *		domlen == -1
 *	relation attribute, char[N]	no
 *		domlen > 0
 *	dummy domain			yes **
 *	string				no
 *
 *	** iff domlen == -1
 */
typedef struct DomainData {
	int	domnum;
	int	attnum;		/* 0 for dummy/string */
	int	domlen;		/* length in bytes of domain type/string */
	short	domtype;	/* flags for domain treatment */
	char 	delim;	 	/* field delimiting character */
	char	*string;	/* string value (if any) */
	OID 	typoutput;	/* typoutput proc to use for text write */
	OID	typinput;	/* typinput proc to use for text read */
} DomainData;

typedef DomainData	*Domain;

/* The only two functions the outside world should see: */
extern Domain  	createdomains();
extern		copyrel();

/* Constants to indicate various facts to createdomains() */
#define	NO_DELIM	((char) -1)
#define	STRING_TYPE	((char *) -1)

/* Flags for the domtype field of a Domain */
#define	C_NONULLS	(0x01)
#define	C_DELIMITED	(0x02)
#define	C_EXTERNAL	(0x04)
#define	C_STRING	(0x08)
#define	C_DUMMY		(0x10)
#define	C_ATTRIBUTE	(0x20)

#define	DomainSetNoNulls(DOMAIN)	(DOMAIN)->domtype |= C_NONULLS
#define	DomainSetDelimited(DOMAIN)	(DOMAIN)->domtype |= C_DELIMITED
#define	DomainSetExternal(DOMAIN)	(DOMAIN)->domtype |= C_EXTERNAL
#define	DomainSetString(DOMAIN)		(DOMAIN)->domtype |= C_STRING
#define	DomainSetDummy(DOMAIN)		(DOMAIN)->domtype |= C_DUMMY
#define	DomainSetAttribute(DOMAIN)	(DOMAIN)->domtype |= C_ATTRIBUTE

#define	DomainIsNoNulls(DOMAIN)		((DOMAIN)->domtype & C_NONULLS)
#define	DomainIsDelimited(DOMAIN)	((DOMAIN)->domtype & C_DELIMITED)
#define	DomainIsExternal(DOMAIN)	((DOMAIN)->domtype & C_EXTERNAL)
#define	DomainIsString(DOMAIN)		((DOMAIN)->domtype & C_STRING)
#define	DomainIsDummy(DOMAIN)		((DOMAIN)->domtype & C_DUMMY)
#define	DomainIsAttribute(DOMAIN)	((DOMAIN)->domtype & C_ATTRIBUTE)

#endif	/* !CopyIncluded */
