
/*==================================================================
 *
 * FILE:
 *   prs2attr.c
 *
 * IDENTIFICATION:
 *$Header$
 *
 * DESCRIPTION:
 *
 *==================================================================
 */

#include "c.h"
#include "palloc.h"
#include "log.h"
#include "ftup.h"	/* ModifyHeapTuple() defined here... */
#include "prs2.h"

/*---------------------------------------------------------------------
 *
 * attributeValuesCreate
 *
 * Given a tuple create the corresponding 'AttributeValues' array.
 * Note that in the beginnign all the 'value' entries are copied
 * form the tuple and that 'isCalculated' are all false.
 *
 */

AttributeValues
attributeValuesCreate(tuple, buffer, relation)
HeapTuple tuple;
Buffer buffer;
Relation relation;
{
    AttributeNumber i;
    AttributeNumber natts;
    AttributeValues attrValues;
    long size;
    Boolean isNull;

    natts = RelationGetNumberOfAttributes(relation);

    size = natts * sizeof(AttributeValuesData);
    attrValues = (AttributeValues) palloc(size);
    if (attrValues == NULL) {
	elog(WARN,"attributeValuesCreate: palloc(%ld) failed",size);
    }

    for (i=1; i<=natts; i++) {
	attrValues[i-1].value = HeapTupleGetAttributeValue(
				tuple,
				buffer,
				i,
				RelationGetTupleDescriptor(relation),
				&isNull);
	attrValues[i-1].isCalculated = (Boolean) 0;
	attrValues[i-1].isNull = isNull;
    }

    return(attrValues);
}

/*---------------------------------------------------------------------
 *
 * attributeValuesFree
 *
 * Free a previously allocated 'AttributeValues' array.
 */
void
attributeValuesFree(a)
AttributeValues a;
{
    pfree(a);
}

/*--------------------------------------------------------------------
 *
 * attributeValuesMakeNewTuple
 *
 * Given the old tuple and the values that the new tuple should have
 * create a new tuple.
 *
 * There are a couple of tricky things in here...
 *
 * Some of the attributes of the tuple (not necessarily all of them
 * though..) have been checked for rules, i.e. we have checked if there
 * is any rule that calculates a value for this attribute.
 * All these checked attributes have the field 'isCalculated' (of their
 * corresponding entry in array 'attrValues') equal to true.
 * On top of that, if there was an applicable rule, then the field
 * 'isChanged' is true (todenote that the value of this attribute
 * has changed because of a rule activation, therefore the value
 * stored in 'tuple' is now obsolete).
 * If no applicable rule was found then 'isChanged' is equal to false
 * and the value as stored in 'tuple' is the correct one.
 *
 * Finally all attributes not checked for rule activations (because
 * we don't need to calculate their values for the time being...)
 * have their 'isCalculated' field equal to false.
 *
 * Obviously enough, if at least one attribute has isChanged==true,
 * we must create a new tuple with the correct values in it (we
 * use 'ModifyHeapTuple()' for this purpose).
 * If no attribute has been changed we can use the olt 'tuple'
 * thus avoiding the copy (efficiency!).
 *
 * However we must also need change the locks of the tuple!
 * Say, if during a retrieve operation, a rule that had a lock of
 * type `LockTypeRetrieveWrite' has been activated and calculated a new
 * value for this attribute, then we must remove this lock, so that if
 * later on (at a higher node in the plan, maybe a join or a topmost
 * result node that is used when the user's operation is an append
 * or delete) we want to find the value of this (laready claculated) 
 * attribute, we do not unnecessarily reactivate the rule.
 * Another case where this happens is when we append a tuple.
 * We have to activate all rules that have `LockTypeAppendWrite' locks,
 * but then we can remove these locks because they are no longer
 * needed (a tuple is only appended once!).
 * The replace case is similar....
 *
 * XXX:
 * Hm.. here comes the tricky question. If no attribute has been
 * changed (i.e. all the 'isChanged' are false) but we must change
 * the locks of the tuple, what shall we do??
 * I thinke that we have the following options, but I don't know
 * which ones are gonna work and which not:
 *    1) We do not copy the tuple, and we just change its
 *       lock so that it points to the new lock:
 *       tuple->t_lcok.l_lock = newLock;
 *    2) We create a new copy of the tuple with the right locks.
 * I decided to do option # 2.
 *  
 *
 * This routine returns 0 if no tuple copy has been made and
 * therefore the old 'tuple' can be used, or 1 if there was
 * a tuple copy, in which case the 'newTupleP' point to
 * the new tuple (note: in this case the buffer for this
 * new tuple is the 'InvalidBuffer')
 */

int
attributeValuesMakeNewTuple(tuple, buffer, attrValues,
			    locks, lockType, relation, newTupleP)
HeapTuple tuple;
Buffer buffer;
AttributeValues attrValues;
RuleLock locks;
Prs2LockType lockType;
Relation relation;
HeapTuple *newTupleP;
{
    AttributeNumber natts;
    Datum *replaceValue;
    char *replaceNull;
    char *replace;
    long size;
    int i;
    bool tupleHasChanged;
    bool locksHaveToChange;
    HeapTuple t;
    int nlocks;
    RuleLock newLocks;
    Prs2OneLock oneLock;
    AttributeNumber attrNo;
    Prs2LockType thisLockType;

    /*
     * find the number of attributes of the tuple
     */
    natts = RelationGetNumberOfAttributes(relation);

    /*
     * Find if there was any attribute change and/or we have
     * to change the locks...
     * 
     */
    tupleHasChanged = false;
    for (i=0; i<natts; i++) {
	if (attrValues[i].isCalculated && attrValues[i].isChanged) {
	    tupleHasChanged = true;
	    break;
	}
    }

    if (tupleHasChanged) {
	/*
	 * Yes, this tuple has changed because of a rule
	 * activation. Create a new tuple with the
	 * right attribute values
	 *
	 * First create some arrays needed by 'ModifyHeapTuple()'
	 */
	size = natts * sizeof(Datum);
	replaceValue = (Datum *) palloc(size);
	if (replaceValue == NULL) {
	    elog(WARN,"attributeValuesCreate: palloc(%ld) failed",size);
	}

	size = natts * sizeof(char);
	replaceNull = (char *) palloc(size);
	if (replaceNull == NULL) {
	    elog(WARN,"attributeValuesCreate: palloc(%ld) failed",size);
	}

	size = natts * sizeof(Datum);
	replace = (char *) palloc(size);
	if (replace == NULL) {
	    elog(WARN,"attributeValuesCreate: palloc(%ld) failed",size);
	}

	/*
	 * Fill these arrays with the appropriate values
	 */
	for (i=0; i<natts; i++) {
	    if (attrValues[i].isCalculated && attrValues[i].isChanged) {
		replaceValue[i] = attrValues[i].value;
		replace[i] = 'r';
		if (attrValues[i].isNull) {
		    replaceNull[i] = 'n';
		} else {
		    replaceNull[i] = ' ';
		}
	    } else {
		replace[i] = ' ';
	    }
	}

	/*
	 * create the new tuple
	 */
	*newTupleP = ModifyHeapTuple( tuple, buffer,
			    relation, replaceValue,
			    replaceNull, replace);
	/*
	 * free the space occupied by the arrays
	 */
	pfree(replaceValue);
	pfree(replaceNull);
	pfree(replace);
    }


    nlocks = prs2GetNumberOfLocks(locks);

    newLocks = prs2MakeLocks();

    locksHaveToChange = false;
    for (i=0; i<nlocks; i++) {
	oneLock = prs2GetOneLockFromLocks(locks, i);
	attrNo = prs2OneLockGetAttributeNumber(oneLock);
	thisLockType = prs2OneLockGetLockType(oneLock);
	if (lockType != thisLockType ||
	    !attrValues[attrNo-1].isCalculated){
	    /*
	     * Copy this lock...
	     */
	    newLocks = prs2AddLock(newLocks,
			    prs2OneLockGetRuleId(oneLock),
			    prs2OneLockGetLockType(oneLock),
			    prs2OneLockGetAttributeNumber(oneLock),
			    prs2OneLockGetPlanNumber(oneLock));
	} else {
	    locksHaveToChange = true;
	}
    }
    /*
     * XXX: For the time being NO locks are stored in tuple,
     * they all come from the RelationRelation.
     * So, WE HAVE to put the locks in the tuple anyway...
     */
    if (prs2GetNumberOfLocks(newLocks) !=0) {
	locksHaveToChange = true;
    }

    if (locksHaveToChange && tupleHasChanged) {
	t = *newTupleP;
	*newTupleP = prs2PutLocksInTuple(
			    *newTupleP, InvalidBuffer,
			    relation, newLocks);
	pfree(t);
	return(1);
    } else if (!locksHaveToChange && tupleHasChanged) {
	/*
	 * this is impossible!!!
	 */
	elog(WARN,"attributeValuesMakeNewTuple: Internal Error");
    } else if (locksHaveToChange && !tupleHasChanged) {
	*newTupleP = prs2PutLocksInTuple(
			    tuple, buffer,
			    relation, newLocks);
	return(1);
    } else if (!locksHaveToChange && !tupleHasChanged) {
	prs2FreeLocks(newLocks);
	return(0);
    }
}
