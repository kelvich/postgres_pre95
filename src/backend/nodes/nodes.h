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

#include "c.h"
#include "tags.h" 


typedef double Cost;
/* #define List LispValue */

/* char	*malloc();
   #define palloc malloc*/

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

/*
 * ====
 * NODE
 * ====
 *
 * Fake superclass for all tagged nodes.
 */

class (Node) {
#define	NodeDefs \
	NodeTag			type;	\
	void			(*printFunc)();	\
	bool			(*equalFunc)()
 /* private: */
	NodeDefs;
 /* public: */
#define	NodeType(_node_)	((Node)_node_)->type
#define	NodeIsValid(_node_)	PointerIsValid((Pointer)(_node_))
};

/*
extern Node	NewNode ARGS((Size size, TypeId type));
extern void	SetNodeType ARGS((Node node, TypeId tag));
extern bool	NodeIsType ARGS((Node node, TypeId tag));
*/

#define	IsA(_node_,_tag_)	NodeIsType((Node)(_node_), classTag(_tag_))
#define	New_Node(_x_)		((_x_)NewNode(classSize(_x_),classTag(_x_)))
#define CreateNode(_x_)		((_x_)NewNode(classSize(_x_),classTag(_x_)))

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
 * CreateNode --
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
 * NodeIsType --
 *	True iff node or one of its ancestors has the given tag.
 *
 * Note:
 *	See also:  NodeIsTagged.
 */
extern
bool
NodeHasTag ARGS((Node	node,NodeTag	tag ));



#endif /* NodesIncluded */
