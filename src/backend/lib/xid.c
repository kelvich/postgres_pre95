/* ----------------------------------------------------------------
 * xid.c --
 *	POSTGRES transaction identifier code.
 *
 * XXX WARNING
 *	Much of this file will change when we change our representation
 *	of transaction ids -cim 3/23/90
 *
 * It is time to make the switch from 5 byte to 4 byte transaction ids
 * This file was totally reworked. -mer 5/22/92
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "utils/palloc.h"
#include "utils/log.h"
#include "utils/memutils.h"

/* ----------------------------------------------------------------
 *	transaction system constants
 *
 *	read the comments for GetNewTransactionId in order to
 *      understand the initial values for AmiTransactionId and
 *      FirstTransactionId. -cim 3/23/90
 * ----------------------------------------------------------------
 */

extern TransactionId NullTransactionId;
extern TransactionId DisabledTransactionId;
extern TransactionId AmiTransactionId;
extern TransactionId FirstTransactionId;


/* ----------------------------------------------------------------
 * 	TransactionIdIsValid
 *
 *	Macro-ize me.
 * ----------------------------------------------------------------
 */

bool
TransactionIdIsValid(transactionId)
	TransactionId	transactionId;
{
	return ((bool) (transactionId != NullTransactionId) );
}

/* ----------------------------------------------------------------
 *	StringFormTransactionId
 *
 *	Not sure if this is still needed -mer 5/22/92
 * ----------------------------------------------------------------
 */

TransactionId
StringFormTransactionId(representation)
	String	representation;
{
	return (atol(representation));
}

/* ----------------------------------------------------------------
 *	TransactionIdFormString
 * ----------------------------------------------------------------
 */

String
TransactionIdFormString(transactionId)
	TransactionId	transactionId;
{
	String			representation;

	/* maximum 32 bit unsigned integer representation takes 10 chars */
	representation = palloc(11);

	(void)sprintf(representation, "%lu", transactionId);

	return (representation);
}

/* ----------------------------------------------------------------
 *	PointerStoreInvalidTransactionId
 *
 *	Maybe do away with Pointer types in these routines.
 *      Macro-ize this one.
 * ----------------------------------------------------------------
 */

void
PointerStoreInvalidTransactionId(destination)
	Pointer		destination;
{
	* (TransactionId *) destination = NullTransactionId;
}

/* ----------------------------------------------------------------
 *	TransactionIdStore
 *
 *      Macro-ize this one.
 * ----------------------------------------------------------------
 */

void
TransactionIdStore(transactionId, destination)
	TransactionId	transactionId;
	TransactionId	*destination;
{
	*destination = transactionId;
}

/* ----------------------------------------------------------------
 *	TransactionIdEquals
 * ----------------------------------------------------------------
 */

bool
TransactionIdEquals(id1, id2)
	TransactionId	id1;
	TransactionId	id2;
{
	return ((bool) (id1 == id2));
}

/* ----------------------------------------------------------------
 *	TransactionIdIsLessThan
 * ----------------------------------------------------------------
 */

bool
TransactionIdIsLessThan(id1, id2)
	TransactionId	id1;
	TransactionId	id2;
{
	return ((bool)(id1 < id2));
}

/* ----------------------------------------------------------------
 *	xidgt
 * ----------------------------------------------------------------
 */

/*
 *	xidgt		- returns 1, iff xid1 > xid2
 *				  0  else;
 */
bool
xidgt(xid1, xid2)
XID 	xid1, xid2;
{

	return( (bool) (xid1 > xid2) );
}

/* ----------------------------------------------------------------
 *	xidge
 * ----------------------------------------------------------------
 */

/*
 *	xidge		- returns 1, iff xid1 >= xid2
 *				  0  else;
 */
bool
xidge(xid1, xid2)
XID 	xid1, xid2;
{
	return( (bool) (xid1 >= xid2) );
}


/* ----------------------------------------------------------------
 *	xidle
 * ----------------------------------------------------------------
 */

/*
 *	xidle		- returns 1, iff xid1 <= xid2
 *				  0  else;
 */
bool
xidle(xid1, xid2)
XID 	xid1, xid2;
{
	return((bool) (xid1 <= xid2) );
}


/* ----------------------------------------------------------------
 *	xideq
 * ----------------------------------------------------------------
 */

/*
 *	xideq		- returns 1, iff xid1 == xid2
 *				  0  else;
 */
bool
xideq(xid1, xid2)
XID 	xid1, xid2;
{
	return( (bool) (xid1 == xid2) );
}



/* ----------------------------------------------------------------
 *	xidmi
 * ----------------------------------------------------------------
 */

/*	xidmi		- returns the distance xid1 - xid2
 *
 */
int
xidmi(xid1, xid2)
XID	xid1, xid2;
{	/* computes the 'distance' between xid1 and xid2:
	 * if there was no xidj generated between the generation of
	 * xid1 and xid2, then the distance (xid1 - xid2) has to be 1 (!!!) */
}

/* ----------------------------------------------------------------
 *	TransactionIdIncrement
 * ----------------------------------------------------------------
 */

void
TransactionIdIncrement(transactionId)
	TransactionId	*transactionId;
{

	(*transactionId)++;
	if (*transactionId == DisabledTransactionId)
		elog(FATAL, "TransactionIdIncrement: exhausted XID's");
	return;
}

/* ----------------------------------------------------------------
 *	TransactionIdAdd
 * ----------------------------------------------------------------
 */
void
TransactionIdAdd(xid, value)    
    TransactionId *xid;
    int		  value;
{
    *xid += value;
    return;
}

/* ----------------------------------------------------------------
 *	TransactionIdGetApproximateTime
 * ----------------------------------------------------------------
 */

Time
TransactionIdGetApproximateTime(transactionId)
	TransactionId	*transactionId;
{
	Time temp;
	temp = (*transactionId) / TransactionsPerSecondAdjustment;
	return(temp);
}
