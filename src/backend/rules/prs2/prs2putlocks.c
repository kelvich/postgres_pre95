/*======================================================================
 *
 * FILE:
 *  prs2putlocks.c
 *
 * $Header$
 *
 * DESCRIPTION:
 *
 *=====================================================================
 */
#include "tmp/postgres.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "rules/prs2.h"
#include "rules/prs2stub.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "planner/clause.h"
#include "planner/clauses.h"


LispValue prs2FindConstantClause();
void prs2FindLockTypeAndAttrNo();
void prs2TupleSystemPutLocks();
LispValue prs2FindConstantQual();
LispValue prs2FindConstantClause();

extern Name get_attname();
extern AttributeNumber get_attnum();

/*------------------------------------------------------------------
 *
 * prs2TupleSystemPutLocks
 *
 * Put the appropriate rule locks.
 * NOTE: currently only relation level locking is implemented
 * NOTE: isTupleLevel is true if this routine is used by the tuple
 * level system, otherwise (if it is used by the query rewrite
 * system) it is false.
 *
 *------------------------------------------------------------------
 */
void
prs2TupleSystemPutLocks(ruleId, relationOid, eventAttributeNo,
			    updatedAttributeNo, eventType, actionType,
			    ruleQual)
ObjectId ruleId;
ObjectId relationOid;
AttributeNumber eventAttributeNo;
AttributeNumber updatedAttributeNo;
EventType eventType;
ActionType actionType;
LispValue ruleQual;
{
    Prs2LockType lockType;
    AttributeNumber attributeNo;
    LispValue constQual;

    prs2FindLockTypeAndAttrNo(eventType, actionType, eventAttributeNo,
			updatedAttributeNo, &lockType, &attributeNo);

    /*
     * Given the rule's qualification, try to extract a
     * constant subqualification (involving only attributes of the CURRENT
     * relation and contstants).
     *
     * NOTE: XXX!
     * We assume the the "varno" of the CURRENT relation is 1
     */
#ifdef NEW_RULE_SYSTEM
    constQual = prs2FindConstantQual(ruleQual, 1);

    if (constQual == LispNil) {
	/*
	 * No such qual exists, use relation level locks
	 */
	prs2PutRelationLevelLocks(ruleId, lockType, relationOid, attributeNo);
    } else {
	prs2PutTupleLevelLocks(ruleId, lockType, relationOid, attributeNo,
			    constQual);
    }
#else NEW_RULE_SYSTEM
    prs2PutRelationLevelLocks(ruleId, lockType, relationOid, attributeNo);
#endif NEW_RULE_SYSTEM

}

/*------------------------------------------------------------------
 *
 * prs2FindLockTypeAndAttrNo
 *
 * Given the event & action types, the attribute number that
 * appears in the event clause of the rule and the attribute number
 * that is updated -if this is a rule of the form
 *     "on... where... do replace CURRENT(...)"
 * or
 *     "on... where... do instead retrieve(...)"
 *
 * then find the type of the 'action' lock that we must put
 * in the relation that appears in the event clause of the rule.
 * and the attribute number of the attribute that must be locked.
 *------------------------------------------------------------------
 */
void
prs2FindLockTypeAndAttrNo(eventType, actionType, eventAttributeNo,
			updatedAttributeNo, lockTypeResult, attributeNoResult)
AttributeNumber eventAttributeNo;
AttributeNumber updatedAttributeNo;
EventType eventType;
ActionType actionType;
Prs2LockType *lockTypeResult;
AttributeNumber *attributeNoResult;
{
    Prs2LockType lockType;
    AttributeNumber attributeNo;

    lockType = LockTypeInvalid;
    attributeNo = InvalidAttributeNumber;

    if (actionType == ActionTypeRetrieveValue &&
	eventAttributeNo == InvalidAttributeNumber) {
	/*
	 * this is a "view" rule....
	 */
	lockType = LockTypeTupleRetrieveRelation;
	attributeNo = InvalidAttributeNumber;
    } else if (actionType == ActionTypeRetrieveValue ||
	actionType == ActionTypeReplaceCurrent ||
	actionType == ActionTypeReplaceNew) {
	/*
	 * In this case the attribute to be locked is the updated
	 * attribute...
	 */
	attributeNo = updatedAttributeNo;
	switch (eventType) {
	    case EventTypeRetrieve:
		lockType = LockTypeTupleRetrieveWrite;
		break;
	    case EventTypeReplace:
		lockType = LockTypeTupleReplaceWrite;
		if (actionType == ActionTypeReplaceCurrent) {
		    elog(WARN, "ON REPLACE rules can not update CURRENT tuple");
		}
		break;
	    case EventTypeAppend:
		lockType = LockTypeTupleAppendWrite;
		if (actionType == ActionTypeReplaceCurrent) {
		    elog(WARN, "ON APPEND rules can not update CURRENT tuple");
		}
		break;
	    case EventTypeDelete:
		lockType = LockTypeTupleDeleteWrite;
		elog(WARN, "ON DELETE rules can not update CURRENT tuple");
		break;
	    default:
		elog(WARN,
		    "prs2FindLockType: Illegal Event type: %c", eventType);
	}
    } else if (actionType == ActionTypeOther) {
	/*
	 * In this case the attribute to be locked is the 'event'
	 * attribute...
	 */
	attributeNo = eventAttributeNo;
	switch (eventType) {
	    case EventTypeRetrieve:
		lockType = LockTypeTupleRetrieveAction;
		break;
	    case EventTypeReplace:
		lockType = LockTypeTupleReplaceAction;
		break;
	    case EventTypeAppend:
		lockType = LockTypeTupleAppendAction;
		break;
	    case EventTypeDelete:
		lockType = LockTypeTupleDeleteAction;
		break;
	    default:
		elog(WARN,
		    "prs2FindLockType: Illegal Event type: %c", eventType);
	}
    } else {
	elog(WARN, "prs2FindLockType: Illegal Action type: %c", actionType);
    }

#ifdef PRS2DEBUG
    printf("prs2FindLockType: (ACTION='%c', EVENT='%c', LOCK='%c')\n",
	    actionType, eventType, lockType);
#endif PRS2DEBUG
    
    /*
     * OK, we found them! update the output attributes.
     */
    *lockTypeResult = lockType;
    *attributeNoResult = attributeNo;

}

/*------------------------------------------------------------------
 *
 * prs2FindConstantQual
 *
 * This routine is given the rule's qualification and returns
 * a "more relaxed" qualification with the following properties:
 *
 * 1) every tuple that satisfies the original qualification
 *    will satisfy the returned qualification (while of course the
 *    opposite is not true in general).
 * 2) this retruned qualification will only involve constants and
 *    .e. attributes of the CURRENT (the one to be
 *    locked). It will NOT have any 'Var' nodes of other tuple variables.
 *
 * NOTE 1: the "Var" nodes of the NEW/CURRENT relation are NOT
 * replaced with "Param" nodes.
 * NOTE 2: We are only interested in the CURRENT relation. We ignore
 * the NEW relation (we treat is as another tuple variable)!
 *
 * This routine can also return a null qualification, which is
 * assumed to be equivalent to 'always true'.
 *
 * For example if the original qual is:
 *
 *	where CURRENT.dept = DEPT.dname and DEPT.floor = 1
 *	and CURRENT.age > 30 and CURRENT.salary > 5000
 *	and CURRENT.salary > EMP.salary and EMP.name = CURRENT.mgr
 *
 * the returned qualification will be:
 *	where CURRENT.age > 30 and CURRENT.salary > 5000
 *
 * If the original qual is:
 *
 *	where CURRENT.dept = DEPT.dname and DEPT.floor = 1
 *
 * then the returned qualification will be null (LispNil).
 *
 * NOTE: 
 * this routine is not always doing the best possible job, i.e. it might
 * not return the most restrictive qual that satisfies the 2 criteria
 * mentioned before. Actually, if it gets confused it might just give up
 * and return a null qual...
 *
 * NOTE 2:
 * 'currentVarno' is the "varno" of the "Var" nodes corresponding
 * to the CURRENT relation (currently this is always 1).
 * 
 *------------------------------------------------------------------
 */
LispValue
prs2FindConstantQual(qual, currentVarno)
LispValue qual;
int currentVarno;
{
    LispValue newQual;
    LispValue cnfify();
    bool isConstant;

    /*
     * use the 'cnfify' routine defined somewhere in the
     * planner to convert the qual to Conjuctive Normal Form
     * (tell 'cnfify' to NOT remove the explicit 'AND's !)
     *
     * NOTE: I am not sure whether we need to make a copy of
     * the qual or not, but better be safe than sorry...
     */ 
    newQual = cnfify(lispCopy(qual), false);

    /*
     * Hacky stuff!
     * If the qual is a very simple one, i.e. "(<operator> <arg1> <arg2>)"
     * then the parser does NOT enclose it into an extra set of
     * parentheses and does NOT add the 'AND' operator.
     */

    /*
     * Now newQual is in CNF, i.e. it has a big 'AND'
     * clause and all its elements are OR clauses of
     * operators or maybe negations of operators.
     *
     * NOTE: there are also a couple of trivial cases:
     * a) the qual is NIL
     * b) the qual has only one clause and no explicit AND
     */
    newQual = prs2FindConstantClause(newQual, currentVarno, &isConstant);

    return(newQual);
    
}

/*------------------------------------------------------------------
 *
 * prs2FindConstantClause
 *
 * Traverse recursively a clause (AND, OR, NOT, Oper etc.)
 * and extract the "relaxed & constant" version of it, i.e. a
 * clause that has only Const nodes & attributes of the CURRENT relation
 * as operands and it is more general than the original query (i.e. every
 * tuple that satisfies the orignal clause will satisfy the new one).
 *
 * The '*isConstant' is true if the returned clause is the same
 * as the original, i.e. the original was already a "constant"
 * qual.
 *------------------------------------------------------------------
 */
LispValue
prs2FindConstantClause(clause, currentVarno, isConstant)
LispValue clause;
int currentVarno;
bool *isConstant;
{
    LispValue t;
    LispValue newSubclause;
    LispValue newAndClauses, newOrClauses;
    bool subclauseIsConst;

    if (null(clause)) {
	*isConstant = true;
	return(LispNil);
    } else if (IsA(clause,Var)) {
	/*
	 * Is it a CURRENT Var node ?
	 */
	if (get_varno(clause) == currentVarno) {
	    *isConstant = true;
	    return(clause);
	} else {
	    /*
	     * no it is a var node of another relation...
	     * tough luck!
	     */
	    *isConstant = false;
	    return(LispNil);
	}
    } else if (IsA(clause,Const)) {
	/*
	 * good! It is a constant....
	 */
	*isConstant = true;
	return(clause);
    } else if (is_clause(clause)) {
	/*
	 * it's an operator.
	 * Check if the operands are constants too...
	 */
	foreach(t, get_opargs(clause)) {
	    newSubclause = prs2FindConstantClause(CAR(t),
				    currentVarno,
				    &subclauseIsConst);
	    if (!subclauseIsConst) {
		/*
		 * at least one operand is not constant.
		 * That condemns the whole operator clause!
		 */
		*isConstant = false;
		return(LispNil);
	    }
	}
	/*
	 * all operands have passed the test!
	 * Return the clause as it is.
	 */
	*isConstant = true;
	return(clause);
    } else if (is_funcclause(clause)) {
	/*
	 * it's a function
	 * Check if its arguments are constants too...
	 */
	foreach(t, get_funcargs(clause)) {
	    newSubclause = prs2FindConstantClause(CAR(t),
				    currentVarno,
				    &subclauseIsConst);
	    if (!subclauseIsConst) {
		/*
		 * at least one operand is not constant.
		 * That condemns the whole operator clause!
		 */
		*isConstant = false;
		return(LispNil);
	    }
	}
	/*
	 * all arguments have passed the test!
	 * Return the clause as it is.
	 */
	*isConstant = true;
	return(clause);
    } else if (and_clause(clause)) {
	/*
	 * It's an AND clause.
	 * Apply the algorithm recursivelly to its
	 * subclauses, and return the conjuction of the results.
	 */
	*isConstant = true;
	newAndClauses = LispNil;
	foreach (t, get_andclauseargs(clause)) {
	    newSubclause = prs2FindConstantClause(CAR(t), 
				    currentVarno,
				    &subclauseIsConst);
	    *isConstant = *isConstant && subclauseIsConst;
	    if (newSubclause != LispNil)
		newAndClauses = nappend1(newAndClauses, newSubclause);
	}
	if (newAndClauses == LispNil) {
	    /*
	     * bad luck! We couldn't find a "relaxed" constant
	     * qualification...
	     */
	    return(LispNil);
	} else {
	    /*
	     * make an AND clause
	     */
	    return(make_andclause(newAndClauses));
	}
    } else if (or_clause(clause)) {
	/*
	 * It's an OR clause. Apply the algorithm recursively
	 * to all its subclauses. If any one of them returns
	 * LispNil though (i.e. can not be reduced to a more "relaxed"
	 * constant clause) then we have to return LispNil too!
	 */
	*isConstant= true;
	newOrClauses = LispNil;
	foreach (t, get_orclauseargs(clause)) {
	    newSubclause = prs2FindConstantClause(CAR(t),
				    currentVarno,
				    &subclauseIsConst);
	    *isConstant = *isConstant && subclauseIsConst;
	    if (newSubclause == LispNil) {
		return(LispNil);
	    } else {
		newOrClauses = nappend1(newOrClauses, newSubclause);
	    }
	}
	if (newOrClauses == LispNil) {
	    /*
	     * Hmm! this can only happen if we don't have
	     * any arguments inthe original OR clause...
	     * In any case, lets return LispNil..
	     */
	    return(LispNil);
	} else {
	    /*
	     * make an OR clause
	     */
	    return(make_orclause(newOrClauses));
	}
    } else if (not_clause(clause)) {
	/*
	 * It's a NOT clause.
	 * If the argument of not is a constant qual then
	 * return LispNil (there is nothing we can do about it..)
	 */
	newSubclause = prs2FindConstantClause(
			    get_notclausearg(clause),
			    currentVarno,
			    &subclauseIsConst);
	if (newSubclause == LispNil || subclauseIsConst) {
	    *isConstant = true;
	    return(LispNil);
	} else {
	    *isConstant = false;
	    return(make_notclause(newSubclause));
	}
    } else {
	/*
	 * well it is something we do not know how to handle...
	 * Play it cool & ignore it...
	 */
	*isConstant = false;
	return(LispNil);
    }
}
