/*
 * ExecNodes.h --
 *	Definitions for executor nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef ExecNodesIncluded
#define	ExecNodesIncluded

#include "pg_lisp.h"
#include "nodes.h"	/* bogus inheritance system */

/*
 *  This #define means that we have supplied a print function for EState
 *  nodes.  The routine that does the printing is in lib/C/printfuncs.c;
 *  it's called by an automatically-generated routine in execnodes.c.  This
 *  print support is intended for use in getting the C->lisp port done
 *  quickly, and should be replaced by something better designed when time
 *  permits.
 */

#define	PrintEStateExists

extern void	PrintEState();
extern bool	EqualEState();

#include "item.h"
#include "sdir.h"
#include "htup.h"
#include "tim.h"
#include "rel.h"

#define abstime AbsoluteTime

/* ----------------------------------------------------------------
 *    EState information
 *
 *	direction
 *	time
 *	owner
 *	locks
 *	subplan_info
 *	error_message
 *	range_table
 *	qualification_tuple
 *	qualification_tuple_id
 *	relation_relation_descriptor
 *	result_relation
 *	result_relation_descriptor
 *
 * ----------------------------------------------------------------
 */

class (EState) public (Node) {
	inherits(Node);
	ScanDirection	direction;
	abstime		time;
	ObjectId	owner;
	List		locks;
	List		subplan_info;
	Name		error_message;
	List		range_table;
	HeapTuple	qualification_tuple;
	ItemPointer	qualification_tuple_id;
	Relation	relation_relation_descriptor;
	ObjectId	result_relation;
	Relation	result_relation_descriptor;
};

/* ----------------
 *	Executor Type information needed by plannodes.h
 * ----------------
 */

/* ----------------------------------------------------------------
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 * ----------------------------------------------------------------
 */

class (StateNode) public (Node) {
#define StateNodeDefs \
      inherits(Node); \
      List 	   LeftTuple; \
      AttributePtr TupType; \
      Pointer 	   TupValue; \
      int	   Level
  /* private: */
      StateNodeDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *    ExistentialState information
 *
 *	SatState	See if we have satisfied the left tree.
 *
 * ----------------------------------------------------------------
 */
class (ExistentialState) public (StateNode) {
   inherits(StateNode);
   List 	SatState;
};

/* ----------------------------------------------------------------
 *    ResultState information
 *
 *   	Loop	 	see if we have been her before.
 *
 * ----------------------------------------------------------------
 */

class (ResultState) public (StateNode) {
   inherits(StateNode);
   int	 	Loop;
};

/* ----------------------------------------------------------------
 *    AppendState information
 *
 *   	whichplan
 *   	nplans
 *   	initialzed
 *   	rtentries
 *
 * ----------------------------------------------------------------
 */

class (AppendState) public (Node) {
   inherits(Node);
   int 		whichplan;
   int 		nplans;
   List 	initialzed;
   List 	rtentries;
};

/* ----------------------------------------------------------------
 *
 *   	BufferPage	First buffer page of tuple for this node.
 *   	OuterTuple	points to the current outer tuple
 *   	PortalFlag	Set to enable portals to work.
 *
 * ----------------------------------------------------------------
 */

class (CommonState) public (StateNode) {
#define CommonStateDefs \
      inherits(StateNode); \
      List 	BufferPage; \
      List 	OuterTuple; \
      List 	PortalFlag
  /* private: */
      CommonStateDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *    NestLoopState information
 *
 * ----------------------------------------------------------------
 */

class (NestLoopState) public (CommonState) {
      inherits(CommonState);
  /* private: */
  /* public: */
};

/* ----------------------------------------------------------------
 *    ScanState information
 *
 *   	RuleFlag	whether a rule is activated
 *   	RuleDesc	rule desc returned by Tuple Rule Mgr
 *   	ProcLeftFlag	need to process right subtree
 *   	OldRelId	need for compare for joins if result relid.
 *   	LispIndexPtr	point to index used in list of indices, if any
 *   	Skeys		Skey structures to scan index rels, if any
 *   	SkeysCount	no of keys in each of the Skey structures
 *
 * ----------------------------------------------------------------
 */

class (ScanState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      List 	RuleFlag;
      List 	RuleDesc;
      List 	ProcLeftFlag;
      List 	OldRelId;
      List 	LispIndexPtr;
      List 	Skeys;
      List 	SkeysCount;
  /* public: */
};

/* ----------------------------------------------------------------
 *    MergeJoinState information
 *
 *   	OSortopI      	outerKey1 sortOp innerKey1 ...
 *  	ISortopO      	innerkey1 sortOp outerkey1 ...
 *   	MarkFlag      	'true' is inner and outer relations marked.
 *   	FrwdMarkPos   	scan mark for forward scan
 *   	BkwdMarkPos	scan mark for backward scan
 *
 * ----------------------------------------------------------------
 */

class (MergeJoinState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      List 	OSortopI;
      List 	ISortopO;
      List 	MarkFlag;
      List 	FrwdMarkPos;
      List 	BkwdMarkPos;
  /* public: */
};

/* ----------------------------------------------------------------
 *    SortState
 *
 *   	Flag	 	indicated whether relation has been sorted
 *   	Keys	      	structures keeping info on sort keys
 *
 * ----------------------------------------------------------------
 */

class (SortState) public (Node) {
      inherits(Node);
  /* private: */
      List 	Flag;
      List 	Keys;
  /* public: */
};


#endif /* ExecNodesIncluded */
