
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
#include "utils/fmgr.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "parser/parse.h"       /* for the AND, NOT, OR */
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
     * Initialize the tuple table stuff...
     */
    tupleTable = ExecCreateTupleTable(1);
    slotnum = ExecAllocTableSlot(tupleTable);
    slot = (TupleTableSlot) ExecGetTableSlot(tupleTable, slotnum);

    econtext = RMakeExprContext();
    (void)ExecStoreTuple(tuple, slot, false);
    set_ecxt_scantuple(econtext, slot);
    set_ecxt_scantype(econtext, tupDesc);
    set_ecxt_scan_buffer(econtext, buffer);
    set_ecxt_innertuple(econtext, NULL);
    set_ecxt_innertype(econtext, NULL);
    set_ecxt_outertuple(econtext, NULL);
    set_ecxt_outertype(econtext, NULL);
    set_ecxt_param_list_info(econtext, NULL);

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
