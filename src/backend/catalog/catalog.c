/*
 * catalog.c --
 *
 */

#include <strings.h>	/* XXX */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/htup.h"
#include "storage/buf.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/catname.h"	/* NameIs{,Shared}SystemRelationName */
#include "catalog/pg_attribute.h"
#include "catalog/pg_type.h"

/*
 * XXX The global relation names should be merged with the ones defined
 * XXX in lib/catalog/catname.c, someday.
 */

char	DATABASE_R[16] = 	"pg_database";
char	DEFAULTS_R[16] = 	"pg_defaults";
char	DEMON_R[16] = 		"pg_demon";
char	LOG_R[16] = 		"pg_log";
char	MAGIC_R[16] = 		"pg_magic";
char	SERVER_R[16] = 		"pg_server";
char	TIME_R[16] = 		"pg_time";
char	TYPE_R[16] = 		"pg_type";
char	USER_R[16] = 		"pg_user";
char	VARIABLE_R[16] = 	"pg_variable";

/* ----------------
 *	WARNING  WARNING  WARNING  WARNING  WARNING  WARNING
 *
 *	keep SharedSysRelns[] in SORTED order!  A binary search
 *	is done on it!
 *
 *	XXX this is a serious hack which should be fixed -cim 1/26/90
 * ----------------
 */
static char *SharedSysRelns[] = {
    DATABASE_R,
    DEFAULTS_R,
    DEMON_R,
    LOG_R,
    MAGIC_R,
    SERVER_R,
    TIME_R,
    USER_R,
    VARIABLE_R,
};

#ifndef	MAXPATHLEN
#define	MAXPATHLEN	80
#endif

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

char    *
relpath(relname)
char    relname[];
{
        static  char    path[1 + 6 + 16] = "../../";
        static  char    *appnd = path + 6;

        if (NameIsSharedSystemRelationName(relname)) {
                strncpy(appnd, relname, 16);
                return(path);
        }
        return(relname);
}

/*
 *	issystem	- returns non-zero iff relname is a system catalog
 *
 *	We now make a new requirement where system catalog relns must begin
 *	with pg_ while user relns are forbidden to do so.  Make the test
 * 	trivial and instantaneous.
 *
 */
bool
issystem(relname)
char	relname[];
{
	if (relname[0] && relname[1] && relname[2])
		return (relname[0] == 'p' && 
			relname[1] == 'g' && 
			relname[2] == '_');
	else
		return FALSE;
}

bool
NameIsSystemRelationName(name)
	Name	name;
{
	return ((bool)issystem(name));
}

bool
NameIsSharedSystemRelationName(relname)
	Name	relname;
{
	register char	**spp;
	register char	**mpp;
	int		len;
	int		mid;
	int		status;

	spp = SharedSysRelns;

	/*
	 * Quick out: if its not a system reln it can't be a shared system reln.
	 */
	if (!issystem(relname))
		return FALSE;
	/*
	 * Total number of Shared System relations
	 */
	len = sizeof (SharedSysRelns) / sizeof (char *);

	/*
	 * While there's more than 2 candidates remaining, run the
	 * binary search algorithm
	 */
	while (len > 2) {
		mid = len >> 1;
		mpp = spp + mid;
		if ((status = strncmp(relname, *mpp, 16)) == 0) {
			return(TRUE);
		}
		else if (status < 0)
			len = mid;
		else {
			spp = mpp + 1;
			len -= mid + 1;
		}
	}
	if (len <= 0)
		return(FALSE);
	if (len == 2) {
		if ((status = strncmp(relname, *spp, 16)) == 0) {
			return(1);
		} else if (status < 0)
			return(FALSE);
		spp++;
	}
	return(strncmp(relname, *spp, 16) == 0);
}

/*
 *	newoid		- returns a unique identifier across all catalogs.
 *
 *	Object Id allocation is now done by GetNewObjectID in
 *	access/transam/varsup.c.  oids are now allocated correctly.
 *
 * old comments:
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
    ObjectId	lastoid;
    
    GetNewObjectId(&lastoid);
    if (! ObjectIdIsValid(lastoid))
	elog(WARN, "newoid: GetNewObjectId returns invalid oid");

    return lastoid;
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
    int			natts;
    AttributeTupleForm	att[];	/* tuple desc */
{
    AttributeTupleForm		*attributeP;
    register TypeTupleForm	typp;
    HeapTuple			tuple;
    int				i;

    if (natts < 0 || natts > MaxHeapAttributeNumber)
	elog(WARN, "fillatt: %d attributes is too large", natts);
    if (natts == 0) {
	elog(DEBUG, "fillatt: called with natts == 0");
	return;
    }

    attributeP = &att[0];

    for (i = 0; i < natts;) {
	tuple = SearchSysCacheTuple(TYPOID, (*attributeP)->atttypid);
	if (!HeapTupleIsValid(tuple)) {
	    elog(WARN, "fillatt: unknown atttypid %ld",
		 (*attributeP)->atttypid);
	} else {
	    typp = (TypeTupleForm) GETSTRUCT(tuple);  /* XXX */
	    (*attributeP)->attlen = typp->typlen;
	    (*attributeP)->attnum = (int16)++i;
	    (*attributeP)->attbyval = typp->typbyval;
	}
	attributeP += 1;
    }
}
