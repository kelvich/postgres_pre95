/* ----------------------------------------------------------------
 *   FILE
 *	xact.c
 *	
 *   DESCRIPTION
 *	top level transaction system support routines
 *
 *   INTERFACE ROUTINES
 *	StartTransactionCommand
 *	CommitTransactionCommand
 *	AbortCurrentTransaction
 *	
 *	StartTransactionBlock
 *	CommitTransactionBlock
 *	AbortTransactionBlock
 *	
 *   NOTES
 *	This file is an attempt at a redesign of the upper layer
 *	of the V1 transaction system which was too poorly thought
 *	out to describe.  This new system hopes to be both simpler
 *	in design, simpler to extend and needs to contain added
 *	functionality to solve problems beyond the scope of the V1
 *	system.  (In particuler, communication of transaction
 *	information between parallel backends has to be supported)
 *
 *	The essential aspects of the transaction system are:
 *
 *		o  transaction id generation
 *		o  transaction log updating
 *		o  memory cleanup
 *		o  cache invalidation
 *		o  lock cleanup
 *
 *	Hence, the functional division of the transaction code is
 *	based on what of the above things need to be done during
 *	a start/commit/abort transaction.  For instance, the
 *	routine AtCommit_Memory() takes care of all the memory
 *	cleanup stuff done at commit time.
 *
 *	The code is layered as follows:
 *
 *		StartTransaction
 *		CommitTransaction
 *		AbortTransaction
 *
 *	are provided to do the lower level work like recording
 *	the transaction status in the log and doing memory cleanup.
 *	above these routines are another set of functions:
 *
 *		StartTransactionCommand
 *		CommitTransactionCommand
 *		AbortCurrentTransaction
 *
 *	These are the routines used in the postgres main processing
 *	loop.  They are sensitive to the current transaction block state
 *	and make calls to the lower level routines appropriately.
 *
 *	Support for transaction blocks is provided via the functions:
 *
 *		StartTransactionBlock
 *		CommitTransactionBlock
 *		AbortTransactionBlock
 *
 *	These are invoked only in responce to a user "BEGIN", "END",
 *	or "ABORT" command.  The tricky part about these functions
 *	is that they are called within the postgres main loop, in between
 *	the StartTransactionCommand() and CommitTransactionCommand().
 *
 *	For example, consider the following sequence of user commands:
 *
 *	1)	begin
 *	2)	retrieve (foo.all)
 *	3)	append foo (bar = baz)
 *	4)	end
 *
 *	in the main processing loop, this results in the following
 *	transaction sequence:
 *
 *	    /	StartTransactionCommand();
 *	1) /	ProcessUtility(); 		<< begin
 *	   \	    StartTransactionBlock();
 *	    \	CommitTransactionCommand();
 *
 *	    /	StartTransactionCommand();
 *	2) <	ProcessQuery();			<< retrieve (foo.all)
 *	    \	CommitTransactionCommand();
 *
 *	    /	StartTransactionCommand();
 *	3) <	ProcessQuery();			<< append foo (bar = baz)
 *	    \	CommitTransactionCommand();
 *
 *	    /	StartTransactionCommand();
 *	4) /	ProcessUtility(); 		<< end
 *	   \	    CommitTransactionBlock();
 *	    \	CommitTransactionCommand();
 *
 *	The point of this example is to demonstrate the need for
 *	StartTransactionCommand() and CommitTransactionCommand() to
 *	be state smart -- they should do nothing in between the calls
 *	to StartTransactionBlock() and EndTransactionBlock() and
 *      outside these calls they need to do normal start/commit
 *	processing.
 *
 *	Furthermore, suppose the "retrieve (foo.all)" caused an abort
 *	condition.  We would then want to abort the transaction and
 *	ignore all subsequent commands up to the "end".
 *	-cim 3/23/90
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "xact.h"
 RcsId("$Header$");

/* ----------------
 *	global variable holding the current transaction state
 * ----------------
 */
TransactionStateData CurrentTransactionStateData = {
    { 0 },			/* transaction id data */
    FirstCommandId,		/* command id */
    0x0,			/* start time */
    TRANS_DEFAULT,		/* transaction state */
    TBLOCK_DEFAULT		/* transaction block state */
};

/* ----------------
 *	info returned when the system is desabled
 *
 *	Note:  I have no idea what the significance of the
 *	       1073741823 in DisabledStartTime.. I just carried
 *	       this over when converting things from the old
 *	       V1 transaction system.  -cim 3/18/90
 * ----------------
 */
TransactionIdData DisabledTransactionIdData = {
    '\177', '\177', '\177', '\177', '\177'
};

CommandId DisabledCommandId =
    (CommandId) '\177';

Time DisabledStartTime =
    (Time) 1073741823;

/* ----------------
 *	overflow flag
 * ----------------
 */
bool CommandIdCounterOverflowFlag;

/* ----------------
 *	catalog creation transaction bootstrapping flag.
 *	This should be eliminated and added to the transaction
 *	state stuff.  -cim 3/19/90
 * ----------------
 */
bool AMI_OVERRIDE = false;

/* ----------------------------------------------------------------
 *		     transaction state accessors
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	IsTransactionState
 *
 *	This returns true if we are currently running a query
 *	within an executing transaction.
 * --------------------------------
 */
bool
IsTransactionState()
{
    TransactionState s = &CurrentTransactionStateData;

    switch (s->state) {
    case TRANS_DEFAULT:		return false;
    case TRANS_START:		return true;
    case TRANS_INPROGRESS:	return true;
    case TRANS_COMMIT:		return true;
    case TRANS_ABORT:		return true;
    case TRANS_DISABLED:	return false;
    }
}

/* --------------------------------
 *	OverrideTransactionSystem
 *
 *	This is used to temporarily disable the transaction
 *	processing system in order to do initialization of
 *	the transaction system data structures and relations
 *	themselves.
 * --------------------------------
 */
int SavedTransactionState;

void
OverrideTransactionSystem(flag)
    bool flag;
{
    TransactionState s = &CurrentTransactionStateData;

    if (flag == true) {
	if (s->state == TRANS_DISABLED)
	    return;

	SavedTransactionState = s->state;
	s->state = TRANS_DISABLED;
    } else {
	if (s->state != TRANS_DISABLED)
	    return;

	s->state = SavedTransactionState;
    }
}

/* --------------------------------
 *	GetCurrentTransactionId
 *
 *	This returns the id of the current transaction, or
 *	the id of the "disabled" transaction.
 * --------------------------------
 */
TransactionId
GetCurrentTransactionId()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	if the transaction system is disabled, we return
     *  the special "disabled" transaction id.
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return (TransactionId) &DisabledTransactionIdData;

    /* ----------------
     *	otherwise return the current transaction id.
     * ----------------
     */
    return (TransactionId) &(s->transactionIdData);
}


/* --------------------------------
 *	GetCurrentCommandId
 * --------------------------------
 */
CommandId
GetCurrentCommandId()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	if the transaction system is disabled, we return
     *  the special "disabled" command id.
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return (CommandId) DisabledCommandId;

    return s->commandId;
}


/* --------------------------------
 *	GetCurrentTransactionStartTime
 * --------------------------------
 */
Time
GetCurrentTransactionStartTime()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	if the transaction system is disabled, we return
     *  the special "disabled" starting time.
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return (Time) DisabledStartTime;
    
    return s->startTime;
}
    

/* --------------------------------
 *	TransactionIdIsCurrentTransactionId
 * --------------------------------
 */
bool
TransactionIdIsCurrentTransactionId(xid)
    TransactionId	xid;
{
    TransactionState s = &CurrentTransactionStateData;
    
    if (AMI_OVERRIDE)
	return false;

    return (bool)
	TransactionIdEquals(xid, &(s->transactionIdData));
}


/* --------------------------------
 *	CommandIdIsCurrentCommandId
 * --------------------------------
 */
bool
CommandIdIsCurrentCommandId(cid)
    CommandId cid;
{
    TransactionState s = &CurrentTransactionStateData;
    
    if (AMI_OVERRIDE)
	return false;

    return 	
	(AsUint8(cid) == AsUint8(s->commandId)) ? true : false;
}


/* --------------------------------
 *	ClearCommandIdCounterOverflowFlag
 * --------------------------------
 */
void
ClearCommandIdCounterOverflowFlag()
{
    CommandIdCounterOverflowFlag = false;
}


/* --------------------------------
 *	CommandCounterIncrement
 * --------------------------------
 */
void	
CommandCounterIncrement()
{
    CurrentTransactionStateData.commandId += 1;
    if (CurrentTransactionStateData.commandId == FirstCommandId) {
	CommandIdCounterOverflowFlag = true;
    }
}

/* ----------------------------------------------------------------
 *		        initialization stuff
 * ----------------------------------------------------------------
 */

void
InitializeTransactionSystem()
{
    InitializeTransactionLog();
}

/* ----------------------------------------------------------------
 *		        StartTransaction stuff
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	AtStart_Cache
 * --------------------------------
 */
void
AtStart_Cache()    
{
    DiscardInvalid();
}

/* --------------------------------
 *	AtStart_Locks
 * --------------------------------
 */
void
AtStart_Locks()    
{
    /* at present, it is unknown to me what belongs here -cim 3/18/90 */
}

/* --------------------------------
 *	AtStart_Memory
 * --------------------------------
 */
void
AtStart_Memory()    
{
    Portal	     portal;
    MemoryContext    portalContext;

    /* ----------------
     *	get the blank portal and its memory context
     * ----------------
     */
    portal = GetPortalByName(NULL);
    portalContext = (MemoryContext) PortalGetHeapMemory(portal);

    /* ----------------
     *	tell system to allocate in the blank portal context
     * ----------------
     */
    (void) MemoryContextSwitchTo(portalContext);
    StartPortalAllocMode(DefaultAllocMode, 0);
}


/* ----------------------------------------------------------------
 *		        CommitTransaction stuff
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	RecordTransactionCommit
 *
 *	Note: the two calls to BufferManagerFlush() exist to ensure
 *	      that data pages are written before log pages.  These
 *	      explicit calls should be replaced by a more efficient
 *	      ordered page write scheme in the buffer manager
 *	      -cim 3/18/90
 * --------------------------------
 */
void
RecordTransactionCommit()    
{
    TransactionId xid;

    /* ----------------
     *	get the current transaction id
     * ----------------
     */
    xid = GetCurrentTransactionId();
    
    /* ----------------
     *	flush the buffer manager pages.  Note: if we have stable
     *  main memory this should be a no-op.
     * ----------------
     */
    BufferManagerFlush();
    
    /* ----------------
     *	have the transaction access methods record the status
     *  of this transaction id in the pg_log / pg_time relations.
     * ----------------
     */
    TransactionIdCommit(xid);
}


/* --------------------------------
 *	AtCommit_Cache
 * --------------------------------
 */
void
AtCommit_Cache()
{
    /* ----------------
     * Make catalog changes visible to me for the next command.
     * Other backends will not process my invalidation messages until
     * after I commit and free my locks--though they will do
     * unnecessary work if I abort.
     * ----------------
     */
    RegisterInvalid(true);
}

/* --------------------------------
 *	AtCommit_Locks
 * --------------------------------
 */
void
AtCommit_Locks()  
{
    TransactionId xid;

    /* ----------------
     *	get the current transaction id
     * ----------------
     */
    xid = GetCurrentTransactionId();

    /* ----------------
     *	XXX What if LMLockReleaseAll fails?  (race condition?) 
     * ----------------
     */
    LMLockReleaseAll(PageLockTableId, xid);
    LMLockReleaseAll(MultiLevelLockTableId, xid);

    /* ----------------
     *	XXX should be in lmgr
     * ----------------
     */
    ImmediateReleaseBufSem();
}

/* --------------------------------
 *	AtCommit_Memory
 * --------------------------------
 */
void
AtCommit_Memory()  
{
    /* ----------------
     *	now that we're "out" of a transaction, have the
     *  system allocate things in the top memory context instead
     *  of the blank portal memory context.
     * ----------------
     */
    EndPortalAllocMode();
    (void) MemoryContextSwitchTo(TopMemoryContext);
}

/* ----------------------------------------------------------------
 *		        AbortTransaction stuff
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	RecordTransactionAbort
 * --------------------------------
 */
void
RecordTransactionAbort()    
{
    TransactionId xid;

    /* ----------------
     *	get the current transaction id
     * ----------------
     */
    xid = GetCurrentTransactionId();
    
    /* ----------------
     *	have the transaction access methods record the status
     *  of this transaction id in the pg_log / pg_time relations.
     * ----------------
     */
    TransactionIdAbort(xid);
    
    /* ----------------
     *	flush the buffer manager pages.  Note: if we have stable
     *  main memory this should be a no-op.
     * ----------------
     */
    BufferManagerFlush();
}

/* --------------------------------
 *	AtAbort_Cache
 * --------------------------------
 */
void
AtAbort_Cache()    
{
    RegisterInvalid(false);
}

/* --------------------------------
 *	AtAbort_Locks
 * --------------------------------
 */
void
AtAbort_Locks()    
{
    TransactionId xid;

    /* ----------------
     *	get the current transaction id
     * ----------------
     */
    xid = GetCurrentTransactionId();
        
    /* ----------------
     *	XXX What if LMLockReleaseAll fails?  (race condition?)
     * ----------------
     */
    LMLockReleaseAll(PageLockTableId, xid);
    LMLockReleaseAll(MultiLevelLockTableId, xid);

    /* ----------------
     *  XXX should be in lmgr
     * ----------------
     */
    ImmediateReleaseBufSem();
}


/* --------------------------------
 *	AtAbort_Memory
 * --------------------------------
 */
void
AtAbort_Memory()    
{
    /* ----------------
     *	after doing an abort transaction, make certain the
     *  system uses the top memory context rather then the
     *  portal memory context (unitl the next transaction).
     * ----------------
     */
    (void) MemoryContextSwitchTo(TopMemoryContext);
}

/* ----------------------------------------------------------------
 *			interface routines
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	StartTransaction
 *
 * --------------------------------
 */
void
StartTransaction()
{
    TransactionState s = &CurrentTransactionStateData;
    extern bool	TransactionInitWasProcessed;	/* XXX style */

    /* ----------------
     *	check the current transaction state
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return;
    
    if (s->state != TRANS_DEFAULT)
	elog(NOTICE, "StartTransaction and not in default state");
    
    /* ----------------
     *	set the current transaction state information
     *  appropriately during start processing
     * ----------------
     */
    s->state = TRANS_START;
    
    /* ----------------
     *	generate a new transaction id
     * ----------------
     */
    GetNewTransactionId(&(s->transactionIdData));
    
    /* ----------------
     *	initialize current transaction state fields
     * ----------------
     */
    s->commandId = 		FirstCommandId;
    s->startTime = 		GetCurrentTime();

    /* ----------------
     *	XXX where should we do this? someplace around transaction
     *      initialization but I don't understand its real purpose.
     *	    -cim 3/19/90
     * ----------------
     */
    if (! TransactionInitWasProcessed) {
	InitMyDatabaseId();
	TransactionInitWasProcessed = true;
    }

    /* ----------------
     *	initialize the various transaction subsystems
     * ----------------
     */
    AtStart_Cache();
    AtStart_Locks();
    AtStart_Memory();

    /* ----------------
     *	done with start processing, set current transaction
     *  state to "in progress"
     * ----------------
     */
    s->state = TRANS_INPROGRESS;      
}

/* --------------------------------
 *	CommitTransaction
 *
 * --------------------------------
 */
void
CommitTransaction()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	check the current transaction state
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return;
    
    if (s->state != TRANS_INPROGRESS)
	elog(NOTICE, "CommitTransaction and not in in-progress state ");
    
    /* ----------------
     *	set the current transaction state information
     *  appropriately during the abort processing
     * ----------------
     */
    s->state = TRANS_COMMIT;

    /* ----------------
     *	do commit processing
     * ----------------
     */
    RecordTransactionCommit();
    AtCommit_Cache();
    AtCommit_Locks();
    AtCommit_Memory();

    /* ----------------
     *	done with commit processing, set current transaction
     *  state back to default
     * ----------------
     */
    s->state = TRANS_DEFAULT;    
}

/* --------------------------------
 *	AbortTransaction
 *
 * --------------------------------
 */
void
AbortTransaction()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	check the current transaction state
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return;
    
    if (s->state != TRANS_INPROGRESS)
	elog(NOTICE, "AbortTransaction and not in in-progress state ");
    
    /* ----------------
     *	set the current transaction state information
     *  appropriately during the abort processing
     * ----------------
     */
    s->state = TRANS_ABORT;
        
    /* ----------------
     *	do abort processing
     * ----------------
     */
    RecordTransactionAbort();
    AtAbort_Cache();
    AtAbort_Locks();
    AtAbort_Memory();

    /* ----------------
     *	done with abort processing, set current transaction
     *  state back to default
     * ----------------
     */
    s->state = TRANS_DEFAULT;
}

/* ----------------------------------------------------------------
 *	transaction system interface functions.   these provide
 *	support for transaction blocks above the start / abort /
 *	commit transaction routines above.
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	StartTransactionCommand
 * --------------------------------
 */
void
StartTransactionCommand()
{
    TransactionState s = &CurrentTransactionStateData;

    switch(s->blockState) {
	/* ----------------
	 *	if we aren't in a transaction block, we
	 *	just do our usual start transaction.
	 * ----------------
	 */
    case TBLOCK_DEFAULT:
	StartTransaction();
	break;
	
	/* ----------------
	 *	We should never experience this -- if we do it
	 *	means the BEGIN state was not changed in the previous
	 *	CommitTransactionCommand().  If we get it, we print
	 *	a warning and change to the in-progress state.
	 * ----------------
	 */
    case TBLOCK_BEGIN:
	elog(NOTICE, "StartTransactionCommand: unexpected TBLOCK_BEGIN");
	s->blockState = TBLOCK_INPROGRESS;
	break;
	
	/* ----------------
	 *	This is the case when are somewhere in a transaction
	 *	block and about to start a new command.  For now we
	 *	do nothing but someday we may do command-local resource
	 *	initialization.
	 * ----------------
	 */
    case TBLOCK_INPROGRESS:
	break;

	/* ----------------
	 *	As with BEGIN, we should never experience this --
	 *	if we do it means the END state was not changed in the
	 *	previous CommitTransactionCommand().  If we get it, we
	 *	print a warning, commit the transaction, start a new
	 *	transaction and change to the default state.
	 * ----------------
	 */
    case TBLOCK_END:
	elog(NOTICE, "StartTransactionCommand: unexpected TBLOCK_END");
	s->blockState = TBLOCK_DEFAULT;
	CommitTransaction();
	StartTransaction();
	break;

	/* ----------------
	 *	Here we are in the middle of a transaction block but
	 *	one of the commands caused an abort so we do nothing
	 *	but remain in the abort state.  Eventually we will get
	 *	to the "END TRANSACTION" which will set things straight.
	 * ----------------
	 */
    case TBLOCK_ABORT:
	break;	

	/* ----------------
	 *	This means we somehow aborted and the last call to
	 *	CommitTransactionCommand() didn't clear the state so
	 *	we remain in the ENDABORT state and mabey next time
	 *	we get to CommitTransactionCommand() the state will
	 *	get reset to default.
	 * ----------------
	 */
    case TBLOCK_ENDABORT:
	elog(NOTICE, "StartTransactionCommand: unexpected TBLOCK_ENDABORT");
	break;	
    }	
}
/* --------------------------------
 *	CommitTransactionCommand
 * --------------------------------
 */
void
CommitTransactionCommand()
{
    TransactionState s = &CurrentTransactionStateData;

    switch(s->blockState) {
	/* ----------------
	 *	if we aren't in a transaction block, we
	 *	just do our usual transaction commit
	 * ----------------
	 */
    case TBLOCK_DEFAULT:
	CommitTransaction();
	break;

	/* ----------------
	 *	This is the case right after we get a "BEGIN TRANSACTION"
	 *	command, but the user hasn't done anything else yet, so
	 *	we change to the "transaction block in progress" state
	 *	and return.   
	 * ----------------
	 */
    case TBLOCK_BEGIN:
	s->blockState = TBLOCK_INPROGRESS;
	break;

	/* ----------------
	 *	This is the case when we have finished executing a command
	 *	someplace within a transaction block.  We increment the
	 *	command counter and return.  Someday we may free resources
	 *	local to the command.
	 * ----------------
	 */
    case TBLOCK_INPROGRESS:
	CommandCounterIncrement();
	break;

	/* ----------------
	 *	This is the case when we just got the "END TRANSACTION"
	 *	statement, so we go back to the default state and
	 *	commit the transaction.
	 * ----------------
	 */
    case TBLOCK_END:
	s->blockState = TBLOCK_DEFAULT;
	CommitTransaction();
	break;
	
	/* ----------------
	 *	Here we are in the middle of a transaction block but
	 *	one of the commands caused an abort so we do nothing
	 *	but remain in the abort state.  Eventually we will get
	 *	to the "END TRANSACTION" which will set things straight.
	 * ----------------
	 */
    case TBLOCK_ABORT:
	break;
	
	/* ----------------
	 *	Here we were in an aborted transaction block which
	 *      just processed the "END TRANSACTION" command from the
	 *	user, so now we return the to default state.
	 * ----------------
	 */
    case TBLOCK_ENDABORT:
	s->blockState = TBLOCK_DEFAULT;  
	break;
    }    
}

/* --------------------------------
 *	AbortCurrentTransaction
 * --------------------------------
 */
void
AbortCurrentTransaction()
{
    TransactionState s = &CurrentTransactionStateData;
    
    switch(s->blockState) {
	/* ----------------
	 *	if we aren't in a transaction block, we
	 *	just do our usual abort transaction.
	 * ----------------
	 */
    case TBLOCK_DEFAULT:
	AbortTransaction();
	break;

	/* ----------------
	 *	If we are in the TBLOCK_BEGIN it means something
	 *	screwed up right after reading "BEGIN TRANSACTION"
	 *	so we enter the abort state.  Eventually an "END
	 *      TRANSACTION" will fix things.
	 * ----------------
	 */
    case TBLOCK_BEGIN:
	s->blockState = TBLOCK_ABORT;
	AbortTransaction();
	break;
		
	/* ----------------
	 *	This is the case when are somewhere in a transaction
	 *	block which aborted so we abort the transaction and
	 *	set the ABORT state.  Eventually an "END TRANSACTION"
	 *	will fix things and restore us to a normal state.
	 * ----------------
	 */
    case TBLOCK_INPROGRESS:
	s->blockState = TBLOCK_ABORT;
	AbortTransaction();
	break;

	/* ----------------
	 *	Here, the system was fouled up just after the
	 *	user wanted to end the transaction block so we
	 *	abort the transaction and put us back into the
	 *	default state.
	 * ----------------
	 */
    case TBLOCK_END:
	s->blockState = TBLOCK_DEFAULT;
	AbortTransaction();
	break;

	/* ----------------
	 *	Here, we are already in an aborted transaction
	 *	state and are waiting for an "END TRANSACTION" to
	 *	come along and lo and behold, we abort again!
	 *	So we just remain in the abort state.
	 * ----------------
	 */
    case TBLOCK_ABORT:
	break;
	
	/* ----------------
	 *	Here we were in an aborted transaction block which
	 *      just processed the "END TRANSACTION" command but somehow
	 *	aborted again.. since we must have done the abort
	 *      processing, we return to the default state.
	 * ----------------
	 */
    case TBLOCK_ENDABORT:
	s->blockState = TBLOCK_DEFAULT;  
	break;
    }
}

/* ----------------------------------------------------------------
 *		       transaction block support
 * ----------------------------------------------------------------
 */
/* --------------------------------
 *	BeginTransactionBlock
 * --------------------------------
 */
void
BeginTransactionBlock()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	check the current transaction state
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return;
    
    if (s->blockState != TBLOCK_DEFAULT)
	elog(NOTICE, "BeginTransactionBlock and not in default state ");
    
    /* ----------------
     *	set the current transaction block state information
     *  appropriately during begin processing
     * ----------------
     */
    s->blockState = TBLOCK_BEGIN;
    
    /* ----------------
     *	do begin processing
     * ----------------
     */
    
    /* ----------------
     *	done with begin processing, set block state to inprogress
     * ----------------
     */
    s->blockState = TBLOCK_INPROGRESS;    
}

/* --------------------------------
 *	EndTransactionBlock
 * --------------------------------
 */
void
EndTransactionBlock()
{
    TransactionState s = &CurrentTransactionStateData;
    
    /* ----------------
     *	check the current transaction state
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return;
    
    if (s->blockState == TBLOCK_INPROGRESS) {
	/* ----------------
	 *  here we are in a transaction block which should commit
	 *  when we get to the upcoming CommitTransactionCommand()
	 *  so we set the state to "END".  CommitTransactionCommand()
	 *  will recognize this and commit the transaction and return
	 *  us to the default state
	 * ----------------
	 */
	s->blockState = TBLOCK_END;
	return;
    }

    if (s->blockState == TBLOCK_ABORT) {
	/* ----------------
	 *  here, we are in a transaction block which aborted
	 *  and since the AbortTransaction() was already done,
	 *  we do whatever is needed and change to the special
	 *  "END ABORT" state.  The upcoming CommitTransactionCommand()
	 *  will recognise this and then put us back in the default
	 *  state.
	 * ----------------
	 */
	s->blockState = TBLOCK_ENDABORT;
	return;
    }
    
    /* ----------------
     *	We should not get here, but if we do, we go to the ENDABORT
     *  state after printing a warning.  The upcoming call to
     *  CommitTransactionCommand() will then put us back into the
     *  default state.
     * ----------------
     */
    elog(NOTICE, "EndTransactionBlock and not inprogress/abort state ");
    s->blockState = TBLOCK_ENDABORT;
}

/* --------------------------------
 *	AbortTransactionBlock
 * --------------------------------
 */
void
AbortTransactionBlock()
{
    TransactionState s = &CurrentTransactionStateData;

    /* ----------------
     *	check the current transaction state
     * ----------------
     */
    if (s->state == TRANS_DISABLED)
	return;
    
    if (s->blockState == TBLOCK_INPROGRESS) {
	/* ----------------
	 *  here we were inside a transaction block and we
	 *  got an abort command from the user, so we move to
	 *  the abort state, do the abort processing and
	 *  then return.   We remain in the abort state until
	 *  the user explicitly gives us an END TRANSACTION.
	 *  Hence, BEGIN and END will always match.
	 * ----------------
	 */
	s->blockState = TBLOCK_ABORT;
    
	/* ----------------
	 *  do abort processing and return
	 * ----------------
	 */
	AbortTransaction();
	return;
    }

    /* ----------------
     *	this case should not be possible, because it would mean
     *  the user entered an "abort" from outside a transaction block.
     *  So we print an error message, abort the transaction and
     *  enter the "ENDABORT" state so we will end up in the default
     *  state after the upcoming CommitTransactionCommand().
     * ----------------
     */
    elog(NOTICE, "AbortTransactionBlock and not inprogress state");
    AbortTransaction();
    s->blockState = TBLOCK_ENDABORT;
}

/* ----------------------------------------------------------------
 *	traps to catch routines using these old routines --
 *	anything using these should be rewritten.
 * ----------------------------------------------------------------
 */

void
UserErrorEndWithoutBegin()
{
    elog(DEBUG, "UserErrorEndWithoutBegin obsoleted");
    abort();
}

void
UserErrorBeginAfterBegin()
{
    elog(DEBUG, "UserErrorBeginAfterBegin obsoleted");
    abort();
}

void
UserErrorAbortWithoutBegin()
{
    elog(DEBUG, "UserErrorAbortWithoutBegin obsoleted");
    abort();
}

void
InternalErrorIllegalStateTransition()
{
    elog(DEBUG, "InternalErrorIllegalStateTransition obsoleted");
    abort();
}

void
FsaMachine() 
{
    elog(DEBUG, "FsaMachine( obsoleted");
    abort();
}

void
InitializeTransactionState()
{
    elog(DEBUG, "InitializeTransactionState obsoleted");
    abort();
}

void
OverrideTransactionState()
{
    elog(DEBUG, "OverrideTransactionState obsoleted");
    abort();
}

bool
IsBlockTransactionState()
{
    elog(DEBUG, "IsBlockTransactionState obsoleted");
    abort();
}

bool
IsOverrideTransactionState()
{
    elog(DEBUG, "IsOverrideTransactionState obsoleted");
    abort();
}

void
StartTransactionStateBlock()
{
    elog(DEBUG, "StartTransactionStateBlock obsoleted");
    abort();
}

void
StartTransactionStateCommand()
{
    elog(DEBUG, "StartTransactionStateCommand obsoleted");
    abort();
}

void
CommitTransactionStateCommand()
{
    elog(DEBUG, "CommitTransactionStateCommand obsoleted");
    abort();
}

void
CommitTransactionStateBlock()
{
    elog(DEBUG, "CommitTransactionStateBlock obsoleted");
    abort();
}

void
AbortCurrentTransactionState()
{
    elog(DEBUG, "AbortCurrentTransactionState obsoleted");
    abort();
}

void
AbortTransactionStateBlock()
{
    elog(DEBUG, "AbortTransactionStateBlock obsoleted");
    abort();
}

void
StartTransactionBlock()
{
    elog(DEBUG, "StartTransactionBlock obsoleted");
    abort();
}

void
CommitTransactionBlock()
{	
    elog(DEBUG, "CommitTransactionBlock obsoleted");
    abort();
}
