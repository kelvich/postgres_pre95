/*
 * command.c --
 *	POSTGRES utility command code.
 */

#include "c.h"

RcsId("$Header$");

#include <strings.h>
#include "access.h"
#include "log.h"

#include "anum.h"
#include "buf.h"
#include "catname.h"
#include "globals.h"	/* for IsUnderPostmaster */
#include "itemptr.h"
#include "heapam.h"
#include "htup.h"
#include "name.h"	/* for NameIsEqual */
#include "portal.h"
#include "rel.h"
#include "relscan.h"
#include "rproc.h"
#include "trange.h"

#include "command.h"

void
EndCommand(commandId)
	String	commandId;
{
	if (IsUnderPostmaster) {
		putnchar("C", 1);
		putint(0, 4);
		putstr(commandId);
		pflush();
	}
}

void
PerformPortalFetch(name, forward, count)
	String	name;
	bool	forward;
	Count	count;
{
	Portal	portal;

	if (name == NULL) {
		elog(WARN, "PerformPortalFetch: blank portal unsupported");
	}

	portal = GetPortalByName(name);
	if (!PortalIsValid(portal)) {
		elog(WARN, "PerformPortalFetch: %s not found", name);
	}

	/*
	 * START HERE
	 */
	elog(NOTICE, "Fetch: %s %s %d unimplemented",
		name, (forward) ? "forw" : "back", count);
}

void
PerformPortalClose(name)
	String	name;
{
	Portal	portal;

	if (name == NULL) {
		elog(WARN, "PerformPortalClose: blank portal unsupported");
	}

	/*
	 * Check that memory context is sane here.
	 */
	portal = GetPortalByName(name);
	if (!PortalIsValid(portal)) {
		elog(WARN, "PerformPortalClose: %s not found", name);
	}

	PortalDestroy(portal);
}

/*
 *	addattribute	- adds an additional attribute to a relation
 *
 *	Adds attribute field(s) to a relation.  Each new attribute
 *	is given attnums in sequential order and is added to the
 *	ATTRIBUTE relation.  If the AMI fails, defunct tuples will
 *	remain in the ATTRIBUTE relation for later vacuuming.
 *	Later, there may be some reserved attribute names???
 *
 *	(If needed, can instead use elog to handle exceptions.)
 *
 *	Note:
 *		Initial idea of ordering the tuple attributes so that all
 *	the variable length domains occured last was scratched.  Doing
 *	so would not speed access too much (in general) and would create
 *	many complications in formtuple, amgetattr, and addattribute.
 *
 *	scan attribute catalog for name conflict (within rel)
 *	scan type catalog for absence of data type (if not arg)
 *	create attnum magically???
 *	create attribute tuple
 *	insert attribute in attribute catalog
 *	modify reldesc
 *	create new relation tuple
 *	insert new relation in relation catalog
 *	delete original relation from relation catalog
 */

addattribute(relname, natts, att)
	char			relname[];
	int			natts;
	struct attribute	*att[];
{
	Relation			relrdesc, attrdesc;
	HeapScanDesc			relsdesc, attsdesc;
	HeapTuple			reltup, atttup;
	struct attribute		*ap, *attp, **app;
	int				i;
	int				minattnum, maxatts;
	HeapTuple			tup;
	struct	skey			key[2];	/* static better? [?] */
	ItemPointerData			oldTID;
	HeapTuple			addtupleheader(), palloctup();
	int				issystem();
	
	if (natts <= 0)
		elog(FATAL, "addattribute: passed %d attributes?!?", natts);
	if (issystem(relname)) {
		elog(WARN, "addattribute: system relation \"%s\" not changed",
		     relname);
		return;
	}

	/* check if any attribute is repeated in list */
	for (i = 1; i < natts; i += 1) {
		int	j;

		for (j = 0; j < i; j += 1) {
			if (NameIsEqual(att[j]->attname, att[i]->attname)) {

				elog(WARN, "addattribute: \"%s\" repeated",
					att[j]->attname);
			}
		}
	}

	relrdesc = amopenr(RelationRelationName);
	key[0].sk_flags = NULL;
	key[0].sk_attnum = RelationNameAttributeNumber;
	key[0].sk_opr = Character16EqualRegProcedure;	/* XXX name= */
	key[0].sk_data = (DATUM) relname;
	relsdesc = ambeginscan(relrdesc, 0, DefaultTimeRange, 1, key);
	reltup = amgetnext(relsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(reltup)) {
		amendscan(relsdesc);
		amclose(relrdesc);
		elog(WARN, "addattribute: relation \"%s\" not found",
		     relname);
		return;
	}
	if (((struct relation *) GETSTRUCT(reltup))->relkind == 'i') {
		elog(WARN, "addattribute: index relation \"%s\" not changed",
		     relname);
		return;
	}
	reltup = palloctup(reltup, InvalidBuffer, relrdesc);
	amendscan(relsdesc);

	minattnum = ((struct relation *) GETSTRUCT(reltup))->relnatts;
	maxatts = minattnum + natts;
	if (maxatts > MAXATTS) {
		pfree((char *) reltup);			/* XXX temp */
		amclose(relrdesc);			/* XXX temp */
		elog(WARN, "addattribute: relations limited to %d attributes",
		     MAXATTS);
		return;
	}

	attrdesc = amopenr(AttributeRelationName);
	key[0].sk_flags = NULL;
	key[0].sk_attnum = AttributeRelationIdAttributeNumber;
	key[0].sk_opr = ObjectIdEqualRegProcedure;
	key[0].sk_data = (DATUM) reltup->t_oid;
	key[1].sk_flags = NULL;
	key[1].sk_attnum = AttributeNameAttributeNumber;
	key[1].sk_opr = Character16EqualRegProcedure;	/* XXX name= */
	key[1].sk_data = (DATUM) NULL;	/* set in each iteration below */
	app = att;
	for (i = minattnum; i < maxatts; ++i) {
		ap = *app++;
		key[1].sk_data = (DATUM) ap->attname;
		attsdesc = ambeginscan(attrdesc, 0, DefaultTimeRange, 2, key);
		tup = amgetnext(attsdesc, 0, (Buffer *) NULL);
		if (HeapTupleIsValid(tup)) {
			pfree((char *) reltup);		/* XXX temp */
			amendscan(attsdesc);		/* XXX temp */
			amclose(attrdesc);		/* XXX temp */
			amclose(relrdesc);		/* XXX temp */
			elog(WARN, "addattribute: attribute \"%s\" exists",
			     ap->attname);
			return;
		}
		amendscan(attsdesc);
		ap->attrelid = reltup->t_oid;
		ap->attnum = 1 + (short) i;
	}
	app = att;
	atttup = addtupleheader(11, sizeof(struct attribute), (char *) ap);
	/* *ap is a place holder */
	
	attp = (struct attribute *) GETSTRUCT(atttup);
	for (i = minattnum; i < maxatts; ++i) {
		bcopy((char *) *app++, (char *) attp, sizeof(*attp));
		aminsert(attrdesc, atttup, (double *) NULL);
	}
	pfree((char *) atttup);
	amclose(attrdesc);
	((struct relation *) GETSTRUCT(reltup))->relnatts = maxatts;
	oldTID = reltup->t_ctid;
	amreplace(relrdesc, &oldTID, reltup);
	pfree((char *) reltup);
	amclose(relrdesc);
}

/*
 *	rename		- See renameatt, renamerel
 */

/*
 *	renameatt	- changes the name of a attribute in a relation
 *
 *	Attname attribute is changed in attribute catalog.
 *	No record of the previous attname is kept (correct?).
 *
 *	get proper reldesc from relation catalog (if not arg)
 *	scan attribute catalog
 *		for name conflict (within rel)
 *		for original attribute (if not arg)
 *	modify attname in attribute tuple
 *	insert modified attribute in attribute catalog
 *	delete original attribute from attribute catalog
 *
 *	XXX Renaming an indexed attribute must (eventually) also change
 *		the attribute name in the associated indexes.
 */

renameatt(relname, oldattname, newattname)
	char	relname[], oldattname[], newattname[];
{
	Relation	relrdesc, attrdesc;
	HeapScanDesc	relsdesc, attsdesc;
	HeapTuple	reltup, oldatttup, newatttup;
	struct skey	key[2];
	ItemPointerData	oldTID;
	int		issystem();
	HeapTuple	palloctup();
	
	if (issystem(relname)) {
		elog(WARN, "renameatt: system relation \"%s\" not modified",
		     relname);
		return;
	}
	key[0].sk_flags = NULL;
	key[0].sk_attnum = RelationNameAttributeNumber;
	key[0].sk_opr = Character16EqualRegProcedure;	/* XXX name= */
	key[0].sk_data = (DATUM) relname;
	relrdesc = amopenr(RelationRelationName);
	relsdesc = ambeginscan(relrdesc, 0, DefaultTimeRange, 1, key);
	reltup = amgetnext(relsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(reltup)) {
		amendscan(relsdesc);
		amclose(relrdesc);
		elog(WARN, "renameatt: relation \"%s\" nonexistent",
		     relname);
		return;
	}
	amendscan(relsdesc);
	amclose(relrdesc);

	key[0].sk_flags = NULL;
	key[0].sk_attnum = AttributeRelationIdAttributeNumber;
	key[0].sk_opr = Integer32EqualRegProcedure;	/* XXX int4= */
	key[0].sk_data = (DATUM) reltup->t_oid;
	key[1].sk_flags = NULL;
	key[1].sk_attnum = AttributeNameAttributeNumber;
	key[1].sk_opr = Character16EqualRegProcedure;	/* XXX name= */
	key[1].sk_data = (DATUM) oldattname;
	attrdesc = amopenr(AttributeRelationName);
	attsdesc = ambeginscan(attrdesc, 0, DefaultTimeRange, 2, key);
	oldatttup = amgetnext(attsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(oldatttup)) {
		amendscan(attsdesc);
		amclose(attrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renameatt: attribute \"%s\" nonexistent",
		     oldattname);
		return;
	}
	if (((struct attribute *) GETSTRUCT(oldatttup))->attnum < 0) {
		elog(WARN, "renameatt: system attribute \"%s\" not renamed",
		     oldattname);
		return;
	}
	oldatttup = palloctup(oldatttup, InvalidBuffer, attrdesc);
	amendscan(attsdesc);

	key[1].sk_data = (DATUM) newattname;
	attsdesc = ambeginscan(attrdesc, 0, DefaultTimeRange, 2, key);
	newatttup = amgetnext(attsdesc, 0, (Buffer *) NULL);
	if (PointerIsValid(newatttup)) {
		pfree((char *) oldatttup);
		amendscan(attsdesc);
		amclose(attrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renameatt: attribute \"%s\" exists",
		     newattname);
		return;
	}
	amendscan(attsdesc);

	bcopy(newattname,
	      (char *) (((struct attribute *) GETSTRUCT(oldatttup))->attname),
	      sizeof(NameData));
	oldTID = oldatttup->t_ctid;
	amreplace(attrdesc, &oldTID, oldatttup); /* insert "fixed" tuple */
	amclose(attrdesc);
	pfree((char *) oldatttup);
}

/*
 *	renamerel	- change the name of a relation
 *
 *	Relname attribute is changed in relation catalog.
 *	No record of the previous relname is kept (correct?).
 *
 *	scan relation catalog
 *		for name conflict
 *		for original relation (if not arg)
 *	modify relname in relation tuple
 *	insert modified relation in relation catalog
 *	delete original relation from relation catalog
 *
 *	XXX Will currently lose track of a relation if it is unable to
 *		properly replace the new relation tuple.
 */

renamerel(oldrelname, newrelname)
	char	oldrelname[], newrelname[];
{
	Relation	relrdesc;		/* for RELATION relation */
	HeapScanDesc	oldsdesc, newsdesc;
	HeapTuple	oldreltup, newreltup;
	struct skey	key;
	ItemPointerData	oldTID;
	char		oldpath[MAXPGPATH], newpath[MAXPGPATH];
	int		issystem();
	char		*relpath();
	HeapTuple	palloctup();
	extern		rename();
	
	if (issystem(oldrelname)) {
		elog(WARN, "renamerel: system relation \"%s\" not renamed",
		     oldrelname);
		return;
	}
	relrdesc = amopenr(RelationRelationName);
	key.sk_flags = NULL;
	key.sk_attnum = RelationNameAttributeNumber;
	key.sk_opr = Character16EqualRegProcedure;	/* XXX name= */
	key.sk_data = (DATUM) oldrelname;
	oldsdesc = ambeginscan(relrdesc, 0, DefaultTimeRange, 1, &key);
	oldreltup = amgetnext(oldsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(oldreltup)) {
		amendscan(oldsdesc);
		amclose(relrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renamerel: relation \"%s\" does not exist",
		     oldrelname);
	}
	oldreltup = palloctup(oldreltup, InvalidBuffer, relrdesc);

	key.sk_data = (DATUM) newrelname;
	newsdesc = ambeginscan(relrdesc, 0, DefaultTimeRange, 1, &key);
	newreltup = amgetnext(newsdesc, 0, (Buffer *) NULL);
	if (PointerIsValid(newreltup)) {
		pfree((char *) oldreltup);
		amendscan(newsdesc);
		amendscan(oldsdesc);
		amclose(relrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renamerel: relation \"%s\" exists",
		     newrelname);
	}
	amendscan(newsdesc);
	bcopy(newrelname,
	      (char *) (((struct relation *) GETSTRUCT(oldreltup))->relname),
	      sizeof(NameData));
	oldTID = oldreltup->t_ctid;
	amreplace(relrdesc, &oldTID, oldreltup); /* insert "fixed" tuple */
	pfree((char *) oldreltup);
	amendscan(oldsdesc);
	amclose(relrdesc);
	(void) strcpy(oldpath, relpath(oldrelname));
	(void) strcpy(newpath, relpath(newrelname));
	if (rename(oldpath, newpath) < 0)
		elog(WARN, "renamerel: unable to rename file: %m");
}
