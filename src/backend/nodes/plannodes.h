/* ----------------------------------------------------------------
 *   FILE
 *	plannodes.h
 *	
 *   DESCRIPTION
 *	definitions for query plan nodes
 *
 *   NOTES
 *	this file is listed in lib/Gen/inherits.sh and in the
 *	INH_SRC list in conf/inh.mk and is used to generate the
 *	obj/lib/C/plannodes.c file
 *
 *	For explanations of what the structure members mean,
 *	see ~postgres/doc/query-tree.t.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PlanNodesIncluded
#define	PlanNodesIncluded

#include "tmp/postgres.h"

#include "nodes/nodes.h"		/* bogus inheritance system */
#include "nodes/pg_lisp.h"
#include "executor/recursion_a.h"       /* recursion stuff that must go first */
#include "nodes/primnodes.h"
#include "rules/prs2stub.h"

/* ----------------------------------------------------------------
 *  Executor State types are used in the plannode structures
 *  so we have to include their definitions too.
 *
 *	Node Type		node information used by executor
 *
 * control nodes
 *
 *	Existential		ExistentialState	exstate;
 *	Result			ResultState		resstate;
 *	Append			AppendState		unionstate;
 *	Parallel		ParallelState		parstate;
 *      Recursive               RecursiveState          recurstate;
 *
 * scan nodes
 *
 *    	Scan ***		ScanState		scanstate;
 *    	IndexScan		IndexScanState		indxstate;
 *
 *	  (*** nodes which inherit Scan also inherit scanstate)
 *
 * join nodes
 *
 *	NestLoop		NestLoopState		nlstate;
 *	MergeJoin		MergeJoinState		mergestate;
 *	HashJoin		HashJoinState		hashjoinstate;
 *
 * materialize nodes
 *
 *	Material		MaterialState		matstate;
 *      Sort			SortState		sortstate;
 *      Unique			UniqueState		uniquestate;
 *      Hash			HashState		hashstate;
 *
 * ----------------------------------------------------------------
 */
#include "nodes/execnodes.h"		/* XXX move executor types elsewhere */


/* ----------------------------------------------------------------
 *	Node Function Declarations
 *
 *  All of these #defines indicate that we have written print/equal/copy
 *  support for the classes named.  The print routines are in
 *  lib/C/printfuncs.c, the equal functions are in lib/C/equalfincs.c and
 *  the copy functions can be found in lib/C/copyfuncs.c
 *
 *  An interface routine is generated automatically by Gen_creator.sh for
 *  each node type.  This routine will call either do nothing or call
 *  an _print, _equal or _copy function defined in one of the above
 *  files, depending on whether or not the appropriate #define is specified.
 *
 *  Thus, when adding a new node type, you have to add a set of
 *  _print, _equal and _copy functions to the above files and then
 *  add some #defines below.
 *
 *  This is pretty complicated, and a better-designed system needs to be
 *  implemented.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	Node Print Function declarations
 * ----------------
 */
#define	PrintPlanExists
#define	PrintResultExists
#define	PrintExistentialExists
#define	PrintAppendExists
#define	PrintRecursiveExists
#define	PrintJoinExists
#define	PrintNestLoopExists
#define	PrintMergeJoinExists
#define	PrintHashJoinExists
#define	PrintScanExists
#define	PrintSeqScanExists
#define	PrintIndexScanExists
#define	PrintTempExists
#define	PrintSortExists
#define	PrintHashExists
#define PrintUniqueExists
#define PrintFragmentExists
#define PrintScanTempsExists

extern void	PrintPlan();
extern void	PrintResult();
extern void	PrintExistential();
extern void	PrintAppend();
extern void	PrintRecursive();
extern void	PrintJoin();
extern void	PrintNestLoop();
extern void	PrintMergeJoin();
extern void	PrintHashJoin();
extern void	PrintScan();
extern void	PrintSeqScan();
extern void	PrintIndexScan();
extern void	PrintTemp();
extern void	PrintSort();
extern void	PrintHash();
extern void     PrintScanTemps();
extern void     PrintFragment();

/* ----------------
 *	Node Equal Function declarations
 * ----------------
 */
#define EqualFragmentExists

extern bool	EqualPlan();
extern bool	EqualResult();
extern bool	EqualExistential();
extern bool	EqualAppend();
extern bool	EqualRecursive();
extern bool	EqualJoin();
extern bool	EqualNestLoop();
extern bool	EqualMergeJoin();
extern bool	EqualHashJoin();
extern bool	EqualScan();
extern bool	EqualSeqScan();
extern bool	EqualIndexScan();
extern bool	EqualTemp();
extern bool	EqualSort();
extern bool	EqualHash();
extern bool     EqualFragment();

/* ----------------
 *	Node Copy Function declarations
 * ----------------
 */
#define	CopyPlanExists
#define	CopyResultExists
#define	CopyExistentialExists
#define	CopyAppendExists
#define	CopyRecursiveExists
#define	CopyJoinExists
#define	CopyNestLoopExists
#define	CopyMergeJoinExists
#define	CopyHashJoinExists
#define	CopyScanExists
#define	CopySeqScanExists
#define	CopyIndexScanExists
#define	CopyTempExists
#define	CopySortExists
#define	CopyHashExists
#define CopyUniqueExists
#define CopyScanTempsExists

extern bool	CopyPlan();
extern bool	CopyResult();
extern bool	CopyExistential();
extern bool	CopyAppend();
extern bool	CopyRecursive();
extern bool	CopyJoin();
extern bool	CopyNestLoop();
extern bool	CopyMergeJoin();
extern bool	CopyHashJoin();
extern bool	CopyScan();
extern bool	CopySeqScan();
extern bool	CopyIndexScan();
extern bool	CopyTemp();
extern bool	CopySort();
extern bool	CopyHash();
extern bool     CopyScanTemps();

/* ----------------------------------------------------------------
 *			node definitions
 * ----------------------------------------------------------------
 */

/* ----------------
 *	Plan node
 * ----------------
 */

/* followind typedef's were added and  'struct Blah''s were replaced
 * with 'BlahPtr' in the #define's.  The reason?  Sprite cannot handle
 * 'struct Blah *blah'.		- ron 'blah blah' choi
 */
typedef	struct EState *EStatePtr;
typedef	struct ReturnState *ReturnStatePtr;
typedef	struct Plan *PlanPtr;

class (Plan) public (Node) {
#define PlanDefs \
	inherits(Node); \
	Cost			cost; \
	Count			plan_size; \
	Count			plan_width; \
	int			fragment; \
	int			parallel; \
	EStatePtr		state; \
	ReturnStatePtr		retstate; \
	List			qptargetlist; \
	List			qpqual; \
	PlanPtr			lefttree; \
	PlanPtr			righttree
 /* private: */
	PlanDefs;
 /* public: */
};


/* --------------
 *  Fragment node
 * --------------
 */
/* followind typedef's were added and  'struct Blah''s were replaced
 * with 'BlahPtr' in the #define's.  The reason?  Sprite cannot handle
 * 'struct Blah *blah'.		- ron 'blah blah' choi
 */
typedef	struct Fragment *FragmentPtr;

class (Fragment) public (Node) {
#define FragmentDefs \
        inherits(Node); \
        Plan                    frag_root; \
        Plan                    frag_parent_op; \
        int                     frag_parallel; \
        List                    frag_subtrees; \
        FragmentPtr         	frag_parent_frag
 /* private: */
        FragmentDefs;
 /* public: */
};

/*
 * ===============
 * Top-level nodes
 * ===============
 */

/* ----------------
 *	existential node
 * ----------------
 */
class (Existential) public (Plan) {
	inherits(Plan);
 /* private: */
	ExistentialState	exstate;
 /* public: */
};

/* ----------------
 *	result node
 * ----------------
 */
class (Result) public (Plan) {
	inherits(Plan);
 /* private: */
	List			resrellevelqual;
	List			resconstantqual;
	ResultState		resstate;
 /* public: */
};

/* ----------------
 *	append node
 * ----------------
 */
class (Append) public (Plan) {
	inherits(Plan);
 /* private: */
	List			unionplans;
	Index			unionrelid;
	List			unionrtentries;
	AppendState		unionstate;
 /* public: */
};

/* ----------------
 *	parallel-append node
 * ----------------
 */
class (Parallel) public (Plan) {
	inherits(Plan);
 /* private: */
	ParallelState		parstate;
 /* public: */
};

/*****=====================
 *  JJJ - recursion node
 *****=====================
 *
 *      recurMethod             method chosen for recursive query evaluation
 *      recurCommand            e.g. append, delete
 *      recurInitPlans          plans/utilities executed once at start
 *      recurLoopPlans          sequence to be repeated until no effects
 *                                      are detected
 *      recurCleanupPlans       plans/utilities executed once at end
 *      recurCheckpoints        indicate which plans affect the true result
 *
 *	recurResultRelationName name of original result relation
 */

class (Recursive) public (Plan) {
        inherits(Plan);
 /* private */
        RecursiveMethod         recurMethod;
        LispValue		recurCommand;
        List                    recurInitPlans;
        List                    recurLoopPlans;
        List                    recurCleanupPlans;
        List                    recurCheckpoints;
	LispValue		recurResultRelationName;
        RecursiveState          recurState;
 /* public: */
};

/*
 * ==========
 * Scan nodes
 * ==========
 */

class (Scan) public (Plan) {
#define	ScanDefs \
	inherits(Plan); \
	Index			scanrelid; \
   	ScanState		scanstate
 /* private: */
	ScanDefs;
 /* public: */
};

/* ----------------
 *	sequential scan node
 * ----------------
 */
class (SeqScan) public (Scan) {
	inherits(Scan);
 /* private: */
 /* public: */
};

class (ScanTemps) public (Plan) {
#define ScanTempDefs \
        inherits(Plan); \
        List        temprelDescs; \
        ScanTempState   scantempState
/* private: */
        ScanTempDefs;
/* public: */
};

/* ----------------
 *	index scan node
 * ----------------
 */
class (IndexScan) public (Scan) {
	inherits(Scan);
 /* private: */
	List			indxid;
	List			indxqual;
	IndexScanState		indxstate;
 /* public: */
};

/*
 * ==========
 * Join nodes
 * ==========
 */

/* ----------------
 *	Rule information stored in Join nodes
 * ----------------
 */
class (JoinRuleInfo) public (Node) {
	inherits(Node);
 /* private: */
	ObjectId	jri_operator;
	AttributeNumber	jri_inattrno;
	AttributeNumber	jri_outattrno;
	RuleLock	jri_lock;
	ObjectId	jri_ruleid;
	Prs2StubId	jri_stubid;
	Prs2OneStub	jri_stub;
	Prs2StubStats	jri_stats;
 /* public: */
};

/* ----------------
 *	Join node
 * ----------------
 */

/* followind typedef's were added and  'struct Blah''s were replaced
 * with 'BlahPtr' in the #define's.  The reason?  Sprite cannot handle
 * 'struct Blah *blah'.		- ron 'blah blah' choi
 */
typedef	struct JoinRuleInfo *JoinRuleInfoPtr;

class (Join) public (Plan) {
#define	JoinDefs \
	inherits(Plan);	\
	JoinRuleInfoPtr	ruleinfo
 /* private: */
	JoinDefs;
 /* public: */
};

/* ----------------
 *	nest loop join node
 * ----------------
 */
class (NestLoop) public (Join) {
	inherits(Join);
 /* private: */
	NestLoopState		nlstate;
	JoinRuleInfo		nlRuleInfo;
 /* public: */
};

/* ----------------
 *	merge join node
 * ----------------
 */
class (MergeJoin) public (Join) {
	inherits(Join);
 /* private: */
	List			mergeclauses;
	ObjectId		mergesortop;
	List			mergerightorder;
	List			mergeleftorder;
	MergeJoinState		mergestate;
 /* public: */
};

/* ----------------
 *	hash join (probe) node
 * ----------------
 */
class (HashJoin) public (Join) {
	inherits(Join);
 /* private: */
	List			hashclauses;
	ObjectId		hashjoinop;
	HashJoinState		hashjoinstate;
 /* public: */
};


/*
 * ==========
 * Temp nodes
 * ==========
 */

class (Temp) public (Plan) {
#define	TempDefs \
	inherits(Plan); \
	ObjectId		tempid; \
	Count			keycount
 /* private: */
	TempDefs;
 /* public: */
};

/* ----------------
 *	materialization node
 * ----------------
 */
class (Material) public (Temp) {
#define MaterialDefs \
	inherits(Temp); \
	MaterialState		matstate
 /* private: */
	MaterialDefs;
 /* public: */
};

/* ----------------
 *	sort node
 * ----------------
 */
class (Sort) public (Temp) {
#define	SortDefs \
	inherits(Temp); \
	SortState		sortstate
 /* private: */
	SortDefs;
 /* public: */
};

/* ----------------
 *	unique node
 * ----------------
 */
class (Unique) public (Temp) {
#define UniqueDefs \
	inherits(Temp); \
	UniqueState		uniquestate
 /* private: */
	UniqueDefs;
 /* public: */
};

/* ----------------
 *	hash build node
 * ----------------
 */
class (Hash) public (Plan) {
#define HashDefs \
	inherits(Plan); \
	Var			hashkey; \
	HashState		hashstate
 /* private: */
	HashDefs;
 /* public: */
};

#endif /* PlanNodesIncluded */
