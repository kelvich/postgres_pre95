/*
 * primnodes.h --
 *	Definitions for parse tree/query tree ("primitive") nodes.
 *
 * Identification:
 *	$Header$
 *
 * NOTES:
 *	For exhaustive explanations of what the structure members mean,
 *	see ~postgres/doc/query-tree.t.
 */

#ifndef PrimNodesIncluded
#define	PrimNodesIncluded

#include "nodes.h"	/* bogus inheritance system */
#include "attnum.h"
#include "oid.h"
#include "name.h"
#include "cat.h"
#define List LispValue

/*
 * ============
 * Resdom nodes
 * ============
 */

class (Resdom) public (Node) {
 /* private: */
	inherits(Node);
	AttributeNumber		resno;
	ObjectId		restype;
	Size			reslen;
	Name			resname;
	Index			reskey;
	OperatorTupleForm	reskeyop; 
 /* public: */
};

/*
 * ==========
 * Expr nodes
 * ==========
 */

class (Expr) public (Node) {
#define	ExprDefs \
	inherits(Node)
 /* private: */
	ExprDefs;
 /* public: */
};


/*
 * Var
 *	varno - 
 *	varattno - 
 *	vartype -
 *	vardotfields - 
 *	
 */

class (Var) public (Expr) {
 /* private: */
	inherits(Expr);
	Index			varno;
	AttributeNumber		varattno;
	ObjectId		vartype;
	List			vardotfields;
	Index			vararrayindex;
	List			varid;
 /* public: */
};


/*
 * Oper
 *	opno - at exit from parser: PG_OPERATOR OID of the operator
 *	       at exit from planner: PG_FUNCTION OID for the operator
 *	oprelationlevel - true iff the operator is relation-level
 *	opresulttype - PG_TYPE OID of the operator's return value
 */

class (Oper) public (Expr) {
 /* private: */
	inherits(Expr);
	ObjectId		opno;
	bool			oprelationlevel;
	ObjectId		opresulttype;
 /* public: */
};


/*
 * Const
 *	consttype - PG_TYPE OID of the constant's value
 *	constlen - length in bytes of the constant's value
 *	constvalue - the constant's value
 *	constisnull - whether the constant is null 
 *		(if true, the other fields are undefined)
 */

class (Const) public (Expr) {
 /* private: */
	inherits(Expr);
	ObjectId		consttype;
	Size			constlen;
	Datum			constvalue;
	bool			constisnull;
 /* public: */
};


/*
 * Param
 *	paramid - numeric identifier for literal-constant parameters ("$1")
 *	paramtype - attribute name for tuple-substitution parameters ("$.foo")
 *		(valid iff paramid == 0)
 *	paramtype - PG_TYPE OID of the parameter's value
 */

class (Param) public (Expr) {
 /* private: */
	inherits(Expr);
	int32			paramid;
	Name			paramname;
	ObjectId		paramtype;
 /* public: */
};


/*
 * Func
 *	funcid - PG_FUNCTION OID of the function
 *	functype - PG_TYPE OID of the function's return value
 *	funcisindex - the function can be evaluated by scanning an index
 *		(set during query optimization)
 */

class (Func) public (Expr) {
 /* private: */
	inherits(Expr);
	ObjectId		funcid;
	ObjectId		functype;
	bool			funcisindex;
 /* public: */
};

#endif /* PrimNodesIncluded */
