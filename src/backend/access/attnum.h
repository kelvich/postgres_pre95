
; /*
; * 
; * POSTGRES Data Base Management System
; * 
; * Copyright (c) 1988 Regents of the University of California
; * 
; * Permission to use, copy, modify, and distribute this software and its
; * documentation for educational, research, and non-profit purposes and
; * without fee is hereby granted, provided that the above copyright
; * notice appear in all copies and that both that copyright notice and
; * this permission notice appear in supporting documentation, and that
; * the name of the University of California not be used in advertising
; * or publicity pertaining to distribution of the software without
; * specific, written prior permission.  Permission to incorporate this
; * software into commercial products can be obtained from the Campus
; * Software Office, 295 Evans Hall, University of California, Berkeley,
; * Ca., 94720 provided only that the the requestor give the University
; * of California a free licence to any derived software for educational
; * and research purposes.  The University of California makes no
; * representations about the suitability of this software for any
; * purpose.  It is provided "as is" without express or implied warranty.
; * 
; */



/*
 * attnum.h --
 *	POSTGRES attribute number definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	AttNumIncluded	/* Include this file only once. */
#define AttNumIncluded	1

#include "c.h"

typedef int16	AttributeNumber;

#define InvalidAttributeNumber	0

typedef uint16	AttributeOffset;

/*
 * AttributeNumberIsValid --
 *	True iff the attribute number is valid.
 */
extern
bool
AttributeNumberIsValid ARGS((
	AttributeNumber	attributeNumber
));

/*
 * AttributeNumberIsForUserDefinedAttribute --
 *	True iff the attribute number corresponds to an user defined attribute.
 */
extern
bool
AttributeNumberIsForUserDefinedAttribute ARGS((
	AttributeNumber	attributeNumber
));

/*
 * AttributeNumberIsInBounds --
 *	True iff attribute number is within given bounds.
 *
 * Note:
 *	Assumes AttributeNumber is an signed type.
 *	Assumes the bounded interval to be (minumum,maximum].
 *	An invalid attribute number is within given bounds.
 */
extern
bool
AttributeNumberIsInBounds ARGS((
	AttributeNumber	attributeNumber,
	AttributeNumber	minimumAttributeNumber,
	AttributeNumber	maximumAttributeNumber
));

/*
 * AttributeNumberGetAttributeOffset --
 *	Returns the attribute offset for an attribute number.
 *
 * Note:
 *	Assumes the attribute number is for an user defined attribute.
 */
extern
AttributeOffset
AttributeNumberGetAttributeOffset ARGS((
	AttributeNumber	attributeNumber
));

/*
 * AttributeOffsetGetAttributeNumber --
 *	Returns the attribute number for an attribute offset.
 */
extern
AttributeNumber
AttributeOffsetGetAttributeNumber ARGS((
	AttributeOffset	attributeOffset
));

#endif	/* !defined(AttNumIncluded) */
