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

/* ----------------
 *	ScanDirectionIsValid
 * ----------------
 */
bool
ScanDirectionIsValid(direction)
    ScanDirection	direction;
{
    return (bool)
	(AsInt8(BackwardScanDirection) <= AsInt8(direction) &&
	 AsInt8(direction) <= AsInt8(ForwardScanDirection));
}

/* ----------------
 *	ScanDirectionIsBackward
 * ----------------
 */
bool
ScanDirectionIsBackward(direction)
    ScanDirection	direction;
{
    Assert(ScanDirectionIsValid(direction));

    return (bool)
	(AsInt8(direction) == AsInt8(BackwardScanDirection));
}

/* ----------------
 *	ScanDirectionIsNoMovement
 * ----------------
 */
bool
ScanDirectionIsNoMovement(direction)
    ScanDirection	direction;
{
    Assert(ScanDirectionIsValid(direction));

    return (bool)
	(AsInt8(direction) == AsInt8(NoMovementScanDirection));
}

/* ----------------
 *	ScanDirectionIsForward
 * ----------------
 */
bool
ScanDirectionIsForward(direction)
    ScanDirection	direction;
{
    Assert(ScanDirectionIsValid(direction));

    return (bool)
	(AsInt8(direction) == AsInt8(ForwardScanDirection));
}

/* ----------------
 *	ScanKeyIsValid
 * ----------------
 */
bool
ScanKeyIsValid(scanKeySize, key)
    ScanKeySize	scanKeySize;
    ScanKey		key;
{
    return (bool)
	(scanKeySize == 0 || PointerIsValid(key));
}

/* ----------------
 *	ScanKeyEntryIsValid
 * ----------------
 */
bool
ScanKeyEntryIsValid(entry)
    ScanKeyEntry	entry;
{
    return (bool) PointerIsValid(entry);
}

/* ----------------
 *	ScanKeyEntryIsLegal
 * ----------------
 */
bool
ScanKeyEntryIsLegal(entry)
    ScanKeyEntry	entry;
{
    bool	legality;

    Assert(ScanKeyEntryIsValid(entry));
    legality = AttributeNumberIsValid(entry->attributeNumber);
    return (legality);
}

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

    Assert(ScanKeyEntryIsLegal(entry));
}
