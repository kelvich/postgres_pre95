/* ----------------------------------------------------------------
 *   FILE
 *      scantemps.c
 *
 *   DESCRIPTION
 *      This code provides support for scanning a list of temporary
 *	relations
 *      
 *   INTERFACE ROUTINES
 *      ExecScanTemps		sequentially scans a list of temporary rels.
 *	ExecInitScanTemps	initializes a scantemps node.
 *	ExecEndScanTemps	releases any storage allocated.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */

#include <sys/file.h>
#include "tmp/postgres.h"

#include "access/itup.h"
#include "utils/relcache.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.a.h"
#include "nodes/primnodes.h"
#include "executor/execdefs.h"
#include "executor/externs.h"
#include "tcop/slaves.h"


/* ----------------------------------------------------------------
 *	ExecScanTemps(node)
 *
 *	Scans a list of temporary relations one by one.  It is
 *	equivalent to an append node above a list of scan nodes,
 *	but much more efficient, because it does not need to do
 *	any system catalog lookup, the reldescs of the temporary
 *	relations are already in the nodes.  It does not do
 *	any projections either.  This node is specially for
 *	collecting results from multiple parallel backends.
 *
 * ----------------------------------------------------------------
 */

TupleTableSlot
ExecScanTemps(node)
ScanTemps node;
{
    ScanTempState 	scantempState;
    HeapScanDesc 	currentScanDesc;
    HeapTuple 		heapTuple;
    HeapTuple		retHeapTuple;
    EState 		estate;
    ScanDirection 	dir;
    int 		whichplan;
    int 		nplans;
    Relation 		tempreldesc;
    TupleTableSlot	slot;
    Buffer		buffer;
    
    estate = (EState) get_state(node);
    scantempState = get_scantempState(node);
    whichplan = get_st_whichplan(scantempState);
    nplans = get_st_nplans(scantempState);
    
#if 0
    elog(DEBUG, "ScanTemps: total %d plans, current plan %d", nplans,whichplan);
#endif
    
    currentScanDesc = get_css_currentScanDesc(scantempState);
    heapTuple = amgetnext(currentScanDesc,
			  (dir == EXEC_BKWD),
			  NULL);
    
    while (heapTuple == NULL) {
	if (++whichplan >= nplans)
	   return NULL;
	
#if 0
	elog(DEBUG, "ScanTemps: changing to plan %d", whichplan);
#endif
	amendscan(currentScanDesc);
	tempreldesc = get_css_currentRelation(scantempState);
	amclose(tempreldesc);
	
	/* 
	 * change to the next temporary relation
	 */
        tempreldesc = (Relation)nth(whichplan, get_temprelDescs(node));
	
	/*
	 * reopen the file for the temp relations, because the file
	 * was opened in another backend, so the virtual file descriptor
	 * is invalid in the current process.
	 */
        tempreldesc->rd_fd = FileNameOpenFile(
				  relpath(&(tempreldesc->rd_rel->relname)),
                                  O_RDWR, 0666);
	
	RelationRegisterTempRel(tempreldesc);
        dir = get_es_direction(estate);
	
        currentScanDesc = ambeginscan(tempreldesc,
                                      (dir == EXEC_BKWD),
                                      NowTimeQual,
                                      0,
                                      NULL);
	
	currentScanDesc->pageskip = get_parallel(node);
	currentScanDesc->initskip = SlaveInfoP[MyPid].groupPid;
        set_css_currentRelation(scantempState, tempreldesc);
        set_css_currentScanDesc(scantempState, currentScanDesc);
	set_st_whichplan(scantempState, whichplan);
	
        heapTuple = amgetnext(currentScanDesc,
                              (dir == EXEC_BKWD),
                              &buffer);
      }

	
    /* -----------------------------------------
     *   make a copy of heapTuple so that later processing
     *   will not modify the tuple on buffer page.
     * -----------------------------------------
     */
    retHeapTuple = (HeapTuple) palloc(heapTuple->t_len);
    bcopy(heapTuple, retHeapTuple, heapTuple->t_len);

    slot = (TupleTableSlot)
	get_css_ScanTupleSlot(scantempState);
    
    ExecStoreTuple(heapTuple,	/* tuple */
		   slot,	/* destination slot */
		   buffer,	/* buffer for this tuple */
		   false);	/* don't use pfree on tuple */

    slot = (TupleTableSlot)
	get_cs_ResultTupleSlot(scantempState);
    
    return (TupleTableSlot)
	ExecStoreTuple(retHeapTuple,   	/* tuple to store */
		       slot,   		/* slot to store in */
		       InvalidBuffer,   /* tuple has no buffer */
		       true); 		/* free the return heap tuple */
}

/* ------------------------------------------------------------------
 *	ExecInitScanTemps
 *
 *	Create scantemp states and first scan descriptor for the
 *	ScanTemps node.
 *
 * ------------------------------------------------------------------
 */

List
ExecInitScanTemps(node, estate, parent)
    ScanTemps 	node;
    EState	estate;
    Plan	parent;
{
    ScanTempState	scantempstate;
    Relation		tempreldesc;
    HeapScanDesc	currentScanDesc;
    ScanDirection	dir;
    List		tempRelDescs;

    set_state(node, estate);

    scantempstate = MakeScanTempState();
    set_scantempState(node, scantempstate);

    /* ----------------
     *	initialize tuple slots
     * ----------------
     */
    ExecInitResultTupleSlot(estate, scantempstate);
    ExecInitScanTupleSlot(estate, scantempstate);
    
    /* ----------------
     * 	initialize tuple type and projection info
     * ----------------
     */
    ExecAssignResultTypeFromTL(node, scantempstate);
    ExecAssignProjectionInfo(node, scantempstate);
    
    /* ----------------
     *	XXX comment me
     * ----------------
     */
    tempRelDescs = get_temprelDescs(node);
    tempreldesc = (Relation)CAR(tempRelDescs);
    tempreldesc->rd_fd =
	FileNameOpenFile(relpath(&(tempreldesc->rd_rel->relname)),
			 O_RDWR, 0666);
    
    RelationRegisterTempRel(tempreldesc);
    
    /* ----------------
     *	XXX comment me
     * ----------------
     */
    dir = get_es_direction(estate);
    currentScanDesc = ambeginscan(tempreldesc,
				  (dir == EXEC_BKWD),
				  NowTimeQual,
				  0,
				  NULL);
    
    currentScanDesc->pageskip = get_parallel(node);
    currentScanDesc->initskip = SlaveInfoP[MyPid].groupPid;
    
    /* ----------------
     *	XXX comment me
     * ----------------
     */
    set_css_currentRelation(scantempstate, tempreldesc);
    set_css_currentScanDesc(scantempstate, currentScanDesc);
    set_st_whichplan(scantempstate, 0);
    set_st_nplans(scantempstate, length(tempRelDescs));

    return LispTrue;
}

/* ---------------------------------------------------------------------
 *	ExecEndScanTemps
 *
 *	frees any storage allocated through C routines.
 *	closes the last temp relation
 *
 * --------------------------------------------------------------------
 */

void
ExecEndScanTemps(node)
ScanTemps node;
{
    Relation	  tempreldesc;
    List	  tempRelDescs;
    Relation	  relation;
    ScanTempState state;
    HeapScanDesc  scanDesc;
    LispValue	  x;

    state = get_scantempState(node);
    tempRelDescs = get_temprelDescs(node);
    relation = get_css_currentRelation(state);
    scanDesc = get_css_currentScanDesc(state);
    
    if (scanDesc != NULL)
	amendscan(scanDesc);
    
    if (relation != NULL)
	amclose(relation);
}
