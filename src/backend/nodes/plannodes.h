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
 *	Existential		ExistentialState	exstate;
 *	Result			ResultState		resstate;
 *	Append			AppendState		unionstate;
 *	NestLoop		NestLoopState		nlstate;
 *    	Scan ***		ScanState		scanstate;
 *      Sort			SortState		sortstate;
 *	MergeJoin		MergeJoinState		mergestate;
 *	HashJoin		HashJoinState		hashstate;
 *      Recursive               RecursiveState          recurstate;
 *
 *	  (*** nodes which inherit Scan also inherit scanstate)
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

extern void	PrintPlan();
extern void	PrintResult();
extern void	PrintExistential();
extern void	PrintAppend();
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

class (Existential) public (Plan) {
	inherits(Plan);
 /* private: */
	ExistentialState	exstate;
 /* public: */
};

class (Result) public (Plan) {
	inherits(Plan);
 /* private: */
	List			resrellevelqual;
	List			resconstantqual;
	ResultState		resstate;
 /* public: */
};

class (Append) public (Plan) {
	inherits(Plan);
 /* private: */
	List			unionplans;
	Index			unionrelid;
	List			unionrtentries;
	AppendState		unionstate;
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
 */

class (Recursive) public (Plan) {
        inherits(Plan);
 /* private */
        RecursiveMethod         recurMethod;
        Command                 recurCommand;
        List                    recurInitPlans;
        List                    recurLoopPlans;
        List                    recurCleanupPlans;
        List                    recurCheckpoints;
        RecursiveState          recurState;
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

class (NestLoop) public (Join) {
	inherits(Join);
 /* private: */
	NestLoopState		nlstate;
 /* public: */
};

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

class (HashJoin) public (Join) {
	inherits(Join);
 /* private: */
	List			hashclauses;
	ObjectId		hashjoinop;
	HashJoinState		hashstate;
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

class (SeqScan) public (Scan) {
	inherits(Scan);
 /* private: */
 /* public: */
};

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

class (Sort) public (Temp) {
	inherits(Temp);
 /* private: */
	SortState		sortstate;
 /* public: */
};

class (Hash) public (Temp) {
	inherits(Temp);
 /* private: */
 /* public: */
};

#endif /* PlanNodesIncluded */
