/*
 * rename.c --
 * renameatt() and renamerel() reside here.
 */

#include <strings.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"

#include "access/attnum.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"

#include "commands/command.h"
#include "commands/copy.h"

#include "executor/execdefs.h"	/* for EXEC_{FOR,BACK,FDEBUG,BDEBUG} */

#include "storage/buf.h"
#include "storage/itemptr.h"

#include "tmp/miscadmin.h"
#include "tmp/portal.h"

#include "utils/excid.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/palloc.h"
#include "utils/rel.h"

#include "catalog/pg_attribute.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
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
	relsdesc = ambeginscan(relrdesc, 0, NowTimeQual, 1, key);
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
	attsdesc = ambeginscan(attrdesc, 0, NowTimeQual, 2, key);
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
	attsdesc = ambeginscan(attrdesc, 0, NowTimeQual, 2, key);
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
	oldsdesc = ambeginscan(relrdesc, 0, NowTimeQual, 1, &key);
	oldreltup = amgetnext(oldsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(oldreltup)) {
		amendscan(oldsdesc);
		amclose(relrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renamerel: relation \"%s\" does not exist",
		     oldrelname);
	}
	oldreltup = palloctup(oldreltup, InvalidBuffer, relrdesc);

	key.sk_data = (DATUM) newrelname;
	newsdesc = ambeginscan(relrdesc, 0, NowTimeQual, 1, &key);
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
