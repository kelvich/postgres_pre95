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
#include "catalog/indexing.h"

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

#include "planner/internal.h"
#include "planner/prepunion.h"	/* for find_all_inheritors */

#ifndef NO_SECURITY
#include "tmp/acl.h"
#include "catalog/syscache.h"
#endif /* !NO_SECURITY */

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

renameatt(relname, oldattname, newattname, userName, recurse)
	char	relname[], oldattname[], newattname[];
	Name	userName;
	int	recurse;
{
	Relation	relrdesc, attrdesc;
	HeapTuple	reltup, oldatttup, newatttup;
	ItemPointerData	oldTID;
	Relation	idescs[Num_pg_attr_indices];

	/*
	 * permissions checking.  this would normally be done in utility.c,
	 * but this particular routine is recursive.
	 *
	 * normally, only the owner of a class can change its schema.
	 */
	if (NameIsSystemRelationName((Name) relname))
	    elog(WARN, "renameatt: class \"%-.*s\" is a system catalog",
		 NAMEDATALEN, relname);
#ifndef NO_SECURITY
	if (!IsBootstrapProcessingMode() &&
	    !pg_ownercheck(userName->data, relname, RELNAME))
	    elog(WARN, "renameatt: you do not own class \"%-.*s\"",
		 NAMEDATALEN, relname);
#endif

	/*
	 * if the 'recurse' flag is set then we are supposed to rename this
	 * attribute in all classes that inherit from 'relname' (as well as
	 * in 'relname').
	 *
	 * any permissions or problems with duplicate attributes will cause
	 * the whole transaction to abort, which is what we want -- all or
	 * nothing.
	 */
	if (recurse) {
	    ObjectId myrelid, childrelid;
	    LispValue child, children;
	    
	    relrdesc = heap_openr(relname);
	    if (!RelationIsValid(relrdesc)) {
		elog(WARN, "renameatt: unknown relation: \"%-.*s\"",
		     NAMEDATALEN, relname);
	    }
	    myrelid = relrdesc->rd_id;
	    heap_close(relrdesc);

   	    /* this routine is actually in the planner */
	    children = find_all_inheritors(lispCons(lispInteger(myrelid),
						    LispNil),
					   LispNil);
	    
	    /*
	     * find_all_inheritors does the recursive search of the
	     * inheritance hierarchy, so all we have to do is process
	     * all of the relids in the list that it returns.
	     */
	    foreach (child, children) {
		NameData childname;
		
		childrelid = CInteger(CAR(child));
		if (childrelid == myrelid)
		    continue;
		relrdesc = heap_open(childrelid);
		if (!RelationIsValid(relrdesc)) {
		    elog(WARN, "renameatt: can't find catalog entry for inheriting class with oid %d",
			 childrelid);
		}
		namecpy(&childname, &(relrdesc->rd_rel->relname));
		heap_close(relrdesc);
		renameatt(&childname, oldattname, newattname,
			  userName, 0);	/* no more recursion! */
	    }
	}

	relrdesc = heap_openr(RelationRelationName);
	reltup = ClassNameIndexScan(relrdesc, relname);
	if (!PointerIsValid(reltup)) {
		heap_close(relrdesc);
		elog(WARN, "renameatt: relation \"%-.*s\" nonexistent",
		     NAMEDATALEN, relname);
		return;
	}
	heap_close(relrdesc);

	attrdesc = heap_openr(AttributeRelationName);
	oldatttup = AttributeNameIndexScan(attrdesc, reltup->t_oid, oldattname);
	if (!PointerIsValid(oldatttup)) {
		heap_close(attrdesc);
		elog(WARN, "renameatt: attribute \"%-.*s\" nonexistent",
		     NAMEDATALEN, oldattname);
	}
	if (((struct attribute *) GETSTRUCT(oldatttup))->attnum < 0) {
		elog(WARN, "renameatt: system attribute \"%-.*s\" not renamed",
		     NAMEDATALEN, oldattname);
	}

	newatttup = AttributeNameIndexScan(attrdesc, reltup->t_oid, newattname);
	if (PointerIsValid(newatttup)) {
		pfree((char *) oldatttup);
		heap_close(attrdesc);
		elog(WARN, "renameatt: attribute \"%-.*s\" exists", 
		     NAMEDATALEN, newattname);
	}

	bcopy(newattname,
	      (char *) (((struct attribute *) GETSTRUCT(oldatttup))->attname),
	      NAMEDATALEN);
	oldTID = oldatttup->t_ctid;

	/* insert "fixed" tuple */
	heap_replace(attrdesc, &oldTID, oldatttup);

	/* keep system catalog indices current */
	CatalogOpenIndices(Num_pg_attr_indices, Name_pg_attr_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_attr_indices, attrdesc, oldatttup);
	CatalogCloseIndices(Num_pg_attr_indices, idescs);

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
	HeapTuple	oldreltup, newreltup;
	ItemPointerData	oldTID;
	char		oldpath[MAXPGPATH], newpath[MAXPGPATH];
	Relation	idescs[Num_pg_class_indices];
	int		issystem();
	char		*relpath();
	extern		rename();
	
	if (issystem(oldrelname)) {
		elog(WARN, "renamerel: system relation \"%-.*s\" not renamed",
		     NAMEDATALEN, oldrelname);
		return;
	}
	if (issystem(newrelname)) {
		elog(WARN, "renamerel: Illegal class name: \"%-.*s\" -- pg_ is reserved for system catalogs",
		     NAMEDATALEN, newrelname);
		return;
	}

	relrdesc = heap_openr(RelationRelationName);
	oldreltup = ClassNameIndexScan(relrdesc, oldrelname);

	if (!PointerIsValid(oldreltup)) {
		heap_close(relrdesc);
		elog(WARN, "renamerel: relation \"%-.*s\" does not exist",
		     NAMEDATALEN, oldrelname);
	}

	newreltup = ClassNameIndexScan(relrdesc, newrelname);
	if (PointerIsValid(newreltup)) {
		pfree((char *) oldreltup);
		heap_close(relrdesc);
		elog(WARN, "renamerel: relation \"%-.*s\" exists", 
		     NAMEDATALEN, newrelname);
	}

	/* rename the directory first, so if this fails the rename's not done */
	(void) strcpy(oldpath, relpath(oldrelname));
	(void) strcpy(newpath, relpath(newrelname));
	if (rename(oldpath, newpath) < 0)
		elog(WARN, "renamerel: unable to rename file: %m");

	bcopy(newrelname,
	      (char *) (((struct relation *) GETSTRUCT(oldreltup))->relname),
	      NAMEDATALEN);
	oldTID = oldreltup->t_ctid;

	/* insert fixed rel tuple */
	heap_replace(relrdesc, &oldTID, oldreltup);

	/* keep the system catalog indices current */
	CatalogOpenIndices(Num_pg_class_indices, Name_pg_class_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_class_indices, relrdesc, oldreltup);
	CatalogCloseIndices(Num_pg_class_indices, idescs);

	pfree((char *) oldreltup);
	heap_close(relrdesc);
}
