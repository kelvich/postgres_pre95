
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
 * pagenum.h --
 *	POSTGRES page number definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	PageNumIncluded	/* Include this file only once. */
#define PageNumIncluded	1

#include "c.h"

#include "part.h"

typedef uint16	PageNumber;

#define FirstPageNumber	0	/* page nubmer for the first page of a block */

typedef uint32	LogicalPageNumber;

#define InvalidLogicalPageNumber	0

/*
 * PageNumberIsValid --
 *	True iff the page number is valid.
 */
extern
bool
PageNumberIsValid ARGS((
	PageNumber	pageNumber,
	PagePartition	pagePartition
));

/*
 * LogicalPageNumberIsValid --
 *	True iff the logical page number is valid.
 */
extern
bool
PageNumberIsValid ARGS((
	PageNumber	pageNumber,
	PagePartition	pagePartition
));

#endif	/* !defined(PageNumIncluded) */
