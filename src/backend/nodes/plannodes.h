/*
 * PlanNodes.h --
 *	Definitions for query tree nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef PlanNodesIncluded
#define	PlanNodesIncluded

#include "nodes.h"		/* bogus inheritance system */
#include "pg_lisp.h"
#include "oid.h"
#include "recursion_a.h"        /* recursion stuff that must go first */

/* ----------------
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
 * ----------------
 */
#include "execnodes.h"		/* XXX move executor types elsewhere */


/*
 *  All of these #defines indicate that we have written print support
 *  for the classes named.  The print routines are in lib/C/printfuncs.c;
 *  an interface routine is generated automatically by Gen_creator.sh for
 *  each node type.
 *
 *  This is pretty complicated, and a better-designed system needs to be
 *  implemented.
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

/* ----------------
 *	Plan node
 * ----------------
 */
class (Plan) public (Node) {
#define PlanDefs \
	inherits(Node); \
	Cost			cost; \
	Index			fragment; \
	struct EState		*state; \
	List			qptargetlist; \
	List			qpqual; \
	struct Plan		*lefttree; \
	struct Plan		*righttree
 /* private: */
	PlanDefs;
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

class (Join) public (Plan) {
#define	JoinDefs \
	inherits(Plan)
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
class (Hash) public (Temp) {
#define HashDefs \
	inherits(Temp); \
	HashState		hashstate
 /* private: */
	HashDefs;
 /* public: */
};

#endif /* PlanNodesIncluded */
