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

static n_shared = sizeof(SharedSystemRelationNames) / sizeof(Name);

extern char		*DataDir;

#ifndef	MAXPATHLEN
#define	MAXPATHLEN	80
#endif

/*
 *	relpath		- path to the relation
 *	Perhaps this should be in-line code in relopen().
 */

char *
relpath(relname)
char    relname[];
{
        char    *path;

        if (NameIsSharedSystemRelationName((Name)relname)) {
		path = (char *) palloc(strlen(DataDir) + sizeof(NameData) + 2);
		sprintf(path, "%s/%.16s", DataDir, relname);
		return (path);
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
 *	XXX this is way bogus. -- pma
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
	register Name	*spp = SharedSystemRelationNames;
	register Name	*mpp;
	int		len = n_shared;
	int		mid;
	int		status;

	/*
	 * Quick out: if it's not a system relation, it can't be a shared
	 * system relation.
	 */
	if (!issystem(relname))
		return FALSE;
	
	/*
	 * While there's more than 2 candidates remaining, run the
	 * binary search algorithm
	 */
	while (len > 2) {
		mid = len >> 1;
		mpp = spp + mid;
		if (!(status = strncmp(relname->data,
				       (*mpp)->data, sizeof(NameData)))) {
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
		if (!(status = strncmp(relname->data,
				       (*spp)->data, sizeof(NameData)))) {
			return(TRUE);
		} else if (status < 0)
			return(FALSE);
		spp++;
	}
	return(!strncmp(relname->data,
			(*spp)->data, sizeof(NameData)));
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
	AttributeTupleForm	*attributeP;
	register TypeTupleForm	typp;
	HeapTuple		tuple;
	int			i;

	if (natts < 0 || natts > MaxHeapAttributeNumber)
		elog(WARN, "fillatt: %d attributes is too large", natts);
	if (natts == 0) {
		elog(DEBUG, "fillatt: called with natts == 0");
		return;
	}
	
	attributeP = &att[0];
	
	for (i = 0; i < natts;) {
		tuple = SearchSysCacheTuple(TYPOID,
					    (*attributeP)->atttypid,
					    (char *) NULL,
					    (char *) NULL,
					    (char *) NULL);
		if (!HeapTupleIsValid(tuple)) {
			elog(WARN, "fillatt: unknown atttypid %ld",
			     (*attributeP)->atttypid);
		} else {
			typp = (TypeTupleForm) GETSTRUCT(tuple);  /* XXX */
			(*attributeP)->attlen = typp->typlen;
			(*attributeP)->attnum = (int16) ++i;
			(*attributeP)->attbyval = typp->typbyval;
		}
		attributeP += 1;
	}
}
