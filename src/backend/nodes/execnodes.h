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
#include "relscan.h"

#define abstime AbsoluteTime

typedef int	*IntPtr;

/* ----------------------------------------------------------------
 *    RelationInfo information
 *
 *	RangeTableIndex		result relation's range table index
 *	RelationDesc		relation descriptor for result relation
 *	NumIndices		number indices existing on result relation
 *	IndexRelationDescs	array of relation descriptors for indices
 * ----------------------------------------------------------------
 */

class (RelationInfo) public (Node) {
    inherits(Node);
    Index		ri_RangeTableIndex;
    Relation		ri_RelationDesc;
    int			ri_NumIndices;
    RelationPtr		ri_IndexRelationDescs;
};

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
 *	qualification_tuple_id		tid of qualification_tuple
 *	relation_relation_descriptor	as it says
 *	result_relation_information	for update queries
 * ----------------------------------------------------------------
 */

class (EState) public (Node) {
      inherits(Node);
      ScanDirection	es_direction;
      abstime		es_time;
      ObjectId		es_owner;
      List		es_locks;
      List		es_subplan_info;
      Name		es_error_message;
      List		es_range_table;
      HeapTuple		es_qualification_tuple;
      ItemPointer	es_qualification_tuple_id;
      Relation		es_relation_relation_descriptor;
      RelationInfo	es_result_relation_info;
};

/* ----------------
 *	Executor Type information needed by plannodes.h
 * ----------------
 */

/* ----------------------------------------------------------------
 *   StateNode information
 *
 *	OuterTuple	points to the current outer tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the outer subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */

class (StateNode) public (Node) {
#define StateNodeDefs \
      inherits(Node); \
      List 	   sn_OuterTuple; \
      AttributePtr sn_TupType; \
      Pointer 	   sn_TupValue; \
      int	   sn_Level; \
      AttributePtr sn_ScanType      
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
 *	OuterTuple	points to the current outer tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the outer subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */

class (ResultState) public (StateNode) {
   inherits(StateNode);
   int	 	rs_Loop;
};

/* ----------------------------------------------------------------
 *    AppendState information
 *
 *	append nodes have this field "unionplans" which is this
 *	list of plans to execute in sequence..  these variables
 *	keep track of things..
 *
 *   	whichplan	which plan is being executed
 *   	nplans		how many plans are in the list
 *   	initialized	array of ExecInitNode() results
 *   	rtentries	range table for the current plan
 * ----------------------------------------------------------------
 */

class (AppendState) public (Node) {
    inherits(Node);
    int 	as_whichplan;
    int 	as_nplans;
    ListPtr 	as_initialized;
    List 	as_rtentries;
};

/* ----------------------------------------------------------------
 *   CommonState information
 *
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	points to the current outer tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the outer subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */

class (CommonState) public (StateNode) {
#define CommonStateDefs \
      inherits(StateNode); \
      bool 	   cs_PortalFlag; \
      Relation     cs_currentRelation; \
      HeapScanDesc cs_currentScanDesc
  /* private: */
      CommonStateDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *    ScanState information
 *
 *   	RuleFlag	whether a rule is activated
 *   	RuleDesc	rule desc returned by Tuple Rule Mgr
 *   	ProcOuterFlag	need to process outer subtree
 *   	OldRelId	need for compare for joins if result relid.
 *
 *   CommonState information
 *
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	points to the current outer tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the outer subplan
 *	ScanType	type of tuples in relation being scanned
 *
 * ----------------------------------------------------------------
 */

class (ScanState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      bool 		ss_RuleFlag;
      List 		ss_RuleDesc;
      bool 		ss_ProcLeftFlag;
      Index 		ss_OldRelId;
  /* public: */
};

/* ----------------------------------------------------------------
 *    IndexScanState information
 *
 *   	IndexPtr		current index in use
 *	NumIndices		number of indices in this scan
 *   	ScanKeys		Skey structures to scan index rels
 *   	NumScanKeys		array of no of keys in each Skey struct
 *	RelationDescs		ptr to array of relation descriptors
 *	ScanDescs		ptr to array of scan descriptors
 * ----------------------------------------------------------------
 */
class (IndexScanState) public (Node) {
    inherits(Node);
    int		     	iss_NumIndices;
    int			iss_IndexPtr;
    ScanKeyPtr		iss_ScanKeys;
    IntPtr		iss_NumScanKeys;
    RelationPtr	     	iss_RelationDescs;
    IndexScanDescPtr 	iss_ScanDescs;
};

/* ----------------------------------------------------------------
 *    NestLoopState information
 *
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	points to the current outer tuple
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
 *   	PortalFlag	Set to enable portals to work.
 *	currentRelation relation being scanned
 *      currentScanDesc current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	points to the current outer tuple
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
      List 	mj_OSortopI;
      List 	mj_ISortopO;
      bool	mj_MarkFlag;
      List 	mj_FrwdMarkPos;
      List 	mj_BkwdMarkPos;
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
      bool	sort_Flag;
      List 	sort_Keys;
  /* public: */
};

/* ----------------------------------------------------------------
 *    ExistentialState information
 *
 *	SatState	See if we have satisfied the left tree.
 *
 *   StateNode information
 *
 *	OuterTuple	points to the current outer tuple
 *  	TupType   	attr type info of tuples from this node
 *   	TupValue   	array to store attr values for 'formtuple'
 *   	Level      	level of the left subplan
 *	ScanType	type of tuples in relation being scanned
 * ----------------------------------------------------------------
 */
class (ExistentialState) public (StateNode) {
   inherits(StateNode);
   List 	ex_SatState;
};


#endif /* ExecNodesIncluded */
