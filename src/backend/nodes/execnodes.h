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
 *	direction			direction of the scan
 *	time				?
 *	owner				?
 *	locks				?
 *	subplan_info			?
 *	error_message			?
 *	range_table			array of scan relation information
 *	qualification_tuple		tuple satisifying qualification
 *	qualification_tuple_id		oid of qualification_tuple
 *	relation_relation_descriptor	as it says
 *	result_relation_oid		for update queries
 *	result_relation_descriptor	for update queries
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
	ObjectId	result_relation_oid;
	Relation	result_relation_descriptor;
};

/* ----------------
 *	Executor Type information needed by plannodes.h
 * ----------------
 */

/* ----------------------------------------------------------------
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */

class (StateNode) public (Node) {
#define StateNodeDefs \
      inherits(Node); \
      List 	   LeftTuple; \
      AttributePtr TupType; \
      Pointer 	   TupValue; \
      int	   Level; \
      AttributePtr ScanType      
  /* private: */
      StateNodeDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *    ResultState information
 *
 *   	Loop	 	see if we have been her before.
 *
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */

class (ResultState) public (StateNode) {
   inherits(StateNode);
   int	 	Loop;
};

/* ----------------------------------------------------------------
 *   CommonState information
 *
 *   	OuterTuple	points to the current outer tuple
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */

class (CommonState) public (StateNode) {
#define CommonStateDefs \
      inherits(StateNode); \
      List 	   OuterTuple; \
      bool 	   PortalFlag; \
      Relation     currentRelation; \
      HeapScanDesc currentScanDesc
  /* private: */
      CommonStateDefs;
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
 *   CommonState information
 *
 *   	OuterTuple	points to the current outer tuple
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 *
 * ----------------------------------------------------------------
 */

class (ScanState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      bool 	RuleFlag;
      List 	RuleDesc;
      bool 	ProcLeftFlag;
      Index 	OldRelId;
      int	IndexPtr;
      ScanKey	Skeys;
      int	SkeysCount;
  /* public: */
};

/* ----------------------------------------------------------------
 *    NestLoopState information
 *
 *   	OuterTuple	points to the current outer tuple
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 *
 * ----------------------------------------------------------------
 */

class (NestLoopState) public (CommonState) {
      inherits(CommonState);
  /* private: */
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
 *   CommonState information
 *
 *   	OuterTuple	points to the current outer tuple
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 *
 * ----------------------------------------------------------------
 */

class (MergeJoinState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      List 	OSortopI;
      List 	ISortopO;
      bool	arkFlag;
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
      bool	Flag;
      List 	Keys;
  /* public: */
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
 *    ExistentialState information
 *
 *	SatState	See if we have satisfied the left tree.
 *
 *   StateNode information
 *
 *	LeftTuple	points to the current left tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */
class (ExistentialState) public (StateNode) {
   inherits(StateNode);
   List 	SatState;
};


#endif /* ExecNodesIncluded */
