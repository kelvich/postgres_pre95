/*===================================================================
 *
 * FILE:
 *   prs2stack.c
 *
 * IDENTIFICATION:
 *   $Header$
 *
 * DESCRIPTION:
 * This file contains some routines used to implement a loop detection
 * mechanism.
 *
 * The Problem:
 * Sometimes a rule in order to calculate the value of an attribute
 * (that has a LockTypeWrite on it) need to execute a plan.
 * This plan might retrieve the same tuple that caused the
 * activation of the rule in the first place, thus reactivating the rule
 * ans causing a loop. 
 * For example, assume the rule:
 *    ON retrieve to EMP.salary where EMP.name = "Mike"
 *    DO INSTEAD retrieve (EMP.salary) where EMP.name = "Fred"
 * This rule states that if we try to retrieve Mike's salary
 * we must substitute Fred's salary instead.
 * If a user tries to retrieve Mike's salary the above rule will be
 * activated, and it will execute the plan:
 *    retrieve (EMP.salary) where EMP.name = "Fred"
 * Lets assume that the planner had decided to use a sequential scan
 * and that Mike's it encounter's Mike's tuple before it encounter's
 * Fred's. In this case the executor will try to retrieve both
 * name and salary of Mike's tuple at the same time (it first
 * retrieves all the attributes needed and *then* checks fo rthe
 * qualification, i.e. to see if name == "Fred".
 * So, as Mike's salary is retrieved, then the rule manager will
 * reactivate the rule above, which will try again to run the plan
 *    retrieve (EMP.salary) where EMP.name = "Fred"
 * which will cuase yet another activation of the rule etc....
 *
 * The Solution:
 * Every time we try to calculate the value of a tuple's attribute
 * by activating a rule, we put in a stack the correpsonding rule oid,
 * tuple oid and attribute number.
 * Before doing that though, we first check to see if there is already
 * a similar entry (same ruleId, tuple oid * attribute number) in the
 * stack. If yes, then we have entered a loop.
 * If no, we go ahead and execute the rule. When the rule has finished
 * its execution, we remove the entry from the stack.
 * Note that the length of the stack is proprtional to the depth
 * of the recursion in the ExecMain/rule manager calls.
 *
 * Some Options:
 * Once we have discovered a rule loop we have to decide what to do.
 * Some alternatives:
 *  1) Abort transaction! (not a good solution, as in some cases
 *     (as in the example above with Fred's & Mike's salary) the rule
 *     is OK, and the loop is created because of the implementation.
 *  2) Ignore this rule and either activate other rules that possibly
 *     exist, or use the value as stored in the tuple.
 *  3) Ignore the tuple altogether!
 *
 * MEMORY MANAGEMENT PROBLEMS:
 * Every time we push an item in the stack we first check if we have
 * enough space. If not, we allocate some new space, copty there the
 * contents of the old one and then free the old one.
 * However, as each recursive call of 'prs2Main' changes the memory
 * context (we do that to avoid memory leaks - see comments in prs2main.c)
 * it is possible that we allocate some space under a certain mem cntxt
 * and when we want to reallocate some more we are under another.
 * This of course causes bad things to happen and our beloved postgres
 * dies a horrible death.
 * So I did an extensive study of all the possible alternatives
 * carefully examining all the tradeoff involved, taking into account
 * all possible softawre engineering considerations, and finally after an
 * extensive period of thought (approximately 10 seconds) I decided
 * to create (yes!) yet another memory context specially designed and
 * custom made for the rule stack.
 *
 *===================================================================
 */

#include "tmp/c.h"
#include "utils/log.h"
#include "rules/prs2.h"
#include "nodes/mnodes.h"
#include "utils/mcxt.h"


#define PRS2_INCREMENT_STACKSIZE 1

/*-----
 * this is a local static variable :
 * Rule Stack Memeory Context!
 */
static GlobalMemory Prs2RuleStackMemoryContext = NULL;

/*-------------------------------------------------------------------
 * prs2RuleStackPush
 *
 *  Add a new entry to the rule stack. First check if there is enough
 * stack space. otherwise reallocate some more memory...
 */
void
prs2RuleStackPush(p, ruleId, tupleOid, attributeNumber)
Prs2EStateInfo p;
ObjectId ruleId;
ObjectId tupleOid;
AttributeNumber attributeNumber;
{
    Prs2Stack temp;
    int newSize;
    MemoryContext oldMemContext;

    /*
     * remember the stack pointer always points
     * to the next free stack entry...
     */
    if (p->prs2StackPointer >= p->prs2MaxStackSize) {
	/*
	 * we need to allocate some more stack room!
	 * switch to `Prs2RuleStackMemoryContext' for all memory
	 * allocations
	 */
	if (Prs2RuleStackMemoryContext == NULL) {
	    elog(WARN,"prs2RuleStackFree: Prs2RuleStackMemoryContext is NULL!");
	}
	oldMemContext =
	    MemoryContextSwitchTo((MemoryContext)Prs2RuleStackMemoryContext);
	/*
	 * allocate more room & free the old one
	 */
	newSize = p->prs2MaxStackSize + PRS2_INCREMENT_STACKSIZE;
	temp = (Prs2Stack) palloc(sizeof(Prs2StackData) * newSize);
	if (temp==NULL) {
	    elog(WARN,
		"prs2RuleStackPush: out of memory (from %d to %d entries)",
		p->prs2MaxStackSize, newSize);
	}
	if (p->prs2Stack != NULL) {
	    bcopy(p->prs2Stack,
		    temp,
		    sizeof(Prs2StackData)*p->prs2MaxStackSize);
	    pfree(p->prs2Stack);
	}
	p->prs2Stack = temp;
	p->prs2MaxStackSize = newSize;
	/*
	 * switch back to the old mem context
	 */
	(void) MemoryContextSwitchTo(oldMemContext);
    }

    /*
     * there is enough space, go and add the new entry..,
     */

    p->prs2Stack[p->prs2StackPointer].ruleId = ruleId;
    p->prs2Stack[p->prs2StackPointer].tupleOid = tupleOid;
    p->prs2Stack[p->prs2StackPointer].attrNo = attributeNumber;

    p->prs2StackPointer += 1;

}

/*-------------------------------------------------------------------
 * prs2RuleStackPop
 *
 *  Discard the top entry of the stack
 */
void
prs2RuleStackPop(p)
Prs2EStateInfo p;
{
    /*
     * the stackPointer always points to the first free
     * stack item
     */
    if (p->prs2StackPointer <= 0) {
	elog(WARN, "prsRuleStackPop: Empty rule stack!");
    }

    p->prs2StackPointer -= 1;

}

/*-------------------------------------------------------------------
 * prs2RuleStackSearch
 *
 *   Search for a stack entry matching the given arguments.
 *   Return true if found, false otherwise...
 */
bool
prs2RuleStackSearch(p, ruleId, tupleOid, attributeNumber)
Prs2EStateInfo p;
ObjectId ruleId;
ObjectId tupleOid;
AttributeNumber attributeNumber;
{
    int i;

    /*
     * the stackPointer always points to the first free stack
     * element
     */
    for (i=0; i<p->prs2StackPointer; i++) {
	if (p->prs2Stack[i].ruleId == ruleId &&
	    p->prs2Stack[i].tupleOid == tupleOid &&
	    p->prs2Stack[i].attrNo == attributeNumber) {
		return(true);
	}
    }

    return(false);
}

/*-------------------------------------------------------------------
 * prs2RuleStackInitialize
 *
 *   Intialize the stack.
 */
Prs2EStateInfo
prs2RuleStackInitialize()
{
    Prs2EStateInfo p;
    MemoryContext oldMemContext;

    /*
     * switch to `Prs2RuleStackMemoryContext' for all memory
     * allocations
     */
    if (Prs2RuleStackMemoryContext == NULL) {
	Prs2RuleStackMemoryContext = CreateGlobalMemory("*prs2RuleStack*");
    }
    oldMemContext =
	MemoryContextSwitchTo((MemoryContext)Prs2RuleStackMemoryContext);

    /*
     * create the stack
     */
    p = (Prs2EStateInfo) palloc(sizeof(Prs2EStateInfoData));
    if (p==NULL) {
	elog(WARN,"prs2RuleStackInitialize: palloc(%ld) failed.\n",
	sizeof(Prs2EStateInfoData));
    }
    p->prs2StackPointer = 0;
    p->prs2MaxStackSize = 0;
    p->prs2Stack = NULL;

    /*
     * switch back to the original mem context
     */
    (void) MemoryContextSwitchTo(oldMemContext);

    return(p);
}

/*-------------------------------------------------------------------
 * prs2RuleStackFree
 *
 *   Free the memory occupied by the stack.
 */
void
prs2RuleStackFree(p)
Prs2EStateInfo p;
{
    MemoryContext oldMemContext;

    /*
     * switch to `Prs2RuleStackMemoryContext' for all memory
     * deallocations
     */
    if (Prs2RuleStackMemoryContext == NULL) {
	elog(WARN,"prs2RuleStackFree: Prs2RuleStackMemoryContext is NULL!");
    }
    oldMemContext =
	MemoryContextSwitchTo((MemoryContext)Prs2RuleStackMemoryContext);

    /*
     * free the Prs2EStateInfo
     */
    if (p!= NULL && p->prs2Stack != NULL) {
	pfree(p->prs2Stack);
    }
    if (p!=NULL)
	pfree(p);

    /*
     * switch back to the original mem context
     */
    (void) MemoryContextSwitchTo(oldMemContext);

}
