/*---------------------------------------------------------------
 *
 * FILE
 *    prs2main.c
 *
 * DESCRIPTION
 *   top level PRS2 rule manager routines (called by the executor)
 *
 * INTERFACE ROUTINES
 *   prs2main()
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "parser/parse.h"	/* RETRIEVE et all are defined here */
#include "utils/log.h"
#include "utils/rel.h"
#include "access/htup.h"
#include "storage/buf.h"
#include "rules/prs2.h"
#include "executor/executor.h"

/*------------------------------------------------------------------
 *
 * prs2main()
 *
 * Activate any rules that apply to the attributes specified.
 *
 *
 * Explanation of arguments:
 *   estate: the EState (executor state)
 *   scanStateRuleInfo:
 *	If the rule manager is called as part of a 'scan'
 *	(i.e. the operation is a 'RETRIEVE' then this contains
 *	some useful info about the scanned relation...
 *   operation:
 *	the kind of operation performed.
 *	this can only be a RETRIEVE (everything else is ignored).
 *   userId:
 *	the user Id (currently ignored).
 *   relation:
 *	the relation where oldTuple & newTuple belong.
 *   oldTuple:
 *      If the operation is a RETRIEVE this is the base tuple, as
 *      returned form the access methods (i.e. no rules have been applied
 *      yet).
 *      If the operation is a DELETE then this is the tuple to be
 *      deleted.
 *      If the operation is a REPLACE then this is the tuple
 *      that is going to be replace with the new tuple.
 *   oldBuffer:
 *	the Buffer of the oldTuple.
 *   newTuple:
 *      If the operation is a APPEND then this is the new tuple
 *      to be appended.
 *      If the operation is a REPLACE then this is the new version
 *      of the tuple (the one that will replace oldTuple).
 *   newBuffer:
 *	the Buffer of the newTuple.
 *   rawTuple:
 *	this is the same as the oldTuple, but exactly as returned
 *	but the access methods. Normally, when a tuple is retrieved,
 *	it is passed to the rule manager and if there are some 
 *	backward chaining rules defined on it, some of its attribute
 * 	values will be changed. If the final operation was a replace, 
 * 	then oldTuple will be this changed tuple, ad rawTuple will be the
 *	tuple, as it was returned by the AM with no rule activations.
 *	'rawTuple' is a valid tuple if the operation is REPLACE,
 * 	and it is ignored in all other cases.
 *   rawBuffer:
 *	the buffer of the rawTuple.
 *   attributeArray
 *	An array of AttributeNumber. These are the attributes that
 *      are affected by the specified operation.
 *      If the operation is a RETRIEVE or a REPLACE then these
 *      are the attributes that are retrieved or replaced.
 *      In the xcase of APPEND & DELETE we assume that all the
 *      attributes of the tuple are affected.
 *   numberOfAttributes
 *      the number of attributes in attributeArray.
 *   returnedTupleP
 *      If the operation was a RETRIEVE and the value of the
 *      oldTuple has changed because of a rule, then this is 
 *      the new tuple (that has to be used by the executor in
 *      the place of the oldTuple.
 *      If the operation was a REPLACE or APPEND and newTuple had
 *      to be changed because of a rule, then this is the new tuple
 *      that the executor has to use in the place of newTuple.
 *   returnedBufferP
 *      The Buffer of the returnedTuple (usually InvalidBuffer).
 *
 * Returned values:
 *   PRS2_STATUS_TUPLE_UNCHANGED
 *      Probably no applicable rules were found. Proceed as normal
 *      returnedTupleP and returnedBufferP should NOT be used.
 *   PRS2_STATUS_TUPLE_CHANGED
 *	One or more rules were activated, (*returnedTupleP) should
 *      be used (instead of oldTuple or newTuple).
 *   PRS2_STATUS_INSTEAD
 *      An instead was specified in the action part of a rule,
 *      and therefore the executor must do nothing.
 *
 */

Prs2Status
prs2Main(estate, scanStateRuleInfo, operation, userId, relation,
	    oldTuple, oldBuffer,
	    newTuple, newBuffer,
	    rawTuple, rawBuffer,
	    attributeArray, numberOfAttributes,
	    returnedTupleP, returnedBufferP)
EState estate;
ScanStateRuleInfo scanStateRuleInfo;
int operation;
int userId;
Relation relation;
HeapTuple oldTuple;
Buffer oldBuffer;
HeapTuple newTuple;
Buffer newBuffer;
AttributeNumberPtr attributeArray;
int numberOfAttributes;
HeapTuple *returnedTupleP;
Buffer *returnedBufferP;
{
    Prs2Status status;
    Buffer localBuffer;
    Prs2EStateInfo prs2EStateInfo;
    int topLevel;
    Relation explainRelation;

    if (relation == NULL) {
	/*
	 * This should not happen!
	 */
	elog(WARN, "prs2main: called with relation = NULL");
    }

    if (returnedBufferP == NULL) {
	returnedBufferP = &localBuffer;
    }

    explainRelation = get_es_explain_relation(estate);

    prs2EStateInfo = get_es_prs2_info(estate);
    if (prs2EStateInfo == NULL) {
	prs2EStateInfo = prs2RuleStackInitialize();
	topLevel = 1;
    } else {
	topLevel = 0;
    }

    switch (operation) {
	case RETRIEVE:
	    status = prs2Retrieve(
				prs2EStateInfo,
				scanStateRuleInfo,
				explainRelation,
				oldTuple,
				oldBuffer,
				attributeArray,
				numberOfAttributes,
				relation,
				returnedTupleP,
				returnedBufferP);
	    break;
	case DELETE:
	    status = prs2Delete(
				prs2EStateInfo,
				explainRelation,
				oldTuple,
				oldBuffer,
				relation);
	    break;
	case APPEND:
	    status = prs2Append(
				prs2EStateInfo,
				explainRelation,
				newTuple,
				newBuffer,
				relation,
				returnedTupleP,
				returnedBufferP);
	    break;
	case REPLACE:
	    status = prs2Replace(
				prs2EStateInfo,
				explainRelation,
				oldTuple,
				oldBuffer,
				newTuple,
				newBuffer,
				rawTuple,
				rawBuffer,
				attributeArray,
				numberOfAttributes,
				relation,
				returnedTupleP,
				returnedBufferP);
	    break;

	default:
	    elog(WARN, "prs2main: illegal operation %d", operation);

    } /* switch */

    if (topLevel) {
	prs2RuleStackFree(prs2EStateInfo);
    }
    return(status);
}
