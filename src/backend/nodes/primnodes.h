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

/*
 *  These #defines indicate that we've written print support routines for
 *  the named classes.  The print routines are in lib/C/printfuncs.c.  The
 *  automatic inheritance generator builds interface and default routines
 *  that call those in printfuncs.c.
 */

#define	PrintResdomExists
#define	PrintExprExists
#define	PrintParamExists
#define	PrintFuncExists
#define	PrintOperExists
#define PrintConstExists
#define PrintVarExists

extern void	PrintResdom();
extern void	PrintExpr();
extern void	PrintParam();
extern void	PrintFunc();
extern void	PrintOper();
extern void	PrintConst();
extern void	PrintVar();

/*
 *  Same for functions that test nodes for equality.
 */

#define	EqualResdomExists
#define	EqualExprExists
#define	EqualParamExists
#define	EqualFuncExists
#define	EqualOperExists
#define EqualConstExists
#define EqualVarExists

#include "nodes.h"	/* bogus inheritance system */
#include "attnum.h"
#include "oid.h"
#include "name.h"
#include "cat.h"
#include "pg_lisp.h"

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
 * ==========
 * ExprContext nodes
 * ==========
 */

class (ExprContext) public (Node) {
#define ExprContextDefs \
	inherits(Node) \
	List	      ecxt_scantuple; \
	AttributePtr  ecxt_scantype; \
	Buffer	      ecxt_scan_buffer; \
	List	      ecxt_innertuple; \
	AttributePtr  ecxt_innertype; \
	Buffer	      ecxt_inner_buffer; \
	List	      ecxt_outertuple; \
	AttributePtr  ecxt_outertype; \
	Buffer	      ecxt_outer_buffer; \
	Relation      ecxt_relation; \
	Index	      ecxt_relid
 /* private: */
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
 * Bool		Expression whose value is either True or False.
 *
 *	boolvalue - the Bool's value (true or false)
 */

class (Bool) public (Expr) {
 /* private: */
	inherits(Expr);
	bool			boolvalue;
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
