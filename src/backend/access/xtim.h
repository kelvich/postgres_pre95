/*
 * xtim.h --
 *	POSTGRES transaction time definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	XTimIncluded	/* Include this file only once. */
#define XTimIncluded	1

#ifndef C_H
#include "c.h"
#endif

#include "tim.h"
#include "xid.h"

/*
 * TransactionIdGetCommitTime --
 *	Returns commit time of transaction associated with an identifier.
 *
 * Note:
 *	Assumes transaction identifier is valid.
 */
extern
Time
TransactionIdGetCommitTime ARGS((
	TransactionId	transactionId
));

#endif	/* !defined(XTimIncluded) */
