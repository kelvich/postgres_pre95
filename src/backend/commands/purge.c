
/*
 * 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1988 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720 provided only that the the requestor give the University
 * of California a free licence to any derived software for educational
 * and research purposes.  The University of California makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 */



/*
 * purge.c --
 *	the POSTGRES purge command.
 *
 * Note:
 *	XXX There are many instances of int32 instead of ...Time.  These
 *	should be changed once it is decided the signed'ness will be.
 */

#include "anum.h"
#include "catname.h"
#include "fmgr.h"
#include "ftup.h"
#include "heapam.h"
#include "log.h"

RcsId("$Header$");

static char	cmdname[] = "RelationPurge";

#define	RELATIVE	01
#define	ABSOLUTE	02

int32
RelationPurge(relationName, absoluteTimeString, relativeTimeString)
	Name	relationName;
	char	*absoluteTimeString, *relativeTimeString;
{
	register		i;
	int32 /* XXX */		absoluteTime = 0, relativeTime = 0;
	bits8			dateTag;
	Relation		relation;
	HeapScan		scan;
	static ScanKeyEntryData	key[1] = {
		{ 0, RelationNameAttributeNumber, F_CHAR16EQ }
	};
	Buffer			buffer;
	HeapTuple		newTuple, oldTuple;
	Time			currentTime;
	char			*values[RelationRelationNumberOfAttributes];
	char			nulls[RelationRelationNumberOfAttributes];
	char			replace[RelationRelationNumberOfAttributes];

	Assert(NameIsValid(relationName));
	/* XXX check: pg_user.usecatupd permission? */

	if (PointerIsValid(absoluteTimeString)) {
		/*
		if (!isabstime(absoluteTimeString))
			elog(WARN, "%s: bad absolute time string %s",
			     cmdname, absoluteTimeString);
		*/
		absoluteTime = abstimein(absoluteTimeString);
		
#ifdef	PURGEDEBUG
		elog(DEBUG, "RelationPurge: absolute time `%s' is %d.",
		    absoluteTimeString, absoluteTime);
#endif	/* defined(PURGEDEBUG) */
	}

	if (PointerIsValid(relativeTimeString)) {
		/*
		if (!isreltime(relativeTimeString))
			elog(WARN, "%s: bad relative time string %s",
			     cmdname, relativeTimeString);
		*/
		relativeTime = reltimein(relativeTimeString);
		
#ifdef	PURGEDEBUG
		elog(DEBUG, "RelationPurge: relative time `%s' is %d.",
		    relativeTimeString, relativeTime);
#endif	/* defined(PURGEDEBUG) */
	}

	/*
	 * Find the RELATION relation tuple for the given relation.
	 */
	relation = RelationNameOpenHeapRelation(RelationRelationName->data);
	key[0].argument.name.value = relationName;
	scan = RelationBeginHeapScan(relation, 0, DefaultTimeRange, 1, 
				     (ScanKey) key);
	oldTuple = HeapScanGetNextTuple(scan, 0, &buffer);
	if (!HeapTupleIsValid(oldTuple)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
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
		dateTag = ABSOLUTE & RELATIVE;
	if (!assertConsistentTimes(absoluteTime, relativeTime, currentTime,
				   dateTag, oldTuple)) {
		HeapScanEnd(scan);
		RelationCloseHeapRelation(relation);
		elog(WARN, "%s: inconsistent times: abs=%d rel=%d time=%d",
		     cmdname, absoluteTime, relativeTime, currentTime);
		return(0);
	}
	for (i = 0; i < RelationRelationNumberOfAttributes; ++i) {
		nulls[i] = ' ';
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
	newTuple = ModifyHeapTuple(oldTuple, buffer, relation, values,
				   nulls, replace);
	/* XXX What do you do with a RuleLock?? */
	/* XXX How do you detect an insertion error?? */
	(void) RelationReplaceHeapTuple(relation, &newTuple->t_ctid, newTuple);
	pfree(newTuple);
	HeapScanEnd(scan);
	RelationCloseHeapRelation(relation);
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
			elog(DEBUG,
			     "%s: expiration %d before current expiration %d",
			     cmdname, absoluteTime, currentExpires);
		} else if (TimeIsValid(currentPreserved) &&
				TimeIsBefore(absoluteTime,
					currentTime + currentPreserved)) {
			elog(DEBUG,
			     "%s: expiration %d before current relative %d",
			     cmdname, absoluteTime, currentPreserved);
		} else if (TimeIsBefore(currentTime, absoluteTime)) {
			elog(DEBUG,
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
			elog(DEBUG,
			     "%s: relative %d before current expiration %d",
			     cmdname, relativeTime, currentExpires);
		} else if (TimeIsValid(currentPreserved) &&
				TimeIsBefore(relativeTime, currentPreserved)) {
			/*
			 * XXX handle this by modifying both relative and
			 * current times.
			 */
			elog(DEBUG,
			     "%s: relative %d before current relative %d",
			     cmdname, relativeTime, currentPreserved);
		} else
			relError = false;
	}
	/* XXX there must be cases when both are set and they interact... */
	return(!(absError || relError));
}
