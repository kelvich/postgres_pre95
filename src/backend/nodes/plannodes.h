/*
 * PlanNodes.h --
 *	Definitions for query tree nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef PlanNodesIncluded
#define	PlanNodesIncluded

#include "nodes.h"	/* bogus inheritance system */
#include "pg_lisp.h"
#include "oid.h"

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
#define	PrintMergeSortExists
#define	PrintHashJoinExists
#define	PrintScanExists
#define	PrintSeqScanExists
#define	PrintIndexScanExists
#define	PrintTempExists
#define	PrintSortExists
#define	PrintHashExists

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
 /* public: */
};

class (Result) public (Plan) {
	inherits(Plan);
 /* private: */
	List			resrellevelqual;
	List			resconstantqual;
 /* public: */
};

class (Append) public (Plan) {
	inherits(Plan);
 /* private: */
	List			unionplans;
	Index			unionrelid;
	List			unionrtentries;
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
 /* public: */
};

class (MergeSort) public (Join) {
	inherits(Join);
 /* private: */
	List			mergeclauses;
	ObjectId		mergesortop;
 /* public: */
};

class (HashJoin) public (Join) {
	inherits(Join);
 /* private: */
	List			hashclauses;
	ObjectId		hashjoinop;
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
	Index			scanrelid
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
 /* public: */
};

class (Hash) public (Temp) {
	inherits(Temp);
 /* private: */
 /* public: */
};

#endif /* PlanNodesIncluded */
