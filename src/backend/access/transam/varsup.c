/* ----------------------------------------------------------------
 *   FILE
 *	varsup.c
 *	
 *   DESCRIPTION
 *	postgres variable relation support routines
 *
 *   INTERFACE ROUTINES
 *	VariableRelationGetNextXid
 *	VariableRelationPutNextXid
 *	VariableRelationGetLastXid
 *	VariableRelationPutLastXid
 *	GetNewTransactionId
 *	UpdateLastCommittedXid
 *   	
 *   NOTES
 *	Read the notes on the transaction system in the comments
 *	above GetNewTransactionId.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "transam.h"
 RcsId("$Header$");

/* ----------------------------------------------------------------
 *	      variable relation query/update routines
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	VariableRelationGetNextXid
 * --------------------------------
 */
void
VariableRelationGetNextXid(xid)
    TransactionId xid;
{
    Buffer buf;
    VariableRelationContents var;
    
    /* ----------------
     *	do nothing before things are initialized
     * ----------------
     */
    if (! RelationIsValid(VariableRelation))
	return;

    /* ----------------
     *	read the variable page, get the the nextXid field and
     *  release the buffer
     * ----------------
     */
    buf = ReadBuffer(VariableRelation, 0, 0);

    if (! BufferIsValid(buf))
	elog(WARN, "VariableRelationGetNextXid: RelationGetBuffer failed");

    var = (VariableRelationContents) BufferGetBlock(buf);

    TransactionIdStore(&(var->nextXidData), xid);

    ReleaseBuffer(buf);
}

/* --------------------------------
 *	VariableRelationGetLastXid
 * --------------------------------
 */
void
VariableRelationGetLastXid(xid)
    TransactionId xid;
{
    Buffer buf;
    VariableRelationContents var;
    
    /* ----------------
     *	do nothing before things are initialized
     * ----------------
     */
    if (! RelationIsValid(VariableRelation))
	return;

    /* ----------------
     *	read the variable page, get the the lastXid field and
     *  release the buffer
     * ----------------
     */
    buf = ReadBuffer(VariableRelation, 0, 0);

    if (! BufferIsValid(buf))
	elog(WARN, "VariableRelationGetNextXid: RelationGetBuffer failed");

    var = (VariableRelationContents) BufferGetBlock(buf);

    TransactionIdStore(&(var->lastXidData), xid);

    ReleaseBuffer(buf);
}

/* --------------------------------
 *	VariableRelationPutNextXid
 * --------------------------------
 */
void
VariableRelationPutNextXid(xid)
    TransactionId xid;
{
    Buffer buf;
    VariableRelationContents var;
    
    /* ----------------
     *	do nothing before things are initialized
     * ----------------
     */
    if (! RelationIsValid(VariableRelation))
	return;

    /* ----------------
     *	read the variable page, update the nextXid field and
     *  write the page back out to disk.
     * ----------------
     */
    buf = ReadBuffer(VariableRelation, 0, 0);

    if (! BufferIsValid(buf))
	elog(WARN, "VariableRelationPutNextXid: RelationGetBuffer failed");

    var = (VariableRelationContents) BufferGetBlock(buf);

    TransactionIdStore(xid, &(var->nextXidData));

    WriteBuffer(buf);
}

/* --------------------------------
 *	VariableRelationPutLastXid
 * --------------------------------
 */
void
VariableRelationPutLastXid(xid)
    TransactionId xid;
{
    Buffer buf;
    VariableRelationContents var;
    
    /* ----------------
     *	do nothing before things are initialized
     * ----------------
     */
    if (! RelationIsValid(VariableRelation))
	return;

    /* ----------------
     *	read the variable page, update the lastXid field and
     *  force the page back out to disk.
     * ----------------
     */
    buf = ReadBuffer(VariableRelation, 0, 0);

    if (! BufferIsValid(buf))
	elog(WARN, "VariableRelationPutLastXid: RelationGetBuffer failed");

    var = (VariableRelationContents) BufferGetBlock(buf);

    TransactionIdStore(xid, &(var->lastXidData));

    WriteBuffer(buf);
}

/* ----------------
 *	GetNewTransactionId
 *
 *	In the version 2 transaction system, transaction id's are
 *	restricted in several ways.
 *
 *	First, all transaction id's are even numbers (4, 88, 121342, etc).
 *	This means the binary representation of the number will never
 *	have the least significent bit set.  This bit is reserved to
 *	indicate that the transaction id does not in fact hold an XID,
 *	but rather a commit time.  This makes it possible for the
 *	vaccuum daemon to disgard information from the log and time
 *	relations for committed tuples.  This is important when archiving
 *	tuples to an optical disk because tuples with commit times
 *	stored in their xid fields will not need to consult the log
 *	and time relations.
 *
 *	Second, since we may someday preform compression of the data
 *	in the log and time relations, we cause the numbering of the
 *	transaction ids to begin at 512.  This means that some space
 *	on the page of the log and time relations corresponding to
 *	transaction id's 0 - 510 will never be used.  This space is
 *	in fact used to store the version number of the postgres
 *	transaction log and will someday store compression information
 *	about the log.
 *
 *	Lastly, rather then access the variable relation each time
 *	a backend requests a new transction id, we "prefetch" 32
 *	transaction id's by incrementing the nextXid stored in the
 *	var relation by 64 (remember only even xid's are legal) and then
 *	returning these id's one at a time until they are exhausted.
 *  	This means we reduce the number of accesses to the variable
 *	relation by 32 for each backend.
 *
 *  	Note:  32 has no special significance.  We don't want the
 *	       number to be too large because if when the backend
 *	       terminates, we lose the xid's we cached.
 *
 * ----------------
 */

#define VAR_PREFETCH	32

int prefetched_xid_count = 0;
TransactionIdData next_prefetched_id;

void
GetNewTransactionId(xid)
    TransactionId xid;
{
    TransactionIdData nextid;

    /* ----------------
     *	during bootstrap initialization, we return the special
     *  bootstrap transaction id.
     * ----------------
     */
    if (AMI_OVERRIDE) {	
	TransactionIdStore(AmiTransactionId, xid);
	return;
    }
 
    /* ----------------
     *  if we run out of prefetched xids, then we get some
     *  more before handing them out to the caller.
     * ----------------
     */
    
    if (prefetched_xid_count == 0) {
	/* ----------------
	 *	obtain exclusive access to the variable relation page
	 * ----------------
	 */
	
	/* ----------------
	 *	get the "next" xid from the variable relation
	 *	and save it in the prefetched id.
	 * ----------------
	 */
	VariableRelationGetNextXid(&nextid);
	TransactionIdStore(&nextid, &next_prefetched_id);
	
	/* ----------------
	 *	now increment the variable relation's next xid
	 *	and reset the prefetched_xid_count.  We multiply
	 *	the id by two because our xid's are always even.
	 * ----------------
	 */
	prefetched_xid_count = VAR_PREFETCH;
	TransactionIdAdd(&nextid, prefetched_xid_count * 2);
	VariableRelationPutNextXid(&nextid);
	
	/* ----------------
	 *	relinquish our lock on the variable relation page
	 * ----------------
	 */
    }
    
    /* ----------------
     *	return the next prefetched xid in the pointer passed by
     *  the user and decrement the prefetch count.  We add two
     *  to id we return the next time this is called because our
     *	transaction ids are always even.
     * ----------------
     */
    TransactionIdStore(&next_prefetched_id, xid);
    TransactionIdAdd(&next_prefetched_id, 2);
    prefetched_xid_count--;
}

/* ----------------
 *	UpdateLastCommittedXid
 * ----------------
 */

void
UpdateLastCommittedXid(xid)
    TransactionId xid;
{
    TransactionIdData lastid;

    /* ----------------
     *	obtain exclusive access to the variable relation page
     * ----------------
     */

    /* ----------------
     *	get the "last committed" transaction id from
     *  the variable relation page.
     * ----------------
     */
    VariableRelationGetLastXid(&lastid);

    /* ----------------
     *	if the transaction id is greater than the last committed
     *  transaction then we update the last committed transaction
     *  in the variable relation.
     * ----------------
     */
    if (TransactionIdIsLessThan(&lastid, xid))
	VariableRelationPutNextXid(xid);

    /* ----------------
     *	relinquish our lock on the variable relation page
     * ----------------
     */
}

