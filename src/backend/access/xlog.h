/*
 * xlog.h --
 *	POSTGRES transaction log definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	XLogIncluded	/* Include this file only once. */
#define XLogIncluded	1

#include "tmp/postgres.h"

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
