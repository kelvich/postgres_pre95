/* ----------------------------------------------------------------
 *   FILE
 *	transsup.c
 *	
 *   DESCRIPTION
 *	postgres transaction access method support code
 *
 *   INTERFACE ROUTINES
 * 	AmiTransactionOverride
 *	CreateTransRelation
 *	CreateNewTransTuple
 *	TransComputeItemPointer
 *
 *	TransItemPointerGetXidStatus
 *	TransItemPointerSetXidStatus
 *	TransItemPointerGetCommitTime
 *	TransItemPointerSetCommitTime
 *	TransGetLastRecordedTransaction
 *
 *   NOTES
 *	This file contains support functions for the high
 *	level access method interface routines found in transam.c
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "transam.h"
 RcsId("$Header$");

/* ----------------------------------------------------------------
 *		      general support routines
 * ----------------------------------------------------------------
 */
    
/* --------------------------------
 *	AmiTransactionOverride
 *
 *	This function is used to manipulate the bootstrap flag.
 * --------------------------------
 */
void
AmiTransactionOverride(flag)
    bool flag;
{
    AMI_OVERRIDE = flag;
}

/* --------------------
 *	CreateTransRelation
 *
 *	This function does the work of creating the given specified
 *	relation using the TransTuple schema.
 * --------------------
 */

AttributeTupleFormData
    TransAtt1 = { 0l, "xid", 28, 0l, 0l, 0l,  5, 1, 0, '\000', '\001' };
AttributeTupleFormData
    TransAtt2 = { 0l, "data",   17, 0l, 0l, 0l, -1, 2, 0, '\000', '\001' };
AttributeTupleForm
    TransTupleDescriptor[ TT_NumAttributes ] = { &TransAtt1, &TransAtt2 };

void
CreateTransRelation(name)
    Name name;			/* relation name */
{
    RelationNameCreateHeapRelation(name,
				   'n',
				   TT_NumAttributes,
				   TransTupleDescriptor);
}

/* --------------------
 *	CreateNewTransTuple
 *
 *	This function is used when we need a new trans tuple on a
 *	new page in the trans relation because no existing page
 *	in the trans relation contains the trans tuple we want --
 *	there is no tuple containing information associated with
 *	the given transaction id in the relation.
 *
 *	This function adds a page to the relation and initializes
 *	the page to contain the needed trans tuple.  An item pointer
 *	to the new trans tuple is returned.
 *
 *   	Notes to minimize confusion:
 *
 *   	VARSIZE is a macro which returns the size field of a varlena object
 *   	VARDATA is a macro which returns the data field of a varlena object
 *
 *	&ttupData[ sizeof(*tuple) ] is the address of the first byte
 *      beyond the tuple header. (tuples are variable length structures)
 * --------------------
 */

ItemPointer	/* location of new trans tuple */
CreateNewTransTuple(relation, id)
    Relation		relation; /* relation */
    TransactionId	id;	  /* base transaction id */
{
    HeapTuple		tuple;
    TransTuple		ttup;
    struct varlena 	*byteArrayP;
    char		ttupData[ TT_HeapTupleSize ];

    /* ----------------
     *	initialize our pointer to the heap tuple header
     * ----------------
     */
    tuple = (HeapTuple) &ttupData[0];

    /* ----------------
     *	initialize our pointer to the first byte in the tuple
     *  past the heap tuple header where the attributes are stored.
     * ----------------
     */
    ttup = (TransTuple) &ttupData[ sizeof(*tuple) ];

    /* ----------------
     *  Now, ttup->xid is the first attribute and ttup->data is
     *  the second.  ttup->data is a variable length attribute.
     *
     *  initialize fields of the heap tuple header.  this is
     *  stolen from the heap code.
     * ----------------
     */
    tuple->t_len = 	   TT_HeapTupleSize;
    tuple->t_lock.l_lock = InvalidRuleLock;
    tuple->t_natts = 	   TT_NumAttributes;
    tuple->t_hoff = 	   sizeof(HeapTupleData);
    tuple->t_bits[0] =     0x3;		/* XXX unportable byte order? */
    tuple->t_bits[1] =     0x0;
    tuple->t_bits[2] =     0x0;
    tuple->t_bits[3] =     0x0;

    /* ----------------
     *    initialize the first (xid) attribute
     * ----------------
     */
    TransactionIdStore(id, &(ttup->xid));

    /* ----------------
     *    initialize the second (data) attribute
     *
     *	  XXX assuming the size of a variable length attribute
     *        being an int32 will change soon when we implement
     *	      arrays and "large attributes".  -cim 3/20/90
     * ----------------
     */
    VARSIZE(&ttup->data) =
	TT_TupleDataSize + sizeof(int32);
	
    bzero(VARDATA(&ttup->data), TT_TupleDataSize);

    /* ----------------
     *    insert tuple in relation and return item pointer.
     *
     *    XXX we have to guarantee that this is appended to
     *	      the relation. -cim 3/20/90
     * ----------------
     */
    (void) RelationInsertHeapTuple(relation, tuple, (double*) NULL);

    return
	(ItemPointer) &(tuple->t_ctid);
}

/* --------------------------------
 *	TransCacheLookup
 *
 *	This checks the (xid, item pointer) cache to see
 *	if we can avoid going to the buffer manager to get the
 *	heap item pointer associated with the TransTuple containing
 *	the status of xid, thus avoiding a disk I/O.
 * --------------------------------
 */

ItemPointer
TransCacheLookup(transactionId, relation)
    TransactionId	transactionId;
    Relation		relation;
{
    return ((ItemPointer) NULL);
}

/* --------------------------------
 *	TransComputeItemPointer
 * --------------------------------
 */
void
TransComputeItemPointer(relation, transactionId, iptr)
    Relation	  	relation; 	/* relation to test */
    TransactionId 	transactionId; 	/* transaction id to test */
    ItemPointer		iptr;		/* item pointer to TransTuple */
{
    BlockNumber			block;
    double			result;
    TransactionIdValueData	xidv;
    
    /* ----------------
     *  we calculate the block number of our transaction
     *  by dividing the transaction id by the number of
     *  transaction things per block.  
     * ----------------
     */
    if (relation == LogRelation)
	itemsPerPage = TT_NumXidStatusPerTuple;
    else if (relation == TimeRelation)
	itemsPerPage = TT_NumTimePerTuple;
    else
	elog(WARN, "TransComputeItemPointer: unknown relation");
    
    /* ----------------
     *	warning! if the transaction id's get too large
     *  then a BlockNumber may not be large enough to hold the results
     *  of our division. 
     * ----------------
     */
    TransactionIdSetTransactionIdValue(transactionId, &xidv);
    result = xidv / itemsPerPage;
    block = result;
    
    /* ----------------
     *	place the computed block number into the given item pointer.
     * ----------------
     */
    ItemPointerSimpleSet(iptr, block, 1);
}


/* ----------------------------------------------------------------
 *		     trans tuple support routines
 * ----------------------------------------------------------------
 */
  
/* --------------------------------
 *	TransTupleGetLastTransactionIdStatus
 *
 *	This returns the status and transaction id of the last
 *	transaction information recorded on the given TransTuple.
 * --------------------------------
 */

XidStatus
TransTupleGetLastTransactionIdStatus(ttup, transactionId)
    TransTuple		ttup;
    TransactionId	transactionId;  /* RETURN */
{
    Index         index;
    Index         maxIndex;
    BitArray      bitArray;
    bits8         bit1;
    bits8	  bit2;
    BitIndex      offset;
    TransactionId TTupXid;
    XidStatus     status;

    /* ----------------
     *	sanity check
     * ----------------
     */
    Assert((ttup != NULL));

    /* ----------------
     *	get the starting transaction id from the trans tuple
     *  and calculate the maximum index into the tuple
     * ----------------
     */
    TTupXid = (TransactionId) &(ttup->xid);
    maxIndex = VARSIZE(&(ttup->data)) * 4;

    /* ----------------
     *	search downward from the top of the tuple data, looking
     *  for the first Non-in progress transaction status.  Since we
     *  are scanning backward, this will be last recorded transaction
     *  status on the tuple.
     * ----------------
     */
    for (index = maxIndex-1; index>=0; index--) {
	offset =    BitIndexOf(index);
	bitArray =  (BitArray) VARDATA(&(ttup->data));

	bit1 =    ((bits8) BitArrayBitIsSet(bitArray, offset++)) << 1;
	bit2 =    (bits8)  BitArrayBitIsSet(bitArray, offset);
	
	status =  (bit1 | bit2) ;

	/* ----------------
	 *  here we have the status of some transaction, so test
	 *  if the status is recorded as "in progress".  If so, then
	 *  we save the transaction id in the place specified by the caller.
	 * ----------------
	 */
	if (status != XID_INPROGRESS) {
	    if (transactionId != NULL) {
		TransactionIdStore(TTupXid, transactionId);
		TransactionIdAdd(transactionId, index);
	    }
	    break;
	}

	if (index == 0) {
	    if (transactionId != NULL)
		TransactionIdStore(TTupXid, transactionId);
	    break;
	}
    }

    /* ----------------
     *	return the status to the user
     * ----------------
     */
    return status;
}

/* --------------------------------
 *	TransTupleGetXidStatus
 *
 *	This returns the status of the desired transaction
 * --------------------------------
 */

XidStatus
TransTupleGetXidStatus(ttup, transactionId, failP)
    TransTuple		ttup;
    TransactionId	transactionId;
    bool		*failP;
{
    Index       index;
    Index       maxIndex;
    BitArray    bitArray;
    bits8       bit1;
    bits8	bit2;
    BitIndex    offset;

    (*failP) = false;
    
    /* ----------------
     *	sanity check
     * ----------------
     */
    if (ttup == NULL) {
	(*failP) = true;
	return;
    }

    /* ----------------
     *	calculate the index into the transaction data where
     *  our transaction status is located
     * ----------------
     */
    index = IntegerTransactionIdMinus(transactionId, ttup->xid);
    maxIndex = VARSIZE(&(ttup->data)) * 4;
   
    if (index >= maxIndex) {
	(*failP) = true;
	return;
    }
      
    /* ----------------
     *	get the data at the specified index
     * ----------------
     */
    offset =    BitIndexOf(index);
    bitArray =  (BitArray) VARDATA(&(ttup->data));

    bit1 =      ((bits8)   BitArrayBitIsSet(bitArray, offset++)) << 1;
    bit2 =      (bits8)    BitArrayBitIsSet(bitArray, offset);

    /* ----------------
     *	return the transaction status to the caller
     * ----------------
     */
    return (XidStatus)
	(bit1 | bit2);
}

/* --------------------------------
 *	TransTupleSetXidStatus
 *
 *	This sets the status of the desired transaction
 * --------------------------------
 */

void
TransTupleSetXidStatus(ttup, transactionId, status, failP)
    TransTuple		ttup;
    TransactionId	transactionId;
    LogValue 		status;
    bool		*failP;
{
    Index       index;
    Index       maxIndex;
    BitArray    bitArray;
    BitIndex    offset;

    (*failP) = false;
    
    /* ----------------
     *	sanity check
     * ----------------
     */
    if (ttup == NULL) {
	(*failP) = true;
	return;
    }

    /* ----------------
     *	calculate the index into the transaction data where
     *  we sould store our transaction status
     * ----------------
     */
    index = IntegerTransactionIdMinus(transactionId, ttup->xid);
    maxIndex = VARSIZE(&(ttup->data)) * 4;

    if (index >= maxIndex) {
	(*failP) = true;
	return;
    }
       
    offset =    BitIndexOf(index);
    bitArray =  (BitArray) VARDATA(&(ttup->data));

    /* ----------------
     *	store the transaction value at the specified offset
     * ----------------
     */
    switch(status) {
    case XID_COMMIT:             /* set 10 */
	BitArraySetBit(bitArray, offset);
	BitArrayClearBit(bitArray, offset + 1);
	break;
    case XID_ABORT:             /* set 01 */
	BitArrayClearBit(bitArray, offset);
	BitArraySetBit(bitArray, offset + 1);
	break;
    case XID_INPROGRESS:        /* set 00 */
	BitArrayClearBit(bitArray, offset);
	BitArrayClearBit(bitArray, offset + 1);
	break;
    default:
	elog(NOTICE,
	     "TransTupleSetXidIdStatus: invalid status: %d (ignored)",
	     status);
	(*failP) = true;
	break;
    }
}

/* --------------------------------
 *	TransTupleGetTransactionCommitTime
 *
 *	This returns the transaction commit time for the
 *	specified transaction id in the trans tuple.
 * --------------------------------
 */

Time
TransTupleGetTransactionCommitTime(ttup, transactionId, failP)
    TransTuple		ttup;
    TransactionId	transactionId;
    bool		*failP;
{
    Time	*timeArray;
    Index	index;
    Index       maxIndex;

    (*failP) = false;
    
    /* ----------------
     *	sanity check
     * ----------------
     */
    if (ttup == NULL) {
	(*failP) = true;
	return InvalidCommitTime;
    }

    /* ----------------
     *	calculate the index into the transaction data where
     *  our transaction commit time is located
     * ----------------
     */
    index = IntegerTransactionIdMinus(transactionId, ttup->xid);
    maxIndex = VARSIZE(&(ttup->data)) / sizeof(Time);
   
    if (index >= maxIndex) {
	(*failP) = true;
	return InvalidCommitTime;
    }

    /* ----------------
     *	return the commit time to the caller
     * ----------------
     */
    timeArray =  (Time *) VARDATA(&(ttup->data));
    return (Time)
	timeArray[ index ];
}

/* --------------------------------
 *	TransTupleSetTransactionCommitTime
 *
 *	This sets the commit time of the specified transaction
 * --------------------------------
 */

void
TransTupleSetTransactionCommitTime(ttup, transactionId, commitTime, failP)
    TransTuple		ttup;
    TransactionId	transactionId;
    Time 		commitTime;
    bool		*failP;
{
    Time	*timeArray;
    Index	index;
    Index       maxIndex;

    (*failP) = false;
    
    /* ----------------
     *	sanity check
     * ----------------
     */
    if (ttup == NULL) {
	(*failP) = true;
	return;
    }

    /* ----------------
     *	calculate the index into the transaction data where
     *  we sould store our transaction status
     * ----------------
     */
    index = IntegerTransactionIdMinus(transactionId, ttup->xid);
    maxIndex = VARSIZE(&(ttup->data)) / sizeof(Time);

    if (index >= maxIndex) {
	(*failP) = true;
	return;
    }

    /* ----------------
     *	store the transaction commit time at the specified index
     * ----------------
     */
    timeArray =  (Time *) VARDATA(&(ttup->data));
    timeArray[ index ] = commitTime;
}

/* ----------------------------------------------------------------
 *	           transam i/o support routines
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	TransItemPointerGetXidStatus
 * --------------------------------
 */
XidStatus
TransItemPointerGetXidStatus(relation, iptr, xid, failP)
    Relation		relation;
    ItemPointer		iptr;
    TransactionId	xid;
    bool		*failP;
{
    PagePartition   	partition;	/* number partitions on page */
    BlockNumber	   	blockNumber;	/* TransTuple block number */
    PageNumber	  	pageNumber;	/* TransTuple page number */
    OffsetNumber	offsetNumber;	/* item number on page of TransTuple */
    Buffer	   	buffer;		/* buffer associated with block */
    Page		page;		/* page containing TransTuple */
    ItemId	   	itemId;		/* item id referencing TransTuple */
    Item		ttitem;		/* TransTuple containing xid status */
    XidStatus		xidstatus;	/* recorded status of xid */
    bool		localfail;      /* bool used if failP = NULL */

    /* ----------------
     *	get the block number and stuff from the item pointer
     * ----------------
     */
    partition =    SinglePagePartition;
    blockNumber =  ItemPointerGetBlockNumber(iptr);
    pageNumber =   ItemPointerGetPageNumber(iptr, partition);
    offsetNumber = ItemPointerGetOffsetNumber(iptr, partition);

    /* ----------------
     *	get the page containing the tuple
     * ----------------
     */
    buffer = 	ReadBuffer(relation, blockNumber, 0);
    page =   	BufferGetPageDebug(buffer, pageNumber);

    /* ----------------
     *	get the item on the page we want
     * ----------------
     */
    itemId =    PageGetItemId(page, offsetNumber - 1);
    ttitem =   	PageGetItem(page, itemId);

    /* ----------------
     *	get the status from the tuple
     * ----------------
     */
    if (failP == NULL)
	failP = &localfail;

    xidstatus = TransTupleGetXidIdStatus(ttitem, xid, failP);

    /* ----------------
     *	release the buffer and return the status
     * ----------------
     */
    ReleaseBuffer(buffer);
    
    return
	xidstatus;
}

/* --------------------------------
 *	TransItemPointerSetXidStatus
 * --------------------------------
 */
void
TransItemPointerSetXidStatus(relation, iptr, xid, status, failP)
    Relation		relation;
    ItemPointer		iptr;
    TransactionId	xid;
    XidStatus		xidstatus;
    bool		*failP;
{
    PagePartition   	partition;	/* number partitions on page */
    BlockNumber	   	blockNumber;	/* TransTuple block number */
    PageNumber	  	pageNumber;	/* TransTuple page number */
    OffsetNumber	offsetNumber;	/* item number on page of TransTuple */
    Buffer	   	buffer;		/* buffer associated with block */
    Page		page;		/* page containing TransTuple */
    ItemId	   	itemId;		/* item id referencing TransTuple */
    Item		ttitem;		/* TransTuple containing xid status */
    bool		localfail;      /* bool used if failP = NULL */
    
    /* ----------------
     *	get the block number and stuff from the item pointer
     * ----------------
     */
    partition =    SinglePagePartition;
    blockNumber =  ItemPointerGetBlockNumber(iptr);
    pageNumber =   ItemPointerGetPageNumber(iptr, partition);
    offsetNumber = ItemPointerGetOffsetNumber(iptr, partition);
    
    /* ----------------
     *	get the page containing the tuple
     * ----------------
     */
    buffer = ReadBuffer(relation, blockNumber, 0);
    page =   BufferGetPageDebug(buffer, pageNumber);
    
    /* ----------------
     *	get the item on the page
     * ----------------
     */
    itemId =   	PageGetItemId(page, offsetNumber - 1);
    ttitem =   	PageGetItem(page, itemId);

    /* ----------------
     *	attempt to update the status of the transaction on the page.
     *  if we are successful, write the page. otherwise release the buffer.
     * ----------------
     */
    if (failP == NULL)
	failP = &localfail;
    
    TransTupleSetXidStatus(ttitem, xid, status, failP);

    if ((*failP) == false)
	WriteBuffer(buffer);
    else
	ReleaseBuffer(buffer);
}

/* --------------------------------
 *	TransItemPointerGetCommitTime
 * --------------------------------
 */
Time
TransItemPointerGetCommitTime(relation, iptr, xid, failP)
    Relation		relation;
    ItemPointer		iptr;
    TransactionId	xid;
    bool		*failP;
{
    PagePartition   	partition;	/* number partitions on btree page */
    BlockNumber	   	blockNumber;	/* TransTuple block number */
    PageNumber	  	pageNumber;	/* TransTuple page number */
    OffsetNumber	offsetNumber;	/* item number on page of TransTuple */
    Buffer	   	buffer;		/* buffer associated with block */
    Page		page;		/* page containing TransTuple */
    ItemId	   	itemId;		/* item id referencing TransTuple */
    Item		ttitem;		/* TransTuple containing xid status */
    Time		xtime;		/* recorded commit time of xid */
    bool		localfail;      /* bool used if failP = NULL */

    /* ----------------
     *	get the block number and stuff from the item pointer
     * ----------------
     */
    partition =    SinglePagePartition;
    blockNumber =  ItemPointerGetBlockNumber(iptr);
    pageNumber =   ItemPointerGetPageNumber(iptr, partition);
    offsetNumber = ItemPointerGetOffsetNumber(iptr, partition);

    /* ----------------
     *	get the page containing the tuple
     * ----------------
     */
    buffer = 	ReadBuffer(relation, blockNumber, 0);
    page =   	BufferGetPageDebug(buffer, pageNumber);

    /* ----------------
     *	get the item on the page we want
     * ----------------
     */
    itemId =    PageGetItemId(page, offsetNumber - 1);
    ttitem =   	PageGetItem(page, itemId);

    /* ----------------
     *	get the commit time from the tuple
     * ----------------
     */
    if (failP == NULL)
	failP = &localfail;

    xtime = TransTupleGetTransactionCommitTime(ttitem, xid, failP);

    /* ----------------
     *	release the buffer and return the commit time
     * ----------------
     */
    ReleaseBuffer(buffer);
    
    if ((*failP) == false)
	return xtime;
    else
	return InvalidCommitTime;
}

/* --------------------------------
 *	TransItemPointerSetCommitTime
 * --------------------------------
 */
void
TransItemPointerSetCommitTime(relation, iptr, xid, xtime, failP)
    Relation		relation;
    ItemPointer		iptr;
    TransactionId	xid;
    Time		xtime;
    bool		*failP;
{
    PagePartition   	partition;	/* number partitions on btree page */
    BlockNumber	   	blockNumber;	/* TransTuple block number */
    PageNumber	  	pageNumber;	/* TransTuple page number */
    OffsetNumber	offsetNumber;	/* item number on page of TransTuple */
    Buffer	   	buffer;		/* buffer associated with block */
    Page		page;		/* page containing TransTuple */
    ItemId	   	itemId;		/* item id referencing TransTuple */
    Item		ttitem;		/* TransTuple containing xid status */
    bool		localfail;      /* bool used if failP = NULL */

    /* ----------------
     *	get the block number and stuff from the item pointer
     * ----------------
     */
    partition =    SinglePagePartition;
    blockNumber =  ItemPointerGetBlockNumber(iptr);
    pageNumber =   ItemPointerGetPageNumber(iptr, partition);
    offsetNumber = ItemPointerGetOffsetNumber(iptr, partition);

    /* ----------------
     *	get the page containing the tuple
     * ----------------
     */
    buffer = 	ReadBuffer(relation, blockNumber, 0);
    page =   	BufferGetPageDebug(buffer, pageNumber);

    /* ----------------
     *	get the item on the page we want
     * ----------------
     */
    itemId =    PageGetItemId(page, offsetNumber - 1);
    ttitem =   	PageGetItem(page, itemId);

    /* ----------------
     *	attempt to update the commit time of the transaction on the page.
     *  if we are successful, write the page. otherwise release the buffer.
     * ----------------
     */
    if (failP == NULL)
	failP = &localfail;
    
    TransTupleSetTransactionCommitTime(ttitem, xid, status, xtime, failP);

    if ((*failP) == false)
	WriteBuffer(buffer);
    else
	ReleaseBuffer(buffer);
}

/* --------------------------------
 *	TransGetLastRecordedTransaction
 * --------------------------------
 */
void
TransGetLastRecordedTransaction(relation, xid, failP)
    Relation		relation;
    TransactionId	xid;		/* return: transaction id */
    bool		*failP;
{
    PagePartition   	partition;	/* number partitions on page */
    BlockNumber	   	blockNumber;	/* TransTuple block number */
    PageNumber	  	pageNumber;	/* TransTuple page number */
    OffsetNumber	offsetNumber;	/* item number on page of TransTuple */
    Buffer	   	buffer;		/* buffer associated with block */
    Page		page;		/* page containing TransTuple */
    ItemId	   	itemId;		/* item id referencing TransTuple */
    Item		ttitem;		/* TransTuple containing xid status */
    BlockNumber		n;		/* number of pages in the relation */

    (*failP) = false;
    
    /* ----------------
     *	we assume the last page of the log contains the last
     *  recorded transaction.  If the relation is empty we return
     *  failure to the user.
     * ----------------
     */
    n = RelationGetNumberOfBlocks(relation);
    if (n == 0) {
	(*failP) = true;
	return;
    }

    /* ----------------
     *	get the page containing the tuple
     * ----------------
     */
    partition =    SinglePagePartition;
    blockNumber =  n-1;
    pageNumber =   0;
    offsetNumber = 0;

    buffer = 	ReadBuffer(relation, blockNumber, 0);
    page =   	BufferGetPageDebug(buffer, pageNumber);

    /* ----------------
     *	get the item on the page we want
     * ----------------
     */
    itemId =    PageGetItemId(page, offsetNumber - 1);
    ttitem =   	PageGetItem(page, itemId);

    (void) TransTupleGetLastTransactionIdStatus(ttitem, xid);
    
    ReleaseBuffer(buffer);
}

