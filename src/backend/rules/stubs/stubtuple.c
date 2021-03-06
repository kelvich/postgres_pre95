
/*===================================================================
 *
 * FILE:
 * stubtuple.c
 *
 * IDENTIFICATION:
 * $Header$
 *
 *
 *===================================================================
 */


#include <stdio.h>
#include "tmp/postgres.h"

#include "utils/log.h"
#include "access/tupdesc.h"
#include "access/heapam.h"
#include "fmgr.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "parse.h"       	/* for the AND, NOT, OR */
#include "nodes/plannodes.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "executor/x_tuples.h"
#include "executor/tuptable.h"

extern ExprContext RMakeExprContext();

/*--------------------------------------------------------------------
 *
 * prs2StubGetLocksForTuple
 *
 * given a collection of stub records and a tuple, find all the locks
 * that the tuple must inherit.
 *
 *--------------------------------------------------------------------
 */
RuleLock
prs2StubGetLocksForTuple(tuple, buffer, tupDesc, stubs)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
Prs2Stub stubs;
{
    int i;
    RuleLock temp, resultLock;
    Prs2OneStub oneStub;

    /*
     * resultLock is initially an empty lock
     */
    resultLock = prs2MakeLocks();

    for (i=0; i<stubs->numOfStubs; i++) {
	oneStub = stubs->stubRecords[i];
	if (prs2StubQualTestTuple(tuple, buffer, tupDesc,
			    oneStub->qualification)){
	    temp = prs2LockUnion(resultLock, oneStub->lock);
	    prs2FreeLocks(resultLock);
	    resultLock = temp;
	}
    }

    return(resultLock);

}

/*--------------------------------------------------------------------
 *
 * prs2StubQualTestTuple
 *
 * test if a tuple satisfies the given qualifications of a
 * stub record.
 *--------------------------------------------------------------------
 */
bool
prs2StubQualTestTuple(tuple, buffer, tupDesc, qual)
HeapTuple tuple;
Buffer buffer;
TupleDescriptor tupDesc;
LispValue qual;
{
    ExprContext econtext;
    bool qualResult;
    TupleTable tupleTable;
    TupleTableSlot slot;
    int slotnum;

    /*
     * Use ExecEvalQual to test for the qualification.
     * To do that create a dummy 'ExprContext' & fill it
     * with the appropriate info.
     *
     * Note also, that as 'ExecQual' expects its tuple
     * to be in a 'tuple table slot' we have to create
     * a dummy one.
     * It would be nice to keep it for a long time and not
     * allocate it & deallocate it continuously,
     * but we can not keep it as a static variable because the
     * memory is pfreed by the end of the Xact.
     * (No, I didn't think all that by just reading the code,
     * I tried it & learned the hard way... sigh!)
     */

    /*
     * Initialize the tuple table stuff.
     */
    tupleTable = ExecCreateTupleTable(1);
    slotnum = 	 ExecAllocTableSlot(tupleTable);
    slot = (TupleTableSlot)
	ExecGetTableSlot(tupleTable, slotnum);
    
    (void) ExecStoreTuple((Pointer)tuple, (Pointer)slot, buffer, false);
    (void) ExecSetSlotDescriptor(slot, tupDesc);
    (void) ExecSetSlotBuffer(slot, buffer);
    
    /*
     *	Initialize expression context.
     */
    econtext = RMakeExprContext();
    set_ecxt_scantuple(econtext, slot);
    set_ecxt_innertuple(econtext, NULL);
    set_ecxt_outertuple(econtext, NULL);
    set_ecxt_param_list_info(econtext, NULL);
    set_ecxt_range_table(econtext, NULL);

    /*
     * NOTE: we must reinitialize the fcache of all Oper nodes to NULL
     * changes the 'fcache' attribute of the operator nodes in it.
     * So if this is a "shared" qual (for instance if it belongs to
     * a "relation level" stub cached in the EState) then
     * the first time we call ExecQual it will initialize the fcache
     * then it will free the memory, and so when we try to call ExecQual
     * for the second time it will see garbage and die a horrible death.
     */
    prs2ReInitQual(qual);
    qualResult = ExecQual(qual, econtext);

    /*
     * release the tuple table stufffffff....
     */
    ExecDestroyTupleTable(tupleTable, false);

    if (qualResult) {
	return(true);
    } else {
	return(false);
    }
}

prs2ReInitQual(list)
LispValue list;
{
    LispValue t;

    if (null(list))
	return;

    if (IsA(list,Oper)) {
	set_op_fcache((Oper)list, NULL);
	return;
    }
    if (consp(list))
	foreach(t, list)
	    prs2ReInitQual(CAR(t));
}
