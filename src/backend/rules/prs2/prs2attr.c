
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

#include "tmp/c.h"
#include "utils/palloc.h"
#include "utils/log.h"
#include "access/ftup.h"	/* ModifyHeapTuple() defined here... */
#include "rules/prs2.h"

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
	attrValues[i-1].isChanged = (Boolean) 0;
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
attributeValuesFree(a, relation)
AttributeValues a;
Relation relation;
{
    int i;
    AttributeNumber natts;
    TupleDescriptor tdesc;

    /*
     * free all space (if any) occupied by datums generated
     * by "prs2RunOnePlanAndGetValue()"
     * All these datums are creted via "datumCopy()"
     */
    tdesc = RelationGetTupleDescriptor(relation);
    natts = RelationGetNumberOfAttributes(relation);
    for(i=1; i<=natts; i++) {
	if (a[i-1].isCalculated && a[i-1].isChanged) {
	    if (!a[i-1].isNull)
		datumFree(a[i-1].value,
		    tdesc->data[i-1]->atttypid,		/* type */
		    tdesc->data[i-1]->attbyval,		/* byVal */
		    (Size) tdesc->data[i-1]->attlen);	/* type length */
	}
    }

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
#ifdef CHANGE_LOCKS
    ---------------------------------------------------
    XXX Currently we do NOT do that !!!         XXXXXXX
    ---------------------------------------------------
 * However we must also need change the locks of the tuple!
 * Say, if during a retrieve operation, a rule that had a lock of
 * type `LockTypeTupleRetrieveWrite' has been activated and calculated a new
 * value for this attribute, then we must remove this lock, so that if
 * later on (at a higher node in the plan, maybe a join or a topmost
 * result node that is used when the user's operation is an append
 * or delete) we want to find the value of this (laready claculated) 
 * attribute, we do not unnecessarily reactivate the rule.
 * Another case where this happens is when we append a tuple.
 * We have to activate all rules that have `LockTypeTupleAppendWrite' locks,
 * but then we can remove these locks because they are no longer
 * needed (a tuple is only appended once!).
 * The replace case is similar....
 *
 * XXX:
 * Hm.. here comes the tricky question. If no attribute has been
 * changed (i.e. all the 'isChanged' are false) but we must change
 * the locks of the tuple, what shall we do??
 * I think that we have the following options, but I don't know
 * which ones are gonna work and which not:
 *    1) We do not copy the tuple, and we just change its
 *       lock so that it points to the new lock:
 *       tuple->t_lcok.l_lock = newLock;
 *    2) We create a new copy of the tuple with the right locks.
 * I decided to do option # 2.
#else
 * If a new tuple is created, then it will always have an empty
 * lock in it.
#endif CHANGE_LOCKS
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


#ifdef CHANGE_LOCKS
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
	prs2PutLocksInTuple(
			    *newTupleP, InvalidBuffer,
			    relation, newLocks);
	return(1);
    } else if (!locksHaveToChange && tupleHasChanged) {
	/*
	 * this is impossible
	 */
	elog(WARN,"attributeValuesMakeNewTuple: Internal error");
    } else if (locksHaveToChange && !tupleHasChanged) {
	prs2PutLocksInTuple( tuple, buffer,
			    relation, newLocks);
	*newTupleP = tuple;
	return(1);
    } else if (!locksHaveToChange && !tupleHasChanged) {
	if (newLocks != InvalidRuleLock)
	    prs2FreeLocks(newLocks);
	return(0);
    }
#else
    if (tupleHasChanged) {
	/*
	 * NOTE: the new tuple, has NO LOCKS IN IT!
	 * (well, actually it has an empty lock).
	 */
	return(1);
    } else {
	return(0);
    }
#endif CHANGE_LOCKS
}

/*--------------------------------------------------------------------
 *
 * attributeValuesCombineNewAndOldTuple
 *
 * Hm.. this is a little bit tricky, so I'll try to do my best:
 * When we want to replace a tuple, the executor has first to retrieve
 * it. So it calls the Access Methods and retrieves what we call the
 * 'raw' tuple, i.e. the tuple as stored in the database.
 * Then it calls the rule manager to activate any 'ON RETRIEVE' rules,
 * and if there are some backward chaining rules that calculate values
 * for some attributes of this tuple, then the rule manager forms a
 * new tuple. This is what we call the 'old' tuple.
 * (NOTE: both the 'old' and 'raw' tuples are stored in the
 * 'qualification_tuple' and 'raw_qualification_tuple' fields of 
 * the 'EState').
 * Finally this 'old' tuple (if it satisfies the qualifications of the
 * replace command) must be replaced. So, the executor forms a new tuple,
 * which is a copy of the 'old' tuple + the updates proposed by the
 * 'replace' command as issued by the user.
 * Then, a final call to the rule manager is made to activate all the
 * 'ON REPLACE' rules. Some of them might be backward chaining rules of
 * the form 'ON REPLACE EMP.salary WHERE .... DO REPLACE CURRENT(desk = ...)'
 * so they might cause even more updated to the tuple and possibly trigger
 * even more rules etc, and to cut a long story short the whole hell can
 * break loose. My completely bug free code handles all these cases
 * in the most perfect way, and what we end up having is:
 * the 'raw' tuple as described above, and a brand 'new' tuple which 
 * incorporates a) all changes made by ON RETRIEVE rules, b) all user
 * updates and c) all changes made by ON REPLACE rules.
 * Well, the tuple that has to be stored in the database (lets call
 * it the 'returned' tuple) must ONLY incorporate (b) and (c).
 * IT MUST NOT INCORPORATE changes made by ON RETRIEVE rules !!!
 * In order to convince all of you that are not as clever as me and
 * can not see the light, here is an example:
 * We have the rule:
 *   ON RETRIEVE TO EMP.salary DO INSTEAD RETRIEVE (salary = 1)
 * and the tuple:
 *   name="spyros", salary = 9999999999999999, status = "prophet"
 * and the user 'replace' command:
 *   replace EMP(status = "god") where EMP.name = "spyros"
 * So the 'raw' tuple is:
 *   name="spyros", salary = 9999999999999999, status = "prophet"
 * the 'old' tuple (after the ON RETRIEVE rule is activated)
 *   name="spyros", salary = 1, status = "prophet"
 * and the 'new' tuple as formed by the executor (after making all changes
 * suggested by the 'replace' command):
 *   name="spyros", salary = 1, status = "god"
 * 
 * Obviously enough, if we store this tuple in the database and
 * then delete the rule, then spyros' salary will be '1' and not
 * what he really deserves (i.e. '9999999999999999').
 * 
 * So, what we have to do, is somehow keep track of what attributes
 * of the 'new' tuple have been changed either by the user ('status')
 * or by ON REPLACE rules (none in our example), and when forming the
 * 'returned' tuple we must use for these attributes the values stored
 * in 'new' tuple, while for all other attributes we have to use
 * the values as stored in 'raw' tuple.
 *
 * In case you haven't suspected it so far, that's exactly what this
 * routine is supposed to do....
 */

HeapTuple
attributeValuesCombineNewAndOldTuple(rawAttrValues, newAttrValues,
		    relation, attributeArray, numberOfAttributes)
AttributeValues rawAttrValues;		/* values as stored in 'raw' tuple */
AttributeValues newAttrValues;		/* values of the 'new' tuple */
Relation relation;
AttributeNumberPtr attributeArray;	/* attributes changed by the user */
AttributeNumber numberOfAttributes;	/* size of 'attributeArray' */
{
    AttributeNumber natts;
    AttributeNumber i,j;
    Boolean replacedByRule;
    Boolean replacedByUser;
    char *nulls;
    Datum *values;
    HeapTuple resultTuple;
    long int size;

    /*
     * find the number of attributes of the tuple
     */
    natts = RelationGetNumberOfAttributes(relation);

    /*
     * Now create some arrays needed by 'ModifyHeapTuple()'
     */
    size = natts * sizeof(Datum);
    values = (Datum *) palloc(size);
    if (values == NULL) {
	elog(WARN,"attributeValuesCreate: palloc(%ld) failed",size);
    }

    size = natts * sizeof(char);
    nulls = (char *) palloc(size);
    if (nulls == NULL) {
	elog(WARN,"attributeValuesCreate: palloc(%ld) failed",size);
    }

    /*
     * Fill these arrays with the right values.
     * For all the attributes of the tuple, check if the
     * attribute has been replaced by the user or by a rule.
     * If yes, then use the value as stored in 'newAttrValues'.
     * Otherwise, use the value as stored in the 'rawAttrValues'.
     */
    for (i=1; i<=natts; i++) {
	replacedByUser = (Boolean) 0;
	for (j=0; j<numberOfAttributes; j++) {
	    if (attributeArray[j] == i) {
		replacedByUser = (Boolean) 1;
		break;
	    }
	}
	replacedByRule = newAttrValues[i-1].isChanged;
	if (replacedByRule || replacedByUser) {
	    values[i-1] = newAttrValues[i-1].value;
	    if (newAttrValues[i-1].isNull) {
		nulls[i-1] = 'n';
	    } else {
		nulls[i-1] = ' ';
	    }
	} else {
	    values[i-1] = rawAttrValues[i-1].value;
	    if (rawAttrValues[i-1].isNull) {
		nulls[i-1] = 'n';
	    } else {
		nulls[i-1] = ' ';
	    }
	}
    }

    /*
     * create the new tuple
     */
    resultTuple = FormHeapTuple(
			RelationGetNumberOfAttributes(relation),
			RelationGetTupleDescriptor(relation),
			values, nulls);
			
    /*
     * free the space occupied by the arrays
     */
    pfree(values);
    pfree(nulls);

    /*
     * return the new tuple
     */
    return(resultTuple);
}

