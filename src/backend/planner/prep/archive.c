/*
 * $Header$
 *
 *  archive.c -- Support for planning scans on archived relations
 */
#include "tmp/c.h"
#include "utils/rel.h"
#include "utils/log.h"
#include "utils/relcache.h"
#include "catalog/pg_relation.h"
#include "nodes/pg_lisp.h"
#include "parser/parsetree.h"

void
plan_archive(rt)
	List rt;
{
	LispValue rtitem;
	LispValue rte;
	LispValue trange;
	ObjectId reloid;
	Relation r;

	foreach(rtitem, rt) {
		rte = CAR(rtitem);
		trange = rt_time(rte);
		if (CAR(trange) != LispNil) {
			reloid = CInteger(rt_relid(rte));
			r = RelationIdGetRelation(reloid);
			if (r->rd_rel->relarch != 'n') {
				rt_flags(rte) = lispCons(lispAtom("archive"),
							 rt_flags(rte));
			}
		}
	}
}

/*
 *  find_archive_rels -- Given a particular relid, find the archive
 *			 relation's relid.
 */

LispValue
find_archive_rels(relid)
	LispValue relid;
{
	ObjectId roid;
	Relation arel;
	LispValue arelid;
	Name arelname;

	/*
	 *  Archive relations are named a,XXXXX where XXXXX == the OID
	 *  of the relation they archive.  Create a string containing
	 *  this name and find the reldesc for the archive relation.
	 */

	roid = CInteger(relid);
	arelname = (Name) palloc(sizeof(NameData));
	sprintf(&arelname->data[0], "a,%ld", roid);
	arel = RelationNameGetRelation(arelname);
	pfree(arelname);

	/* got it, now build a LispValue to return */
	arelid = lispList();
	CAR(arelid) = lispInteger(arel->rd_id);
	CDR(arelid) = lispList();
	CAR(CDR(arelid)) = lispInteger(roid);
	CDR(CDR(arelid)) = LispNil;

	return (arelid);
}
