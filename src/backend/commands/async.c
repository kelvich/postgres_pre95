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

typedef struct {
    char relname[16];
    SLNode Node;
} PendingNotify;

static SLList pendingNotifies;
static int initialized = 0;	/* statics in this module initialized? */

/*
 *--------------------------------------------------------------
 * Async_Notify --
 *
 *      Adds the relation to the list of pending notifies.
 *      All notification happens at end of commit.
 *      
 *
 * Results:
 *      XXX
 *
 * Side effects:
 *      All tuples for relname in pg_listener are updated.
 *
 *--------------------------------------------------------------
 */
void
Async_Notify(relname)
     char *relname;
{
    PendingNotify *p;
    HeapTuple lTuple;
    struct listener *lStruct;
    Relation lRel;
    HeapScanDesc sRel;
    TupleDescriptor tdesc;
    Buffer b;
    Datum d;
    int notifypending, isnull;

    if (! initialized) {
	SLNewList(&pendingNotifies,offsetof(PendingNotify,Node));
	initialized = 1;
    }

    elog(NOTICE,"Async_Notify: %s",relname);
    lRel = heap_openr("pg_listener");
    tdesc = RelationGetTupleDescriptor(lRel);
    sRel = heap_beginscan(lRel,0,NowTimeQual,0,(ScanKey)NULL);
    while (HeapTupleIsValid(lTuple = heap_getnext(sRel,0,&b))) {
	d = (Datum) heap_getattr(lTuple,b,Anum_pg_listener_relname,tdesc,
				 &isnull);
	if (! strcmp(relname,DatumGetPointer(d))) {
	    d = (Datum) heap_getattr(lTuple,b,Anum_pg_listener_notify,tdesc,
				     &isnull);
	    notifypending = (int)DatumGetPointer(d);
	    if (! notifypending) {
		ItemPointerData oldTID;
		oldTID = lTuple->t_ctid;
		((struct listener *)GETSTRUCT(lTuple))->notification = 1;
		heap_replace(lRel,&oldTID,lTuple);
	    }
	}
	ReleaseBuffer(b);
    }
    heap_endscan(sRel);
    heap_close(lRel);
}

#if 0
static int
AsyncExistsPendingNotify(relname)
     char *relname;
{
    for (p = (PendingNotify *)SLGetHead(&pendingNotifies); p != NULL;
	 p = (PendingNotify *)SLGetSucc(&p->Node)) {
	if (!strcmp(p->relname,relname)) {
	    return 1;
	}
    }
    return 0;
}
#endif

/*
 *--------------------------------------------------------------
 * Async_NotifyAtCommit --
 *
 *      Signal all backends listening on relations pending notification.
 *
 *      This corresponds to the 'notify <relation>' command in 
 *      postquel.
 *
 *   XXX: what if we signal ourselves?
 *
 * Results:
 *      XXX
 *
 * Side effects:
 *      XXX
 *
 *--------------------------------------------------------------
 */
void Async_NotifyAtCommit()
{
    char *relname;
    Relation r;
    TupleDescriptor tdesc;
    HeapScanDesc s;
    HeapTuple htup;
    Buffer b;
    Datum d;
    char *relnamei;
    int pid;
    int isnull;
    int notifypending;
    static int reentrant;	/* hack:
				   This is a transaction itself, so we
				   don't want to loop at commit time
				   processing */
    if (reentrant)
      return;
    reentrant = 1;
    StartTransactionCommand();

    if (! initialized) {
	SLNewList(&pendingNotifies,offsetof(PendingNotify,Node));
	initialized = 1;
    }

    r = heap_openr("pg_listener");
    tdesc = RelationGetTupleDescriptor(r);
    s = heap_beginscan(r,0,NowTimeQual,0,(ScanKey)NULL);
    while (HeapTupleIsValid(htup = heap_getnext(s,0,&b))) {
	d = (Datum) heap_getattr(htup,b,Anum_pg_listener_notify,tdesc,
				 &isnull);
	notifypending = (int)DatumGetPointer(d);
	if (notifypending) {
	    d = (Datum) heap_getattr(htup,b,Anum_pg_listener_pid,tdesc,&isnull);
	    pid = (int) DatumGetPointer(d);
	    if (kill (pid,SIGUSR2) < 0) {
		extern int errno;
		if (errno == ESRCH) { /* no such process */
		    heap_delete(r,&htup->t_ctid);
		}
	    }
	}
	ReleaseBuffer(b);
    }
    heap_endscan(s);
    heap_close(r);
    CommitTransactionCommand();
    reentrant = 0;
}

#if 0
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
    SLNewList(&pendingNotifies,offsetof(PendingNotify,Node));
    intialized = 1;
}
#endif

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

    elog(NOTICE,"Async_Listen: %s",relname);
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
	if (!strcmp(relnamei,relname)) {
	    d = (Datum) heap_getattr(htup,b,Anum_pg_listener_pid,tdesc,&isnull);
	    pid = (int) DatumGetPointer(d);
	    if (pid != ourPid) {
		alreadyListener = 1;
		break;
	    }
	}
	ReleaseBuffer(b);
    }
    heap_endscan(s);

    tup = heap_formtuple(Natts_pg_listener,
			 &lDesc->rd_att,
			 values,
			 nulls);
    heap_insert(lDesc,tup,(double *)NULL);
    
    pfree(tup);
    if (alreadyListener) {
	elog(NOTICE,"Async_Listen: already one listener on %s (possibly dead)",relname);
    }
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
    char *relname;
    extern whereToSendOutput;
    Relation r;
    HeapScanDesc s;
    TupleDescriptor tdesc;
    Datum d;
    Buffer b;
    HeapTuple htup;
    int isnull, notifypending, pid;
    char msg[1025];
    int ourpid = getpid();

    bzero(msg,sizeof(msg));
    
    if (whereToSendOutput != Remote) {
	elog(NOTICE,"Async_NotifyPortal: no asynchronous notifcation on interactive sessions");
	return;
    }

    /* Sorry, this code is mix-n-match styles of using getstruct and
     * heap_getattr.
     */
    StartTransactionCommand();
    {
    /* debugging */
	FILE *f;
	f = fopen("/dev/ttyp6","w");
	fprintf(f,"Got signal\n",msg);

    r = heap_openr("pg_listener");
    tdesc = RelationGetTupleDescriptor(r);
    s = heap_beginscan(r,0,NowTimeQual,0,(ScanKey)NULL);
    while (HeapTupleIsValid(htup = heap_getnext(s,0,&b))) {
	d = (Datum) heap_getattr(htup,b,Anum_pg_listener_notify,tdesc,
				 &isnull);
	notifypending = (int)DatumGetPointer(d);
	if (notifypending) {
	    d = (Datum) heap_getattr(htup,b,Anum_pg_listener_pid,tdesc,&isnull);
	    pid = (int) DatumGetPointer(d);
	    if (pid == ourpid) {
		ItemPointerData oldTID;
		d = (Datum) heap_getattr(htup,b,Anum_pg_listener_relname,tdesc,&isnull);
		relname = DatumGetPointer(d);
		oldTID = htup->t_ctid;
		/* unset notify flag */
		((struct listener *)GETSTRUCT(htup))->notification = 0;
		heap_replace(r,&oldTID,htup);

		/* notify front end of presence,
		   but not any more detail */
		sprintf(msg,"A%s %d",relname,pid);
		/* debugging */
		fprintf(f,"%s\n",msg);
		if (pq_sendoob("A",1)<0) {
		    extern int errno;
		    fprintf(f,"pq_sendoob failed: errno=%d",errno);
		}
		/* call backend PQ lib -- different memory context */
		{
		    MemoryContext orig;
		    if (notifyContext == NULL) {
			notifyContext = CreateGlobalMemory("notify");
		    }
		    orig = MemoryContextSwitchTo((MemoryContext)notifyContext);
		    PQappendNotify(relname,pid);
		    (void) MemoryContextSwitchTo(orig);
		}
	    }
	}
	ReleaseBuffer(b);
    }
	fclose(f);
    }
    heap_endscan(s);
    heap_close(r);
    CommitTransactionCommand();

}
