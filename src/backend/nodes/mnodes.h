/*
 *   FILE
 *	mnodes.h
 *
 *   DESCRIPTION
 *	POSTGRES memory context node definitions.
 *
 *   NOTES
 *
 *   IDENTIFICATION
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
	Pointer	(*alloc)();	/* part of typedef */
/* need to use free as a #define, so can't use free */
	void	(*free_p)();	/* part of typedef */
	Pointer	(*realloc)();	/* part of typedef */
	String	(*getName)();	/* part of typedef */
	void	(*dump)();	/* part of typedef */
} MemoryContextMethodsData;	/* part of typedef */

typedef MemoryContextMethodsData	*MemoryContextMethods;

class (MemoryContext) public (Node) {
#define MemoryContextDefs \
	inherits0(Node); \
	MemoryContextMethods	method
  /* private: */
	MemoryContextDefs;
  /* protected: */
  /* public: */
};
#define LispNodeIncrementRef(_node_)	(((LispNode)_node_)->refCount += 1)
#define LispNodeDecrementRef(_node_)	(((LispNode)_node_)->refCount -= 1)
#define LispNodeGetRefCount(_node_)	(((LispNode)_node_)->refCount)
#define LispNodeSetRefCount(_node_,_cnt_) (LispNodeGetRefCount(_node_) = _cnt_)

class (GlobalMemory) public (MemoryContext) {
	inherits1(MemoryContext);
	AllocSetData	setData;	/* set of allocated items */
	String		name;
	OrderedElemData	elemData;	/* member of set of GlobalMemory */
};

class (PortalMemoryContext) public (MemoryContext) {
#define PortalMemoryContextDefs \
	inherits1(MemoryContext)
  /* private: */
	PortalMemoryContextDefs;
  /* protected: */
  /* public: */
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
#define MemoryContextIsValid(context) ((bool) IsA(context,MemoryContext))

#endif	/* !defined(MNodesIncluded) */
