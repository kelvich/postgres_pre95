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

#include "commands/copy.h"

#include "executor/execdefs.h"	/* for EXEC_{FOR,BACK,FDEBUG,BDEBUG} */

#include "storage/buf.h"
#include "storage/itemptr.h"

#include "tmp/miscadmin.h"
#include "tmp/portal.h"
#include "tcop/dest.h"
#include "commands/command.h"

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
	
	if (issystem(relname)) {
		elog(WARN, "renameatt: system relation \"%s\" not modified",
		     relname);
		return;
	}
	ScanKeyEntryInitialize((ScanKeyEntry)&key[0], NULL, RelationNameAttributeNumber,
						   Character16EqualRegProcedure, (Datum) relname);
	relrdesc = heap_openr(RelationRelationName);
	relsdesc = heap_beginscan(relrdesc, 0, NowTimeQual, 1, key);
	reltup = heap_getnext(relsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(reltup)) {
		heap_endscan(relsdesc);
		heap_close(relrdesc);
		elog(WARN, "renameatt: relation \"%s\" nonexistent",
		     relname);
		return;
	}
	heap_endscan(relsdesc);
	heap_close(relrdesc);

	ScanKeyEntryInitialize((ScanKeyEntry)&key[0], NULL, AttributeRelationIdAttributeNumber,
	                       Integer32EqualRegProcedure, (Datum)reltup->t_oid);
	ScanKeyEntryInitialize((ScanKeyEntry)&key[1], NULL, AttributeNameAttributeNumber,
	                       Character16EqualRegProcedure, (Datum)oldattname);
	attrdesc = heap_openr(AttributeRelationName);
	attsdesc = heap_beginscan(attrdesc, 0, NowTimeQual, 2, key);
	oldatttup = heap_getnext(attsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(oldatttup)) {
		heap_endscan(attsdesc);
		heap_close(attrdesc);	/* XXX should be unneeded eventually */
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
	heap_endscan(attsdesc);

	key[1].sk_data = (DATUM) newattname;
	attsdesc = heap_beginscan(attrdesc, 0, NowTimeQual, 2, key);
	newatttup = heap_getnext(attsdesc, 0, (Buffer *) NULL);
	if (PointerIsValid(newatttup)) {
		pfree((char *) oldatttup);
		heap_endscan(attsdesc);
		heap_close(attrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renameatt: attribute \"%s\" exists",
		     newattname);
		return;
	}
	heap_endscan(attsdesc);

	bcopy(newattname,
	      (char *) (((struct attribute *) GETSTRUCT(oldatttup))->attname),
	      sizeof(NameData));
	oldTID = oldatttup->t_ctid;
	heap_replace(attrdesc, &oldTID, oldatttup); /* insert "fixed" tuple */
	heap_close(attrdesc);
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
	extern		rename();
	
	if (issystem(oldrelname)) {
		elog(WARN, "renamerel: system relation \"%s\" not renamed",
		     oldrelname);
		return;
	}
	relrdesc = heap_openr(RelationRelationName);

	ScanKeyEntryInitialize((ScanKeyEntry)&key, NULL, RelationNameAttributeNumber,
	                       Character16EqualRegProcedure, (Datum) oldrelname);

	oldsdesc = heap_beginscan(relrdesc, 0, NowTimeQual, 1, &key);
	oldreltup = heap_getnext(oldsdesc, 0, (Buffer *) NULL);
	if (!PointerIsValid(oldreltup)) {
		heap_endscan(oldsdesc);
		heap_close(relrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renamerel: relation \"%s\" does not exist",
		     oldrelname);
	}
	oldreltup = palloctup(oldreltup, InvalidBuffer, relrdesc);

	key.sk_data = (DATUM) newrelname;
	newsdesc = heap_beginscan(relrdesc, 0, NowTimeQual, 1, &key);
	newreltup = heap_getnext(newsdesc, 0, (Buffer *) NULL);
	if (PointerIsValid(newreltup)) {
		pfree((char *) oldreltup);
		heap_endscan(newsdesc);
		heap_endscan(oldsdesc);
		heap_close(relrdesc);	/* XXX should be unneeded eventually */
		elog(WARN, "renamerel: relation \"%s\" exists",
		     newrelname);
	}
	heap_endscan(newsdesc);
	bcopy(newrelname,
	      (char *) (((struct relation *) GETSTRUCT(oldreltup))->relname),
	      sizeof(NameData));
	oldTID = oldreltup->t_ctid;
	heap_replace(relrdesc, &oldTID, oldreltup); /* insert "fixed" tuple */
	pfree((char *) oldreltup);
	heap_endscan(oldsdesc);
	heap_close(relrdesc);
	(void) strcpy(oldpath, relpath(oldrelname));
	(void) strcpy(newpath, relpath(newrelname));
	if (rename(oldpath, newpath) < 0)
		elog(WARN, "renamerel: unable to rename file: %m");
}
