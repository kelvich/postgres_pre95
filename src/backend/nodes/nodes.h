/*
 * nodes.h --
 *	Definitions for tagged nodes.
 *
 * Identification:
 *	$Header$
 *
 * NOTES:
 *	Why do things this way, you ask.
 *
 *	(1) Eventually this code should be transmogrified into C++ classes,
 *	    and this is more or less compatible with those things.
 *	(2) Unions suck.
 *
 *	As long as all inheritance declarations are put at the beginning of
 *	the structure in a consistent order this is perfectly legal (sharing
 *	of initial structure members is K&R C).
 */

#ifndef NodesIncluded
#define	NodesIncluded

#include "tmp/c.h"
#include "tags.h" 

/* ----------------------------------------------------------------
 *		    miscellanious node defines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	NO_NODE_CHECKING, if defined, turns off the built in
 *	argument sanity checking done within the generated
 *	set_ and get_ accessor functions.
 *
 *	If not defined, then sanity checking is done.  This makes
 *	things a bit slower but makes debugging easier..
 * ----------------
 */

/* #undef NO_NODE_CHECKING */

/*
 * We define NO_NODE_CHECKING for now because we are trying to get speed
 * out of this thing...
 */

#define NO_NODE_CHECKING

/* ----------------
 *	I don't know why this is here.  Most likely a hack..
 *	-cim 6/3/90
 * ----------------
 */
typedef double Cost;

/*
 *	Set up a single-inheritance mechanism based on cpp.  Ick.
 *
 *	public() is a noise-word that exists solely for our inheritance-graph
 *	  generation script.
 */

#define	public(_x_)
#define	inherits(_x_)	CppConcat(_x_,Defs)
#define	classObj(_x_)	struct CppConcat(_,_x_)
#define	classSize(_x_)	((Size) sizeof(classObj(_x_)))
#define	classTag(_x_)	((NodeTag) CppConcat(T_,_x_))
#define	class(_x_)	typedef classObj(_x_) *_x_; classObj(_x_)

/*
 * TypeId
 * XXX An "enum" would be slightly cleaner but
 *	(1) "enum" isn't universal yet
 *	(2) the Saber disbelief of typedefs makes them a pain
 */

typedef	unsigned int		TypeId;
typedef unsigned int		NodeTag;
extern TypeId			_InvalidTypeId;
#define	TypeIdIsValid(t)	((t) < (TypeId)_InvalidTypeId)

/* ----------------------------------------------------------------
 *			Node class definition
 *
 *	The Node class is the superclass for all other classes
 *	in this inheritance system.
 * ----------------------------------------------------------------
 */

class (Node) {
#define	NodeDefs \
	NodeTag			type;	\
	void			(*outFunc)();	\
	bool			(*equalFunc)(); \
	bool			(*copyFunc)()
 /* private: */
	NodeDefs;
 /* public: */
};

#define	NodeType(_node_)	((Node)_node_)->type
#define	NodeIsValid(_node_)	PointerIsValid((Pointer)(_node_))

#define	IsA(_node_,_tag_)	NodeIsType((Node)(_node_), classTag(_tag_))
#define	New_Node(_x_)		((_x_)NewNode(classSize(_x_),classTag(_x_)))
#define CreateNode(_x_)		((_x_)NewNode(classSize(_x_),classTag(_x_)))

#define ExactNodeType(_node_,_tag_) (NodeType(_node_) == classTag(_tag_))

/*
 * "Clause" macros for finding various nodes follow
 */

#define fast_is_clause(_clause_) (ExactNodeType(CAR((LispValue) _clause_),Oper))
#define fast_is_funcclause(_clause_) (ExactNodeType(CAR((LispValue) _clause_),Func))
#define fast_not_clause(_clause_) (CInteger(CAR((LispValue) _clause_)) == NOT)
#define fast_or_clause(_clause_) (CInteger(CAR((LispValue) _clause_)) == OR)

/* ----------------------------------------------------------------
 *		      extern declarations follow
 * ----------------------------------------------------------------
 */
/*
extern Node	NewNode ARGS((Size size, TypeId type));
extern void	SetNodeType ARGS((Node node, TypeId tag));
extern bool	NodeIsType ARGS((Node node, TypeId tag));
*/


/*
 * NodeTagIsValid --
 *      True iff node tag is valid.
 */
extern bool NodeTagIsValid ARGS(( NodeTag tag ));

/*
 * NodeGetTag --
 *	Returns tag of a node.
 *
 * Note:
 *	Assumes node is valid.
 */
extern
NodeTag
NodeGetTag ARGS(( Node	node ));

/*
 * NewNode --
 *	Returns a new node of the given size and tag.
 */
extern
Node
NewNode ARGS((Size	size,NodeTag	tag ));

/*
 * NodeSetTag --
 *	Sets tag of a node.
 *
 * Note:
 *	Assumes node is valid pointer.
 *	Assumes tag is valid.
 */
extern
void
NodeSetTag ARGS((
	Node	node,
	NodeTag	tag
));

/*
 * NodeHasTag --
 *	True iff node or one of its ancestors has the given tag.
 *
 * Note:
 *	See also:  NodeIsTagged.
 */
extern
bool
NodeHasTag ARGS((Node	node,NodeTag	tag ));



#endif /* NodesIncluded */
