
/*====================================================================
 *
 * FILE: prs2tup.c
 *
 * $Header$
 *
 * DESCRIPTION:
 * Routines to insert/remove "tuple-level" locks in the tuples of a relation.
 *
 *====================================================================
 */

#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "utils/fmgr.h"
#include "executor/execdefs.h"	/* for the EXEC_RESULT */
#include "rules/prs2.h"
#include "parser/parsetree.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "utils/lsyscache.h"

LispValue MakeRangeTableEntry();
LispValue MakeRoot();
LispValue planner();

/*--------------------------------------------------------------------
 *
 * prs2PutTupleLevelLocks
 *
 * This routines puts "tuple-level" locks to the tuple of a relation.
 *
 * The relation is specified by its oid "relationOid".
 * "ruleId", "lockType" and "attributeNo" are the rule oid, the type
 * of lock and the attribute number to be locked.
 *
 * Finally, "constQual" is a qualification which involves only
 * attributes og the CURRENT relation (i.e. of the relation to be locked)
 * and constants. (i.e. it has no joins, etc.).
 *
 * This routine locks all the
 * tuples of the relation that satisfy this 'constQual'.
 * It first creates the appropriate parse tree, calls the planner
 * to create the plan, and then executes it.
 *
 *--------------------------------------------------------------------
 */

void
prs2PutTupleLevelLocks(ruleId, lockType, relationOid, attributeNo, constQual)
ObjectId ruleId;
Prs2LockType lockType;
ObjectId relationOid;
AttributeNumber attributeNo;
LispValue constQual;
{
    
    RuleLock newlock;
    Const newlockConst;
    Var var;
    Func func;
    Resdom resdom;
    LispValue root;
    LispValue tlist, expr;
    LispValue rtable, rtentry;
    LispValue parseTree, plan;
    Name relname;
    NameData lockName;

    /* ---------------------
     * The parsetree we want to create corresponds to a query
     * of the form:
     *
     * 		replace REL(lock = prs2LockUnion(REL.lock, <newlock>))
     *		where <constQual>
     * ---------------------
     */

    /*
     * Create the range table.
     * The range table has only one entry, corresponding to the
     * given relation.
     *
     * NOTE: XXX!!!!!!!!
     * Currently all the "Var" nodes of "consQual" correspond to the CURRENT
     * relation and have "varno" = 1. (the parser always puts
     * the CURRENT relation at the beginning of the range table).
     * If this changes, then we would probably have to add some
     * dummy range table entries or change the "varno"s of all the
     * Var nodes of the qualification.
     */
    relname = get_rel_name(relationOid);
    rtentry = MakeRangeTableEntry(relname, LispNil, relname);
    rtable = lispCons(rtentry, LispNil);

    /*
     * now create the target list.
     * This corresponds to something like:
     * (lock = prs2LockUnion(REL.lock, newlock)
     *
     * First create the newlock.
     * NOTE: 'ActionPlanNumber' is a #defined constant...
     *
     * Then create a constant containing the new lock.
     * NOTE: Rule locks are of type "lock" (oid = 31) registered in
     * pg_type. They have a length of -1 (i.e. variable length type).
     */
    newlock = prs2MakeLocks();
    newlock = prs2AddLock(newlock, ruleId, lockType,
			attributeNo, ActionPlanNumber);
    newlockConst = MakeConst(
			PRS2_LOCK_TYPEID,	/* type of "lock" */
			(Size) -1,		/* length of "lock" */
			PointerGetDatum(newlock),	/* lock data */
			(bool) false,		/* constisnull */
			(bool) false);		/* constbyVal */
    /*
     * Now create a Func node, coresponding to the
     * function call:
     *		lockadd(REL.lock, newlock)
     */
    var = MakeVar(
	    (Index) 1,		/* varno = index of relation in rtable */
	    RuleLockAttributeNumber,	/* attr no of rule lock */
	    PRS2_LOCK_TYPEID,		/* vartype */
	    LispNil,			/* vardotfields */
	    (Index) 0,			/* vararrayindex */
	    lispCons(lispInteger(1),
		    lispCons(lispInteger(RuleLockAttributeNumber), LispNil)),
					/* varid */
	    (ObjectId) 0);		/* varelemtype */
    func = MakeFunc(
	    (ObjectId) F_LOCKADD,	/* funcid - PG_PROC.oid */
	    PRS2_LOCK_TYPEID,		/* functype - return type */
	    (bool) false,		/* funcisindex - set by planner */
	    0,				/* funcsize - set by executor */
	    NULL);			/* func_fcache - set by executor */
    expr = lispCons(func, lispCons(var, lispCons(newlockConst, LispNil)));

    /*
     * the target list is a list containing a list consisting of
     * a Resdom node and the expression calculated before.
     */
    bzero((char*) (&(lockName.data[0])), sizeof(lockName.data));
    strcpy((char *)(&(lockName.data[0])), "lock");
    resdom = MakeResdom(
		RuleLockAttributeNumber,	/* resno */
		PRS2_LOCK_TYPEID,		/* restype */
		(Size) -1,			/* reslen */
		&(lockName.data[0]),		/* resname */
		(Index) 0,			/* reskey */
		0,				/* reskeyop */
		0);				/* resjunk */
    tlist = lispCons(lispCons(resdom,lispCons(expr,LispNil)), LispNil);

    /*
     * Finally create the parse tree...
     */
    root = MakeRoot(1,				/* NumLevels */
		    lispAtom("replace"),	/* command name */
		    lispInteger(1),		/* index in range table */
		    rtable,			/* range table */
		    lispInteger(0),		/* priority */
		    LispNil,			/* rule info */
		    LispNil,			/* unique flag */
		    LispNil,			/* sort flag */
		    tlist);			/* target list */
    
    parseTree = lispCons(root, LispNil);
    parseTree = nappend1(parseTree, tlist);
    parseTree = nappend1(parseTree, constQual);

    /*
     * Now call the planner to plan the parse tree
     */
    plan = planner(parseTree);

    /*
     * finally execute the plan.
     * NOTE: 'prs2RunOnePlan' takes as argument a list of the
     * parsetree and plan...
     */
    prs2RunOnePlan(lispCons(parseTree, lispCons(plan,LispNil)),
		    NULL,
		    NULL,			/* prs2EstateInfo */
		    EXEC_RESULT);		/* feature */
}
