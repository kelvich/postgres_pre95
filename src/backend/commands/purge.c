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
#include "utils/fmgr.h"
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
	AbsoluteTime		absoluteTime = InvalidTime;
	RelativeTime		relativeTime = InvalidTime;
	bits8			dateTag;
	Relation		relation;
	HeapScanDesc		scan;
	static ScanKeyEntryData	key[1] = {
		{ 0, RelationNameAttributeNumber, F_CHAR16EQ }
	};
	Buffer			buffer;
	HeapTuple		newTuple, oldTuple;
	Time			currentTime;
	char			*values[Natts_pg_relation];
	char			nulls[Natts_pg_relation];
	char			replace[Natts_pg_relation];

	Assert(NameIsValid(relationName));

	if (PointerIsValid(absoluteTimeString)) {
		if (!isabstime(absoluteTimeString, NULL)) {
			elog(NOTICE, "%s: bad absolute time string \"%s\"",
			     cmdname, absoluteTimeString);
			absoluteTimeString[0] = '\0';
			elog(WARN, "purge not executed");
		}
		absoluteTime = (int32) nabstimein(absoluteTimeString);
		absoluteTimeString[0] = '\0';
	}

#ifdef	PURGEDEBUG
		elog(DEBUG, "RelationPurge: absolute time `%s' is %d.",
		    absoluteTimeString, absoluteTime);
#endif	/* defined(PURGEDEBUG) */

	if (PointerIsValid(relativeTimeString)) {
		if (isreltime(relativeTimeString, NULL, NULL, NULL) != 1) {
			elog(WARN, "%s: bad relative time string \"%s\"",
			     cmdname, relativeTimeString);
		}
		relativeTime = reltimein(relativeTimeString);

#ifdef	PURGEDEBUG
		elog(DEBUG, "RelationPurge: relative time `%s' is %d.",
		    relativeTimeString, relativeTime);
#endif	/* defined(PURGEDEBUG) */
	}

	/*
	 * Find the RELATION relation tuple for the given relation.
	 */
	relation = heap_openr(RelationRelationName->data);
	key[0].argument = NameGetDatum(relationName);
	fmgr_info(key[0].procedure, &key[0].func, &key[0].nargs);

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
	if (!TimeIsValid(relativeTime)) {
		dateTag = ABSOLUTE;
		if (!TimeIsValid(absoluteTime))
			absoluteTime = currentTime;
	} else if (!TimeIsValid(absoluteTime))
		dateTag = RELATIVE;
	else
		dateTag = ABSOLUTE | RELATIVE;
	if (!assertConsistentTimes(absoluteTime, relativeTime, currentTime,
				   dateTag, oldTuple)) {
		heap_endscan(scan);
		heap_close(relation);
		elog(WARN, "%s: inconsistent times", cmdname);
		return(0);
	}
	for (i = 0; i < Natts_pg_relation; ++i) {
		nulls[i] = heap_attisnull(oldTuple, i+1) ? 'n' : ' ';
		values[i] = NULL;
		replace[i] = ' ';
	}
	if (dateTag & ABSOLUTE) {
		values[RelationExpiresAttributeNumber-1] =
			(char *) absoluteTime;
		replace[RelationExpiresAttributeNumber-1] = 'r';
	}
	if (dateTag & RELATIVE) {
		values[RelationPreservedAttributeNumber-1] =
			(char *) relativeTime;
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
	pfree(newTuple);
	
	heap_endscan(scan);
	heap_close(relation);
	return(1);
}


assertConsistentTimes(absoluteTime, relativeTime, currentTime, dateTag, tuple)
	Time		absoluteTime, relativeTime, currentTime;
	bits8		dateTag;
	HeapTuple	tuple;
{
	Time	currentExpires, currentPreserved;
	bool	absError = false, relError = false;

	currentExpires =
		((RelationTupleForm) GETSTRUCT(tuple))->relexpires;
	currentPreserved =
		((RelationTupleForm) GETSTRUCT(tuple))->relpreserved;
	if (dateTag & ABSOLUTE) {
		absError = true;
		if (TimeIsValid(currentExpires) &&
		    TimeIsBefore(absoluteTime, currentExpires)) {
			elog(NOTICE,
			     "%s: expiration %s before current expiration %d",
			     cmdname, absoluteTime, currentExpires);
		} else if (TimeIsValid(currentPreserved) &&
				TimeIsBefore(absoluteTime,
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
		if (TimeIsValid(currentExpires) &&
				TimeIsBefore(currentTime + relativeTime,
					currentExpires)) {
			elog(NOTICE,
			     "%s: relative %d before current expiration %d",
			     cmdname, relativeTime, currentExpires);
		} else if (TimeIsValid(currentPreserved) &&
				TimeIsBefore(relativeTime, currentPreserved)) {
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
