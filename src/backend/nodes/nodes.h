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
#include "pg_lisp.h"

typedef double Cost;
#define List LispValue
char	*malloc();
#define palloc malloc
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
#define	classTag(_x_)	((TypeId) CppConcat(T_,_x_))
#define	class(_x_)	typedef classObj(_x_) *_x_; classObj(_x_)

/*
 * TypeId
 * XXX An "enum" would be slightly cleaner but
 *	(1) "enum" isn't universal yet
 *	(2) the Saber disbelief of typedefs makes them a pain
 */

typedef	unsigned int		TypeId;
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
	TypeId			type
 /* private: */
	NodeDefs;
 /* public: */
#define	NodeType(_node_)	(((Node)(_node_))->type)
#define	NodeIsValid(_node_)	PointerIsValid((Pointer)(_node_))
};


extern Node	NewNode ARGS((Size size, TypeId type));
extern void	SetNodeType ARGS((Node node, TypeId tag));
extern bool	NodeIsType ARGS((Node node, TypeId tag));
/*
extern Node NewNode();
extern void SetNodeType();
extern bool NodeIsType();
*/

#define	IsA(_node_,_tag_)	NodeIsType((Node)(_node_), classTag(_tag_))
#define	New_Node(_x_)		((_x_)NewNode(classSize(_x_),classTag(_x_)))
#define CreateNode(_x_)		((_x_)NewNode(classSize(_x_),classTag(_x_)))

#endif /* NodesIncluded */
