/* ----------------------------------------------------------------
 *   FILE
 *	transam.c
 *	
 *   DESCRIPTION
 *	postgres transaction log/time interface routines
 *
 *   INTERFACE ROUTINES
 *	RecoveryCheckingEnabled
 *	SetRecoveryCheckingEnabled
 *	InitializeTransactionLog
 *	TransactionIdDidCommit
 *	TransactionIdDidAbort
 *	TransactionIdIsInProgress
 *	TransactionIdCommit
 *	TransactionIdAbort
 *	TransactionIdSetInProgress
 *   	
 *   NOTES
 *	This file contains the high level access-method
 *	interface to the transaction system.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "transam.h"
 RcsId("$Header$");

/* ----------------
 *    global variables holding pointers to relations used
 *    by the transaction system.  These are initialized by
 *    InitializeTransactionLog().
 * ----------------
 */

Relation LogRelation	  = (Relation) NULL;
Relation TimeRelation	  = (Relation) NULL;
Relation VariableRelation = (Relation) NULL;

/* ----------------
 *	transaction recovery state variables
 *
 *	When the transaction system is initialized, we may
 *	need to do recovery checking.  This decision is decided
 *	by the postmaster or the user by supplying the backend
 *	with a special flag.  In general, we want to do recovery
 *	checking whenever we are running without a postmaster
 *	or when the number of backends running under the postmaster
 *	goes from zero to one. -cim 3/21/90
 * ----------------
 */
bool RecoveryCheckingEnableState = false;

/* ----------------
 *	recovery checking accessors
 * ----------------
 */
int
RecoveryCheckingEnabled()
{    
    return RecoveryCheckingEnableState;
}

void
SetRecoveryCheckingEnabled(state)
    bool state;
{    
    RecoveryCheckingEnableState = (state == true);
}

/* ----------------------------------------------------------------
 *	postgres log/time access method interface
 *
 *	TransactionLogTest
 *	TransactionLogUpdate
 *	========
 *	   these functions do work for the interface
 *	   functions - they search/retrieve and append/update
 *	   information in the log and time relations.
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	TransactionLogTest
 * --------------------------------
 */

bool	/* true/false: does transaction id have specified status? */
TransactionLogTest(transactionId, status)
    TransactionId 	transactionId; 	/* transaction id to test */
    TransactionStatus 	status;		/* transaction status */
{
    ItemPointerData	xptr;		/* item pointer to TransTuple */
    XidStatus		xidstatus;	/* recorded status of xid */
    bool		fail;      	/* success/failure */
    
    /* ----------------
     * 	during initialization consider all transactions
     *  as having been committed
     * ----------------
     */
    if (! RelationIsValid(LogRelation))
	return (bool) (status == XID_COMMIT);

    /* ----------------
     *	compute the item pointer corresponding to the
     *  page containing our transaction id.
     * ----------------
     */
    TransComputeItemPointer(transactionId, &xptr);
    xidstatus = TransItemPointerGetXidStatus(LogRelation, &xptr, &fail);

    if (! fail)
	return (bool)
	    (status == xidstatus);

    /* ----------------
     *	  here the block didn't contain the tuple we wanted
     * ----------------
     */
    elog(WARN, "TransactionLogTest: failed to get xidstatus");
}

/* --------------------------------
 *	TransactionLogUpdate
 * --------------------------------
 */
void
TransactionLogUpdate(transactionId, status)
    TransactionId 	transactionId;	/* trans id to update */
    TransactionStatus 	status;		/* new trans status */
{
    ItemPointerData  	xptr;		/* item pointer to TransTuple */
    ItemId	   	itemId;		/* item id referencing TransTuple */
    Item		ttitem;		/* TransTuple containing xid status */
    bool		fail;      	/* success/failure */
    Time 		currentTime;	/* time of this transaction */

    /* ----------------
     * 	during initialization we don't record any updates.
     * ----------------
     */
    if (! RelationIsValid(LogRelation))
	return;
    
    /* ----------------
     *    get the transaction commit time
     * ----------------
     */
    currentTime = GetSystemTime();
    
    /* ----------------
     *    update the log relation
     * ----------------
     */
    TransComputeItemPointer(LogRelation, transactionId, &xptr);
    TransItemPointerSetXidStatus(LogRelation,
				 &xptr,
				 transactionId,
				 status,
				 &fail);
    
    /* ----------------
     *	now we update the time relation, if necessary
     *  (we only record commit times)
     * ----------------
     */
    if (RelationIsValid(TimeRelation) && status == XID_COMMIT) {
	TransComputeItemPointer(TimeRelation, transactionId, &xptr);
	TransItemPointerSetTransactionCommitTime(TimeRelation,
						 &xptr,
						 transactionId,
						 currentTime,
						 &fail);
    }
    
    /* ----------------
     *	now we update the "last committed transaction" field
     *  in the variable relation if we are recording a commit.
     * ----------------
     */
    if (RelationIsValid(VariableRelation) && status == XID_COMMIT)
	UpdateLastCommittedXid(transactionId);
}

/* ----------------------------------------------------------------
 *		     transaction recovery code
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	TransRecover
 *
 *    	preform transaction recovery checking.
 *
 *	Note: this should only be preformed if no other backends
 *	      are running.  This is known by the postmaster and
 *	      conveyed by the postmaster passing a "do recovery checking"
 *	      flag to the backend.
 *
 *	here we get the last recorded transaction from the log,
 *	get the "last" and "next" transactions from the variable relation
 *	and then preform some integrity tests:
 *
 *    	1) No transaction may exist higher then the "next" available
 *         transaction recorded in the variable relation.  If this is the
 *         case then it means either the log or the variable relation
 *         has become corrupted.
 *
 *      2) The last committed transaction may not be higher then the
 *         next available transaction for the same reason.
 *
 *      3) The last recorded transaction may not be lower then the
 *         last committed transaction.  (the reverse is ok - it means
 *         that some transactions have aborted since the last commit)
 *
 *	Here is what the proper situation looks like.  The line
 *	represents the data stored in the log.  'c' indicates the
 *      transaction was recorded as committed, 'a' indicates an
 *      abortted transaction and '.' represents information not
 *      recorded.  These may correspond to in progress transactions.
 *
 *	     c  c  a  c  .  .  a  .  .  .  .  .  .  .  .  .  .
 *		      |                 |
 *		     last	       next
 *
 *	Since "next" is only incremented by GetNewTransactionId() which
 *      is called when transactions are started.  Hence if there
 *      are commits or aborts after "next", then it means we committed
 *      or aborted BEFORE we started the transaction.  This is the
 *	rational behind constraint (1).
 *
 *      Likewise, "last" should never greater then "next" for essentially
 *      the same reason - it would imply we committed before we started.
 *      This is the reasoning for (2).
 *
 *	(3) implies we may never have a situation such as:
 *
 *	     c  c  a  c  .  .  a  c  .  .  .  .  .  .  .  .  .
 *		      |                 |
 *		     last	       next
 *
 *      where there is a 'c' greater then "last".
 *
 *      Recovery checking is more difficult in the case where
 *      several backends are executing concurrently because the
 *	transactions may be executing in the other backends.
 *      So, we only do recovery stuff when the backend is explicitly
 *      passed a flag on the command line.
 * --------------------------------
 */

void
TransRecover(logRelation)
    Relation logRelation;
{
    TransactionId logLastXid;
    TransactionId varLastXid;
    TransactionId varNextXid;

#if 0    
    /* ----------------
     *    first get the last recorded transaction in the log.
     * ----------------
     */
    TransGetLastRecordedTransaction(logRelation, logLastXid, &fail);
    if (fail == true)
	elog(WARN, "TransRecover: failed TransGetLastRecordedTransaction");
    
    /* ----------------
     *    next get the "last" and "next" variables
     * ----------------
     */
    VariableRelationGetLastXid(varLastXid);
    VariableRelationGetNextXid(varNextXid);

    /* ----------------
     *    intregity test (1)
     * ----------------
     */
    if (TransactionIdIsLessThan(varNextXid, logLastXid))
	elog(WARN, "TransRecover: varNextXid < logLastXid");
	    
    /* ----------------
     *    intregity test (2)
     * ----------------
     */
	    	
    /* ----------------
     *    intregity test (3)
     * ----------------
     */

    /* ----------------
     *  here we have a valid "
     *
     *		**** RESUME HERE ****
     * ----------------
     */
    varNextXid = TransactionIdDup(varLastXid);
    TransactionIdIncrement(varNextXid);

    VarPut(var, VAR_PUT_LASTXID, varLastXid);
    VarPut(var, VAR_PUT_NEXTXID, varNextXid);
#endif
}
    
/* ----------------------------------------------------------------
 *			Interface functions
 *
 *	InitializeTransactionLog
 *	========
 *	   this function (called near cinit) initializes
 *	   the transaction log, time and variable relations.
 *
 *	TransactionId DidCommit
 *	TransactionId DidAbort
 *	TransactionId IsInProgress
 *	========
 *	   these functions test the transaction status of
 *	   a specified transaction id.
 *
 *	TransactionId Commit
 *	TransactionId Abort
 *	TransactionId SetInProgress
 *	========
 *	   these functions set the transaction status
 *	   of the specified xid. TransactionIdCommit() also
 *	   records the current time in the time relation
 *	   and updates the variable relation counter.
 *
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	InitializeTransactionLog
 * --------------------------------
 */

void
InitializeTransactionLog()
{
    Relation	  logRelation;
    Relation	  timeRelation;
    Relation	  varRelation;
    MemoryContext oldContext;
    bool	  fail;
    
    /* ----------------
     *    don't do anything during bootstrapping
     * ----------------
     */
    if (AMI_OVERRIDE)
	return;
    
    /* ----------------
     *	 disable the transaction system so the access methods
     *   don't interfere during initialization.
     * ----------------
     */
    OverrideTransactionSystem(true);
    
    /* ----------------
     *	make sure allocations occur within the top memory context
     *  so that our log management structures are protected from
     *  garbage collection at the end of every transaction.
     * ----------------
     */
    oldContext = MemoryContextSwitchTo(TopMemoryContext); 
    
    /* ----------------
     *   first open the log and time relations
     *   (these are created by amiint so they are guarantted to exist)
     * ----------------
     */
    logRelation = 	RelationNameOpenHeapRelation(LogRelationName);
    timeRelation = 	RelationNameOpenHeapRelation(TimeRelationName);
    VariableRelation = 	RelationNameOpenHeapRelation(VariableRelationName);

    /* ----------------
     *   if we have a virgin database, we initialize the log and time
     *	 relation by committing the AmiTransactionId (id 1) and we
     *   initialize the variable relation by setting the next available
     *   transaction id to FirstTransactionId (id 2).
     * ----------------
     */
    n = RelationGetNumberOfBlocks(logRelation);
    if (n == 0) {
	/* ----------------
	 *   XXX transaction log update requires that LogRelation be
	 *       valid so we temporarily set it so we can initialize
	 *	 things properly.  This could be done cleaner.
	 * ----------------
	 */
	LogRelation =  logRelation;
	TimeRelation = timeRelation;
	
	TransactionLogUpdate(&AmiTransactionId, XID_COMMIT);
	VariableRelationSetNextXid(&FirstTransactionId);
	
	LogRelation =  (Relation) NULL;
	TimeRelation = (Relation) NULL;
	
    } else if (RecoveryCheckingEnabled()) {
	/* ----------------
	 *	if we have a pre-initialized database and if the
	 *	preform recovery checking flag was passed then we
	 *	do our database integrity checking.
	 * ----------------
	 */
	TransRecover();
    }
    
    /* ----------------
     *	now re-enable the transaction system
     * ----------------
     */
    OverrideTransactionSystem(false);
    
    /* ----------------
     *	instantiate the global variables
     * ----------------
     */
    LogRelation = 	logRelation;
    TimeRelation = 	timeRelation;

    /* ----------------
     *	restore the memory context to the previous context
     *  before we return from initialization.
     * ----------------
     */
    MemoryContextSwitchTo(oldContext);
}

/* --------------------------------
 *	TransactionId DidCommit
 *	TransactionId DidAbort
 *	TransactionId IsInProgress
 * --------------------------------
 */

bool	/* true if given transaction committed */
TransactionIdDidCommit(transactionId)
    TransactionId transactionId;
{
    if (AMI_OVERRIDE)
	return true;
    
    return
	TransactionLogTest(transactionId, XID_COMMIT);
}

bool	/* true if given transaction aborted */
TransactionIdDidAbort(transactionId)
    TransactionId transactionId;
{
    if (AMI_OVERRIDE)
	return false;
    
    return
	TransactionLogTest(transactionId, XID_ABORT);
}

bool	/* true if given transaction neither committed nor aborted */
TransactionIdIsInProgress(transactionId)
    TransactionId transactionId;
{
    if (AMI_OVERRIDE)
	return false;
    
    return
	TransactionLogTest(transactionId, XID_INPROGRESS);
}

/* --------------------------------
 *	TransactionId Commit
 *	TransactionId Abort
 *	TransactionId SetInProgress
 * --------------------------------
 */

void
TransactionIdCommit(transactionId)
    TransactionId transactionId;
{
    if (AMI_OVERRIDE)
	return;
    
    TransactionLogUpdate(transactionId, XID_COMMIT);
}

void
TransactionIdAbort(transactionId)
    TransactionId transactionId;
{
    if (AMI_OVERRIDE)
	return;
    
    TransactionLogUpdate(transactionId, XID_ABORT);
}

void
TransactionIdSetInProgress(transactionId)
    TransactionId transactionId;
{
    if (AMI_OVERRIDE)
	return;
    
    TransactionLogUpdate(transactionId, XID_INPROGRESS);
}
