/*======================================================================
 *
 * FILE:
 *  prs2putlocks.c
 *
 * $Header$
 *
 * DESCRIPTION:
 *
 * Take a deeeeeeep breath & read. If you can avoid hacking the code
 * below (i.e. if you have not been "volunteered" by the boss to do this
 * dirty job) avoid it at all costs. Try to do something less dangerous
 * for your (mental) health. Go home and watch horror movies on TV.
 * Read some Lovecraft. Join the Army. Go and spend a few nights in
 * people's park. Commit suicide...
 * Hm, you keep reading, eh? Oh, well, then you deserve what you get.
 * Welcome to the gloomy labyrinth of the tuple level rule system, my
 * poor hacker...
 *
 * This code is responsible for putting the rule locks and rule stubs.
 * for tuple-level-system rules.
 * For the time being we support the following implementations:
 *
 * ------------------------
 * A) Relation Level Locks:
 * ------------------------
 * All we do is to put a "relation level lock",
 * i.e. a lock to the appropriate pg_relation tuple. No rule stubs involved.
 * ALL the tuples of the relation are assumed locked by this lock.
 * This is the easiest implementation & it is quaranteed to work,
 * iff we use it for ALL rules we define.
 * If we use it for some rules only and for the others we use tuple level
 * locks, then read the discussion below....
 *
 * -----------------------------
 * B) (simple) Tuple Level Locks
 * -----------------------------
 * We start with the original rule qualification, and we extract a
 * "constant" qualification. This qualifications must satisfy
 * the following criteria:
 *	1) all tuples satisfying the original qualiifcation will
 *	satisfy the "constant" qual too (altohgou the opposite
 *	might not be true in general)
 *	2) the constan qualification will only reference attributes
 * 	of the CURRENT (locked) relation, i.e. it won't have any joins.
 * For example if the rule's qual is :
 *	where CURRENT.name = Mike 
 *	and CURRENT.dept = DEPT.dname
 *	and DEPT.floor = 1
 *	and CURRENT.age > 30
 *
 * the constant qual would be:
 *	where CURRENT.name = Mike and CURRENT.age>30
 *
 * We then add locks to all the tuples that satisfy the constant
 * qualification. We also add a stub record (which contains this
 * constant qualification, the rule locks and other misc info).
 * Every time we append/replace a tuple, we check one by one all the
 * stub records. If the new tuple satisfies the qualification of
 * such a stub record, we add the appropriate locks to it.
 * 
 * Simple, isn't it? Well, as the philosopher said, "if everything
 * seems to go right, then you have overlooked something".
 * Assume the rules:
 * 
 *		RULE 1:
 *		on retrieve to EMP.salary
 *		where current.name = "spyros"
 *		do instead retrieve (salary=1)
 *
 *		RULE 2:
 *		on retrieve to EMP.desk
 *		where current.salary = 1
 *		do instead retrieve (desk = "what desk?")
 *
 * Lets say that we have a tuple:
 *   [ NAME	SALARY	DESK  ]
 *   [ spyros	2	steel ]
 * Well, if we define RULE_1 first and RULE_2 second, everything is
 * OK. However, if we define RULE_2 first, then we will NOT put
 * a lock to spyros' tuple, because his salary is 2.
 * So, when later on we define RULE_1 we must put in spyros' tuple not
 * only the locks of RULE_1, but also the locks for RULE_2!
 *
 * To make things even worse, lets rewrite rule 1:
 * RULE 1:	on retrieve to EMP.salary
 *		where current.name = "spyros"
 *		do instead retrieve (E.salary) where E.name = "mike"
 * And lets say that initially mike has a salary equal to 2.
 * Now even if we define RULE_1 first and RULE_2 second, still
 * spyros' tuple will not get any locks. If later on we change mike's
 * salary from 2 to 1, then we are in trouble....
 *
 * So, to cut a long story longer:
 * a) we have to lock all the tuples that satisfy the constant qual OR
 *    have a "write" lock in any attribute referenced by this qual.
 *    Note that if such a "write" lock is a "relation level lock" (i.e
 *    it -implicitly- locks ALL the tuples of the relation), then we
 *    have to lock all the tuples of the relation too, so it
 *    would be better if we use a relation level lock too.
 * b) when we put a lock to a tuple, then if this lock is a "write"
 *    lock (i.e. if our new rule is something like "on retrieve ... do
 *    retrieve" we have also to check all the relation level stub records.
 *    For every stub if its qualification references the attribute
 *    we are currently putting this "write" lock on, then we have also to
 *    add to this tuple the stub's lock... And of course if this stub's
 *    lock is a "write" lock too, we have to repeat the process, until
 *    there are no more new "write" locks added.
 *
 * OK, you say, yes things do look bad, but anyway, we will survive...
 * Take a 5 minute (or hour or day or -even better- year) break, relax,
 * think about the good things in life (if any - remember you are just
 * a graduate student) and when you feel ready start reading again....
 *
 * If we have the rules RULE_1 & RULE_2 defined above, and RULE_1
 * was for some reason implemented with a relation level lock, then
 * all the tuples of EMP have a lock on salary, so RULE_2 would
 * end up locking all the tuples too, i.e. it would be better
 * to implement it with a relation level lock too.
 * If we define first RULE_2 (using tuple level locks) and then
 * we define RULE_1, then we have to delete RULE_1 (and all other rules
 * that use EMP.salary) and redefine it using relation level locks.
 *
 * OK, I said whatever I had to say. But before you say something
 * stupid like "well, at least it's over", remember that it took me
 * sometime to discover the problems above. Who know how many other
 * problems remain to be discovered too...
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
#include "utils/lsyscache.h"

/*------------------------------------------------------------------
 * prs2AddTheNewRule
 *
 * put all the apropriate rule locks/stubs & update the system relations.
 *
 * First try to implement this rule as a tuple-level-lock rule,
 * and if that fails, try a relation-level-lock.
 *
 *------------------------------------------------------------------
 */
void
prs2AddTheNewRule(r)
Prs2RuleData r;
{
    Prs2LockType lockType;
    AttributeNumber attributeNo;
    bool status;

    switch (r->eventType) {
	case EventTypeRetrieve:
	case EventTypeDelete:
	case EventTypeReplace:
	    /*
	     * first check if we can use tuple level locking.
	     * If no, then use relation level locks...
	     */
	    status = prs2DefTupleLevelLockRule(r);
	    if (!status) {
		prs2DefRelationLevelLockRule(r);
	    }
	    break;
	case EventTypeAppend:
	    /*
	     * "On append" rules only use relation level locks for the time
	     * being...
	     */
	    prs2DefRelationLevelLockRule(r);
	    break;
	default:
	    elog(WARN,
		"prs2TupleSystemPutLocks: Illegal event type %c",
		r->eventType);
    }
}

/*------------------------------------------------------------------
 * prs2DeleteTheOldRule
 *
 * delete a rule given its oid...
 *------------------------------------------------------------------
 */
void prs2DeleteTheOldRule(ruleId)
ObjectId ruleId;
{

    ObjectId relationId;

    /*
     * find the relation locked by the rule
     */
    relationId = get_eventrelid(ruleId);
    if (relationId == InvalidObjectId) {
	/*
	 * This should not happen!
	 */
	elog(WARN, "Can not find 'event' relation for rule %d", ruleId);
    }

    /*
     * Is it a tuple level lock rule ?
     */
    if (prs2IsATupleLevelLockRule(ruleId, relationId)) {
	prs2UndefTupleLevelLockRule(ruleId, relationId);
    } else {
	prs2UndefRelationLevelLockRule(ruleId, relationId);
    }
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
prs2FindLockTypeAndAttrNo(r, lockTypeResult, attributeNoResult)
Prs2RuleData r;
Prs2LockType *lockTypeResult;
AttributeNumber *attributeNoResult;
{
    Prs2LockType lockType;
    AttributeNumber attributeNo;

    lockType = LockTypeInvalid;
    attributeNo = InvalidAttributeNumber;

    if (r->actionType == ActionTypeRetrieveValue &&
	r->eventAttributeNumber == InvalidAttributeNumber) {
	/*
	 * this is a "view" rule....
	 */
	lockType = LockTypeRetrieveRelation;
	attributeNo = InvalidAttributeNumber;
    } else if (r->actionType == ActionTypeRetrieveValue ||
	r->actionType == ActionTypeReplaceCurrent ||
	r->actionType == ActionTypeReplaceNew) {
	/*
	 * In this case the attribute to be locked is the updated
	 * attribute...
	 */
	attributeNo = r->updatedAttributeNumber;
	switch (r->eventType) {
	    case EventTypeRetrieve:
		lockType = LockTypeRetrieveWrite;
		break;
	    case EventTypeReplace:
		lockType = LockTypeReplaceWrite;
		if (r->actionType == ActionTypeReplaceCurrent) {
		    elog(WARN, "ON REPLACE rules can not update CURRENT tuple");
		}
		break;
	    case EventTypeAppend:
		lockType = LockTypeAppendWrite;
		if (r->actionType == ActionTypeReplaceCurrent) {
		    elog(WARN, "ON APPEND rules can not update CURRENT tuple");
		}
		break;
	    case EventTypeDelete:
		lockType = LockTypeDeleteWrite;
		elog(WARN, "ON DELETE rules can not update CURRENT tuple");
		break;
	    default:
		elog(WARN,
		    "prs2FindLockType: Illegal Event type: %c", r->eventType);
	}
    } else if (r->actionType == ActionTypeOther) {
	/*
	 * In this case the attribute to be locked is the 'event'
	 * attribute...
	 */
	attributeNo = r->eventAttributeNumber;
	switch (r->eventType) {
	    case EventTypeRetrieve:
		lockType = LockTypeRetrieveAction;
		break;
	    case EventTypeReplace:
		lockType = LockTypeReplaceAction;
		break;
	    case EventTypeAppend:
		lockType = LockTypeAppendAction;
		break;
	    case EventTypeDelete:
		lockType = LockTypeDeleteAction;
		break;
	    default:
		elog(WARN,
		    "prs2FindLockType: Illegal Event type: %c", r->eventType);
	}
    } else {
	elog(WARN, "prs2FindLockType: Illegal Action type: %c", r->actionType);
    }

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

/*----------------------------------------------------------------------
 * prs2IsATupleLevelLockRule
 *
 * return true if this is a tuple-level-lock rule.
 * Check the stubs of the relation and if you find a stub for this rule
 * there, then it is a tuple-level-lock rule, otherwise not!
 *
 *----------------------------------------------------------------------
 */
bool
prs2IsATupleLevelLockRule(ruleId, relationId)
ObjectId ruleId;
ObjectId relationId;
{
    Prs2Stub stubs;
    int i;
    bool res;

    stubs = prs2GetRelationStubs(relationId);

    res = false;

    for (i=0; i<stubs->numOfStubs; i++) {
	if (stubs->stubRecords[i]->ruleId == ruleId) {
	    res = true;
	    break;
	}
    }

    prs2FreeStub(stubs);

    return(res);
}


