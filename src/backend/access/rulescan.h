/*========================================================================
 *
 * FILE:
 * rulescan.h
 *
 * HEADER:
 * $Header$
 *
 *-----------------------------------------------------------------------
 * NOTE:
 * -----
 * This file is not supposed to be included directly!
 * Normally all these definitions would habe gone to "prs2.h", but
 * as you can see they use the `EState' defined in "execnodes.h", 
 * which in turn includes "prs2.h", thus creating a circular dependency.
 *
 * And as the "inh" shell scripts are not clever enough to distinguish
 * between a normal typedef (like the ones in this file) and the
 * magic "Class" and "Public" definitions, I decided to create this
 * extra file, is is included in "execnodes.h".
 * If you want to use the structures defined in this file,
 * you must NOT include it directly. Include "execnodes.h" instead!
 *
 *------------------------------------------------------------------------
 *
 *
 * SCAN STATE RULE INFORMATION
 *
 * NOTE: These definitions normally would go to 'prs2.h', but as
 * doing so creates circular dependencies, I decided to put them
 * here.
 *
 * In every scan node we have to stroe some information about rules.
 * So, `CommonScanState' (see execnodes.h) contain a field called
 * 'css_ruleInfo' where all this information is stored.
 * This information should be correctly initialized at the same time the
 * rest of `ScanState' is also initialized (this happens when the
 * executor is called to perform the EXEC_START operation).
 * 
 * The scan state rule information contains the current fields:
 * 
 * ruleList: this is a linked list of records containing information
 * about all the 'view' like rules, i.e. rules of the form
 *       ON retrieve to <relation>
 *       DO [ instead ] retrieve ......
 * These rules are processed first by the executor (i.e. before we even
 * call the access methods for the first time) to retrieve (one by one)
 * all the tuples returned by the action pat of the rule.
 * Then if none of these 'view' rules was an "instead" rule, we process
 * the "real" tuples (the ones returned by the access methods).
 * 
 * Each record in ruleList can be either a ruleId - plan number pair, 
 * or a plan (a LispValue) corresponding to a rule (NOTE: each rule
 * can have more than one plans!) or a query descriptor - EState
 * pair (in which case we have started retrieving tuples, one by one).
 * A NULL ruleList means that there are no (more) rules to be processed.
 * 
 * insteadRuleFound: True if there was at least one "instead"
 * rule was encountered, false otherwise.
 *
 * relationLock: the (relation level) rule locks for the scanned
 * relation (found in the Relation relation)
 * 
 */

#include "rules/prs2stub.h"

#ifndef RuleScan_H
#define RuleScan_H 1


#define PRS2_RULELIST_RULEID	1
#define PRS2_RULELIST_PLAN	2
#define PRS2_RULELIST_QUERYDESC	3
#define PRS2_RULELIST_DONE	99
#define PRS2_RULELIST_INVALID	100

typedef struct Prs2RuleListItem {
    int type;
    struct Prs2RuleListItem *next;
    union {
	struct ruleId {
	    ObjectId ruleId;
	    Prs2PlanNumber planNumber;
	} ruleId;
	struct rulePlan {
	    LispValue plan;
	} rulePlan;
	struct {
	    LispValue queryDesc;
	    struct EState *estate; /* XXXXX THIS IS A HACK! */
	} queryDesc;
    } data;
} Prs2RuleListItem;

typedef Prs2RuleListItem *Prs2RuleList;

typedef struct ScanStateRuleInfoData {
    Prs2RuleList ruleList;
    bool insteadRuleFound;
    RuleLock relationLocks;
    Prs2Stub relationStubs;
    bool relationStubsHaveChanged;
} ScanStateRuleInfoData;

typedef ScanStateRuleInfoData *ScanStateRuleInfo;

/*-------------
 * Create and initialize a 'ScanStateRuleInfo'
 */
extern
ScanStateRuleInfo
prs2MakeScanStateRuleInfo ARGS((
    Relation relation
));

/*-------------
 * Activate the "view" rules & return one by one all the appropriate
 * tuples...
 */
extern
HeapTuple
prs2GetOneTupleFromViewRules ARGS((
    ScanStateRuleInfo	scanStateRuleInfo,
    Prs2EStateInfo	prs2EStateInfo,
    Relation		explainRel
));

/*============== END OF RULE_SCAN_INFO DEFINITIONS ======================*/

#endif RuleScan_H
