/*
 * relation.h --
 *	Definitions for internal planner nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef RelationIncluded
#define RelationIncluded

#include "tmp/c.h"
#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/nodes.h"

#define TLE LispValue
#define TL LispValue

#include "relation.gen"

/*
 *  These #defines indicate that we have supplied print routines for the
 *  named classes.  The routines are in lib/C/printfuncs.c; interface routines
 *  are automatically generated from this .h file.  When time permits, this
 *  system ought to be redesigned.
 */

#define	PrintRelExists
#define	PrintSortKeyExists
#define	PrintPathExists
#define	PrintIndexPathExists
#define	PrintJoinPathExists
#define	PrintMergePathExists
#define	PrintHashPathExists
#define	PrintOrderKeyExists
#define	PrintJoinKeyExists
#define	PrintMergeOrderExists
#define	PrintCInfoExists
#define	PrintJInfoExists
#define PrintHInfoExists
#define PrintJoinMethodExists

extern void     PrintRel();
extern void	PrintSortKey();
extern void	PrintPath();
extern void	PrintIndexPath();
extern void	PrintJoinPath();
extern void	PrintMergePath();
extern void	PrintHashPath();
extern void	PrintOrderKey();
extern void	PrintJoinKey();
extern void	PrintMergeOrder();
extern void	PrintCInfo();
extern void	PrintJInfo();
extern void     PrintHInfo();
extern void     PrintJoinMethod();

/* equal funcs that exist in lib/C/equalfunc.c and aren't defined elsewhere*/
#define EqualCInfoExists 1
#define EqualJoinMethodExists 1
#define EqualPathExists 1
#define EqualIndexPathExists 1
#define EqualJoinPathExists 1
#define EqualMergePathExists 1
#define EqualHashPathExists 1
#define EqualJoinKeyExists 1
#define EqualMergeOrderExists 1
#define EqualHInfoExists 1
#define EqualIndexScanExists 1
#define EqualJInfoExists 1
#define EqualEStateExists 1
#define EqualHeapTupleExists 1
#define EqualRelationExists 1
#define EqualLispValueExists 1
#define EqualIterExists 1
#define EqualStreamExists 1

/* copy funcs that exist in lib/C/copyfunc.c and aren't defined elsewhere*/
#define CopyNodeExists 1
#define CopyMaterialExists 1
#define CopyRelExists 1
#define CopySortKeyExists 1
#define CopyPathExists 1
#define CopyIndexPathExists 1
#define CopyJoinPathExists 1
#define CopyMergePathExists 1
#define CopyHashPathExists 1
#define CopyOrderKeyExists 1
#define CopyJoinKeyExists 1
#define CopyMergeOrderExists 1
#define CopyCInfoExists 1
#define CopyJoinMethodExists 1
#define CopyHInfoExists 1
#define CopyMInfoExists 1
#define CopyJInfoExists 1
#define CopyIterExists 1
#define CopyStreamExists 1

/* out funcs from lib/C/outfuncs.c that aren't defined elsewhere */
#define OutPlanInfoExists 1
#define OutRelExists 1
#define OutTLEExists 1
#define OutTLExists 1
#define OutSortKeyExists 1
#define OutPathExists 1
#define OutIndexPathExists 1
#define OutJoinPathExists 1
#define OutMergePathExists 1
#define OutHashPathExists 1
#define OutOrderKeyExists 1
#define OutJoinKeyExists 1
#define OutMergeOrderExists 1
#define OutCInfoExists 1
#define OutJoinMethodExists 1
#define OutHInfoExists 1
#define OutJInfoExists 1
#define OutDatumExists 1
#define OutIterExists 1
#define OutStreamExists 1


/*
 * Relid
 *	List of relation identifiers (indexes into the rangetable).
 */

typedef	List	Relid;

/*
 * Rel
 *	Per-base-relation information
 *
 *	Parts of this data structure are specific to various scan and join
 *	mechanisms.  It didn't seem worth creating new node types for them.
 *
 *	relids - List of relation indentifiers
 *	indexed - true if the relation has secondary indices
 *	pages - number of pages in the relation
 *	tuples - number of tuples in the relation
 *	size - number of tuples in the relation after restrictions clauses
 *	       have been applied
 *	width - number of bytes per tuple in the relation after the
 *		appropriate projections have been done
 *	targetlist - List of TargetList nodes
 *	pathlist - List of Path nodes, one for each possible method of
 *		   generating the relation
 *	unorderedpath - a Path node generating this relation whose resulting
 *			tuples are unordered (this isn't necessarily a
 *			sequential scan path, e.g., scanning with a hash index
 *			leaves the tuples unordered)
 *	cheapestpath -  least expensive Path (regardless of final order)
 *      pruneable - flag to let the planner know whether it can prune the plan
 *                  space of this Rel or not.  -- JMH, 11/11/92
 *
 *   * If the relation is a (secondary) index it will have the following
 *	three fields:
 *
 *	class - List of PG_AMOPCLASS OIDs for the index
 *	indexkeys - List of base-relation attribute numbers that are index keys
 *	ordering - List of PG_OPERATOR OIDs which order the indexscan result
 *
 *   * The presence of the remaining fields depends on the restrictions
 *	and joins which the relation participates in:
 *
 *	clauseinfo - List of ClauseInfo nodes, containing info about each
 *		     qualification clause in which this relation participates
 *	joininfo  - List of JoinInfo nodes, containing info about each join
 *		    clause in which this relation participates
 *	innerjoin - List of Path nodes that represent indices that may be used
 *		    as inner paths of nestloop joins
 */

/* hack */

/* the following typedef's were added and 
 * 'struct Blah *blah' replaced with 'BlahPtr blah' in the #define's.  
 * the reason?  sprite cannot handle 'struct Blah *blah'.         
 * - ron 'blah blah' choi
 */
typedef struct Path *PathPtr;

class (Rel) public (Node) {
	inherits0(Node);
  /* all relations: */
	Relid	relids;
  /* catalog statistics information */
	bool	indexed;
	Count	pages;
	Count	tuples;
	Count	size;
	Count	width;
  /* materialization information */
	List	targetlist;
	List	pathlist;
	PathPtr	unorderedpath;
	PathPtr	cheapestpath;
	bool    pruneable;
  /* used solely by indices: */
	List	classlist;
	List	indexkeys;
	oid	indproc;
  /* used by various scans and joins: */
	List	ordering;
	List	clauseinfo;
	List	joininfo;
	List	innerjoin;
	List	superrels;
};
#undef Path

extern Var get_expr ARGS(( TLE foo));
extern Resdom get_resdom ARGS(( TLE foo));
extern TLE get_entry  ARGS(( TL foo));
extern List get_joinlist ARGS(( TL foo));

class (SortKey) public (Node) {
	inherits0(Node);
	List		varkeys;
	List		sortkeys;
	Relid		relid;
	List		sortorder;
};



class (Path) public (Node) {
#define	PathDefs \
	inherits0(Node); \
	int32		pathtype; \
	Rel		parent; \
	Cost		path_cost; \
	List		p_ordering; \
	List		keys; \
	SortKey		pathsortkey; \
	Cost		outerjoincost; \
	Relid		joinid; \
	List            locclauseinfo
/* private: */
	PathDefs;
 /* public: */
};


class (IndexPath) public (Path) {
	inherits1(Path);
 /* private: */
	List		indexid;
	List		indexqual;
 /* public: */
};

/* the following typedef's were added and 
 * 'struct Blah *blah' replaced with 'BlahPtr blah' in the #define's.  
 * the reason?  sprite cannot handle 'struct Blah *blah'.         
 * - ron 'blah blah' choi
 */
typedef struct path *pathPtr;

class (JoinPath) public (Path) {
#define JoinPathDefs \
	inherits1(Path); \
	List		pathclauseinfo; \
	pathPtr	outerjoinpath; \
	pathPtr	innerjoinpath
 /* private: */
	JoinPathDefs;
 /* public: */
};

class (MergePath) public (JoinPath) {
	inherits2(JoinPath);
 /* private: */
	List		path_mergeclauses;
	List		outersortkeys;
	List		innersortkeys;
 /* public: */
};

class (HashPath) public (JoinPath) {
	inherits2(JoinPath);
 /* private: */
	List		path_hashclauses;
	List		outerhashkeys;
	List		innerhashkeys;
 /* public: */
};

/******
 * Keys
 ******/

class (OrderKey) public (Node) {
	inherits0(Node);
	int 	attribute_number;
	Index	array_index;
};

class (JoinKey) public (Node) {
	inherits0(Node);
	LispValue outer;
	LispValue  inner;
};

class (MergeOrder) public (Node) {
	inherits0(Node);
	ObjectId join_operator;
	ObjectId left_operator;
	ObjectId right_operator;
	ObjectId left_type;
	ObjectId right_type;
};

/*******
 * clause info
 *******/

class (CInfo) public (Node) {
	inherits0(Node);
	LispValue	clause;
	Cost		selectivity;
	bool		notclause;
	List		indexids;
/* mergesort only */
	MergeOrder	mergesortorder;
/* hashjoin only */
	ObjectId	hashjoinoperator;
	Relid		cinfojoinid;
};

class (JoinMethod) public (Node) {
#define JoinMethodDefs \
	inherits0(Node); \
	List            jmkeys; \
	List            clauses
 /* private: */
	JoinMethodDefs;
 /* public: */
};

class (HInfo) public (JoinMethod) {
	inherits1(JoinMethod);
	ObjectId        hashop;
};

class (MInfo) public (JoinMethod) {
	inherits1(JoinMethod);
	MergeOrder         m_ordering;
};

class (JInfo) public (Node) {
	inherits0(Node);
	List		otherrels;
	List		jinfoclauseinfo;
	bool		mergesortable;
	bool		hashjoinable;
	bool		inactive;
};

class (Iter) public (Node) {
        inherits0(Node);
	LispValue            iterexpr;
};

/*
** Stream:
**   A stream represents a root-to-leaf path in a plan tree (i.e. a tree of
** JoinPaths and Paths).  The stream includes pointers to all Path nodes,
** as well as to any clauses that reside above Path nodes.  This structure
** is used to make Path nodes and clauses look similar, so that Predicate
** Migration can run.
**
**     pathptr -- pointer to the current path node
**       cinfo -- if NULL, this stream node referes to the path node.  Otherwise
**              this is a pointer to the current clause.
**  clausetype -- whether cinfo is in locclauseinfo or pathclauseinfo in the 
**                path node
**    upstream -- linked list pointer upwards
**  downstream -- ditto, downwards
**     groupup -- whether or not this node is in a group with the node upstream
**   groupcost -- total cost of the group that node is in
**    groupsel -- total selectivity of the group that node is in
*/
typedef struct Stream *StreamPtr;
class (Stream) public (Node) {
        inherits0(Node);
	pathPtr pathptr;
	CInfo cinfo;
	int clausetype;
        StreamPtr upstream;
	StreamPtr downstream;
	bool groupup;
	Cost groupcost;
	Cost groupsel;
};
#endif /* RelationIncluded */
