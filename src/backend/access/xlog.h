
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
 * xlog.h --
 *	POSTGRES transaction log definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	XLogIncluded	/* Include this file only once. */
#define XLogIncluded	1

#include "c.h"

#include "xid.h"

/*
 * InitializeTransactionLog --
 *	Initializes transaction logging.
 */
extern
void
InitializeTransactionLog ARGS((
	void
));

/*
 * TransactionIdDidCommit --
 *	True iff transaction associated with the identifier did commit.
 *
 * Note:
 *	Assumes transaction identifier is valid.
 */
extern
bool
TransactionIdDidCommit ARGS((
	TransactionId	transactionId
));

/*
 * TransactionIdDidAborted --
 *	True iff transaction associated with the identifier did abort.
 *
 * Note:
 *	Assumes transaction identifier is valid.
 *	XXX Is this unneeded?
 */
extern
bool
TransactionIdDidAbort ARGS((
	TransactionId	transactionId
));

/*
 * TransactionIdCommit --
 *	Commits the transaction associated with the identifier.
 *
 * Note:
 *	Assumes transaction identifier is valid.
 */
extern
void
TransactionIdCommit ARGS((
	TransactionId	transactionId
));

/*
 * TransactionIdAbort --
 *	Aborts the transaction associated with the identifier.
 *
 * Note:
 *	Assumes transaction identifier is valid.
 */
extern
void
TransactionIdAbort ARGS((
	TransactionId	transactionId
));

#endif	/* !defined(XLogIncluded) */
