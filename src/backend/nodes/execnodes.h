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
 *	this class holds the information saying what attributes
 *	are the key attributes for this index. -cim 10/15/89
 *
 *	NumKeyAttributes	number of key attributes for this index
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
 *	whenever we update an existing relation, we have to
 *	update indices on the relation.  The RelationInfo class
 *	is used to hold all the information on result relations,
 *	including indices.. -cim 10/15/89
 *
 *	RangeTableIndex		result relation's range table index
 *	RelationDesc		relation descriptor for result relation
 *	NumIndices		number indices existing on result relation
 *	IndexRelationDescs	array of relation descriptors for indices
 *	IndexInfoPtr		array of key/attr info for indices
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
 *
 *|	Note: the bogus classes CommonState and CommonScanState exist only
 *|	      because our inheritance system only allows single inheritance
 *|	      and we have to have unique slot names.  Hence two or more
 *|	      classes which want to have a common slot must ALL inherit
 *|	      the slot from some other class.  (This is a big hacks to
 *|	      allow our classes to share slot names..)
 *|
 *|	Example:
 *|	      the class Result and the class NestLoop nodes both want
 *|	      a slot called "OuterTuple" so they both have to inherit
 *|	      it from some other class.  In this case they inherit
 *|	      it from CommonState.  "CommonState" and "CommonScanState" are
 *|	      the best names I could come up with for this sort of
 *|	      stuff.
 *|
 *|	      As a result, many classes have extra slots which they
 *|	      don't use.  These slots are denoted (unused) in the
 *|	      comment preceeding the class definition.  If you
 *|	      comes up with a better idea of a way of doing things
 *|	      along these lines, then feel free to make your idea
 *|	      known to me.. -cim 10/15/89
 * ----------------
 */

/* ----------------------------------------------------------------
 *   CommonState information
 *
 *|	this is a bogus class used to hold slots so other
 *|	nodes can inherit them...
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the outer subplan
 *	ScanType	   type of tuples in relation being scanned
 *	ScanAttributes	   attribute numbers of interest in this tuple
 *	NumScanAttributes  number of attributes of interest..
 *
 *|	Currently the value of Level is ignored by almost everything
 *|	and since I don't understand it's purpose, it may go
 *|	away soon.
 *|
 *|	ScanType should probably mean the type of tuples
 *|	in the subplan being scanned for nodes without underlying
 *|	relations.
 *|
 *|	ScanAttributes and NumScanAttributes are new -- they are
 *|	used to keep track of the attribute numbers of attributes
 *|	which are actually inspected by the query so the rule manager
 *|	doesn't have to needlessluy check for rules on attributes
 *|	that won't ever be inspected..
 *|
 *|	-cim 10/15/89
 * ----------------------------------------------------------------
 */

class (CommonState) public (Node) {
#define CommonStateDefs \
      inherits(Node); \
      List 	   	  cs_OuterTuple; \
      AttributePtr 	  cs_TupType; \
      Pointer 	   	  cs_TupValue; \
      int	   	  cs_Level; \
      AttributePtr 	  cs_ScanType; \
      AttributeNumberPtr  cs_ScanAttributes; \
      int		  cs_NumScanAttributes
  /* private: */
      CommonStateDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *   ResultState information
 *
 *   	Loop	 	   flag which tells us to quit when we
 *			   have already returned a constant tuple.
 *
 *   CommonState information
 *
 *	OuterTuple	   points to the current outer tuple
 *  	TupType   	   attr type info of tuples from this node
 *   	TupValue   	   array to store attr values for 'formtuple'
 *   	Level      	   level of the outer subplan
 *	ScanType	   type of tuples in relation being scanned (unused)
 *	ScanAttributes	   attribute numbers of interest in tuple (unused)
 *	NumScanAttributes  number of attributes of interest.. (unused)
 * ----------------------------------------------------------------
 */

class (ResultState) public (CommonState) {
   inherits(CommonState);
   int	 	rs_Loop;
};

/* ----------------------------------------------------------------
 *   AppendState information
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
 *   CommonScanState information
 *
 *	CommonScanState is a class like CommonState, but is used more
 *	by the nodes like SeqScan and Sort which want to
 *	keep track of an underlying relation.
 *
 *	currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *
 *   CommonState information
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

class (CommonScanState) public (CommonState) {
#define CommonScanStateDefs \
      inherits(CommonState); \
      Relation     css_currentRelation; \
      HeapScanDesc css_currentScanDesc
  /* private: */
      CommonScanStateDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *   ScanState information
 *
 *   	ProcOuterFlag	   need to process outer subtree
 *   	OldRelId	   need for compare for joins if result relid.
 *
 *   CommonScanState information
 *
 *	currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *
 *   CommonState information
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

class (ScanState) public (CommonScanState) {
      inherits(CommonScanState);
  /* private: */
      bool 			ss_ProcOuterFlag;
      Index 			ss_OldRelId;
  /* public: */
};

/* ----------------------------------------------------------------
 *   IndexScanState information
 *
 *|	index scans don't use CommonScanState because
 *|	the underlying AM abstractions for heap scans and
 *|	index scans are too different..  It would be nice
 *|	if the current abstraction was more useful but ... -cim 10/15/89
 *
 *   	IndexPtr	   current index in use
 *	NumIndices	   number of indices in this scan
 *   	ScanKeys	   Skey structures to scan index rels
 *   	NumScanKeys	   array of no of keys in each Skey struct
 *	RelationDescs	   ptr to array of relation descriptors
 *	ScanDescs	   ptr to array of scan descriptors
 *
 *   CommonState information
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
class (IndexScanState) public (CommonState) {
    inherits(CommonState);
    int		     	iss_NumIndices;
    int			iss_IndexPtr;
    ScanKeyPtr		iss_ScanKeys;
    IntPtr		iss_NumScanKeys;
    RelationPtr	     	iss_RelationDescs;
    IndexScanDescPtr 	iss_ScanDescs;
};

/* ----------------------------------------------------------------
 *   NestLoopState information
 *
 *   	PortalFlag	   Set to enable portals to work.
 *
 *   CommonState information
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
      bool 	   nl_PortalFlag;
  /* public: */
};

/* ----------------------------------------------------------------
 *   SortState information
 *
 *|	sort nodes are really just a kind of a scan since
 *|	we implement sorts by retrieveing the entire subplan
 *|	into a temp relation, sorting the temp relation into
 *|	another sorted relation, and then preforming a simple
 *|	unqualified sequential scan on the sorted relation..
 *|	-cim 10/15/89
 *
 *   	Flag	 	indicated whether relation has been sorted
 *   	Keys	      	scan key structures used to keep info on sort keys
 *	TempRelation	temporary relation containing result of executing
 *			the subplan.
 *
 *   CommonScanState information
 *
 *	currentRelation    relation descriptor of sorted relation
 *      currentScanDesc    current scan descriptor for scan
 *
 *   CommonState information
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

class (SortState) public (CommonScanState) {
      inherits(CommonScanState);
  /* private: */
      bool	sort_Flag;
      Pointer 	sort_Keys;
      Relation  sort_TempRelation;
  /* public: */
};

/* ----------------------------------------------------------------
 *   MergeJoinState information
 *
 *   	OSortopI      	   outerKey1 sortOp innerKey1 ...
 *  	ISortopO      	   innerkey1 sortOp outerkey1 ...
 *   	MarkFlag      	   'true' is inner and outer relations marked.
 *   	FrwdMarkPos   	   scan mark for forward scan
 *   	BkwdMarkPos	   scan mark for backward scan
 *
 *   CommonState information
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
 *   HashJoinState information
 *
 *   CommonState information
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

class (HashJoinState) public (CommonState) {
      inherits(CommonState);
  /* private: */
  /* public: */
};

/* ----------------------------------------------------------------
 *   ExistentialState information
 *
 *	SatState	   See if we have satisfied the left tree.
 *
 *   CommonState information
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
class (ExistentialState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      List 	ex_SatState;
  /* public: */
};


#endif /* ExecNodesIncluded */
