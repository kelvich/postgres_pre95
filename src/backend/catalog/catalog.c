static	char	catalog_c[] = "$Header$";

#include <strings.h>
#include <sys/file.h>
#include "access.h"
#include "catname.h"	/* XXX for definitions of NameIs{,Shared}SystemRelationName */
#include "context.h"
#include "fmgr.h"
#include "log.h"
#include "name.h"

#include "c.h"

#include "buf.h"
#include "cat.h"
#include "oid.h"
#include "trange.h"

/*
 *	catalog.c	- system catalog utility functions
 */

char	AGGREGATE_R[16] = "pg_aggregate";
char	AM_R[16] = "pg_am";
char	AMOP_R[16] = "pg_amop";
char	ATTRIBUTE_R[16] = "pg_attribute";
char	DATABASE_R[16] = "pg_database";
char	DEFAULTS_R[16] = "pg_defaults";
char	DEMON_R[16] = "pg_demon";
char	INDEX_R[16] = "pg_index";
char	INHERITPROC_R[16] = "pg_inheritproc";
char	INHERITS_R[16] = "pg_inherits";
char	IPL_R[16] = "pg_ipl";
char	LANGUAGE_R[16] = "pg_language";
char	LOG_R[16] = "pg_log";
char	MAGIC_R[16] = "pg_magic";
char	OPCLASS_R[16] = "pg_opclass";
char	OPERATOR_R[16] = "pg_operator";
char	PARG_R[16] = "pg_parg";
char	PROC_R[16] = "pg_proc";
char	RELATION_R[16] = "pg_relation";
char	RULE_R[16] = "pg_rule";
char	SERVER_R[16] = "pg_server";
char	STATISTIC_R[16] = "pg_statistic";
char	TIME_R[16] = "pg_time";
char	TYPE_R[16] = "pg_type";
char	USER_R[16] = "pg_user";
char	VARIABLE_R[16] = "pg_variable";
char	VERSION_R[16] = "pg_version";

static	char	*SystemRelname[] = {
	AGGREGATE_R, AM_R, AMOP_R, ATTRIBUTE_R, DATABASE_R, DEFAULTS_R, DEMON_R,
	INDEX_R, INHERITPROC_R, INHERITS_R, IPL_R, LANGUAGE_R, LOG_R, MAGIC_R,
	OPCLASS_R, OPERATOR_R, PARG_R, PROC_R, RELATION_R, RULE_R, SERVER_R,
	STATISTIC_R, TIME_R, TYPE_R, USER_R, VARIABLE_R, VERSION_R
};

static	char	**Spp;			/* system relname pointer */

#ifndef	MAXPATHLEN
#define	MAXPATHLEN	80
#endif

static	char	IsDbdb[] = {
	'\0', '\0', '\0', '\0', '\001', '\001', '\001',
	'\0', '\0', '\0', '\0', '\0', '\001', '\001',
	'\0', '\0', '\0', '\0', '\0', '\0', '\001',
	'\0', '\001', '\0', '\001', '\001', '\0'
};

/*
 *	relpath		- path to the relation
 *
 *	Note:  returns a static value.  Maybe should be able to handle
 *	temporary relations and arbitrary files (buffered psort temps)
 *	with proper names in addition to the other standard relation
 *	names.
 *
 *	Perhaps this should be in-line code in relopen().
 */

char	*
relpath(relname)
char	relname[];
{
	static	char	path[1 + 6 + 16] = "../../";
	static	char	*appnd = path + 6;

	if (issystem(relname) && IsDbdb[Spp - SystemRelname]) {
		strncpy(appnd, relname, 16);
		return(path);
	}
	return(relname);
}

/*
 *	issystem	- returns non-zero iff relname is a system catalog
 *
 *	Uses a binary search on the SystemRelname table.  Hashing would
 *	probably be more efficient since the relation names are static.
 *	Currently, leaves Spp pointing to the SystemRelname string.
 */

int
issystem(relname)
char	relname[];
{
	register char	**spp;
	register char	**mpp;
	int		len;
	int		mid;
	int		status;

	spp = SystemRelname;
	len = sizeof (SystemRelname) / sizeof (char *);
	while (len > 2) {
		mid = len >> 1;
		mpp = spp + mid;
		if ((status = strncmp(relname, *mpp, 16)) == 0) {
			Spp = mpp;
			return(1);
		}
		else if (status < 0)
			len = mid;
		else {
			spp = mpp + 1;
			len -= mid + 1;
		}
	}
	if (len <= 0)
		return(0);
	if (len == 2) {
		if ((status = strncmp(relname, *spp, 16)) == 0) {
			Spp = spp;
			return(1);
		} else if (status < 0)
			return(0);
		spp++;
	}
	Spp = spp;
	return(strncmp(relname, *spp, 16) == 0);
}

bool
NameIsSystemRelationName(name)
	Name	name;
{
	return ((bool)issystem(name));
}

bool
NameIsSharedSystemRelationName(name)
	Name	name;
{
	return ((bool)(NameIsSystemRelationName(name) && IsDbdb[Spp - SystemRelname]));
}

/*
 *	newoid		- returns a unique identifier across all catalogs.
 *
 *	This needs to change soon, it fails if there are too many more
 *	than one call per second when postgres restarts after it dies.
 *
 *	The distribution of OID's should be done by the POSTMASTER.
 *	Also there needs to be a facility to preallocate OID's.  Ie.,
 *	for a block of OID's to be declared as invalid ones to allow
 *	user programs to use them for temporary object identifiers.
 */

ObjectId
newoid()
{
	static ObjectId	lastoid = InvalidObjectId;
	long		time();			/* XXX lint */

	if (lastoid != InvalidObjectId)
		return(++lastoid);
	return(lastoid = time((long *)0));
}

/*
 *	fillatt		- fills the ATTRIBUTE relation fields from the TYP
 *
 *	Expects that the atttypid domain is set for each att[].
 *	Returns with the attnum, and attlen domains set.
 *	attnum, attproc, atttyparg, ... should be set by the user.
 *
 *	In the future, attnum may not be set?!? or may be passed as an arg?!?
 *
 *	Current implementation is very inefficient--should cashe the
 *	information if this is at all possible.
 *
 *	Check to see if this is really needed, and especially in the case
 *	of index tuples.
 */

fillatt(natts, att)
int		natts;
struct	attribute *att[];
{
	register struct attribute  **attp;
	register TypeTupleForm	typp;
	struct	skey	key;
	TUPLE		*tup;
	struct	reldesc	*rdesc;
	struct	relscan	*sdesc;
	int		i;
	struct	reldesc	*amopenr();
	struct	relscan	*ambeginscan();
	TUPLE		*amgetnext();
	extern		amendscan(), amclose();

	if (natts < 0 || natts > MAXATTS)
		elog(WARN, "fillatt: %d attributes is too large", natts);
	if (natts == 0) {
		elog(DEBUG, "fillatt: called with natts == 0");
		return;
	}
	/* XXX catcache */
	rdesc = amopenr(TYPE_R);
	key.sk_flags = 0;
	key.sk_attnum = T_OID;		/* typid */
	key.sk_opr = F_INT4EQ;
	attp = att;
	for (i = 0; i < natts;) {
		key.sk_data = (DATUM)(*attp)->atttypid;
		sdesc = ambeginscan(rdesc, 0, DefaultTimeRange, 1, &key);
		tup = (TUPLE *)amgetnext(sdesc, 0, (Buffer *)NULL);
		if (!PointerIsValid(tup)) {
			elog(WARN,
				"fillatt: unknown atttypid %ld",
				(long)key.sk_data);
		}
		typp = (TypeTupleForm)GETSTRUCT((&tup->tp_t));	/* XXX */
		(*attp)->attlen = typp->typlen;
		(*attp)->attnum = (int16)++i;
		(*attp)->attbyval = typp->typbyval;
		amendscan(sdesc);
		attp++;
	}
	amclose(rdesc);
}
