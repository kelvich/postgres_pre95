/*
 * mnodes.h --
 *	POSTGRES memory context node definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef	MNodesIncluded
#define MNodesIncluded	1	/* Include this file only once */

#include "tmp/c.h"

#include "utils/memutils.h"
#include "tmp/fstack.h"

#include "nodes/nodes.h"

/*
 * MemoryContext --
 *	A logical context in which memory allocations occur.
 *
 * The types of memory contexts can be thought of as members of the
 * following inheritance hierarchy with properties summarized below.
 *
 *			Node
 *			|
 *		MemoryContext___
 *		/		\
 *	GlobalMemory	PortalMemoryContext
 *			/		\
 *	PortalVariableMemory	PortalHeapMemory
 *
 *			Flushed at	Flushed at	Checkpoints
 *			Transaction	Portal
 *			Commit		Close
 *
 * GlobalMemory			n		n		n
 * PortalVariableMemory		n		y		n
 * PortalHeapMemory		y		y		y
 */

typedef struct MemoryContextMethodsData {
	Pointer	(*alloc)();
/* BAD PROTOTYPE DELETED -- glass */
/*		ARGS((classObj(MemoryContext) context, uint32 length));*/
	void	(*free_p)(); /* need to use free as a #define, so can't use free */
/* BAD PROTOTYPE DELETED -- glass */
/*		ARGS((classObj(MemoryContext) context, Pointer pointer));*/
	Pointer	(*realloc)();
/* BAD PROTOTYPE DELETED -- glass */
/*	    ARGS((
			classObj(MemoryContext) context,
			Pointer pointer,
			uint32 length
		));*/
	String	(*getName)();
/* BAD PROTOTYPE DELETED -- glass */
/*   ARGS((classObj(MemoryContext) context)); */
#if	0
/*
 * Do these make sense as general methods?  Probably not, but I am not
 * sure, yet. -hirohama
 */
	void	(*reset) ARGS((classObj(MemoryContext) context));
	void	(*destroy) ARGS((classObj(MemoryContext) context));
#endif
	void	(*dump)();
/* BAD PROTOTYPE DELETED -- glass */	
	    /* ARGS((classObj(MemoryContext) context));*/
} MemoryContextMethodsData;

typedef MemoryContextMethodsData	*MemoryContextMethods;



class (MemoryContext) public (Node) {
#define MemoryContextDefs \
	inherits0(Node); \
	MemoryContextMethods	method
/* private: */
	MemoryContextDefs;
/* protected: */
#define LispNodeIncrementRef(_node_)	(((LispNode)_node_)->refCount += 1)
#define LispNodeDecrementRef(_node_)	(((LispNode)_node_)->refCount -= 1)
#define LispNodeGetRefCount(_node_)	(((LispNode)_node_)->refCount)
#define LispNodeSetRefCount(_node_,_cnt_)	\
	(LispNodeGetRefCount(_node_) = _cnt_)
/* public: */
	/* XXX fill me in */
};

class (GlobalMemory) public (MemoryContext) {
	inherits1(MemoryContext);
	AllocSetData	setData;	/* set of allocated items */
	String		name;
	OrderedElemData	elemData;	/* member of set of GlobalMemory */
	/* XXX fill me in */
};

class (PortalMemoryContext) public (MemoryContext) {
#define PortalMemoryContextDefs \
	inherits1(MemoryContext)
/* private: */
	PortalMemoryContextDefs;
/* protected: */
/* public: */
	/* XXX fill me in */
};

class (PortalVariableMemory) public (PortalMemoryContext) {
	inherits2(PortalMemoryContext);
	AllocSetData	setData;
};

class (PortalHeapMemory) public (PortalMemoryContext) {
	inherits2(PortalMemoryContext);
	Pointer		block;
	FixedStackData	stackData;
};

/*
 * MemoryContextIsValid --
 *	True iff memory context is valid.
 */
#define MemoryContextIsValid(context) \
    ((bool) IsA(context,MemoryContext))

#endif	/* !defined(MNodesIncluded) */
