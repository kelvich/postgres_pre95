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
 * GENERIC RULE INFORMATION FOR A RELATION
 *
 * NOTE: These definitions normally would go to 'prs2.h', but as
 * doing so creates circular dependencies, I decided to put them
 * here.
 *
 * When we retrieve a tuple from a relation we have to call the
 * rule manager. Therfore, in every scan node we have to store some
 * information about rules.
 * So, `CommonScanState' (see execnodes.h) contain a field called
 * 'css_ruleInfo' where all this information is stored.
 * This information should be correctly initialized at the same time the
 * rest of `ScanState' is also initialized (this happens when the
 * executor is called to perform the EXEC_START operation).
 *
 * Similarly when we update a relation (i.e. append. delete or replace a
 * tuple of this relation) we must store some information about the rules
 * defined in this relation (which is called the "result relation").
 * This information is stored in the EState.
 *
 * We use the same structure for both these cases, because they
 * share a lot of information.
 * However some of the fields of this structure apply only to one
 * of these cases and are given null values in the other.
 *
 * The scan state rule information contains the current fields:
 * 
 * ruleList: this is a linked list of records containing information
 *	about all the 'view' like rules, i.e. rules of the form
 *         ON retrieve to <relation>
 *         DO [ instead ] retrieve ......
 *	These rules are processed first by the executor (i.e. before we even
 *	 call the access methods for the first time) to retrieve (one by one)
 *	all the tuples returned by the action pat of the rule.
 *	Then if none of these 'view' rules was an "instead" rule, we process
 *	the "real" tuples (the ones returned by the access methods).
 *	
 *	Each record in ruleList can be either a ruleId - plan number pair, 
 *	or a plan (a LispValue) corresponding to a rule (NOTE: each rule
 *	can have more than one plans!) or a query descriptor - EState
 *	pair (in which case we have started retrieving tuples, one by one).
 *	A NULL ruleList means that there are no (more) rules to be processed.
 *
 *	This field only makes sense if we are retrieving tuples from a
 *	relation.
 * 
 * insteadViewRuleFound: True if there was at least one "instead" view
 *	rule was encountered, false otherwise.
 *	This field is only used on retrieve operations.
 *
 * relationLock: the (relation level) rule locks for the scanned
 *	relation (found in the Relation relation)
 *	This is used in both retrieve/update operations.
 *
 * relationStubs: the (relation level) rule stubs.
 *	Normally we would only use them in append & replace operetions
 *	(not on delete and/or retrieves).
 *	But we also have to use them when we run "rule plans"
 *	that add/delete rule locks and/or rule stubs.
 *	These "rule plans" run when import/export locks are broken,
 *	and for the time being they look
 *
 * relationStubsHaveChanged: a boolean flag showing whether
 *	the stubs of a relation have been updated.
 *	This only happens when we run "rule plans" that update
 *	rule locks and/or rule stubs.
 *
 * ignoreTupleLocks: If true, then ignore all the tuple level locks
 *	we'll find in the tuples of this relation.
 *	This is more or less a hack for "pg_class".
 *	Currently we store the relation level locks in the appropriate
 *	tuple of "pg_class". But as the rule manager must not confuse
 *	these locks with tuple level locks of rules defined on "pg_class",
 *	we set this flag whenever the relation we scan is "pg_class".
 *	That of course means that we can not define rules (that use tuple
 *	level locks) on "pg_class", but that's OK....
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
	    Pointer estate; /* THIS IS AN ESTATE */
	} queryDesc;
    } data;
} Prs2RuleListItem;

typedef Prs2RuleListItem *Prs2RuleList;

typedef struct RelationRuleInfoData {
    Prs2RuleList ruleList;
    bool insteadViewRuleFound;
    RuleLock relationLocks;
    Prs2Stub relationStubs;
    bool relationStubsHaveChanged;
    bool ignoreTupleLocks;
} RelationRuleInfoData;

typedef RelationRuleInfoData *RelationRuleInfo;

/*-------------
 * Create and initialize a 'RelationRuleInfo'
 */
extern
RelationRuleInfo
prs2MakeRelationRuleInfo ARGS((
    Relation relation,
    int operation
));

/*-------------
 * Activate the "view" rules & return one by one all the appropriate
 * tuples...
 */
extern
HeapTuple
prs2GetOneTupleFromViewRules ARGS((
    RelationRuleInfo	scanStateRuleInfo,
    Prs2EStateInfo	prs2EStateInfo,
    Relation		relation,		   
    Relation		explainRel
));

/*============== END OF RULE_SCAN_INFO DEFINITIONS ======================*/

#endif RuleScan_H
