/* ----------------------------------------------------------------
 *   FILE
 *      execnodes.h
 *      
 *   DESCRIPTION
 *      definitions for executor state nodes
 *
 *   NOTES
 *      this file is listed in lib/Gen/inherits.sh and in the
 *      INH_SRC list in conf/inh.mk and is used to generate the
 *      obj/lib/C/plannodes.c file
 *
 *   IDENTIFICATION
 *      $Header$
 * ----------------------------------------------------------------
 */

#ifndef ExecNodesIncluded
#define ExecNodesIncluded

#include "tmp/postgres.h"

#include "executor/recursion_a.h" /* recursion stuff that must go first - JRB */
#include "nodes/primnodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/nodes.h"              /* bogus inheritance system */
#include "rules/params.h"             /* parameterized plan stuff... */
#include "rules/prs2.h"               /* for the prs2_info field of EState */

#include "storage/item.h"
#include "access/sdir.h"
#include "access/htup.h"
#include "access/tupdesc.h"
#include "utils/rel.h"
#include "access/relscan.h"
#include "executor/hashjoin.h"
#include "executor/tuptable.h"

/* ----------------------------------------------------------------
 *      Node Function Declarations
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

#define PrintEStateExists

extern void     PrintEState();
extern bool     EqualEState();

/* ----------------------------------------------------------------
 *                      Executor Support Types
 * ----------------------------------------------------------------
 */

#define abstime AbsoluteTime

typedef int     *IntPtr;
typedef void    (*HookFunction)();

/* ----------------
 *    IndexInfo information
 *
 *      this class holds the information saying what attributes
 *      are the key attributes for this index. -cim 10/15/89
 *
 *      NumKeyAttributes        number of key attributes for this index
 *      KeyAttributeNumbers     array of attribute numbers used as keys
 * ----------------
 */

class (IndexInfo) public (Node) {
    inherits(Node);
    int                 ii_NumKeyAttributes;
    AttributeNumberPtr  ii_KeyAttributeNumbers;
};

typedef IndexInfo       *IndexInfoPtr;

/* ----------------
 *    RelationInfo information
 *
 *      whenever we update an existing relation, we have to
 *      update indices on the relation.  The RelationInfo class
 *      is used to hold all the information on result relations,
 *      including indices.. -cim 10/15/89
 *
 *      RangeTableIndex         result relation's range table index
 *      RelationDesc            relation descriptor for result relation
 *      NumIndices              number indices existing on result relation
 *      IndexRelationDescs      array of relation descriptors for indices
 *      IndexInfoPtr            array of key/attr info for indices
 * ----------------
 */

class (RelationInfo) public (Node) {
    inherits(Node);
    Index               ri_RangeTableIndex;
    Relation            ri_RelationDesc;
    int                 ri_NumIndices;
    RelationPtr         ri_IndexRelationDescs;
    IndexInfoPtr        ri_IndexRelationInfo;
};

/* ----------------
 *      TupleCount node information
 *
 *      retrieved       number of tuples seen by ExecRetrieve
 *      appended        number of tuples seen by ExecAppend
 *      deleted         number of tuples seen by ExecDelete
 *      replaced        number of tuples seen by ExecReplace
 *      inserted        number of index tuples inserted
 *      processed       number of tuples processed by the plan
 * ----------------
 */
class (TupleCount) public (Node) {
      inherits(Node);
  /* private: */
      int       tc_retrieved;
      int       tc_appended;
      int       tc_deleted;
      int       tc_replaced;
      int       tc_inserted;
      int       tc_processed;
  /* public: */
};

/* ----------------
 *	Note:  the executor tuple table is managed and manipulated by special
 *	code and macros in executor/tuple.c and lib/H/executor/tuptable.h
 *	-cim 1/18/90
 *
 *	TupleTableSlot information
 *
 *	    shouldFree		boolean - should we call pfree() on tuple
 *	    descIsNew		boolean - true when tupleDescriptor changes
 *	    tupleDescriptor	type information kept regarding the tuple data
 *
 *	LispValue information
 *
 *	    val.car		pointer to tuple data
 *	    cdr			pointer to special info (currenty unused)
 *
 *	The executor stores pointers to tuples in a ``tuple table''
 *	which is composed of TupleTableSlot's.  Some of the tuples
 *	are pointers to buffer pages and others are pointers to
 *	palloc'ed memory and the shouldFree variable tells us when
 *	we may call pfree() on a tuple.  -cim 9/23/90
 *
 *	In the implementation of nested-dot queries such as
 *	"retrieve (EMP.hobbies.all)", a single scan may return tuples
 *	of many types, so now we return pointers to tuple descriptors
 *	along with tuples returned via the tuple table.  -cim 1/18/90
 * ----------------
 */
class (TupleTableSlot) public (LispValue) {
      inherits(LispValue);
  /* private: */
      bool		ttc_shouldFree;
      bool		ttc_descIsNew;
      TupleDescriptor	ttc_tupleDescriptor;
  /* public: */
};

/* ----------------
 *    ExprContext
 *
 *      this class holds the "current context" information
 *      needed to evaluate expressions.  For example, if an
 *      expression refers to an attribute in the current inner tuple
 *      then we need to know what the current inner tuple is and
 *      so we look at the expression context.  Most of this
 *      information was originally stored in global variables
 *      in the Lisp system.
 *
 *      ExprContexts are stored in CommonState nodes and so
 *      all executor state nodes which inherit from CommonState
 *      have ExprContexts although not all of them need it.
 *      
 *  SOON: ExprContext will contain indexes of tuples in the tupleTable
 *        stored in the EState, rather than pointers which it now stores.
 *        It might still be used to store type/buffer information though.
 *        (I haven't thought that out yet) -cim 6/20/90
 * ----------------
 */
class (ExprContext) public (Node) {
#define ExprContextDefs \
        inherits(Node); \
        TupleTableSlot ecxt_scantuple; \
        AttributePtr   ecxt_scantype; \
        Buffer         ecxt_scan_buffer; \
        TupleTableSlot ecxt_innertuple; \
        AttributePtr   ecxt_innertype; \
        Buffer         ecxt_inner_buffer; \
        TupleTableSlot ecxt_outertuple; \
        AttributePtr   ecxt_outertype; \
        Buffer         ecxt_outer_buffer; \
        Relation       ecxt_relation; \
        Index          ecxt_relid; \
        ParamListInfo  ecxt_param_list_info
 /* private: */
        ExprContextDefs;
 /* public: */
};

/* ----------------
 *    JunkFilter
 *
 *    this class is used to store information regarding junk attributes.
 *    A junk attribute is an attribute in a tuple that is needed only for
 *    storing intermediate information in the executor, and does not belong
 *    in the tuple proper.  For example, when we do a delete or replace
 *    query, the planner adds an entry to the targetlist so that the tuples
 *    returned to ExecutePlan() contain an extra attribute: the t_ctid of
 *    the tuple to be deleted/replaced.  This is needed for amdelete() and
 *    amreplace().  In doing a delete this does not make much of a
 *    difference, but in doing a replace we have to make sure we disgard
 *    all the junk in a tuple before calling amreplace().  Otherwise the
 *    inserted tuple will not have the correct schema.  This solves a
 *    problem with hash-join and merge-sort replace plans.  -cim 10/10/90
 *
 *    targetList:	the original target list (including junk attributes).
 *    length:		the length of 'targetList'.
 *    tupType:		the tuple descriptor for the "original" tuple
 *			(including the junk attributes).
 *    cleanTargetList:	the "clean" target list (junk attributes removed).
 *    cleanLength:	the length of 'cleanTargetList'
 *    cleanTupTyp:	the tuple descriptor of the "clean" tuple (with
 *			junk attributes removed).
 *    cleanMap:		A map with the correspondance between the non junk
 *			attributes of the "original" tuple and the 
 *			attributes of the "clean" tuple.
 * ----------------
 */
class (JunkFilter) public (JunkFilter) {
      inherits(Node);
  /* private: */
      List			jf_targetList;
      int			jf_length;
      AttributePtr		jf_tupType;
      List			jf_cleanTargetList;
      int			jf_cleanLength;
      AttributePtr		jf_cleanTupType;
      AttributeNumberPtr	jf_cleanMap;
  /* public: */
};

/* ----------------
 *    EState information
 *
 *      direction                       direction of the scan
 *      time                            ?
 *      owner                           ?
 *      locks                           ?
 *      subplan_info                    ?
 *      error_message                   ?
 *
 *      range_table                     array of scan relation information
 *
 *      qualification_tuple             tuple satisifying qualification
 *					(going away soon)
 *
 *      qualification_tuple_id          tid of qualification_tuple
 *					(going away soon)
 *
 *      qualification_tuple_buffer      buffer of qualification_tuple
 *					(going away soon)
 *
 *      raw_qualification_tuple         tuple satsifying qualification
 *                                      but with no rules activated.
 *					(going away soon)
 *
 *      relation_relation_descriptor    as it says
 *
 *      into_relation_descriptor        relation being retrieved "into"
 *
 *      result_relation_information     for update queries
 *
 *      tuplecount                      summary of tuples processed
 *
 *      param_list_info                 information needed to transform
 *                                      Param nodes into Const nodes
 *
 *      prs2_info                       Information used by the rule
 *                                      manager (for loop detection
 *                                      etc.). Must be initialized to NULL
 *
 *      explain_relation                The relation descriptor of the
 *                                      result relation of an 'explain'
 *                                      command. NULL if this is not
 *                                      an explain command.
 * 
 *      BaseId                          during InitPlan(), each node is
 *                                      given a number.  this is the next
 *                                      number to be assigned.
 *
 *      tupleTable                      this is a pointer to an array
 *                                      of pointers to tuples used by
 *                                      the executor at any given moment.
 *
 *	junkFilter			contains information used to
 *					extract junk attributes from a tuple.
 *					(see JunkFilter above)
 * ----------------
 */

class (EState) public (Node) {
      inherits(Node);
      ScanDirection     es_direction;
      abstime           es_time;
      ObjectId          es_owner;
      List              es_locks;
      List              es_subplan_info;
      Name              es_error_message;
      List              es_range_table;
      HeapTuple         es_qualification_tuple;
      ItemPointer       es_qualification_tuple_id;
      Buffer		es_qualification_tuple_buffer;
      HeapTuple         es_raw_qualification_tuple;
      Relation          es_relation_relation_descriptor;
      Relation          es_into_relation_descriptor;
      RelationInfo      es_result_relation_info;
      TupleCount        es_tuplecount;
      ParamListInfo     es_param_list_info;
      Prs2EStateInfo    es_prs2_info;
      Relation          es_explain_relation;
      int               es_BaseId;
      TupleTable        es_tupleTable;
      JunkFilter	es_junkFilter;
};

/* ----------------
 *	ReturnState
 *
 *	XXX comment me
 * ----------------
 */
class (ReturnState) public (Node) {
      inherits(Node);
      Relation resultTmpRelDesc;
};

/* ----------------
 *      Executor Type information needed by plannodes.h
 *
 *|     Note: the bogus classes CommonState and CommonScanState exist only
 *|           because our inheritance system only allows single inheritance
 *|           and we have to have unique slot names.  Hence two or more
 *|           classes which want to have a common slot must ALL inherit
 *|           the slot from some other class.  (This is a big hack to
 *|           allow our classes to share slot names..)
 *|
 *|     Example:
 *|           the class Result and the class NestLoop nodes both want
 *|           a slot called "OuterTuple" so they both have to inherit
 *|           it from some other class.  In this case they inherit
 *|           it from CommonState.  "CommonState" and "CommonScanState" are
 *|           the best names I could come up with for this sort of
 *|           stuff.
 *|
 *|           As a result, many classes have extra slots which they
 *|           don't use.  These slots are denoted (unused) in the
 *|           comment preceeding the class definition.  If you
 *|           comes up with a better idea of a way of doing things
 *|           along these lines, then feel free to make your idea
 *|           known to me.. -cim 10/15/89
 * ----------------
 */

/* ----------------
 *   HookNode information
 *
 *      HookNodes are used to attach debugging routines to the plan
 *      at runtime.  The hook functions are passed a pointer
 *      to the node with which they are associated when they are
 *      called.
 *
 *      at_initnode	function called in ExecInitNode when hook is assigned
 *      pre_procnode    function called just before ExecProcNode
 *      pre_endnode     function called just before ExecEndNode
 *      post_initnode   function called just after ExecInitNode
 *      post_procnode   function called just after ExecProcNode
 *      post_endnode    function called just after ExecEndNode
 *      data            pointer to user defined information
 *
 *      Note:  there cannot be a pre_initnode call as this node's
 *              base node is assigned during the initnode processing.
 * ----------------
 */
class (HookNode) public (Node) {
#define HookNodeDefs \
      inherits(Node); \
      HookFunction      hook_at_initnode; \
      HookFunction      hook_pre_procnode; \
      HookFunction      hook_pre_endnode; \
      HookFunction      hook_post_initnode; \
      HookFunction      hook_post_procnode; \
      HookFunction      hook_post_endnode; \
      Pointer           hook_data
  /* private: */
      HookNodeDefs;
  /* public: */
};

/* ----------------------------------------------------------------
 *               Common Executor State Information
 * ----------------------------------------------------------------
 */

/* ----------------
 *      BaseNode information
 *
 *      BaseNodes are the fundamental superclass for all the
 *      remaining executor state nodes.  They contain the important
 *      fields:
 *
 *      id              integer used to keep track of other information
 *                      associated with a specific executor state node.
 *
 *	parent		backward pointer to this node's parent node.
 *
 *	parent_state	backward pointer to this node's parent node's
 *			private state.
 *
 *      hook            pointer to this node's hook node, if it exists.  
 *
 *      The id is used to keep track of executor resources and
 *      tuples during the query and the hook is used to attach
 *      debugging information.
 * ----------------
 */
class (BaseNode) public (Node) {
#define BaseNodeDefs \
      inherits(Node); \
      int               base_id; \
      Pointer		base_parent; \
      Pointer		base_parent_state; \
      HookNode          base_hook
  /* private: */
      BaseNodeDefs;
  /* public: */
};

/* ----------------
 *   CommonState information
 *
 *|     this is a bogus class used to hold slots so other
 *|     nodes can inherit them...
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the outer subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 *
 *              note: not all executor nodes make use of their
 *                    result tuple slot because they don't make copies
 *                    of tuples.
 *
 *|     Currently the value of Level is ignored by almost everything
 *|     and since I don't understand it's purpose, it may go
 *|     away soon.
 *|
 *|     ScanType should probably mean the type of tuples
 *|     in the subplan being scanned for nodes without underlying
 *|     relations.
 *|
 *|     ScanAttributes and NumScanAttributes are new -- they are
 *|     used to keep track of the attribute numbers of attributes
 *|     which are actually inspected by the query so the rule manager
 *|     doesn't have to needlessluy check for rules on attributes
 *|     that won't ever be inspected..
 *|
 *|     -cim 10/15/89
 *
 *      ExprContext contains the node's expression context which
 *      is created and initialized at ExecInit time.  Keeping these
 *      around reduces memory allocations and allows nodes to refer
 *      to tuples in other nodes (albeit in a gross fashion). -cim 10/30/89
 *
 *  SOON: ExprContext will contain indexes of tuples in the tupleTable
 *        stored in the EState, rather than pointers which it now stores.
 *        -cim 6/20/90
 * ----------------
 */

class (CommonState) public (BaseNode) {
#define CommonStateDefs \
      inherits(BaseNode); \
      TupleTableSlot      cs_OuterTuple; \
      AttributePtr        cs_TupType; \
      Pointer             cs_TupValue; \
      int                 cs_Level; \
      AttributePtr        cs_ScanType; \
      AttributeNumberPtr  cs_ScanAttributes; \
      int                 cs_NumScanAttributes; \
      TupleTableSlot      cs_ResultTupleSlot; \
      ExprContext         cs_ExprContext 
  /* private: */
      CommonStateDefs;
  /* public: */
};


/* ----------------------------------------------------------------
 *               Control Node State Information
 * ----------------------------------------------------------------
 */

/* ----------------
 *   ExistentialState information
 *
 *      SatState           See if we have satisfied the left tree.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */
class (ExistentialState) public (CommonState) {
      inherits(CommonState);
  /* private: */
      List      ex_SatState;
  /* public: */
};

/* ----------------
 *   ResultState information
 *
 *      Loop               flag which tells us to quit when we
 *                         have already returned a constant tuple.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the outer subplan
 *      ScanType           type of tuples in relation being scanned (unused)
 *      ScanAttributes     attribute numbers of interest in tuple (unused)
 *      NumScanAttributes  number of attributes of interest.. (unused)
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (ResultState) public (CommonState) {
   inherits(CommonState);
   int          rs_Loop;
};

/* ----------------
 *   AppendState information
 *
 *      append nodes have this field "unionplans" which is this
 *      list of plans to execute in sequence..  these variables
 *      keep track of things..
 *
 *      whichplan       which plan is being executed
 *      nplans          how many plans are in the list
 *      initialized     array of ExecInitNode() results
 *      rtentries       range table for the current plan
 * ----------------
 */

class (AppendState) public (BaseNode) {
    inherits(BaseNode);
    int         as_whichplan;
    int         as_nplans;
    ListPtr     as_initialized;
    List        as_rtentries;
};

/* ----------------------------------------------------------------
 *   RecursiveState information -JRB 12/11/89
 *
 *      Recursive nodes have some fields labelled "recurInitPlans",
 *      "recurLoopPlans", and "recurCleanupPlans" which are lists
 *      of plans and utilities to execute in sequence.  The following
 *      variables keep track of things...
 *
 *      whichSequence   which sequence of the three is being used
 *      whichPlan       which plan in the sequence is being executed
 *      numPlans        how many plans are in each list (list of 3)
 *      initialized     array of ExecInitNode() results
 *      rtentries       range table for the current plan
 *      effectsNoted    flag indicating whether the result relation
 *                      has been successfully affected in present
 *                      iteration.
 *      tempsSwapped    flag indicating whether the temporary relations
 *                      mentioned in the plan are used as initially
 *                      planned, or have switched places.
 * ----------------------------------------------------------------
 */

class (RecursiveState) public (BaseNode) {
    inherits(BaseNode);
    int         rcs_whichSequence;
    int         rcs_whichPlan;
    List        rcs_numPlans;
    ListPtr     rcs_initialized;
    List        rcs_rtentries;
    bool        rcs_effectsNoted;
    bool        rcs_tempsSwapped;
};

/* ----------------
 *   ParallelState information
 *
 *      XXX these fields will change! -cim 1/24/89
 *
 *      parallel nodes have this field "parallelplans" which is this
 *      list of plans to execute in parallel..  these variables
 *      keep track of things..
 *
 *      whichplan       which plan is being executed
 *      nplans          how many plans are in the list
 *      initialized     array of ExecInitNode() results
 *      rtentries       range table for the current plan
 * ----------------
 */

class (ParallelState) public (BaseNode) {
    inherits(BaseNode);
    int         ps_whichplan;
    int         ps_nplans;
    ListPtr     ps_initialized;
    List        ps_rtentries;
};

/* ----------------------------------------------------------------
 *               Scan State Information
 * ----------------------------------------------------------------
 */

/* ----------
 * NOTE:
 * This file (rulescan.h) must be included here because it uses
 * the EState defined previously in this file.
 * See comments in rulescan.h for more details....
 * ----------
 */
#include "access/rulescan.h"

/* ----------------
 *   CommonScanState information
 *
 *      CommonScanState is a class like CommonState, but is used more
 *      by the nodes like SeqScan and Sort which want to
 *      keep track of an underlying relation.
 *
 *      currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *      ruleInfo           information needed by the rule manager.
 *      ScanTupleSlot      pointer to slot in tuple table holding scan tuple
 *      ViewTupleSlot       ... holding the view tuple from the rmgr.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the outer subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (CommonScanState) public (CommonState) {
#define CommonScanStateDefs \
      inherits(CommonState); \
      Relation          css_currentRelation; \
      HeapScanDesc      css_currentScanDesc; \
      ScanStateRuleInfo css_ruleInfo; \
      Pointer           css_ScanTupleSlot; \
      Pointer           css_ViewTupleSlot
  /* private: */
      CommonScanStateDefs;
  /* public: */
};

/* ----------------
 *   ScanState information
 *
 *      ProcOuterFlag      need to process outer subtree
 *      OldRelId           need for compare for joins if result relid.
 *
 *   CommonScanState information
 *
 *      currentRelation    relation being scanned
 *      currentScanDesc    current scan descriptor for scan
 *      ScanTupleSlot      pointer to slot in tuple table holding scan tuple
 *      ViewTupleSlot       ... holding the view tuple from the rmgr.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (ScanState) public (CommonScanState) {
      inherits(CommonScanState);
  /* private: */
      bool                      ss_ProcOuterFlag;
      Index                     ss_OldRelId;
  /* public: */
};

class (ScanTempState) public (CommonScanState) {
	inherits(CommonScanState);
/* private: */
        int         st_whichplan;
        int         st_nplans;
/* public: */
};

/* ----------------
 *   IndexScanState information
 *
 *|     index scans don't use CommonScanState because
 *|     the underlying AM abstractions for heap scans and
 *|     index scans are too different..  It would be nice
 *|     if the current abstraction was more useful but ... -cim 10/15/89
 *
 *      IndexPtr           current index in use
 *      NumIndices         number of indices in this scan
 *      ScanKeys           Skey structures to scan index rels
 *      NumScanKeys        array of no of keys in each Skey struct
 *      RuntimeKeyInfo     array of array of flags for Skeys evaled at runtime
 *      RelationDescs      ptr to array of relation descriptors
 *      ScanDescs          ptr to array of scan descriptors
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */
class (IndexScanState) public (CommonState) {
    inherits(CommonState);
    int                 iss_NumIndices;
    int                 iss_IndexPtr;
    ScanKeyPtr          iss_ScanKeys;
    IntPtr              iss_NumScanKeys;
    Pointer             iss_RuntimeKeyInfo;
    RelationPtr         iss_RelationDescs;
    IndexScanDescPtr    iss_ScanDescs;
};


/* ----------------------------------------------------------------
 *               Join State Information
 * ----------------------------------------------------------------
 */

/* ----------------
 *   JoinState information
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */
class (JoinState) public (CommonState) {
#define JoinStateDefs \
    inherits(CommonState)
  /* private: */
    JoinStateDefs;
  /* public: */
};

/* ----------------
 *   NestLoopState information
 *
 *      PortalFlag         Set to enable portals to work.
 *
 *   JoinState information
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (NestLoopState) public (JoinState) {
      inherits(JoinState);
  /* private: */
      bool         nl_PortalFlag;
  /* public: */
};

/* ----------------
 *   MergeJoinState information
 *
 *      OSortopI           outerKey1 sortOp innerKey1 ...
 *      ISortopO           innerkey1 sortOp outerkey1 ...
 *      JoinState          current "state" of join. see executor.h
 *      MarkedTupleSlot    pointer to slot in tuple table for marked tuple
 *
 *   JoinState information
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (MergeJoinState) public (JoinState) {
      inherits(JoinState);
  /* private: */
      List           mj_OSortopI;
      List           mj_ISortopO;
      int            mj_JoinState;
      TupleTableSlot mj_MarkedTupleSlot;
  /* public: */
};

typedef File	*FileP;
typedef char	*charP;

/* ----------------
 *   HashJoinState information
 *
 *      hj_HashTable           	address of the hash table for the hashjoin
 *	hj_HashTableShmId	shared memory id of hash table
 *      hj_CurBucket           	the current hash bucket that we are searching
 *				for matches of the current outer tuple
 *      hj_CurTuple            	the current matching inner tuple in the
 *				current hash bucket
 *	hj_CurOTuple		the current matching inner tuple in the
 *				current hash overflow chain
 *      hj_InnerHashKey     	the inner hash key in the hashjoin condition
 *	hj_OuterBatches		file descriptors for outer batches
 *	hj_InnerBatches		file descriptors for inner batches
 *	hj_OuterReadPos		current read position of outer batch
 *	hj_OuterReadBlk		current read block of outer batch
 *	hj_SavedTupleSlot	xxx comment me
 *	hj_TemporaryTupleSlot	xxx comment me
 *
 *   JoinState information
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (HashJoinState) public (JoinState) {
      inherits(JoinState);
  /* private: */
      HashJoinTable     hj_HashTable;
      IpcMemoryId	hj_HashTableShmId;
      HashBucket        hj_CurBucket;
      HeapTuple         hj_CurTuple;
      OverflowTuple	hj_CurOTuple;
      Var               hj_InnerHashKey;
      FileP		hj_OuterBatches;
      FileP		hj_InnerBatches;
      charP		hj_OuterReadPos;
      int		hj_OuterReadBlk;
      Pointer   	hj_SavedTupleSlot;
      Pointer   	hj_TemporaryTupleSlot;
  /* public: */
};


/* ----------------------------------------------------------------
 *               Materialization State Information
 * ----------------------------------------------------------------
 */

/* ----------------
 *   MaterialState information
 *
 *      materialize nodes are used to materialize the results
 *      of a subplan into a temporary relation.
 *
 *      Flag            indicated whether subplan has been materialized
 *      TempRelation    temporary relation containing result of executing
 *                      the subplan.
 *
 *   CommonScanState information
 *
 *      currentRelation    relation descriptor of sorted relation
 *      currentScanDesc    current scan descriptor for scan
 *      ScanTupleSlot      pointer to slot in tuple table holding scan tuple
 *      ViewTupleSlot       ... holding the view tuple from the rmgr.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple' (unused)
 *      Level              level of the left subplan (unused)
 *      ScanType           type of tuples in relation being scanned (unused)
 *      ScanAttributes     attribute numbers of interest in tuple (unused)
 *      NumScanAttributes  number of attributes of interest.. (unused)
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (MaterialState) public (CommonScanState) {
      inherits(CommonScanState);
  /* private: */
      bool      mat_Flag;
      Relation  mat_TempRelation;
  /* public: */
};

/* ----------------
 *   SortState information
 *
 *|     sort nodes are really just a kind of a scan since
 *|     we implement sorts by retrieveing the entire subplan
 *|     into a temp relation, sorting the temp relation into
 *|     another sorted relation, and then preforming a simple
 *|     unqualified sequential scan on the sorted relation..
 *|     -cim 10/15/89
 *
 *      Flag            indicated whether relation has been sorted
 *      Keys            scan key structures used to keep info on sort keys
 *      TempRelation    temporary relation containing result of executing
 *                      the subplan.
 *
 *   CommonScanState information
 *
 *      currentRelation    relation descriptor of sorted relation
 *      currentScanDesc    current scan descriptor for scan
 *      ScanTupleSlot      pointer to slot in tuple table holding scan tuple
 *      ViewTupleSlot       ... holding the view tuple from the rmgr.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple' (unused)
 *      Level              level of the left subplan (unused)
 *      ScanType           type of tuples in relation being scanned (unused)
 *      ScanAttributes     attribute numbers of interest in tuple (unused)
 *      NumScanAttributes  number of attributes of interest.. (unused)
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (SortState) public (CommonScanState) {
#define SortStateDefs \
      inherits(CommonScanState); \
      bool      sort_Flag; \
      Pointer   sort_Keys; \
      Relation  sort_TempRelation
  /* private: */
      SortStateDefs;
  /* public: */
};

/* ----------------
 *   UniqueState information
 *
 *      Unique nodes are used "on top of" sort nodes to discard
 *      duplicate tuples returned from the sort phase.  Basically
 *      all it does is compare the current tuple from the subplan
 *      with the previously fetched tuple stored in OuterTuple and
 *      if the two are identical, then we just fetch another tuple
 *      from the sort and try again.
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple' (unused)
 *      Level              level of the left subplan (unused)
 *      ScanType           type of tuples in relation being scanned (unused)
 *      ScanAttributes     attribute numbers of interest in tuple (unused)
 *      NumScanAttributes  number of attributes of interest.. (unused)
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (UniqueState) public (CommonState) {
      inherits(CommonState);
  /* private: */
  /* public: */
};

/* ----------------
 *   HashState information
 *
 *	hashBatches	   file descriptors for the batches
 *
 *   CommonState information
 *
 *      OuterTuple         points to the current outer tuple
 *      TupType            attr type info of tuples from this node
 *      TupValue           array to store attr values for 'formtuple'
 *      Level              level of the left subplan
 *      ScanType           type of tuples in relation being scanned
 *      ScanAttributes     attribute numbers of interest in this tuple
 *      NumScanAttributes  number of attributes of interest..
 *      ResultTupleSlot    pointer to slot in tuple table for projected tuple
 *      ExprContext        node's current expression context
 * ----------------
 */

class (HashState) public (CommonState) {
      inherits(CommonState);
  /* private: */
  FileP		hashBatches;
  /* public: */
};

#endif /* ExecNodesIncluded */
