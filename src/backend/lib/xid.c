/* ----------------------------------------------------------------
 * xid.c --
 *	POSTGRES transaction identifier code.
 *
 * XXX WARNING
 *	Much of this file will change when we change our representation
 *	of transaction ids -cim 3/23/90
 * ----------------------------------------------------------------
 */

#include "c.h"

RcsId("$Header$");

#include <math.h>	/* for floor ... */

#include "bit.h"
#include "clib.h"
#include "log.h"
#include "palloc.h"
#include "postgres.h"	/* XXX for XID, remove this when XID -> TransactionId */
#include "tim.h"

#include "xid.h"

/* ----------------------------------------------------------------
 *	transaction system constants
 *
 *	read the comments for GetNewTransactionId in order to
 *      understand the initial values for AmiTransactionId and
 *      FirstTransactionId. -cim 3/23/90
 * ----------------------------------------------------------------
 */

static TransactionIdData NullTransactionIdData = { { 0, 0, 0, 0, 0 } };
TransactionId NullTransactionId = &NullTransactionIdData;

static TransactionIdData TransactionIdOneData = { { 0, 0, 0, 2, 0 } };
TransactionId AmiTransactionId = &TransactionIdOneData;

static TransactionIdData FirstTransactionIdData = { { 0, 0, 0, 2, 2 } };
TransactionId FirstTransactionId = &FirstTransactionIdData;


/* ----------------------------------------------------------------
 * 	TransactionIdIsValid
 * ----------------------------------------------------------------
 */

bool
TransactionIdIsValid(transactionId)
	TransactionId	transactionId;
{
	return ((bool)(PointerIsValid(transactionId) &&
		(transactionId->data[4] != NullTransactionId->data[4] ||
			transactionId->data[3] != NullTransactionId->data[3] ||
			transactionId->data[2] != NullTransactionId->data[2] ||
			transactionId->data[1] != NullTransactionId->data[1] ||
			transactionId->data[0] != NullTransactionId->data[0])));		
}

/* ----------------------------------------------------------------
 *	StringFormTransactionId
 * ----------------------------------------------------------------
 */

TransactionId
StringFormTransactionId(representation)
	String	representation;
{
	TransactionId		transactionId;
	TransactionIdValueData	valueData;

	StringSetTransactionIdValue(representation, &valueData);
	
	transactionId = LintCast(TransactionId,
		palloc(sizeof transactionId->data));

	TransactionIdValueSetTransactionId(&valueData, transactionId);

	return (transactionId);
}

/* ----------------------------------------------------------------
 *	TransactionIdFormString
 * ----------------------------------------------------------------
 */

String
TransactionIdFormString(transactionId)
	TransactionId	transactionId;
{
	TransactionIdValueData	valueData;
	String			representation;

	TransactionIdSetTransactionIdValue(transactionId, &valueData);

	representation = TransactionIdValueFormString(&valueData);

	return (representation);
}

/* ----------------------------------------------------------------
 *	PointerStoreInvalidTransactionId
 * ----------------------------------------------------------------
 */

void
PointerStoreInvalidTransactionId(destination)
	Pointer		destination;
{
	Assert(PointerIsValid(destination));

	MemoryCopy((String)destination, (String)NullTransactionId,
		TransactionIdDataSize);
}

/* ----------------------------------------------------------------
 *	TransactionIdStore
 * ----------------------------------------------------------------
 */

void
TransactionIdStore(transactionId, destination)
	TransactionId	transactionId;
	Pointer		destination;
{
	Assert(TransactionIdIsValid(transactionId));
	Assert(PointerIsValid(destination));

	MemoryCopy((String)destination, (String)transactionId,
		TransactionIdDataSize);
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
	return ((bool)(id1->data[4] == id2->data[4] &&
			id1->data[3] == id2->data[3] &&
			id1->data[2] == id2->data[2] &&
			id1->data[1] == id2->data[1] &&
			id1->data[0] == id2->data[0]));
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
	TransactionIdValueData	valueData1;
	TransactionIdValueData	valueData2;

	TransactionIdSetTransactionIdValue(id1, &valueData1);
	TransactionIdSetTransactionIdValue(id2, &valueData2);

	return ((bool)((valueData2 - valueData1) > 0.7));
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
{	extern	int	strcmp();

	return( (bool) (strcmp(xid1, xid2) > 0) );
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
{	extern	int	strcmp();

	return( (bool) (strcmp(xid1, xid2) >= 0) );
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
{	extern	int	strcmp();

	return((bool) (strcmp(xid1, xid2) <= 0) );
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
{	extern	int	strcmp();

	return( (bool) (strcmp(xid1, xid2) == 0) );
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
 *	TransactionIdValueIsValid
 * ----------------------------------------------------------------
 */

bool
TransactionIdValueIsValid(value)
	TransactionIdValue	value;
{
	return ((bool)(PointerIsValid(value) &&
		*value != NullTransactionIdValue));
}

/* ----------------------------------------------------------------
 *	StringSetTransactionIdValue
 * ----------------------------------------------------------------
 */

void
StringSetTransactionIdValue(representation, value)
	String			representation;
	TransactionIdValue	value;
{
	extern double		atof();

	*value = atof(representation);
}

/* ----------------------------------------------------------------
 *	TransactionIdValueFormString
 * ----------------------------------------------------------------
 */

String
TransactionIdValueFormString(value)
	TransactionIdValue	value;
{
	String	representation;

	/* maximum 40 bit unsigned integer representation takes 12 chars */
	representation = palloc(13);

	(void)sprintf(representation, "%.0f", *value);

	return (representation);
}

/* ----------------------------------------------------------------
 *	TransactionIdValueSetTransactionId
 * ----------------------------------------------------------------
 */

void
TransactionIdValueSetTransactionId(idValue, id)
	TransactionIdValue	idValue;
	TransactionId		id;
{
	TransactionIdValueData	valueData;
	uint8			*digitPointer;
	int16			digitsLeft;

	
	valueData = *idValue;
	digitPointer = &id->data[sizeof id->data - 1];

	for (digitsLeft = sizeof id->data - 1; digitsLeft >= 0;
			digitsLeft -= 1) {

		TransactionIdValueData	tempData;

		tempData = floor(valueData / TransactionMultiplierPerByte);
		*digitPointer = valueData -
			(TransactionMultiplierPerByte * tempData);
		valueData = tempData;
		digitPointer -= 1;
	}
}

/* ----------------------------------------------------------------
 *	TransactionIdSetTransactionIdValue
 * ----------------------------------------------------------------
 */

void
TransactionIdSetTransactionIdValue(id, idValue)
	TransactionId		id;
	TransactionIdValue	idValue;
{
	TransactionIdValueData	valueData;
	uint8			*digitPointer;
	int16			digitsLeft;
#ifdef	sun
	uint16			digit;
#endif	/* defined(sun) */

	digitPointer = &id->data[0];
	valueData = 0;

	for (digitsLeft = sizeof id->data - 1; digitsLeft >= 0;
			digitsLeft -= 1) {

		valueData *= TransactionMultiplierPerByte;
#ifdef	sun
		digit = AsUint8(*digitPointer);
		valueData += digit;
#else	/* !defined(sun) */
		valueData += AsUint8(*digitPointer);
#endif	/* !defined(sun) */
		digitPointer += 1;
	}

	*idValue = valueData;
}

/* ----------------------------------------------------------------
 *	TransactionIdIncrement
 * ----------------------------------------------------------------
 */

void
TransactionIdIncrement(transactionId)
	TransactionId	transactionId;
{
	uint8	*digitPointer;
	int16	digitsLeft;

	digitPointer = &transactionId->data[sizeof transactionId->data - 1];

	for (digitsLeft = sizeof transactionId->data - 1; digitsLeft >= 0;
			digitsLeft -= 1) {

		*digitPointer = AsUint8(1 + AsUint8(*digitPointer));

		if (!(AsUint8(*digitPointer) == 0)) {
			return;
		}
		digitPointer -= 1;
	}
	elog(FATAL, "TransactionIdIncrement: exhausted XID's");
}

/* ----------------------------------------------------------------
 *	TransactionIdAdd
 * ----------------------------------------------------------------
 */
void
TransactionIdAdd(xid, value)    
    TransactionId xid;
    int		  value;
{
    TransactionIdValueData xidv;

    TransactionIdSetTransactionIdValue(xid, &xidv);
    xidv += value;
    TransactionIdValueSetTransactionId(&xidv, xid);
}

/* ----------------------------------------------------------------
 *	TransactionIdGetApproximateTime
 * ----------------------------------------------------------------
 */

Time
TransactionIdGetApproximateTime(transactionId)
	TransactionId	transactionId;
{
	TransactionIdValueData	valueData;

	TransactionIdSetTransactionIdValue(transactionId, &valueData);

	valueData /= TransactionsPerSecondAdjustment;
	return((Time)valueData);
}
