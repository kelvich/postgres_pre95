/*
 * relation.h --
 *	Definitions for internal planner nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef RelationIncluded
#define RelationIncluded

#include "c.h"
#include "primnodes.h"
#include "nodes.h"

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


class (Rel) public (Node) {
	inherits(Node);
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
	struct Path *unorderedpath; /* these should be Path's but recursive */
	struct Path *cheapestpath;  /* definitions make it impossible. */
/* used solely by indices: */
	List	classlist;
	List	indexkeys;
/* used by various scans and joins: */
	List	ordering;
	List	clauseinfo;
	List	joininfo;
	List	innerjoin;
};

class (TLE) public (Node) {
	inherits(Node);
	Resdom	resdom;
	Node	expr;
};

class (TL) public (Node) {
	inherits(Node);
	TLE		entry;
	List		joinlist;
};

class (SortKey) public (Node) {
	inherits(Node);
	List		varkeys;
	List		sortkeys;
	Relid		relid;
	List		sortorder;
};



class (Path) public (Node) {
	inherits(Node);
#define PathDefs \
	int32		pathtype; \
	Rel		parent; \
	Cost		cost; \
	List		ordering; \
	List		keys; \
	SortKey		sortpath
 /* private: */
	PathDefs;
 /* public: */
};


class (IndexPath) public (Path) {
	inherits(Path);
 /* private: */
	ObjectId	indexid;
	List		indexqual;
 /* public: */
};

class (JoinPath) public (Path) {
	inherits(Path);
#define JoinPathDefs \
	List		clauseinfo; \
	struct path	*outerjoinpath; \
	struct path	*innerjoinpath; \
	Cost		outerjoincost; \
	Relid		joinid
 /* private: */
	JoinPathDefs;
 /* public: */
};

class (MergePath) public (JoinPath) {
	inherits(JoinPath);
 /* private: */
	List		mergeclauses;
	List		outersortkeys;
	List		innersortkeys;
 /* public: */
};

class (HashPath) public (JoinPath) {
	inherits(JoinPath);
 /* private: */
	List		hashclauses;
	List		outerhashkeys;
	List		innerhashkeys;
 /* public: */
};

/******
 * Keys
 ******/

class (OrderKey) public (Node) {
    inherits(Node);
    int 	attribute_number;
    Index	array_index;
}

class (JoinKey) public (Node) {
    inherits(Node);
    Node outer;
    Node inner;
}

class (MergeOrder) public (Node) {
    ObjectId join_operator;
    ObjectId left_operator;
    ObjectId right_operator;
    ObjectId left_type;
    ObjectId right_type;
}

/*******
 * clause info
 *******/

class (CInfo) public (Node) {
	inherits(Node);
	Expr		clause;
	Cost		selectivity;
	bool		notclause;
	List		indexids;
/* mergesort only */
	MergeOrder	mergesortorder;
/* hashjoin only */
	ObjectId	hashjoinoperator;
};

class (JInfo) public (Node) {
	inherits(Node);
	List		otherrels;
	List		clauseinfo;
	bool		mergesortable;
	bool		hashjoinable;
};

#endif /* RelationIncluded */
