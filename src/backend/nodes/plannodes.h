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
#include "nodes/primnodes.h"
#include "nodes/plannodes.gen"
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
 *	Node Out Function declarations
 * ----------------
 */
#define	OutPlanExists
#define	OutResultExists
#define	OutExistentialExists
#define	OutAppendExists
#define	OutJoinExists
#define OutJoinRuleInfoExists
#define	OutNestLoopExists
#define	OutMergeJoinExists
#define	OutHashJoinExists
#define	OutScanExists
#define	OutSeqScanExists
#define	OutIndexScanExists
#define	OutTempExists
#define OutAggExists
#define	OutSortExists
#define	OutHashExists
#define OutUniqueExists
#define OutFragmentExists
#define OutScanTempsExists


/* ----------------
 *	Node Equal Function declarations
 * ----------------
 */
#define EqualFragmentExists


/* ----------------
 *	Node Copy Function declarations
 * ----------------
 */
#define	CopyPlanExists
#define	CopyResultExists
#define	CopyExistentialExists
#define	CopyAppendExists
#define	CopyJoinExists
#define CopyJoinRuleInfoExists
#define	CopyNestLoopExists
#define	CopyMergeJoinExists
#define	CopyHashJoinExists
#define	CopyScanExists
#define	CopySeqScanExists
#define	CopyIndexScanExists
#define	CopyTempExists
#define CopyAggExists
#define	CopySortExists
#define	CopyHashExists
#define CopyUniqueExists
#define CopyScanTempsExists


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
typedef	struct Plan *PlanPtr;

class (Plan) public (Node) {
#define PlanDefs \
	inherits0(Node); \
	Cost			cost; \
	Count			plan_size; \
	Count			plan_width; \
	Count			plan_tupperpage; \
	int			fragment; \
	int			parallel; \
	EStatePtr		state; \
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
        inherits0(Node); \
        Plan           	frag_root; \
        Plan           	frag_parent_op; \
        int            	frag_parallel; \
        List           	frag_subtrees; \
        FragmentPtr    	frag_parent_frag; \
	List		frag_parsetree; \
	bool		frag_is_inprocess; \
	float		frag_iorate
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
	inherits1(Plan);
 /* public: */
};

/* ----------------
 *	result node
 * ----------------
 */
class (Result) public (Plan) {
	inherits1(Plan);
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
	inherits1(Plan);
 /* private: */
	List			unionplans;
	Index			unionrelid;
	List			unionrtentries;
	AppendState		unionstate;
 /* public: */
};

/*
 * ==========
 * Scan nodes
 * ==========
 */

class (Scan) public (Plan) {
#define	ScanDefs \
	inherits1(Plan); \
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
	inherits2(Scan);
 /* private: */
 /* public: */
};

class (ScanTemps) public (Plan) {
#define ScanTempDefs \
        inherits1(Plan); \
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
	inherits2(Scan);
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
	inherits0(Node);
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
	inherits1(Plan);	\
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
	inherits2(Join);
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
	inherits2(Join);
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
	inherits2(Join);
 /* private: */
	List			hashclauses;
	ObjectId		hashjoinop;
	HashJoinState		hashjoinstate;
	HashJoinTable		hashjointable;
	IpcMemoryKey		hashjointablekey;
	int			hashjointablesize;
	bool			hashdone;
 /* public: */
};


/*
 * ==========
 * Temp nodes
 * ==========
 */

class (Temp) public (Plan) {
#define	TempDefs \
	inherits1(Plan); \
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
	inherits2(Temp); \
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
	inherits2(Temp); \
	SortState		sortstate
 /* private: */
	SortDefs;
 /* public: */
};
/* ---------------
 *      aggregate node
 * ---------------
 */
class (Agg) public (Temp) {
#define AggDefs \
	inherits2(Temp); \
	String                  aggname; \
	AggState                aggstate
    /* private */
	AggDefs;
    /* public: */
};

/* ----------------
 *	unique node
 * ----------------
 */
class (Unique) public (Temp) {
#define UniqueDefs \
	inherits2(Temp); \
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
	inherits1(Plan); \
	Var			hashkey; \
	HashState		hashstate; \
	HashJoinTable		hashtable; \
	IpcMemoryKey		hashtablekey; \
	int			hashtablesize
 /* private: */
	HashDefs;
 /* public: */
};

/* ---------------------
 *	choose node
 * ---------------------
 */
class (Choose) public (Plan) {
#define ChooseDefs \
	inherits1(Plan); \
	List			chooseplanlist
/* private: */
	ChooseDefs;
/* public: */
};
#endif /* PlanNodesIncluded */
