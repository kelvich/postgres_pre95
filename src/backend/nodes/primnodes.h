/* ----------------------------------------------------------------
 *   FILE
 *	primnodes.h
 *	
 *   DESCRIPTION
 *	Definitions for parse tree/query tree ("primitive") nodes.
 *
 *   NOTES
 *	this file is listed in lib/Gen/inherits.sh and in the
 *	INH_SRC list in conf/inh.mk and is used to generate the
 *	obj/lib/C/primnodes.c file
 *
 *	For explanations of what the structure members mean,
 *	see ~postgres/doc/query-tree.t.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PrimNodesIncluded
#define	PrimNodesIncluded

#include "tmp/postgres.h"

#include "access/att.h"
#include "access/attnum.h"
#include "storage/buf.h"
#include "utils/rel.h"
#include "utils/fcache.h"
#include "rules/params.h"

#include "nodes/nodes.h"	/* bogus inheritance system */
#include "nodes/pg_lisp.h"
#include "primnodes.gen"

/* ----------------------------------------------------------------
 *	Node Function Declarations
 *	
 *  All of these #defines indicate that we have written print/equal/copy
 *  support for the classes named.  The print routines are in
 *  lib/C/printfuncs.c, the equal functions are in lib/C/equalfincs.c and
 *  the copy functions can be found in lib/C/copyfuncs.c
 *
 *  An interface routine is generated automatically by Gen_creator.sh for
 *  each node type.  This routine will call either do nothing or call
 *  an _print, _equal or _copy function defined in one of the above
 *  files, depending on whether or not the appropriate #define is specified.
 *
 *  Thus, when adding a new node type, you have to add a set of
 *  _print, _equal and _copy functions to the above files and then
 *  add some #defines below.
 *
 *  This is pretty complicated, and a better-designed system needs to be
 *  implemented.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	Node Out Function declarations
 * ----------------
 */
#define	OutResdomExists
#define	OutExprExists
#define	OutParamExists
#define	OutFuncExists
#define	OutOperExists
#define OutConstExists
#define OutVarExists
#define OutArrayExists
#define OutArrayRefExists
#define OutFjoinExists

/* ----------------
 *	Node Equal Function declarations
 * ----------------
 */
#define	EqualResdomExists
#define	EqualExprExists
#define	EqualParamExists
#define	EqualFuncExists
#define	EqualOperExists
#define EqualConstExists
#define EqualVarExists
#define EqualArrayExists
#define EqualArrayRefExists
#define EqualFjoinExists

/* ----------------
 *	Node Copy Function declarations
 * ----------------
 */
#define	CopyResdomExists
#define	CopyExprExists
#define	CopyParamExists
#define	CopyFuncExists
#define	CopyOperExists
#define CopyConstExists
#define CopyVarExists
#define CopyArrayExists
#define CopyArrayRefExists
#define CopyFjoinExists


/* ----------------------------------------------------------------
 *			node definitions
 * ----------------------------------------------------------------
 */

/* ----------------
 * Resdom
 *	resno		- XXX comment me.
 *	restype		- XXX comment me.
 *      rescomplex      - is the restype complex?
 *	reslen		- XXX comment me.
 *	resname		- XXX comment me.
 *	reskey		- XXX comment me.
 *	reskeyop	- XXX comment me.
 *	resjunk		- set to nonzero to eliminate the attribute
 *			  from final target list  e.g., ctid for replace
 *			  and delete
 *
 *	XXX reskeyop is USED as an int within the print functions
 *	    so that's what the copy function uses so if its
 *	    in fact an int, then the OperatorTupleForm declaration
 *	    below is incorrect.  -cim 5/1/90
 * ----------------
 */
class (Resdom) public (Node) {
 /* private: */
	inherits0(Node);
	AttributeNumber		resno;
	ObjectId		restype;
	bool                    rescomplex;
	Size			reslen;
	Name			resname;
	Index			reskey;
	OperatorTupleForm	reskeyop; 
	int			resjunk;
 /* public: */
};

/* -------------
 * Fjoin
 *      initialized	- true if the Fjoin has already been initialized for
 *                        the current target list evaluation
 *	nNodes		- The number of Iter nodes returning sets that the
 *			  node will flatten
 *	outerList	- 1 or more Iter nodes
 *	inner		- exactly one Iter node.  We eval every node in the
 *			  outerList once then eval the inner node to completion
 *			  pair the outerList result vector with each inner
 *			  result to form the full result.  When the inner has
 *			  been exhausted, we get the next outer result vector
 *			  and reset the inner.
 *	results		- The complete (flattened) result vector
 *      alwaysNull	- a null vector to indicate sets with a cardinality of
 *			  0, we treat them as the set {NULL}.
 */
class (Fjoin) public (Node) {
	inherits0(Node);
	bool			fj_initialized;
	int			fj_nNodes;
	List			fj_innerNode;
	DatumPtr		fj_results;
	BoolPtr			fj_alwaysDone;
};

/* ----------------
 * Expr
 * ----------------
 */
class (Expr) public (Node) {
#define	ExprDefs \
	inherits0(Node)
 /* private: */
	ExprDefs;
 /* public: */
};

/* ----------------
 * Var
 *	varno 		- index of this var's relation in the range table
 *	varattno 	- attribute number of this var
 *	vartype 	- pg_type tuple oid for the type of this var
 *	varid 		- cons cell containing (varno, (varattno))
 *	varslot 	- cached pointer to addr of expr cxt slot
 * ----------------
 */
class (Var) public (Expr) {
 /* private: */
	inherits1(Expr);
	Index			varno; 
	AttributeNumber		varattno;
	ObjectId		vartype;
	List			varid;
	Pointer			varslot;
 /* public: */
};

/* ----------------
 * Oper
 *	opno 		- PG_OPERATOR OID of the operator
 *	opid 		- PG_PROC OID for the operator
 *	oprelationlevel - true iff the operator is relation-level
 *	opresulttype 	- PG_TYPE OID of the operator's return value
 *	opsize 		- size of return result (cached by executor)
 *	op_fcache	- XXX comment me.
 *
 * ----
 * NOTE: in the good old days 'opno' used to be both (or either, or
 * neither) the pg_operator oid, and/or the pg_proc oid depending 
 * on the postgres module in question (parser->pg_operator,
 * executor->pg_proc, planner->both), the mood of the programmer,
 * and the phase of the moon (rumors that it was also depending on the day
 * of the week are probably false). To make things even more postgres-like
 * (i.e. a mess) some comments were referring to 'opno' using the name
 * 'opid'. Anyway, now we have two separate fields, and of course that
 * immediately removes all bugs from the code...	[ sp :-) ].
 * ----------------
 */
class (Oper) public (Expr) {
 /* private: */
	inherits1(Expr);
	ObjectId		opno;
	ObjectId		opid;
	bool			oprelationlevel;
	ObjectId		opresulttype;
	int			opsize;
	FunctionCachePtr	op_fcache;
 /* public: */
};


/* ----------------
 * Const
 *	consttype - PG_TYPE OID of the constant's value
 *	constlen - length in bytes of the constant's value
 *	constvalue - the constant's value
 *	constisnull - whether the constant is null 
 *		(if true, the other fields are undefined)
 *	constbyval - whether the information in constvalue
 *		if passed by value.  If true, then all the information
 *		is stored in the datum. If false, then the datum
 *		contains a pointer to the information.
 *      constisset - whether the const represents a set.  The const
 *              value corresponding will be the query that defines
 *              the set.
 * ----------------
 */
class (Const) public (Expr) {
 /* private: */
	inherits1(Expr);
	ObjectId		consttype;
	Size			constlen;
	Datum			constvalue;
	bool			constisnull;
	bool			constbyval;
	bool                    constisset;
 /* public: */
};

/* ----------------
 * Param
 *	paramkind - specifies the kind of parameter. The possible values
 *	for this field are specified in "params.h", and they are:
 *
 * 	PARAM_NAMED: The parameter has a name, i.e. something
 *              like `$.salary' or `$.foobar'.
 *              In this case field `paramname' must be a valid Name.
 *
 *	PARAM_NUM:   The parameter has only a numeric identifier,
 *              i.e. something like `$1', `$2' etc.
 *              The number is contained in the `paramid' field.
 *
 * 	PARAM_NEW:   Used in PRS2 rule, similar to PARAM_NAMED.
 * 	             The `paramname' and `paramid' refer to the "NEW" tuple
 *		     The `pramname' is the attribute name and `paramid'
 *	    	     is the attribute number.
 *
 *	PARAM_OLD:   Same as PARAM_NEW, but in this case we refer to
 *              the "OLD" tuple.
 *
 *	paramid - numeric identifier for literal-constant parameters ("$1")
 *	paramname - attribute name for tuple-substitution parameters ("$.foo")
 *	paramtype - PG_TYPE OID of the parameter's value
 *      param_tlist - allows for projection in a param node.
 * ----------------
 */
class (Param) public (Expr) {
 /* private: */
	inherits1(Expr);
	int			paramkind;
	AttributeNumber		paramid;
	Name			paramname;
	ObjectId		paramtype;
	List                    param_tlist;
 /* public: */
};


/* ----------------
 * Func
 *	funcid 		- PG_FUNCTION OID of the function
 *	functype 	- PG_TYPE OID of the function's return value
 *	funcisindex 	- the function can be evaluated by scanning an index
 *			  (set during query optimization)
 *	funcsize 	- size of return result (cached by executor)
 *	func_fcache 	- runtime state while running this function.  Where
 *                        we are in the execution of the function if it
 *                        returns more than one value, etc.
 *                        See utils/fcache.h
 *      func_tlist      - projection of functions returning tuples
 *      func_planlist   - result of planning this func, if it's a PQ func
 * ----------------
 */
class (Func) public (Expr) {
 /* private: */
	inherits1(Expr);
	ObjectId		funcid;
	ObjectId		functype;
	bool			funcisindex;
	int			funcsize;
	FunctionCachePtr	func_fcache;
	List                    func_tlist;
	List                    func_planlist;
 /* public: */
};

/* ----------------
 * Array
 *	arrayelemtype	- base type of the array's elements (homogenous!)
 *	arrayelemlength	- length of that type
 *	arrayelembyval	- can you pass this element by value?
 *	arrayndim   - number of dimensions of the array
 *	arraylow	- base for array indexing
 *	arrayhigh	- limit for array indexing
 *	arraylen	-
 * ----------------
 *
 *  memo from mao:  the array support we inherited from 3.1 is just
 *  wrong.  when time exists, we should redesign this stuff to get
 *  around a bunch of unfortunate implementation decisions made there.
 */
class (Array) public (Expr) {
 /* private: */
	inherits1(Expr);
	ObjectId		arrayelemtype;
	int				arrayelemlength;
	bool			arrayelembyval;
	int 			arrayndim;
	IntArray		arraylow;
	IntArray		arrayhigh;
	int				arraylen;
 /* public: */
};

/* ----------------
 *  ArrayRef:
 *	refelemtype	- type of the element referenced here
 *	refelemlength	- length of that type
 *	refelembyval	- can you pass this element type by value?
 *	refupperindexpr	- expressions that evaluate to upper array index
 *	reflowerexpr- the expressions that evaluate to a lower array index
 *	refexpr		- the expression that evaluates to an array
 *	refassignexpr- the expression that evaluates to the new value 
 *  to be assigned to the array in case of replace.
 * ----------------
 */
class (ArrayRef) public (Expr) {
 /* private: */
	inherits1(Expr);
	int			refattrlength;
	int			refelemlength;
	ObjectId		refelemtype;
	bool			refelembyval;
	LispValue		refupperindexpr;
	LispValue		reflowerindexpr;
	LispValue		refexpr;
	LispValue		refassgnexpr;
};
#endif /* PrimNodesIncluded */
