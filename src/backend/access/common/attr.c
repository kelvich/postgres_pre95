/* ----------------------------------------------------------------
 *   FILE
 *	attr.c
 *	
 *   DESCRIPTION
 *	postgres attribute predicates
 *
 *   INTERFACE ROUTINES
 *	AttributeIsValid
 *	AttributeNumberIsValid
 *	AttributeNumberIsForUserDefinedAttribute
 *	AttributeNumberIsInBounds
 *	AttributeNumberGetAttributeOffset
 *	AttributeOffsetGetAttributeNumber
 *	
 *   NOTES
 *	this file contains the old att.c and attnum.c stuff
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
#include "tmp/c.h"

#include "access/att.h"
#include "access/attnum.h"
#include "access/tupdesc.h"
#include "access/attval.h"

#include "utils/memutils.h"
#include "utils/log.h"

RcsId("$Header$");

/* ----------------
 *	AttributeNumberIsForUserDefinedAttribute
 * ----------------
 */
bool
AttributeNumberIsForUserDefinedAttribute(attributeNumber)
    AttributeNumber	attributeNumber;
{
    return (bool)
	(attributeNumber > 0);
}

/* ----------------
 *	AttributeNumberIsInBounds
 * ----------------
 */
bool
AttributeNumberIsInBounds(attributeNumber,
			  minimumAttributeNumber,
			  maximumAttributeNumber)
    
    AttributeNumber	attributeNumber;
    AttributeNumber	minimumAttributeNumber;
    AttributeNumber	maximumAttributeNumber;
{
    return (bool)
	OffsetIsInBounds(attributeNumber,
			 minimumAttributeNumber,
			 maximumAttributeNumber);
}

/* ----------------
 *	AttributeNumberGetAttributeOffset
 * ----------------
 */
AttributeOffset
AttributeNumberGetAttributeOffset(attributeNumber)
    AttributeNumber	attributeNumber;
{
    Assert(AttributeNumberIsForUserDefinedAttribute(attributeNumber)==true);
    return (attributeNumber - 1);
}

/* ----------------
 *	AttributeOffsetGetAttributeNumber
 * ----------------
 */
AttributeNumber
AttributeOffsetGetAttributeNumber(attributeOffset)
    AttributeOffset	attributeOffset;
{
    return (1 + attributeOffset);
}

