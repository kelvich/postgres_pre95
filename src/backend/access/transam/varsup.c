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
 *	VariableRelationPutLastXid
 *	GetNewTransactionId
 *	UpdateLastCommittedXid
 *   	
 *   NOTES
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
 * ----------------
 */

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
     *	here we "prefetch" 16 transaction id's by incrementing the
     *  nextXid stored in the var relation by 16 and then returning
     *  these id's one at a time until they are exhausted.
     *
     *  This means we reduce the number of times we access the var
     *  relation's page by 16.
     *
     *  Note:  16 has no special significance.  We don't want the
     *	       number to be too large because if the system crashes
     *	       we lose the xid's we prefetched.
     * ----------------
     */
    
    if (prefetched_xid_count == 0) {
	/* ----------------
	 *	obtain exclusive access to the variable relation page
	 * ----------------
	 */
	
	/* ----------------
	 *	get the next xid from the variable relation
	 *	and save it in the prefetched id.
	 * ----------------
	 */
	VariableRelationGetNextXid(&nextid);
	TransactionIdStore(&nextid, &next_prefetched_id);

	/* ----------------
	 *	now increment the variable relation's next xid
	 *	and reset the prefetched_xid_count
	 * ----------------
	 */
	prefetched_xid_count = 16;
	TransactionIdAdd(&nextid, prefetched_xid_count);
	VariableRelationPutNextXid(&nextid);
	
	/* ----------------
	 *	relinquish our lock on the variable relation page
	 * ----------------
	 */
    }

    /* ----------------
     *	return the next prefetched xid in the pointer passed by
     *  the user and decrement the prefetch count.
     * ----------------
     */
    TransactionIdStore(&next_prefetched_id, xid);
    TransactionIdIncrement(&next_prefetched_id);
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
	VariableRelationPutLextXid(xid);

    /* ----------------
     *	relinquish our lock on the variable relation page
     * ----------------
     */
}

