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
#include "params.h"	/* parameterized plan stuff... */

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
 *    IndexInfo information
 *
 *	NumKeyAttributes	number of key attributes
 *	KeyAttributeNumbers	array of attribute numbers used as keys
 * ----------------------------------------------------------------
 */

class (IndexInfo) public (Node) {
    inherits(Node);
    int			ii_NumKeyAttributes;
    AttributeNumberPtr	ii_KeyAttributeNumbers;
};

typedef IndexInfo	*IndexInfoPtr;

/* ----------------------------------------------------------------
 *    RelationInfo information
 *
 *	RangeTableIndex		result relation's range table index
 *	RelationDesc		relation descriptor for result relation
 *	NumIndices		number indices existing on result relation
 *	IndexRelationDescs	array of relation descriptors for indices
 *	IndexInfoPtr		array
 * ----------------------------------------------------------------
 */

class (RelationInfo) public (Node) {
    inherits(Node);
    Index		ri_RangeTableIndex;
    Relation		ri_RelationDesc;
    int			ri_NumIndices;
    RelationPtr		ri_IndexRelationDescs;
    IndexInfoPtr	ri_IndexRelationInfo;
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
 *	into_relation_descriptor	relation being retrieved "into"
 *	result_relation_information	for update queries
 *	param_list_info			information needed to transform
 *					Param nodes into Const nodes
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
      Relation		es_into_relation_descriptor;
      RelationInfo	es_result_relation_info;
      ParamListInfo	es_param_list_info;
};


/* ----------------
 *	Executor Type information needed by plannodes.h
 * ----------------
 */

/* ----------------------------------------------------------------
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the outer subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
 * ----------------------------------------------------------------
 */

class (StateNode) public (Node) {
#define StateNodeDefs \
      inherits(Node); \
      List 	   	  sn_OuterTuple; \
      AttributePtr 	  sn_TupType; \
      Pointer 	   	  sn_TupValue; \
      int	   	  sn_Level; \
      AttributePtr 	  sn_ScanType; \
      AttributeNumberPtr  sn_ScanAttributes; \
      int		  sn_NumScanAttributes
  /* private: */
      StateNodeDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *    ResultState information
 *
 *   	Loop	 	   flag which tells us to quit when we
 *			   have already returned a constant tuple.
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the outer subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
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
 *   	PortalFlag	   set to enable portals to work (??? -cim)
 *	currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the outer subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
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
 *   	ProcOuterFlag	   need to process outer subtree
 *   	OldRelId	   need for compare for joins if result relid.
 *
 *   CommonState information
 *
 *   	PortalFlag	   Set to enable portals to work.
 *	currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the left subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
 * ----------------------------------------------------------------
 */

class (ScanState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      bool 			ss_ProcLeftFlag;
      Index 			ss_OldRelId;
  /* public: */
};

/* ----------------------------------------------------------------
 *    IndexScanState information
 *
 *   	IndexPtr	   current index in use
 *	NumIndices	   number of indices in this scan
 *   	ScanKeys	   Skey structures to scan index rels
 *   	NumScanKeys	   array of no of keys in each Skey struct
 *	RelationDescs	   ptr to array of relation descriptors
 *	ScanDescs	   ptr to array of scan descriptors
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the left subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
 * ----------------------------------------------------------------
 */
class (IndexScanState) public (StateNode) {
    inherits(StateNode);
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
 *   	PortalFlag	   Set to enable portals to work.
 *	currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the left subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
 * ----------------------------------------------------------------
 */

class (NestLoopState) public (CommonState) {
      inherits(CommonState);
  /* private: */
  /* public: */
};

/* ----------------------------------------------------------------
 *   SortState information
 *
 *   	Flag	 	indicated whether relation has been sorted
 *   	Keys	      	scan key structures used to keep info on sort keys
 *	TempRelation	temporary relation containing result of executing
 *			the subplan.
 *
 *   CommonState information
 *
 *   	PortalFlag	   Set to enable portals to work. (unused)
 *	currentRelation    relation descriptor of sorted relation
 *      currentScanDesc    current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple' (unused)
 *   	Level      	   level of the left subplan (unused)
 *	ScanType	   type of tuples in relation being scanned (unused)
 *	ScanAttributes	   attribute numbers of interest in this tuple (unused)
 *	NumScanAttributes  number of attributes of interest.. (unused)
 * ----------------------------------------------------------------
 */

class (SortState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      bool	sort_Flag;
      Pointer 	sort_Keys;
      Relation  sort_TempRelation;
  /* public: */
};

/* ----------------------------------------------------------------
 *    MergeJoinState information
 *
 *   	OSortopI      	   outerKey1 sortOp innerKey1 ...
 *  	ISortopO      	   innerkey1 sortOp outerkey1 ...
 *   	MarkFlag      	   'true' is inner and outer relations marked.
 *   	FrwdMarkPos   	   scan mark for forward scan
 *   	BkwdMarkPos	   scan mark for backward scan
 *
 *   CommonState information
 *
 *   	PortalFlag	   Set to enable portals to work.
 *	currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the left subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
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
 *    ExistentialState information
 *
 *	SatState	   See if we have satisfied the left tree.
 *
 *   StateNode information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the left subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
 * ----------------------------------------------------------------
 */
class (ExistentialState) public (StateNode) {
   inherits(StateNode);
   List 	ex_SatState;
};


#endif /* ExecNodesIncluded */
