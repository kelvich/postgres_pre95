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

#define EqualCInfoExists 1
#define EqualJInfoExists 1
#define EqualHInfoExists 1
#define EqualJoinMethodExists 1
#define EqualHashPathExists 1
#define EqualIndexScanExists 1
#define EqualMergeOrderExists 1
#define EqualJoinPathExists 1
#define EqualPathExists 1
#define EqualIndexPathExists 1
#define EqualJoinKeyExists 1
#define EqualMergePathExists 1

extern bool     EqualRel();
extern bool	EqualSortKey();
extern bool	EqualPath();
extern bool	EqualIndexPath();
extern bool	EqualJoinPath();
extern bool	EqualMergePath();
extern bool	EqualHashPath();
extern bool	EqualOrderKey();
extern bool	EqualJoinKey();
extern bool	EqualMergeOrder();
extern bool	EqualCInfo();
extern bool	EqualJInfo();
extern bool     EqualIndexScan();
extern bool     EqualHInfo();
extern bool     EqualJoinmethod();

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

#define TLE LispValue
#define TL LispValue

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
	SortKey		sortpath; \
	Cost		outerjoincost; \
	Relid		joinid
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
	Expr		clause;
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

#endif /* RelationIncluded */
