/*
 * purge.c --
 *	the POSTGRES purge command.
 *
 * Note:
 *	XXX There are many instances of int32 instead of ...Time.  These
 *	should be changed once it is decided the signed'ness will be.
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/tqual.h"	/* for NowTimeQual */
#include "catalog/catname.h"
#include "catalog/indexing.h"
#include "fmgr.h"
#include "utils/log.h"
#include "utils/nabstime.h"

#include "catalog/pg_relation.h"

static char	cmdname[] = "RelationPurge";

#define	RELATIVE	01
#define	ABSOLUTE	02

int32
RelationPurge(relationName, absoluteTimeString, relativeTimeString)
	Name	relationName;
	char	*absoluteTimeString, *relativeTimeString;
{
	register		i;
	AbsoluteTime		absoluteTime = INVALID_ABSTIME;
	RelativeTime		relativeTime = INVALID_RELTIME;
	bits8			dateTag;
	Relation		relation;
	HeapScanDesc		scan;
	static ScanKeyEntryData	key[1] = {
		{ 0, RelationNameAttributeNumber, F_CHAR16EQ }
	};
	Buffer			buffer;
	HeapTuple		newTuple, oldTuple;
	AbsoluteTime		currentTime;
	char			*values[Natts_pg_relation];
	char			nulls[Natts_pg_relation];
	char			replace[Natts_pg_relation];
	Relation		idescs[Num_pg_class_indices];

	Assert(NameIsValid(relationName));

	/*
	 * XXX for some reason getmyrelids (in inval.c) barfs when
	 * you heap_replace tuples from these classes.  i thought
	 * setheapoverride would fix it but it didn't.  for now,
	 * just disallow purge on these classes.
	 */
	if (NameIsEqual(RelationRelationName, relationName) ||
	    NameIsEqual(AttributeRelationName, relationName) ||
	    NameIsEqual(AccessMethodRelationName, relationName) ||
	    NameIsEqual(AccessMethodOperatorRelationName, relationName)) {
		elog(WARN, "%s: cannot purge catalog \"%.16s\"",
		     cmdname, relationName->data);
	}

	if (PointerIsValid(absoluteTimeString)) {
		absoluteTime = (int32) nabstimein(absoluteTimeString);
		absoluteTimeString[0] = '\0';
		if (absoluteTime == INVALID_ABSTIME) {
			elog(NOTICE, "%s: bad absolute time string \"%s\"",
			     cmdname, absoluteTimeString);
			elog(WARN, "purge not executed");
		}
	}

#ifdef	PURGEDEBUG
		elog(DEBUG, "%s: absolute time `%s' is %d.",
		    cmdname, absoluteTimeString, absoluteTime);
#endif	/* defined(PURGEDEBUG) */

	if (PointerIsValid(relativeTimeString)) {
		if (isreltime(relativeTimeString, NULL, NULL, NULL) != 1) {
			elog(WARN, "%s: bad relative time string \"%s\"",
			     cmdname, relativeTimeString);
		}
		relativeTime = reltimein(relativeTimeString);

#ifdef	PURGEDEBUG
		elog(DEBUG, "%s: relative time `%s' is %d.",
		    cmdname, relativeTimeString, relativeTime);
#endif	/* defined(PURGEDEBUG) */
	}

	/*
	 * Find the RELATION relation tuple for the given relation.
	 */
	relation = heap_openr(RelationRelationName->data);
	key[0].argument = NameGetDatum(relationName);
	fmgr_info(key[0].procedure, (func_ptr *) &key[0].func, &key[0].nargs);

	scan = heap_beginscan(relation, 0, NowTimeQual, 1, (ScanKey) key);
	oldTuple = heap_getnext(scan, 0, &buffer);
	if (!HeapTupleIsValid(oldTuple)) {
		heap_endscan(scan);
		heap_close(relation);
		elog(WARN, "%s: no such relation: %s", cmdname, relationName);
		return(0);
	}

	/*
	 * Dig around in the tuple.
	 */
	currentTime = GetCurrentTransactionStartTime();
	if (!RelativeTimeIsValid(relativeTime)) {
		dateTag = ABSOLUTE;
		if (!AbsoluteTimeIsValid(absoluteTime))
			absoluteTime = currentTime;
	} else if (!AbsoluteTimeIsValid(absoluteTime))
		dateTag = RELATIVE;
	else
		dateTag = ABSOLUTE | RELATIVE;

/* This check is unneccesary - see comment on assertConsistentTimes */
#if 0
	if (!assertConsistentTimes(absoluteTime, relativeTime, currentTime,
				   dateTag, oldTuple)) {
		heap_endscan(scan);
		heap_close(relation);
		elog(WARN, "%s: inconsistent times", cmdname);
		return(0);
	}
#endif

	for (i = 0; i < Natts_pg_relation; ++i) {
		nulls[i] = heap_attisnull(oldTuple, i+1) ? 'n' : ' ';
		values[i] = NULL;
		replace[i] = ' ';
	}
	if (dateTag & ABSOLUTE) {
		values[RelationExpiresAttributeNumber-1] =
			(char *) UInt32GetDatum(absoluteTime);
		replace[RelationExpiresAttributeNumber-1] = 'r';
	}
	if (dateTag & RELATIVE) {
		values[RelationPreservedAttributeNumber-1] =
			(char *) UInt32GetDatum(relativeTime);
		replace[RelationPreservedAttributeNumber-1] = 'r';
	}

	/*
	 * Change the RELATION relation tuple for the given relation.
	 */
	newTuple = heap_modifytuple(oldTuple, buffer, relation, values,
				    nulls, replace);
	
	/* XXX What do you do with a RuleLock?? */
	/* XXX How do you detect an insertion error?? */
	heap_replace(relation, &newTuple->t_ctid, newTuple);

	/* keep the system catalog indices current */
	CatalogOpenIndices(Num_pg_class_indices, Name_pg_class_indices, idescs);
	CatalogIndexInsert(idescs, Num_pg_class_indices, relation, newTuple);
	CatalogCloseIndices(Num_pg_class_indices, idescs);

	pfree(newTuple);
	
	heap_endscan(scan);
	heap_close(relation);
	return(1);
}

/*
 * This routine doesn't make any sense, as purge only sets variables 
 * referenced by vacuum.  After vacuuming the relation will contain tuples 
 * that existed from a specific point in time onwards.  It is entirely 
 * possible that any tuples that are being discarded were already discarded
 * during a previous vacuum, but that has no ill effects and we do not need
 * to check for it.
 */
assertConsistentTimes(absoluteTime, relativeTime, currentTime, dateTag, tuple)
	AbsoluteTime    absoluteTime, currentTime;
        RelativeTime	relativeTime;
	bits8		dateTag;
	HeapTuple	tuple;
{
	AbsoluteTime	currentExpires;
	RelativeTime	currentPreserved;
	bool	absError = false, relError = false;

	currentExpires =
		((RelationTupleForm) GETSTRUCT(tuple))->relexpires;
	currentPreserved =
		((RelationTupleForm) GETSTRUCT(tuple))->relpreserved;
	if (dateTag & ABSOLUTE) {
		absError = true;
		if (AbsoluteTimeIsValid(currentExpires) &&
		    AbsoluteTimeIsBefore(absoluteTime, currentExpires)) {
			elog(NOTICE,
			     "%s: expiration %s before current expiration %d",
			     cmdname, absoluteTime, currentExpires);
		} else if (RelativeTimeIsValid(currentPreserved) &&
				AbsoluteTimeIsBefore(absoluteTime,
					currentTime + currentPreserved)) {
			elog(NOTICE,
			     "%s: expiration %d before current relative %d",
			     cmdname, absoluteTime, currentPreserved);
		} else if (AbsoluteTimeIsAfter(absoluteTime, currentTime)) {
			elog(NOTICE,
			     "%s: expiration %d after current time %d",
			     cmdname, absoluteTime, currentTime);
		} else
			absError = false;
	}
	if (dateTag & RELATIVE) {
		relError = true;
		if (AbsoluteTimeIsValid(currentExpires) &&
				AbsoluteTimeIsBefore(currentTime + relativeTime,
					currentExpires)) {
			elog(NOTICE,
			     "%s: relative %d before current expiration %d",
			     cmdname, relativeTime, currentExpires);
		} else if (RelativeTimeIsValid(currentPreserved) &&
				AbsoluteTimeIsBefore(relativeTime, currentPreserved)) {
		        /* ???  ^^^^^^^^ ----------- ^^^^^^^^ ??? */

			/*
			 * XXX handle this by modifying both relative and
			 * current times.
			 */
			elog(NOTICE,
			     "%s: relative %d before current relative %d",
			     cmdname, relativeTime, currentPreserved);
		} else
			relError = false;
	}
	/* XXX there must be cases when both are set and they interact... */
	return(!(absError || relError));
}
