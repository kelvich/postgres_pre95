/* ----------------------------------------------------------------
 *   FILE
 *	async.c
 *	
 *   DESCRIPTION
 *	Asynchronous notification
 *
 *   INTERFACE ROUTINES
 *      void Async_Notify(char *relname);
 *      void Async_Listen(char *relname,int pid);
 *      void Async_Unlisten(char *relname, int pid);
 *
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */
/* New Async Notification Model:
 * 1. Multiple backends on same machine.  Multiple backends listening on
 *    one relation.
 *
 * 2. One of the backend does a 'notify <relname>'.  For all backends that
 *    are listening to this relation (all notifications take place at the
 *    end of commit),
 *    2.a  If the process is the same as the backend process that issued
 *         notification (we are notifying something that we are listening),
 *         signal the corresponding frontend over the comm channel using the
 *         out-of-band channel.
 *    2.b  For all other listening processes, we send kill(2) to wake up
 *         the listening backend.
 * 3. Upon receiving a kill(2) signal from another backend process notifying
 *    that one of the relation that we are listening is being notified,
 *    we can be in either of two following states:
 *    3.a  We are sleeping, wake up and signal our frontend.
 *    3.b  We are in middle of another transaction, wait until the end of
 *         of the current transaction and signal our frontend.
 * 4. Each frontend receives this notification and prcesses accordingly.
 *
 * -- jw, 12/28/93
 *
 */
/*
 * The following is the old model which does not work.
 */
/*
 * Model is:
 * 1. Multiple backends on same machine.
 *
 * 2. Query on one backend sends stuff over an asynchronous portal by 
 *    appending to a relation, and then doing an async. notification
 *    (which takes place after commit) to all listeners on this relation.
 *
 * 3. Async. notification results in all backends listening on relation 
 *    to be woken up, by a process signal kill(2), with name of relation
 *    passed in shared memory.
 *
 * 4. Each backend notifies its respective frontend over the comm
 *    channel using the out-of-band channel.
 *
 * 5. Each frontend receives this notification and processes accordingly.
 *
 * #4,#5 are changing soon with pending rewrite of portal/protocol.
 *
 */


#include <strings.h>
#include <signal.h>
#include <errno.h>
#include "tmp/postgres.h"

RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) postgres.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "access/attnum.h"
#include "access/ftup.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/xact.h"

#include "commands/copy.h"
#include "storage/buf.h"
#include "storage/itemptr.h"
#include "tmp/miscadmin.h"
#include "tmp/portal.h"
#include "utils/excid.h"
#include "utils/log.h"
#include "utils/mcxt.h"
#include "utils/palloc.h"
#include "utils/rel.h"

#include "nodes/pg_lisp.h"
#include "tcop/dest.h"
#include "commands/command.h"

#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_relation.h"
#include "catalog/pg_type.h"
#include "catalog/pg_listener.h"

#include "executor/execdefs.h"
#include "executor/execdesc.h"

#include "tmp/simplelists.h"

#define INIT_NOTIFY_LIST \
  if (!initialized) { \
    SLNewList(&pendingNotifies, offsetof(PendingNotifyNode, node)); \
    initialized = 1; \
  }

typedef struct {
  NameData relname;
  SLNode node;
} PendingNotifyNode;

static int notifyFrontEndPending = 0;
static int notifyIssued = 0;
static int initialized = 0;
static SLList pendingNotifies;      

void Async_NotifyHandler(void);
void Async_Notify(char *);
void Async_NotifyAtCommit(void);
void Async_NotifyAtAbort(void);
void Async_Listen(char *, int);
void Async_Unlisten(char *, int);
void Async_NotifyFrontEnd(void);
static int AsyncExistsPendingNotify(char *);
static void ClearPendingNotify(void);

/*
 *--------------------------------------------------------------
 * Async_NotifyHandler --
 *
 *      This is the signal handler for SIGUSR2.  When the backend
 *      is signaled, the backend can be in two states.
 *      1. If the backend is in the middle of another transaction,
 *         we set the flag, notifyFrontEndPending, and wait until
 *         the end of the transaction to notify the front end.
 *      2. If the backend is not int the middle of another transaction,
 *         we notify the front end immediately.
 *
 *      -- jw, 12/28/93
 * Results:
 *      none
 *
 * Side effects:
 *      none
 */
void Async_NotifyHandler()
{
  extern TransactionState CurrentTransactionState;

  if ((CurrentTransactionState->state == TRANS_DEFAULT) ||
      (CurrentTransactionState->blockState == TRANS_DEFAULT)) {
    elog(DEBUG, "Waking up sleeping backend process");
    Async_NotifyFrontEnd();
  } else {
    elog(DEBUG, "Process is in the middle of another transaction, state = %d, block state = %d",
	 CurrentTransactionState->state,
	 CurrentTransactionState->blockState);
    notifyFrontEndPending = 1;
  }
}

/*
 *--------------------------------------------------------------
 * Async_Notify --
 *
 *      Adds the relation to the list of pending notifies.
 *      All notification happens at end of commit.
 *      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 *      All notification of backend processes happens here,
 *      then each backend notifies its corresponding front end at
 *      the end of commit.
 *
 *      This correspond to 'notify <relname>' postquel command
 *      -- jw, 12/28/93
 *
 * Results:
 *      XXX
 *
 * Side effects:
 *      All tuples for relname in pg_listener are updated.
 *
 *--------------------------------------------------------------
 */
void Async_Notify(char *relname)
{

  HeapTuple lTuple, rTuple;
  Relation lRel;
  HeapScanDesc sRel;
  TupleDescriptor tdesc;
  ScanKeyData key;
  Buffer b;
  Datum d, value[3];
  int isnull;
  char repl[3], nulls[3];

  PendingNotifyNode *notify;
  
  elog(DEBUG,"Async_Notify: %s",relname);

  INIT_NOTIFY_LIST;
  notify = (PendingNotifyNode *) malloc(sizeof(PendingNotifyNode));
  bzero(notify->relname.data, sizeof(NameData));
  namestrcpy(notify->relname, relname);
  SLNewNode(&notify->node);
  SLAddHead(&pendingNotifies, &notify->node);
  
  ScanKeyEntryInitialize(&key.data[0], 0,
			 Anum_pg_listener_relname,
			 Character16EqualRegProcedure,
			 NameGetDatum(notify->relname.data));
  
  lRel = heap_openr(Name_pg_listener);
  tdesc = RelationGetTupleDescriptor(lRel);
  sRel = heap_beginscan(lRel, 0, NowTimeQual, 1, &key);
  
  nulls[0] = nulls[1] = nulls[2] = ' ';
  repl[0] = repl[1] = repl[2] = ' ';
  repl[Anum_pg_listener_notify - 1] = 'r';
  value[0] = value[1] = value[2] = (Datum) 0;
  value[Anum_pg_listener_notify - 1] = Int32GetDatum(1);
  
  while (HeapTupleIsValid(lTuple = heap_getnext(sRel, 0, &b))) {
    d = (Datum) heap_getattr(lTuple, b, Anum_pg_listener_notify,
			     tdesc, &isnull);
    if (!DatumGetInt32(d)) {
      rTuple = heap_modifytuple(lTuple, b, lRel, value, nulls, repl);
      (void) heap_replace(lRel, &lTuple->t_ctid, rTuple);
    }
    ReleaseBuffer(b);
  }
  heap_endscan(sRel);
  heap_close(lRel);
  notifyIssued = 1;
}

/*
 *--------------------------------------------------------------
 * Async_NotifyAtCommit --
 *
 *      Signal our corresponding frontend process on relations that
 *      were notified.  Signal all other backend process that
 *      are listening also.
 *
 *      -- jw, 12/28/93
 *
 * Results:
 *      XXX
 *
 * Side effects:
 *      Tuples in pg_listener that has our listenerpid are updated so
 *      that the notification is 0.  We do not want to notify frontend
 *      more than once.
 *
 *      -- jw, 12/28/93
 *
 *--------------------------------------------------------------
 */
void Async_NotifyAtCommit()
{
  HeapTuple lTuple;
  Relation lRel;
  HeapScanDesc sRel;
  TupleDescriptor tdesc;
  ScanKeyData key;
  Datum d;
  int ourpid, isnull;
  Buffer b;
  extern TransactionState CurrentTransactionState;

  INIT_NOTIFY_LIST;
  
  if ((CurrentTransactionState->state == TRANS_DEFAULT) ||
      (CurrentTransactionState->blockState == TRANS_DEFAULT)) {

    if (notifyIssued) {		/* 'notify <relname>' issued by us */
      notifyIssued = 0;
      StartTransactionCommand();
      elog(DEBUG, "Async_NotifyAtCommit.");
      ScanKeyEntryInitialize(key.data, 0,
			     Anum_pg_listener_notify,
			     Integer32EqualRegProcedure,
			     Int32GetDatum(1));
      lRel = heap_openr(Name_pg_listener);
      sRel = heap_beginscan(lRel, 0, NowTimeQual, 1, &key);
      tdesc = RelationGetTupleDescriptor(lRel);
      ourpid = getpid();
      
      while (HeapTupleIsValid(lTuple = heap_getnext(sRel, 0, &b))) {
	d = (Datum) heap_getattr(lTuple, b, Anum_pg_listener_relname,
				 tdesc, &isnull);
	
	if (AsyncExistsPendingNotify((char *) DatumGetPointer(d))) {
	  d = (Datum) heap_getattr(lTuple, b, Anum_pg_listener_pid,
				   tdesc, &isnull);
	  
	  if (ourpid == DatumGetInt32(d)) {
	    elog(DEBUG, "Notifying self, setting notifyFronEndPending to 1");
	    notifyFrontEndPending = 1;
	  } else {
	    elog(DEBUG, "Notifying others");
	    if (kill(DatumGetInt32(d), SIGUSR2) < 0) {
	      if (errno == ESRCH) {
		heap_delete(lRel, &lTuple->t_ctid);
	      }
	    }
	  }
	}
	ReleaseBuffer(b);
      }
      CommitTransactionCommand();
      ClearPendingNotify();
    }

    if (notifyFrontEndPending) {	/* we need to notify the frontend of
					   all pending notifies. */
      notifyFrontEndPending = 1;
      Async_NotifyFrontEnd();
    }
  }
}

/*
 *--------------------------------------------------------------
 * Async_NotifyAtAbort --
 *
 *      Gets rid of pending notifies.  List elements are automatically
 *      freed through memory context.
 *      
 *
 * Results:
 *      XXX
 *
 * Side effects:
 *      XXX
 *
 *--------------------------------------------------------------
 */
void
Async_NotifyAtAbort()
{
  extern TransactionState CurrentTransactionState;
  
  if (notifyIssued) {
    ClearPendingNotify();
  }
  notifyIssued = 0;
  SLNewList(&pendingNotifies, offsetof(PendingNotifyNode, node));
  initialized = 1;

  if ((CurrentTransactionState->state == TRANS_DEFAULT) ||
      (CurrentTransactionState->blockState == TRANS_DEFAULT)) {
    if (notifyFrontEndPending) { /* don't forget to notify front end */
      Async_NotifyFrontEnd();
    }
  }
}

/*
 *--------------------------------------------------------------
 * Async_Listen --
 *
 *      Register a backend (identified by its Unix PID) as listening
 *      on the specified relation.  
 *
 *      This corresponds to the 'listen <relation>' command in 
 *      postquel.
 *
 *      One listener per relation, pg_listener relation is keyed
 *      on (relname,pid) to provide multiple listeners in future.
 *
 * Results:
 *      pg_listeners is updated.
 *
 * Side effects:
 *      XXX
 *
 *--------------------------------------------------------------
 */
void Async_Listen(relname, pid)
     char *relname;
     int pid;
{
    Datum values[Natts_pg_listener];
    char nulls[Natts_pg_listener];
    TupleDescriptor tdesc;
    HeapScanDesc s;
    HeapTuple htup,tup;
    Relation lDesc;
    Buffer b;
    Datum d;
    int i, isnull;
    int alreadyListener = 0;
    int ourPid = getpid();
    char *relnamei;

    elog(DEBUG,"Async_Listen: %s",relname);
    for (i = 0 ; i < Natts_pg_listener; i++) {
        nulls[i] = ' ';
        values[i] = NULL;
    }
    
    i = 0;
    values[i++] = (Datum) relname;
    values[i++] = (Datum) pid;
    values[i++] = (Datum) 0;	/* no notifies pending */
    
    lDesc = heap_openr(Name_pg_listener);

    /* is someone already listening.  One listener per relation */
    tdesc = RelationGetTupleDescriptor(lDesc);
    s = heap_beginscan(lDesc,0,NowTimeQual,0,(ScanKey)NULL);
    while (HeapTupleIsValid(htup = heap_getnext(s,0,&b))) {
	d = (Datum) heap_getattr(htup,b,Anum_pg_listener_relname,tdesc,
				 &isnull);
	relnamei = DatumGetPointer(d);
	if (!strncmp(relnamei,relname, NAMEDATALEN)) {
	    d = (Datum) heap_getattr(htup,b,Anum_pg_listener_pid,tdesc,&isnull);
	    pid = (int) DatumGetPointer(d);
	    if (pid == ourPid) {
		alreadyListener = 1;
	    }
	}
	ReleaseBuffer(b);
    }
    heap_endscan(s);

    if (alreadyListener) {
      elog(NOTICE, "Async_Listen: We are already listening on %s",
	   relname);
      return;
    }

    tup = heap_formtuple(Natts_pg_listener,
			 &lDesc->rd_att,
			 values,
			 nulls);
    heap_insert(lDesc,tup,(double *)NULL);
    
    pfree((Pointer)tup);
/*    if (alreadyListener) {
	elog(NOTICE,"Async_Listen: already one listener on %s (possibly dead)",relname);
    }*/
    heap_close(lDesc);
   
}

/*
 *--------------------------------------------------------------
 * Async_Unlisten --
 *
 *      Remove the backend from the list of listening backends
 *      for the specified relation.
 *    
 *      This would correspond to the 'unlisten <relation>' 
 *      command in postquel, but there isn't one yet.
 *
 * Results:
 *      pg_listeners is updated.
 *
 * Side effects:
 *      XXX
 *
 *--------------------------------------------------------------
 */
void Async_Unlisten(relname,pid)
     char *relname;
     int pid;
{
    Relation lDesc;
    HeapTuple lTuple;
    lTuple = SearchSysCacheTuple(LISTENREL,relname,pid);
    lDesc = heap_openr(Name_pg_listener);
    if (lTuple != NULL) {
	heap_delete(lDesc,&lTuple->t_ctid);
    }
    heap_close(lDesc);
}

/*
 * --------------------------------------------------------------
 * Async_NotifyFrontEnd --
 *
 *      Perform an asynchronous notification to front end over
 *      portal comm channel.  The name of the relation which contains the
 *      data is sent to the front end.
 *
 *      We remove the notification flag from the pg_listener tuple
 *      associated with our process.
 *
 * Results:
 *      XXX
 *
 * Side effects:
 *      NB: This is the signal handler for SIGUSR2.  We could have been
 *      in the middle of dumping portal data.  
 *      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *         This is no longer the signal handler.  -- jw, 12/28/93
 *
 *      We make use of the out-of-band channel to transmit the
 *      notification to the front end.  The actual data transfer takes
 *      place at the front end's request.
 *
 * --------------------------------------------------------------
 */
GlobalMemory notifyContext = NULL;
void Async_NotifyFrontEnd()
{
  extern whereToSendOutput;
  HeapTuple lTuple, rTuple;
  Relation lRel;
  HeapScanDesc sRel;
  TupleDescriptor tdesc;
  ScanKeyData key[2];
  Datum d, value[3];
  char repl[3], nulls[3], msg[18];
  Buffer b;
  int ourpid, isnull;
  
  notifyFrontEndPending = 0;

  elog(DEBUG, "Async_NotifyFrontEnd: notifying front end.");
  
  StartTransactionCommand();
  ourpid = getpid();
  ScanKeyEntryInitialize(key[0].data, 0,
			 Anum_pg_listener_notify,
			 Integer32EqualRegProcedure,
			 Int32GetDatum(1));
  ScanKeyEntryInitialize(key[1].data, 0,
			 Anum_pg_listener_pid,
			 Integer32EqualRegProcedure,
			 Int32GetDatum(ourpid));
  lRel = heap_openr(Name_pg_listener);
  tdesc = RelationGetTupleDescriptor(lRel);
  sRel = heap_beginscan(lRel, 0, NowTimeQual, 2, key);

  nulls[0] = nulls[1] = nulls[2] = ' ';
  repl[0] = repl[1] = repl[2] = ' ';
  repl[Anum_pg_listener_notify - 1] = 'r';
  value[0] = value[1] = value[2] = (Datum) 0;
  value[Anum_pg_listener_notify - 1] = Int32GetDatum(0);
		 
  while (HeapTupleIsValid(lTuple = heap_getnext(sRel, 0, &b))) {
    d = (Datum) heap_getattr(lTuple, b, Anum_pg_listener_relname,
			     tdesc, &isnull);
    rTuple = heap_modifytuple(lTuple, b, lRel, value, nulls, repl);
    (void) heap_replace(lRel, &lTuple->t_ctid, rTuple);
    
    /* notifying the front end */
    
    if (whereToSendOutput == Remote) {
      pq_putnchar("A", 1);
      pq_putint(ourpid, 4);
      pq_putstr(DatumGetName(d));
      pq_flush();
    } else {
      elog(NOTICE, "Async_NotifyFrontEnd: no asynchronous notification to frontend on interactive sessions");
    }
    ReleaseBuffer(b);
  }
  CommitTransactionCommand();
}

static int AsyncExistsPendingNotify(char *relname)
{
  PendingNotifyNode *p;
  
  for (p = (PendingNotifyNode *)SLGetHead(&pendingNotifies); p != NULL;
       p = (PendingNotifyNode *)SLGetSucc(&p->node)) {
    if (!strncmp(p->relname.data, relname, NAMEDATALEN)) {
      return 1;
    }
  }
  return 0;
}

static void ClearPendingNotify()
{
  PendingNotifyNode *p;

  for (p = (PendingNotifyNode *) SLGetHead(&pendingNotifies) ; p != NULL ;
       p = (PendingNotifyNode *) SLGetHead(&pendingNotifies)) {
    SLRemove(&p->node);
    free(p);
  }
}

