
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
 * buf.h --
 *	Basic buffer manager data types.
 *
 * Identification:
 *	$Header$
 */

#ifndef	BufIncluded	/* Include this file only once. */
#define BufIncluded	1

#include "c.h"

#include "block.h"

#define InvalidBuffer	(-1)
#define UnknownBuffer	(-2)

typedef int	Buffer;

/*
 * RelationGetNumberOfBlocks --
 *	Returns the buffer descriptor associated with a page in a relation.
 */
extern
BlockNumber
RelationGetNumberOfBlocks ARGS((
	const Relation	relation,
));

/*
 * BufferIsValid --
 *	True iff the buffer is valid.
 * Note:
 *	BufferIsValid(InvalidBuffer) is False.
 *	BufferIsValid(UnknownBuffer) is False.
 */
extern
bool
BufferIsValid ARGS ((
	Buffer	buffer
));

/*
 * BufferIsInvalid --
 *	True iff the buffer is invalid.
 *
 * Note:
 *	BufferIsInvalid(UnknownBuffer) is False.
 */
extern
bool
BufferIsInvalid ARGS ((
	Buffer	buffer
));

/*
 * BufferIsUnknown --
 *	True iff the buffer is unknown.
 *
 * Note:
 *	BufferIsUnknown(InvalidBuffer) is False.
 */
extern
bool
BufferIsUnknown ARGS ((
	Buffer	buffer
));

#endif	/* !defined(BufIncluded) */
