/* ----------------------------------------------------------------
 *   FILE
 *	copyfuncs.c
 *	
 *   DESCRIPTION
 *	 Copy functions for Postgres tree nodes.
 *
 *   INTERFACE ROUTINES
 *	lispCopy		- partial copy function for lisp structures
 *	NodeCopy		- function to recursively copy a node 
 *	CopyObject		- \ simpler interfaces to 
 *	CopyObjectUsing		- /  NodeCopy()
 *	_copyXXX		- copy function for class XXX
 *
 *   NOTES
 *	These routines are intended for use during development, and should
 *	be improved before we do a production version.  Right now, every node
 *	we create ships around a pointer to the function that knows how to
 *	copy it.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

/* ----------------
 *	this list of header files taken from other node file headers.
 * ----------------
 */
#include <stdio.h>

#include "c.h"
#include "datum.h"
#include "pg_lisp.h"
#include "nodes.h"
#include "primnodes.h"
#include "plannodes.h"
#include "execnodes.h"
#include "relation.h"
#include "recursion.h"
#include "recurplanner.h"

#include "fmgr.h"
#include "heapam.h"
#include "log.h"
#include "oid.h"
#include "syscache.h"
#include "htup.h"

/* ----------------------------------------------------------------
 *	lispCopy
 *
 * 	This copy function is used to make copies of just the
 *	"internal nodes" (lisp structures) but not copy "leaf"
 *	nodes.  In simpler terms, the recursion stops at the
 *	first non-lisp node type.  This function allocates
 *	memory with palloc().
 * ----------------------------------------------------------------
 */

LispValue
lispCopy(lispObject)
     LispValue lispObject;
{
     LispValue newnode;
     if (lispObject == LispNil)
	 return(LispNil);
     
     newnode = (LispValue) lispAlloc();
     newnode->type = lispObject->type;
     newnode->printFunc = lispObject->printFunc;
     newnode->equalFunc = lispObject->equalFunc;
     newnode->copyFunc = lispObject->copyFunc;
     
     switch (lispObject->type) {
     case T_LispSymbol:
	  newnode->val.name = lispObject->val.name;
	  newnode->cdr = LispNil;
	  break;
     case T_LispStr:
	  newnode->val.str = lispObject->val.str;
	  newnode->cdr = LispNil;
	  break;
     case T_LispInt:
	  newnode->val.fixnum = lispObject->val.fixnum;
	  newnode->cdr = LispNil;
	  break;
     case T_LispFloat:
	  newnode->val.flonum = lispObject->val.flonum;
	  newnode->cdr = LispNil;
	  break;
     case T_LispVector:
	  newnode->val.veci = lispObject->val.veci;
	  newnode->cdr = LispNil;
	  break;
     case T_LispList:
	  newnode->val.car = lispCopy(lispObject->val.car);
	  newnode->cdr = lispCopy(lispObject->cdr);
	  break;
     default:
	  newnode = lispObject;
     }
     return (newnode);
}

/* ----------------------------------------------------------------
 *	NodeCopy
 *	CopyObject
 *	CopyObjectUsing
 *
 *	These functions provide support for copying complex
 *	trees of all node types which supply simple copy functions
 *	defined below.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	NodeCopy is a function which calls the node's own
 *	copy function to copy the node using the specified allocation
 *	function and update the value of the "to" pointer.
 * ----------------
 */
bool
NodeCopy(from, toP, alloc)
    Node 	from;
    Node 	*toP;
    char *	(*alloc)();
{
    bool	(*copyfunc)();

    /* ----------------
     *	if the "from" node is NULL, then just assign NULL to the 
     *  "to" node. 
     * ----------------
     */
    if (from == NULL) {
	(*toP) = NULL;
	return true;
    }
    
    /* ----------------
     *	if the copy function doesn't exist then we print
     *  a warning and do a pointer assignment.
     * ----------------
     */
    copyfunc = from->copyFunc;
    if (copyfunc == NULL) {
	elog(NOTICE,
	     "NodeCopy: NULL copy function (type: %d) -- doing ptr assign",
	     from->type);
	(*toP) = from;
	return true;
    } else
	return (*copyfunc)(from, toP, alloc);
}

/* ----------------
 *	CopyObject is a simpler top level interface
 *	to the NodeCopy.  Given a pointer to
 *	the root of a node tree, it returns a pointer to a copy
 *	of the tree which was allocated with palloc.
 * ----------------
 */
Node
CopyObject(from)
    Node from;
{
    Node copy;
    extern Pointer palloc();
    
    if (NodeCopy(from, &copy, palloc) == true)
	return copy;
    else
	return NULL;
}

/* ----------------
 *	CopyObjectUsing() is identical to CopyObject() but takes
 *	a second parameter which is a pointer to the allocation
 *	function to use in making the copy.
 *
 *	In particular, this function is needed in the parallel
 *	executor code to place copies of query plans into shared
 *	memory.
 * ----------------
 */
Node
CopyObjectUsing(from, alloc)
    Node 	from;
    char *	(*alloc)();
{
    Node copy;

    if (alloc == NULL) {
	elog(NOTICE, "CopyObjectUsing: alloc function is NULL!");
	return NULL;
    }
	
    if (NodeCopy(from, &copy, alloc) == true)
	return copy;
    else
	return NULL;
}

/* ----------------------------------------------------------------
 *	NodeCopy/CopyObject/CopyObjectUsing support functions
 * ----------------------------------------------------------------
 */

/* ----------------
 *	Node_Copy
 *
 *	a macro to simplify calling of NodeCopy from within
 *	the copy support routines:
 *
 *		if (NodeCopy(from->state, &(newnode->state), alloc) != true)
 *		   return false;
 *
 *	becomes
 *
 *		Node_Copy(from, newnode, alloc, state);
 *
 * ----------------
 */
#define Node_Copy(a, b, c, d) \
    if (NodeCopy(((a)->d), &((b)->d), c) != true) { \
       return false; \
    } 

/* ----------------
 *	COPY_CHECKARGS, COPY_CHECKNULL and COPY_NEW are macros used
 *	to simplify all the _copy functions.
 * ----------------
 */
#define COPY_CHECKARGS() \
    if (to == NULL || alloc == NULL) { \
       return false; \
    } 
				      
#define COPY_CHECKNULL() \
    if (from == NULL) { \
	(*to) = NULL; \
	return true; \
    } 

#define COPY_NEW(c) \
    newnode = (c) (*alloc)(classSize(c)); \
    if (newnode == NULL) { \
	return false; \
    } 

/* ****************************************************************
 *		       nodes.h copy functions
 * ****************************************************************
 */

/* ----------------
 *	CopyNodeFields
 *
 *	This function copies the fields of the Node node.  It is used by
 *	all the copy functions for classes which inherit from Node.
 * ----------------
 */
bool CopyNodeFields(from, newnode, alloc)
    Node 	from;
    Node 	newnode;
    char *	(*alloc)();
{
    newnode->type = from->type;
    newnode->printFunc = from->printFunc;
    newnode->equalFunc = from->equalFunc;
    newnode->copyFunc = from->copyFunc;
    return true;
}
   
/* ----------------
 *	_copyNode
 * ----------------
 */
        
bool
_copyNode(from, to, alloc)
    Node	from;
    Node	*to;
    char *	(*alloc)();
{
    Node	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Node);
    
    /* ----------------
     *	copy the node
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    
    (*to) = newnode;
    return true;
}

/* ****************************************************************
 *		     plannodes.h copy functions
 * ****************************************************************
 */

/* ----------------
 *	CopyPlanFields
 *
 *	This function copies the fields of the Plan node.  It is used by
 *	all the copy functions for classes which inherit from Plan.
 * ----------------
 */
bool CopyPlanFields(from, newnode, alloc)
    Plan 	from;
    Plan 	newnode;
    char *	(*alloc)();
{
    newnode->cost = 	from->cost;
    newnode->fragment = 	from->fragment;
    
    Node_Copy(from, newnode, alloc, state);
    Node_Copy(from, newnode, alloc, qptargetlist);
    Node_Copy(from, newnode, alloc, qpqual);
    Node_Copy(from, newnode, alloc, lefttree);
    Node_Copy(from, newnode, alloc, righttree);
    return true;
}
    
/* ----------------
 *	_copyPlan
 * ----------------
 */
bool
_copyPlan(from, to, alloc)
    Plan	from;
    Plan	*to;
    char *	(*alloc)();
{
    Plan	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Plan);
    
    /* ----------------
     *	copy the node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    
    (*to) = newnode;
    return true;
}

    
/* ----------------
 *	_copyExistential
 * ----------------
 */
bool
_copyExistential(from, to, alloc)
    Existential	from;
    Existential	*to;
    char *	(*alloc)();
{
    Existential	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Existential);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node 
     * ----------------
     */
    Node_Copy(from, newnode, alloc, exstate);
    
    (*to) = newnode;
    return true;
}

    
/* ----------------
 *	_copyResult
 * ----------------
 */
bool
_copyResult(from, to, alloc)
    Result	from;
    Result	*to;
    char *	(*alloc)();
{
    Result	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Result);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node 
     * ----------------
     */
    Node_Copy(from, newnode, alloc, resrellevelqual);
    Node_Copy(from, newnode, alloc, resconstantqual);
    Node_Copy(from, newnode, alloc, resstate);
        
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	_copyAppend
 * ----------------
 */
bool
_copyAppend(from, to, alloc)
    Append	from;
    Append	*to;
    char *	(*alloc)();
{
    Append	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Append);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node 
     * ----------------
     */
    Node_Copy(from, newnode, alloc, unionplans);
    Node_Copy(from, newnode, alloc, unionrelid);
    Node_Copy(from, newnode, alloc, unionrtentries);
    Node_Copy(from, newnode, alloc, unionstate);
        
    (*to) = newnode;
    return true;
}

    
/* ----------------
 *	_copyParallel
 * ----------------
 */
bool
_copyParallel(from, to, alloc)
    Parallel	from;
    Parallel	*to;
    char *	(*alloc)();
{
    Parallel	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Parallel);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node 
     * ----------------
     */
    Node_Copy(from, newnode, alloc, parstate);
        
    (*to) = newnode;
    return true;
}

    
/* ----------------
 *	_copyRecursive
 * ----------------
 */
bool
_copyRecursive(from, to, alloc)
    Recursive	from;
    Recursive	*to;
    char *	(*alloc)();
{
    Recursive	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Recursive);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node 
     * ----------------
     */
    newnode->recurMethod = from->recurMethod;
    Node_Copy(from, newnode, alloc, recurCommand);
    Node_Copy(from, newnode, alloc, recurInitPlans);
    Node_Copy(from, newnode, alloc, recurLoopPlans);
    Node_Copy(from, newnode, alloc, recurCleanupPlans);
    Node_Copy(from, newnode, alloc, recurCheckpoints);
    Node_Copy(from, newnode, alloc, recurResultRelationName);
    Node_Copy(from, newnode, alloc, recurState);
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	CopyScanFields
 *
 *	This function copies the fields of the Scan node.  It is used by
 *	all the copy functions for classes which inherit from Scan.
 * ----------------
 */
bool CopyScanFields(from, newnode, alloc)
    Scan 	from;
    Scan 	newnode;
    char *	(*alloc)();
{
    newnode->scanrelid = from->scanrelid;
    Node_Copy(from, newnode, alloc, scanstate);
    return true;
}
    

/* ----------------
 *	_copyScan
 * ----------------
 */
bool
_copyScan(from, to, alloc)
    Scan	from;
    Scan	*to;
    char *	(*alloc)();
{
    Scan	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Scan);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyScanFields(from, newnode, alloc);

    (*to) = newnode;
    return true;
}
    

/* ----------------
 *	_copySeqScan
 * ----------------
 */
bool
_copySeqScan(from, to, alloc)
    SeqScan	from;
    SeqScan	*to;
    char *	(*alloc)();
{
    SeqScan	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(SeqScan);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyScanFields(from, newnode, alloc);

    (*to) = newnode;
    return true;
}
 
/* ----------------
 *	_copyIndexScan
 * ----------------
 */
bool
_copyIndexScan(from, to, alloc)
    IndexScan	from;
    IndexScan	*to;
    char *	(*alloc)();
{
    IndexScan	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(IndexScan);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyScanFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node 
     * ----------------
     */
    Node_Copy(from, newnode, alloc, indxid);
    Node_Copy(from, newnode, alloc, indxqual);
    Node_Copy(from, newnode, alloc, indxstate);
    
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	CopyJoinFields
 *
 *	This function copies the fields of the Join node.  It is used by
 *	all the copy functions for classes which inherit from Join.
 * ----------------
 */
bool CopyJoinFields(from, newnode, alloc)
    Join 	from;
    Join 	newnode;
    char *	(*alloc)();
{
    return true;
}
    

/* ----------------
 *	_copyJoin
 * ----------------
 */
bool
_copyJoin(from, to, alloc)
    Join	from;
    Join	*to;
    char *	(*alloc)();
{
    Join	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Join);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyJoinFields(from, newnode, alloc);

    (*to) = newnode;
    return true;
}
    

/* ----------------
 *	_copyNestLoop
 * ----------------
 */
bool
_copyNestLoop(from, to, alloc)
    NestLoop	from;
    NestLoop	*to;
    char *	(*alloc)();
{
    NestLoop	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(NestLoop);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyJoinFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, nlstate);
    
    (*to) = newnode;
    return true;
}


/* ----------------
 *	_copyMergeJoin
 * ----------------
 */
bool
_copyMergeJoin(from, to, alloc)
    MergeJoin	from;
    MergeJoin	*to;
    char *	(*alloc)();
{
    MergeJoin	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(MergeJoin);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyJoinFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, mergeclauses);
    newnode->mergesortop = from->mergesortop;
    Node_Copy(from, newnode, alloc, mergerightorder);
    Node_Copy(from, newnode, alloc, mergeleftorder);
    Node_Copy(from, newnode, alloc, mergestate);
    
    (*to) = newnode;
    return true;
}
    

/* ----------------
 *	_copyHashJoin
 * ----------------
 */
bool
_copyHashJoin(from, to, alloc)
    HashJoin	from;
    HashJoin	*to;
    char *	(*alloc)();
{
    HashJoin	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(HashJoin);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyJoinFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, hashclauses);
    newnode->hashjoinop = from->hashjoinop;
    Node_Copy(from, newnode, alloc, hashjoinstate);
    
    (*to) = newnode;
    return true;
}
    

/* ----------------
 *	CopyTempFields
 *
 *	This function copies the fields of the Temp node.  It is used by
 *	all the copy functions for classes which inherit from Temp.
 * ----------------
 */
bool CopyTempFields(from, newnode, alloc)
    Temp 	from;
    Temp 	newnode;
    char *	(*alloc)();
{
    newnode->tempid = from->tempid;
    newnode->keycount = from->keycount;
    return true;
}
    

/* ----------------
 *	_copyTemp
 * ----------------
 */
bool
_copyTemp(from, to, alloc)
    Temp	from;
    Temp	*to;
    char *	(*alloc)();
{
    Temp	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Temp);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyTempFields(from, newnode, alloc);

    (*to) = newnode;
    return true;
}
 
/* ----------------
 *	_copyMaterial
 * ----------------
 */
bool
_copyMaterial(from, to, alloc)
    Material	from;
    Material	*to;
    char *	(*alloc)();
{
    Material	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Material);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyTempFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, matstate);
    
    (*to) = newnode;
    return true;
}
    
 
/* ----------------
 *	_copySort
 * ----------------
 */
bool
_copySort(from, to, alloc)
    Sort	from;
    Sort	*to;
    char *	(*alloc)();
{
    Sort	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Sort);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyTempFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, sortstate);
    
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	_copyUnique
 * ----------------
 */
bool
_copyUnique(from, to, alloc)
    Unique	from;
    Unique	*to;
    char *	(*alloc)();
{
    Unique	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Unique);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyTempFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, uniquestate);
    
    (*to) = newnode;
    return true;
}
    
 
/* ----------------
 *	_copyHash
 * ----------------
 */
bool
_copyHash(from, to, alloc)
    Hash	from;
    Hash	*to;
    char *	(*alloc)();
{
    Hash	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Hash);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPlanFields(from, newnode, alloc);
    CopyTempFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, hashstate);
    
    (*to) = newnode;
    return true;
}

/* ****************************************************************
 *		       primnodes.h copy functions
 * ****************************************************************
 */

/* ----------------
 *	_copyResdom
 * ----------------
 */
bool
_copyResdom(from, to, alloc)
    Resdom	from;
    Resdom	*to;
    char *	(*alloc)();
{
    Resdom	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Resdom);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     *
     * XXX reskeyop is USED as an int within the print functions
     *	    so that's what the copy function uses, but its
     *	    declared as an OperatorTupleForm in primnodes.h
     *      One or the other is wrong, and I'm assuming
     *	    it should be an int.  -cim 5/1/90
     * ----------------
     */
    newnode->resno   = from->resno;
    newnode->restype = from->restype;
    newnode->reslen  = from->reslen;

    if (from->resname != NULL)
	newnode->resname = (Name)
	    strcpy((char *) (*alloc)(strlen(from->resname)+1), from->resname);
    else
	newnode->resname = (Name) NULL;
    
    newnode->reskey  = from->reskey;
    newnode->reskeyop = from->reskeyop; /* USED AS AN INT (see above) */
    
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	CopyExprFields
 *
 *	This function copies the fields of the Expr node.  It is used by
 *	all the copy functions for classes which inherit from Expr.
 * ----------------
 */
bool CopyExprFields(from, newnode, alloc)
    Expr 	from;
    Expr 	newnode;
    char *	(*alloc)();
{
    return true;
}

/* ----------------
 *	_copyExpr
 * ----------------
 */
bool
_copyExpr(from, to, alloc)
    Expr	from;
    Expr	*to;
    char *	(*alloc)();
{
    Expr	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Expr);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyExprFields(from, newnode, alloc);

    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyVar
 * ----------------
 */
bool
_copyVar(from, to, alloc)
    Var	from;
    Var	*to;
    char *	(*alloc)();
{
    Var	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Var);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyExprFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->varno = 	 from->varno;
    newnode->varattno =  from->varattno;
    newnode->vartype = 	 from->vartype;
    
    Node_Copy(from, newnode, alloc, vardotfields);
    
    newnode->vararrayindex = from->vararrayindex;
    
    Node_Copy(from, newnode, alloc, varid);    

    newnode->varelemtype = from->varelemtype;
    
    (*to) = newnode;
    return true;
}


/* ----------------
 *	_copyOper
 * ----------------
 */
bool
_copyOper(from, to, alloc)
    Oper	from;
    Oper	*to;
    char *	(*alloc)();
{
    Oper	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Oper);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyExprFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->opno = 	   from->opno;
    newnode->oprelationlevel = from->oprelationlevel;
    newnode->opresulttype =    from->opresulttype;
    newnode->opsize = 	   from->opsize;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyConst
 * ----------------
 */
bool
_copyConst(from, to, alloc)
    Const	from;
    Const	*to;
    char *	(*alloc)();
{
    static ObjectId 	cached_type;
    static bool		cached_typbyval;

    Const	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Const);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyExprFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->consttype = 	from->consttype;
    newnode->constlen = 	from->constlen;

    /* ----------------
     *	XXX super cheesy hack until parser/planner
     *  puts in the right values here.
     * ----------------
     */
    if (cached_type != from->consttype) {
	HeapTuple	typeTuple;
	TypeTupleForm	typeStruct;
	
	/* ----------------
	 *   get the type tuple corresponding to the paramList->type,
	 *   If this fails, returnValue has been pre-initialized
	 *   to "null" so we just return it.
	 * ----------------
	 */
	typeTuple = SearchSysCacheTuple(TYPOID,
					from->consttype,
					NULL,
					NULL,
					NULL);

	/* ----------------
	 *   get the type length and by-value from the type tuple and
	 *   save the information in our one element cache.
	 * ----------------
	 */
	Assert(PointerIsValid(typeTuple));
	
	typeStruct = (TypeTupleForm) GETSTRUCT(typeTuple);
	cached_typbyval = (typeStruct)->typbyval ? true : false ;
	cached_type = from->consttype;
    }
    
    from->constbyval = cached_typbyval;
    
    /* ----------------
     *	copying the Datum in a const node is a bit trickier
     *  because it might be a pointer and it might also be of
     *  variable length...
     * ----------------
     */
    if (from->constbyval == true) {
	/* ----------------
	 *  passed by value so just copy the datum.
	 * ----------------
	 */
	newnode->constvalue = 	from->constvalue;
    } else {
	/* ----------------
	 *  not passed by value. datum contains a pointer.
	 * ----------------
	 */
	if (from->constlen != -1) {
	    /* ----------------
	     *	fixed length structure
	     * ----------------
	     */
	    newnode->constvalue = PointerGetDatum((*alloc)(from->constlen));
	    bcopy(from->constvalue, newnode->constvalue, from->constlen);
	} else {
	    /* ----------------
	     *	variable length structure.  here the length is stored
	     *  in the first int pointed to by the constval.
	     * ----------------
	     */
	    int length;
	    length = *((int *) newnode->constvalue);
	    newnode->constvalue = PointerGetDatum((*alloc)(length));
	    bcopy(from->constvalue, newnode->constvalue, length);
	}
    }
    newnode->constisnull = 	from->constisnull;
    newnode->constbyval = 	from->constbyval;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyParam
 * ----------------
 */
bool
_copyParam(from, to, alloc)
    Param	from;
    Param	*to;
    char *	(*alloc)();
{
    Param	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Param);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyExprFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->paramkind = from->paramkind;
    newnode->paramid = from->paramid;

    if (from->paramname != NULL)
	newnode->paramname = (Name)
	    strcpy((char *) (*alloc)(strlen(from->paramname)+1),
		   from->paramname);
    else
	newnode->paramname = (Name) NULL;
    
    newnode->paramtype = from->paramtype;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyFunc
 * ----------------
 */
bool
_copyFunc(from, to, alloc)
    Func	from;
    Func	*to;
    char *	(*alloc)();
{
    Func	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Func);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyExprFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->funcid = 	from->funcid;
    newnode->functype = 	from->functype;
    newnode->funcisindex = 	from->funcisindex;
    newnode->funcsize = 	from->funcsize;
    
    (*to) = newnode;
    return true;
}

/* ****************************************************************
 *			relation.h copy functions
 * ****************************************************************
 */

/* ----------------
 *	_copyRel
 * ----------------
 */
bool
_copyRel(from, to, alloc)
    Rel	from;
    Rel	*to;
    char *	(*alloc)();
{
    Rel	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Rel);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, relids);
    
    newnode->indexed = from->indexed;
    newnode->pages =   from->pages;
    newnode->tuples =  from->tuples;
    newnode->size =    from->size;
    newnode->width =   from->width;

    Node_Copy(from, newnode, alloc, targetlist);
    Node_Copy(from, newnode, alloc, pathlist);
    Node_Copy(from, newnode, alloc, unorderedpath);
    Node_Copy(from, newnode, alloc, cheapestpath);
    Node_Copy(from, newnode, alloc, classlist);
    Node_Copy(from, newnode, alloc, indexkeys);
    Node_Copy(from, newnode, alloc, ordering);
    Node_Copy(from, newnode, alloc, clauseinfo);
    Node_Copy(from, newnode, alloc, joininfo);
    Node_Copy(from, newnode, alloc, innerjoin);
    Node_Copy(from, newnode, alloc, superrels);
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copySortKey
 * ----------------
 */
bool
_copySortKey(from, to, alloc)
    SortKey	from;
    SortKey	*to;
    char *	(*alloc)();
{
    SortKey	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(SortKey);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, varkeys);
    Node_Copy(from, newnode, alloc, sortkeys);
    Node_Copy(from, newnode, alloc, relid);
    Node_Copy(from, newnode, alloc, sortorder);

    (*to) = newnode;
    return true;
}

/* ----------------
 *	CopyPathFields
 *
 *	This function copies the fields of the Path node.  It is used by
 *	all the copy functions for classes which inherit from Path.
 * ----------------
 */
bool CopyPathFields(from, newnode, alloc)
    Path 	from;
    Path 	newnode;
    char *	(*alloc)();
{
    newnode->pathtype =  from->pathtype;
    Node_Copy(from, newnode, alloc, parent);
    newnode->path_cost = from->path_cost;
    Node_Copy(from, newnode, alloc, p_ordering);
    Node_Copy(from, newnode, alloc, keys);
    Node_Copy(from, newnode, alloc, sortpath);
    newnode->outerjoincost = from->outerjoincost;
    Node_Copy(from, newnode, alloc, joinid);
	
    return true;
}

/* ----------------
 *	_copyPath
 * ----------------
 */
bool
_copyPath(from, to, alloc)
    Path	from;
    Path	*to;
    char *	(*alloc)();
{
    Path	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(Path);
    
    /* ----------------
     *	copy the node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPathFields(from, newnode, alloc);
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyIndexPath
 * ----------------
 */
bool
_copyIndexPath(from, to, alloc)
    IndexPath	from;
    IndexPath	*to;
    char *	(*alloc)();
{
    IndexPath	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(IndexPath);
    
    /* ----------------
     *	copy the node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPathFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, indexid);
    Node_Copy(from, newnode, alloc, indexqual);

    (*to) = newnode;
    return true;
}

/* ----------------
 *	CopyJoinPathFields
 *
 *	This function copies the fields of the JoinPath node.  It is used by
 *	all the copy functions for classes which inherit from JoinPath.
 * ----------------
 */
bool CopyJoinPathFields(from, newnode, alloc)
    JoinPath 	from;
    JoinPath 	newnode;
    char *	(*alloc)();
{
    Node_Copy(from, newnode, alloc, pathclauseinfo);
    Node_Copy(from, newnode, alloc, outerjoinpath);
    Node_Copy(from, newnode, alloc, innerjoinpath);
    
    return true;
}

/* ----------------
 *	_copyJoinPath
 * ----------------
 */
bool
_copyJoinPath(from, to, alloc)
    JoinPath	from;
    JoinPath	*to;
    char *	(*alloc)();
{
    JoinPath	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(JoinPath);
    
    /* ----------------
     *	copy the node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPathFields(from, newnode, alloc);
    CopyJoinPathFields(from, newnode, alloc);
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyMergePath
 * ----------------
 */
bool
_copyMergePath(from, to, alloc)
    MergePath	from;
    MergePath	*to;
    char *	(*alloc)();
{
    MergePath	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(MergePath);
    
    /* ----------------
     *	copy the node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPathFields(from, newnode, alloc);
    CopyJoinPathFields(from, newnode, alloc);

    /* ----------------
     *	copy the remainder of the node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, path_mergeclauses);
    Node_Copy(from, newnode, alloc, outersortkeys);
    Node_Copy(from, newnode, alloc, innersortkeys);
	
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyHashPath
 * ----------------
 */
bool
_copyHashPath(from, to, alloc)
    HashPath	from;
    HashPath	*to;
    char *	(*alloc)();
{
    HashPath	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(HashPath);
    
    /* ----------------
     *	copy the node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyPathFields(from, newnode, alloc);
    CopyJoinPathFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, path_hashclauses);
    Node_Copy(from, newnode, alloc, outerhashkeys);
    Node_Copy(from, newnode, alloc, innerhashkeys);
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyOrderKey
 * ----------------
 */
bool
_copyOrderKey(from, to, alloc)
    OrderKey	from;
    OrderKey	*to;
    char *	(*alloc)();
{
    OrderKey	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(OrderKey);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->attribute_number = 	from->attribute_number;
    newnode->array_index = 		from->array_index;
    
    (*to) = newnode;
    return true;
}
    

/* ----------------
 *	_copyJoinKey
 * ----------------
 */
bool
_copyJoinKey(from, to, alloc)
    JoinKey	from;
    JoinKey	*to;
    char *	(*alloc)();
{
    JoinKey	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(JoinKey);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, outer);
    Node_Copy(from, newnode, alloc, inner);
    
    (*to) = newnode;
    return true;
}
   
/* ----------------
 *	_copyMergeOrder
 * ----------------
 */
bool
_copyMergeOrder(from, to, alloc)
    MergeOrder	from;
    MergeOrder	*to;
    char *	(*alloc)();
{
    MergeOrder	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(MergeOrder);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->join_operator = 	from->join_operator;
    newnode->left_operator = 	from->left_operator;
    newnode->right_operator = 	from->right_operator;
    newnode->left_type = 		from->left_type;
    newnode->right_type = 		from->right_type;
    
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	_copyCInfo
 * ----------------
 */
bool
_copyCInfo(from, to, alloc)
    CInfo	from;
    CInfo	*to;
    char *	(*alloc)();
{
    CInfo	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(CInfo);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, clause);
    newnode->selectivity = 	from->selectivity;
    newnode->notclause = 	from->notclause;
    Node_Copy(from, newnode, alloc, indexids);
    Node_Copy(from, newnode, alloc, mergesortorder);
    Node_Copy(from, newnode, alloc, hashjoinoperator);
    Node_Copy(from, newnode, alloc, cinfojoinid);
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	CopyJoinMethodFields
 *
 *	This function copies the fields of the JoinMethod node.  It is used by
 *	all the copy functions for classes which inherit from JoinMethod.
 * ----------------
 */
bool CopyJoinMethodFields(from, newnode, alloc)
    JoinMethod 	from;
    JoinMethod 	newnode;
    char *	(*alloc)();
{
    Node_Copy(from, newnode, alloc, jmkeys);
    Node_Copy(from, newnode, alloc, clauses);
    return true;
}
    
/* ----------------
 *	_copyJoinMethod
 * ----------------
 */
bool
_copyJoinMethod(from, to, alloc)
    JoinMethod	from;
    JoinMethod	*to;
    char *	(*alloc)();
{
    JoinMethod	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(JoinMethod);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyJoinMethodFields(from, newnode, alloc);

    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	_copyHInfo
 * ----------------
 */
bool
_copyHInfo(from, to, alloc)
    HInfo	from;
    HInfo	*to;
    char *	(*alloc)();
{
    HInfo	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(HInfo);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyJoinMethodFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->hashop = from->hashop;
    
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	_copyMInfo
 * ----------------
 */
bool
_copyMInfo(from, to, alloc)
    MInfo	from;
    MInfo	*to;
    char *	(*alloc)();
{
    MInfo	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(MInfo);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);
    CopyJoinMethodFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, m_ordering);
    
    (*to) = newnode;
    return true;
}
    
/* ----------------
 *	_copyJInfo
 * ----------------
 */
bool
_copyJInfo(from, to, alloc)
    JInfo	from;
    JInfo	*to;
    char *	(*alloc)();
{
    JInfo	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(JInfo);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, otherrels);
    Node_Copy(from, newnode, alloc, jinfoclauseinfo);
    newnode->mergesortable = from->mergesortable;
    newnode->hashjoinable =  from->hashjoinable;
    newnode->inactive =      from->inactive;
    
    (*to) = newnode;
    return true;
}

/* ****************
 *	     execnodes.h routines have no copy functions
 * ****************
 */

/* ****************
 *	      mnodes.h routines have no copy functions
 * ****************
 */

/* ****************************************************************
 *		    pg_lisp.h copy functions
 * ****************************************************************
 */

/* ----------------
 *	_copyLispValue
 * ----------------
 */
bool
_copyLispValue(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    elog(NOTICE, "_copyLispValue: LispValue - assuming LispList");
    Node_Copy(from, newnode, alloc, val.car);
    Node_Copy(from, newnode, alloc, cdr);

    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyLispSymbol
 * ----------------
 */
bool
_copyLispSymbol(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->val.name = from->val.name;
    newnode->cdr = LispNil;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyLispStr
 * ----------------
 */
bool
_copyLispStr(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    if (from->val.name != NULL)
	newnode->val.name = (char *)
	    strcpy((char *) (*alloc)(strlen(from->val.name)+1),
		   from->val.name);
    else
	newnode->val.name = NULL;
    
    newnode->cdr = LispNil;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyLispInt
 * ----------------
 */
bool
_copyLispInt(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->val.fixnum = from->val.fixnum;
    newnode->cdr = LispNil;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyLispFloat
 * ----------------
 */
bool
_copyLispFloat(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    newnode->val.flonum = from->val.flonum;
    newnode->cdr = LispNil;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyLispVector
 * ----------------
 */
bool
_copyLispVector(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    if (from->val.veci == NULL)
	newnode->val.veci == NULL;
    else {
	/* ----------------
	 *  XXX am unsure about meaning of "size" field of vectori
	 *  structure: is it the size of the data array or the size
	 *  of the whole structure.  The code below will work in
	 *  either case -- the worst that will happen is we'll waste
	 *  a few bytes. -cim 4/30/90
	 * ----------------
	 */
	newnode->val.veci = (struct vectori *)
	    (*alloc)(sizeof(struct vectori) + from->val.veci->size);
	newnode->val.veci->size = from->val.veci->size;
	bcopy(from->val.veci->data,
	      newnode->val.veci->data,
	      from->val.veci->size);
    } 
    newnode->cdr = LispNil;
    
    (*to) = newnode;
    return true;
}

/* ----------------
 *	_copyLispList
 * ----------------
 */
bool
_copyLispList(from, to, alloc)
    LispValue	from;
    LispValue	*to;
    char *	(*alloc)();
{
    LispValue	newnode;

    COPY_CHECKARGS();
    COPY_CHECKNULL();
    COPY_NEW(LispValue);
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, val.car);
    Node_Copy(from, newnode, alloc, cdr);
    
    (*to) = newnode;
    return true;
}
