/* ----------------------------------------------------------------
 *   FILE
 *	scan.c
 *	
 *   DESCRIPTION
 *	scan direction and key code
 *
 *   INTERFACE ROUTINES
 *	
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"
#include "access/sdir.h"
#include "access/attnum.h"
#include "access/skey.h"

RcsId("$Header$");

/*
 *	ScanDirectionIsValid, ScanDirectionIsBackward
 *	ScanDirectionIsNoMovement, ScanDirectionIsForward
 *		.. are now macros in sdir.h
 *
 *	ScanKeyIsValid, ScanKeyEntryIsValid, ScanKeyEntryIsLegal
 *		.. are now macros in skey.h -cim 4/27/91
 */

/* ----------------
 *	ScanKeyEntrySetIllegal
 * ----------------
 */
void
ScanKeyEntrySetIllegal(entry)
    ScanKeyEntry	entry;
{
    Assert(ScanKeyEntryIsValid(entry));

    entry->flags = 0;	/* just in case... */
    entry->attributeNumber = InvalidAttributeNumber;
    entry->procedure = 0;	/* should be InvalidRegProcedure */
}

/* ----------------
 *	ScanKeyEntryInitialize
 * ----------------
 */
void
ScanKeyEntryInitialize(entry, flags, attributeNumber, procedure, argument)
    ScanKeyEntry	entry;
    bits16		flags;
    AttributeNumber	attributeNumber;
    RegProcedure	procedure;
    Datum		argument;
{
    Assert(ScanKeyEntryIsValid(entry));

    entry->flags = flags;
    entry->attributeNumber = attributeNumber;
    entry->procedure = procedure;
    entry->argument = argument;
	fmgr_info(procedure, &entry->func, &entry->nargs);

    Assert(ScanKeyEntryIsLegal(entry));
}
