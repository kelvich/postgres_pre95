/* ----------------------------------------------------------------
 *   FILE
 *	ex_qual.c
 *	
 *   DESCRIPTION
 *	Routines to evaluate qualification and targetlist expressions
 *
 *   INTERFACE ROUTINES
 *    	ExecEvalExpr	- evaluate an expression and return a datum
 *    	ExecQual	- return true/false if qualification is satisified
 *    	ExecTargetList	- form a new tuple by projecting the given tuple
 *
 *   NOTES
 *	ExecEvalExpr() and ExecEvalVar() are hotspots.  making these faster
 *	will speed up the entire system.  Unfortunately they are currently
 *	implemented recursively..  Eliminating the recursion is bound to
 *	improve the speed of the executor.
 *
 *	ExecTargetList() is used to make tuple projections.  Rather then
 *	trying to speed it up, the execution plan should be pre-processed
 *	to facilitate attribute sharing between nodes wherever possible,
 *	instead of doing needless copying.  -cim 5/31/91
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/align.h"
#include "parser/parsetree.h"
#include "parser/parse.h" /* for NOT used in macros in ExecEvalExpr */
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "planner/keys.h"
#include "nodes/mnodes.h"
#include "catalog/pg_language.h"
#include "executor/executor.h"
 RcsId("$Header$");
/* ----------------
 *	externs and constants
 * ----------------
 */


/*
 * XXX Used so we can get rid of use of Const nodes in the executor.
 * Currently only used by ExecHashGetBucket and set only by ExecMakeVarConst
 * and by ExecEvalArrayRef.
 */
Boolean execConstByVal;
int 	execConstLen;

/* ----------------
 *	sysattrlen
 *	sysattrbyval
 *
 *	these have been moved to access/tuple/tuple.c
 * ----------------
 */

/* --------------------------------
 *	array_cast
 * --------------------------------
 */
Datum
array_cast(value, byval, len)
    char *value;
    bool byval;
    int len;
{
    if (byval) {
        switch (len) {
	case 1:
	    return((Datum) * value);
	case 2:
	    return((Datum) * (int16 *) value);
	case 3:
	case 4:
	    return((Datum) * (int32 *) value);
	default:
	    elog(WARN, "ExecEvalArrayRef: byval and elt len > 4!");
	    return(NULL);
            break;
        }
    } else
        return (Datum) value;
}

/* --------------------------------
 *    ExecEvalArrayRef
 *
 *     This function takes an ArrayRef and returns a Const Node which
 *     is the actual array reference. It is used to de-reference an array.
 *
 *  This code knows about the internal storage scheme for both fixed-length
 *  arrays (like char16, int28, etc) and for variable length arrays.  It
 *  currently assumes that fixed-length arrays are contiguous blobs of memory
 *  where the first element is in the starting position.  Variable length
 *  arrays are presumed to have the following format:
 *
 *  <length_in_bytes><elem_1><elem_2>...<elem_n>
 *
 *  It checks to see if <length_in_bytes> / <number_of_elements> = typlen
 *  as a sanity check for variable length arrays.
 *
 *     -Greg
 *
 *    XXX this code should someday handle the case where there is
 *        an expression of the form:  a.b[ expression ] rather then
 *        just a.b[ constant ] as it does now.  99% of this can be
 *        done by using ExecEvalExpr() but it would mean some more
 *        work in the parser. -cim 6/20/90
 * --------------------------------
 */
Datum
ExecEvalArrayRef(object, indirection, array_len, element_len, byval, isNull)
    Datum 	object;
    int32	array_len;
    int32	element_len;
    int32	indirection;
    bool	byval;
    Boolean 	*isNull;
{
    int 	i;
    char 	*array_scanner;
    int 	nelems;
    int		bytes;
    int		nbytes;
    int		offset;
    bool 	done = false;
    char 	*retval;
    
    *isNull = 		false;
    array_scanner = 	(char *) object;
    execConstByVal = 	byval;
    
    /*
     * Postgres uses array[1..n] - change to C array syntax of array[0..n-1]
     */
    indirection--;
    
    if (array_len < 0) {
        nbytes = * (int32 *) array_scanner;
        array_scanner += sizeof(int32);
	
        /* variable length array of variable length elts */
        if (element_len < 0) {
            bytes = nbytes - sizeof(int32);
            i = 0;
            while (bytes > 0 && !done) {
                if (i == indirection) {
                    retval = array_scanner;
                    done = true;
                }
                bytes -= LONGALIGN(* (int32 *) array_scanner);
                array_scanner += LONGALIGN(* (int32 *) array_scanner);
                i++;
            }
	    
            if (! done) {
                if (bytes == 0) { /* array[i] does not exist */
                    *isNull = true;
                    return(NULL);
                } else { /* bytes < 0 - error */
                    elog(WARN, "ExecEvalArrayRef: improperly formatted array");
                }
            }
            execConstLen = (int) * (int32 *) retval;
            return (Datum) retval;
	    
        } else {
	    /* variable length array of fixed length elements */
	    
	    nbytes -= sizeof(int32);
            offset = indirection * element_len;
	    
            /*
             * off the end of the array
             */
            if (nbytes - offset < 1) {
                *isNull = true;
                return(NULL);
            }
	    
            execConstLen = element_len;
            retval = array_scanner + offset;
	    
            return
		array_cast(retval, byval, element_len);
        }
	
    } else {
	/* fixed length array of fixed length elements */
	
        if (element_len < 0) {
	    /* fixed length array of variable length elements!???!!?! */
            elog(WARN, "ExecEvalArrayRef: malformed array node");
        }
	
        offset = element_len * indirection;
	
        if (array_len - offset < 1) {
	    *isNull = true;
	    return(NULL);
        }
	
        execConstLen = element_len;
        retval = array_scanner + offset;
        return
	    array_cast(retval, byval, element_len);
    }
}

/* ----------------------------------------------------------------
 *    	ExecEvalVar
 *    
 *    	Returns a Datum whose value is the value of a range
 *	variable with respect to given expression context.
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalVar(variable, econtext, isNull)
    Var 	variable;
    ExprContext econtext;
    Boolean 	*isNull;
{
    Datum	    	result;
    TupleTableSlot  	slot;
    AttributeNumber 	attnum;
    TupleDescriptor    	tupType;
    Buffer	    	tupBuffer;
    List	    	array_info;
    
    HeapTuple		heapTuple;
    TupleDescriptor	tuple_type;
    Buffer 		buffer;
    ObjectId		typeoid;
    bool		byval;
    Pointer		value;
    int16   		len;

    /* ----------------
     *	get the slot we want
     * ----------------
     */
    switch(get_varno(variable)) {
    case INNER: /* get the tuple from the inner node */
	slot = get_ecxt_innertuple(econtext);
	break;
	    
    case OUTER: /* get the tuple from the outer node */
	slot = get_ecxt_outertuple(econtext);
	break;

    default: /* get the tuple from the relation being scanned */
	slot = get_ecxt_scantuple(econtext);
	break;
    } 

    /* ----------------
     *   extract tuple information from the slot
     * ----------------
     */
    heapTuple = (HeapTuple) ExecFetchTuple((Pointer) slot);
    tuple_type = 	    ExecSlotDescriptor((Pointer)slot);
    buffer =		    ExecSlotBuffer((Pointer)slot);
    
    attnum =  	get_varattno(variable);
	    
    /* ----------------
     *	get the attribute our of the tuple.
     * ----------------
     */
    result =  (Datum)
	heap_getattr(heapTuple,	 /* tuple containing attribute */
		     buffer,	 /* buffer associated with tuple */
		     attnum,	 /* attribute number of desired attribute */
		     tuple_type, /* tuple descriptor of tuple */
		     isNull);	 /* return: is attribute null? */

    /* ----------------
     *	return null if att is null
     * ----------------
     */
    if (*isNull)
	return NULL;
    
    /* ----------------
     *   get length and type information..
     *   ??? what should we do about variable length attributes
     *	 - variable length attributes have their length stored
     *     in the first 4 bytes of the memory pointed to by the
     *     returned value.. If we can determine that the type
     *     is a variable length type, we can do the right thing.
     *	   -cim 9/15/89
     * ----------------
     */
    if (attnum < 0) {
	/* ----------------
	 *  If this is a pseudo-att, we get the type and fake the length.
	 *  There ought to be a routine to return the real lengths, so
	 *  we'll mark this one ... XXX -mao
	 * ----------------
	 */
        len = 	sysattrlen(attnum);  	/* XXX see -mao above */
	byval = sysattrbyval(attnum);	/* XXX see -mao above */
    } else {
        len =   tuple_type->data[ attnum-1 ]->attlen;
	byval = tuple_type->data[ attnum-1 ]->attbyval ? true : false ;
    }

    /* ----------------
     * Here we process array indirections.  How it works is ExecEvalArrayRef
     * continuously "refines" result until it is what we really want.  IE,
     * if you have a "text[]" attribute, the call to amgetattr above will
     * get you a attribute of type text[] (or _text).  If we are expanding
     *
     * a[1][2]
     *
     * where the type of a is text[], the first call to ExecEvalArrayRef
     * will return something of type text, which will be a[1].  The next
     * call will return something of type char, which will be a[1][2].
     *
     * A "null result" is if we go off the end of the array.  Note that doing
     * an elog(WARN) is NOT correct behavior - we may be indirecting on a
     * variable length array.
     * ----------------
     */
    array_info = get_vararraylist(variable);
    if (array_info != NULL) {
	List ind_cons;
	Array indirection;
	
        /* -----------------
         * note: ExecEvalArrayRef sets execConstByVal and execConstLen.
         * -----------------
         */
        foreach (ind_cons, array_info) {
	    indirection = (Array) CAR(ind_cons);
            result = ExecEvalArrayRef(result,
                                      get_arraylow(indirection),
                                      get_arraylen(indirection),
                                      get_arrayelemlength(indirection),
                                      get_arrayelembyval(indirection),
                                      isNull);
            if (*isNull)
		return(NULL);
        }
    } else {
        execConstByVal = byval;
        execConstLen = 	 len;
    }

    return result;
}
 
/* ----------------------------------------------------------------
 *      ExecEvalParam
 *
 *      Returns the value of a parameter.  A param node contains
 *	something like ($.name) and the expression context contains
 *	the current parameter bindings (name = "sam") (age = 34)...
 *	so our job is to replace the param node with the datum
 *	containing the appropriate information ("sam").
 *
 *	Q: if we have a parameter ($.foo) without a binding, i.e.
 *	   there is no (foo = xxx) in the parameter list info,
 *	   is this a fatal error or should this be a "not available"
 *	   (in which case we shoud return a Const node with the
 *	    isnull flag) ?  -cim 10/13/89
 *
 *      Minor modification: Param nodes now have an extra field,
 *      `paramkind' which specifies the type of parameter 
 *      (see params.h). So while searching the paramList for
 *      a paramname/value pair, we have also to check for `kind'.
 *     
 *      NOTE: The last entry in `paramList' is always an
 *      entry with kind == PARAM_INVALID.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalParam(expression, econtext)
    Param 	expression;
    ExprContext econtext;
{

    Name 	  	thisParameterName;
    int		  	thisParameterKind;
    AttributeNumber	thisParameterId;
    int 		i;
    int 	  	matchFound;
    Const 		constNode;
    ParamListInfo	paramList;
    
    thisParameterName = get_paramname(expression);
    thisParameterKind = get_paramkind(expression);
    thisParameterId =   get_paramid(expression);
    paramList = 	get_ecxt_param_list_info(econtext);
    
    /*
     * search the list with the parameter info to find a matching name.
     * An entry with an InvalidName denotes the last element in the array.
     */
    matchFound = 0;
    if (paramList != NULL) {
	/*
	 * search for an entry in 'paramList' that matches
	 * the `expression'.
	 */
	while(paramList->kind != PARAM_INVALID && !matchFound) {
	    switch (thisParameterKind) {
		case PARAM_NAMED:
		    if (thisParameterKind == paramList->kind &&
		        NameIsEqual(paramList->name, thisParameterName)){
			    matchFound = 1;
		    } 
		    break;
		case PARAM_NUM:
		    if (thisParameterKind == paramList->kind &&
			paramList->id == thisParameterId) {
			    matchFound = 1;
		    }
		    break;
		case PARAM_OLD:
		case PARAM_NEW:
		    if (thisParameterKind == paramList->kind &&
			paramList->id == thisParameterId)
		    {
			matchFound = 1;
			/*
			 * sanity check
			 */
		        if (! NameIsEqual(paramList->name, thisParameterName)){
			    elog(WARN,
		"ExecEvalParam: new/old params with same id & diff names");
			}
		    }
		    break;
		default:
		    /*
		     * oops! this is not supposed to happen!
		     */
		    elog(WARN, "ExecEvalParam: invalid paramkind %d",
		    thisParameterKind);
	    }
	    if (! matchFound) {
		paramList++;
	    }
	} /*while*/
    } /*if*/
    
    if (!matchFound) {
	/*
	 * ooops! we couldn't find this parameter
	 * in the parameter list. Signal an error
	 */
	elog(WARN, "ExecEvalParam: Unknown value for parameter %s",
	     thisParameterName);
    }
    
    /*
     * return the value.
     */

    return(paramList->value);
}
 

/* ----------------------------------------------------------------
 *    	ExecEvalOper / ExecEvalFunc support routines
 * ----------------------------------------------------------------
 */
 
static HeapTuple  	currentTuple;
static TupleDescriptor 	currentExecutorDesc;
static Buffer      	currentBuffer;
static Relation 	currentRelation;

/* ----------------
  *	ArgumentIsRelation
 *
 *	used in ExecMakeFunctionResult() to see if we need to
 *	call SetCurrentTuple().
 * ----------------
 */
bool
ArgumentIsRelation(arg)
    List arg;
{
    if (arg == NULL)
	return false;

    if (listp(arg) && ExactNodeType(CAR(arg),Var))
	return (bool)
	    (((Var) (CAR(arg)))->vartype == RELATION);
    
    return false;
}

/* ----------------
 *	SetCurrentTuple
 *
 *	used in ExecMakeFunctionResult() to set the current values
 *	for calls to GetAttribute()
 * ----------------
 */
void
SetCurrentTuple(econtext)
    ExprContext  econtext;
{
    TupleTableSlot slot;

    /* ----------------
     *	take a guess at which slot to use for the "current" tuple.
     *  someday this will have to be smarter -cim 5/31/91
     * ----------------
     */
    slot = get_ecxt_innertuple(econtext);
    if (slot == NULL)
	slot = get_ecxt_outertuple(econtext);
    if (slot == NULL)
	slot = get_ecxt_scantuple(econtext);
    
    /* ----------------
     *	if we have a slot, get it's info
     * ----------------
     */
    if (slot != NULL) {
	currentExecutorDesc =   ExecSlotDescriptor((Pointer)slot);
	currentBuffer = 	ExecSlotBuffer((Pointer)slot);
	currentTuple =          (HeapTuple) ExecFetchTuple((Pointer) slot);
    } else {
	currentExecutorDesc =   NULL;
	currentBuffer = 	InvalidBuffer;
	currentTuple =          NULL;
    }
	
    currentRelation = get_ecxt_relation(econtext);
}

/* ----------------
 *	GetAttribute
 *
 *	This is a function which returns the value of the
 *	named attribute out of the "current" tuple.  User defined
 *	C functions which take a tuple as an argument are expected
 *	to use this.  Ex: overpaid(EMP) might call GetAttribute().
 *
 *	The "current" tuple and it's schema information is set by
 *	the executor in advance of any function calls.
 * ----------------
 */
Datum
GetAttribute(attname)
    char *attname;
{
    Datum 		retval;
    AttributeNumber 	attno;
    Boolean 		isnull = (Boolean) false;

    /* ----------------
     *	get attribute number from attname and use it to get
     *  attr value from the "current" tuple.
     * ----------------
     */
    attno = varattno(currentRelation, attname);
    
    retval = (Datum)
	heap_getattr(currentTuple,
		     currentBuffer,
		     attno,
		     currentExecutorDesc,
		     &isnull);
    if (isnull)
	return NULL;
    else
	return retval;
}

/* ----------------
 *	ExecMakeFunctionResult
 * ----------------
 */
Datum
ExecMakeFunctionResult(fcache, arguments, econtext)
    FunctionCachePtr fcache;
    List 	arguments;
     ExprContext econtext;
{
    List	arg;
    Datum	args[MAXFMGRARGS];
    Boolean	isNull;
    int 	i;

    /* ----------------
     *  We need to check CAR(arg) to see if it is a RELATION type, in which
     *  case we need to call the stuff to set up GetAttribute as well as
     *  shift the argument list to handle the special case for tuple arguments.
     * ----------------
     */
    if (ArgumentIsRelation(arguments)) {
	arguments = CDR(arguments);
  	SetCurrentTuple(econtext);
    }

    /* ----------------
     *	arguments is a list of expressions to evaluate
     *  before passing to the function manager.
     *  We collect the results of evaluating the expressions
     *  into a datum array (args) and pass this array to arrayFmgr()
     * ----------------
     */
    if (fcache->nargs != 0) {
	if (fcache->nargs > MAXFMGRARGS) 
	    elog(WARN, "ExecMakeFunctionResult: too many arguments");

   	i = 0;
	foreach (arg, arguments) {
	    /* ----------------
	     *   evaluate the expression
	     * ----------------
	     */
	    args[i++] = (Datum)
		ExecEvalExpr((Node) CAR(arg), econtext, &isNull); 
	}
    }

    /* ----------------
     *   now return the value gotten by calling the function manager, 
     *   passing the function the evaluated parameter values. 
     * ----------------
     */
/* temp fix */
    if (fmgr_func_lang(fcache->foid) == POSTQUELlanguageId)
	return (Datum) fmgr_array_args(fcache->foid, fcache->nargs, args);
    else 
	return (Datum)
	    fmgr_by_ptr_array_args(fcache->func, fcache->nargs, args);
}

/* ----------------------------------------------------------------
 *    	ExecEvalOper
 *    	ExecEvalFunc
 *    
 *    	Evaluate the functional result of a list of arguments by calling the
 *    	function manager.  Note that in the case of operator expressions, the
 *    	optimizer had better have already replaced the operator OID with the
 *    	appropriate function OID or we're hosed.
 *
 * old comments
 *    	Presumably the function manager will not take null arguments, so we
 *    	check for null arguments before sending the arguments to (fmgr).
 *    
 *    	Returns the value of the functional expression.
 * ----------------------------------------------------------------
 */
  
/* ----------------------------------------------------------------
 *	ExecEvalOper
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEvalExpr
 ****/

Datum
ExecEvalOper(opClause, econtext, isNull)
    List        opClause;
    ExprContext econtext;
    Boolean 	*isNull;
{
    Oper	op; 
    List	argList;
    FunctionCachePtr fcache;
    
    /* ----------------
     *  an opclause is a list (op args).  (I think)
     *
     *  we extract the oid of the function associated with
     *  the op and then pass the work onto ExecMakeFunctionResult
     *  which evaluates the arguments and returns the result of
     *  calling the function on the evaluated arguments.
     * ----------------
     */
    op = 	(Oper) get_op(opClause);
    argList =   (List) get_opargs(opClause);

    /*
     * get the fcache from the Oper node.
     * If it is NULL, then initialize it
     */
    fcache = get_op_fcache(op);
    if (fcache == NULL) {
        set_fcache(op, get_opid(op));
    	fcache = get_op_fcache(op);
    }
    
    return
	ExecMakeFunctionResult(fcache, argList, econtext);
}
 
/* ----------------------------------------------------------------
 *	ExecEvalFunc
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalFunc(funcClause, econtext, isNull)
    Func        funcClause;
    ExprContext econtext;
    Boolean 	*isNull;
{
    Func	func;
    List	argList;
    FunctionCachePtr fcache;
    
    /* ----------------
     *  an funcclause is a list (func args).  (I think)
     *
     *  we extract the oid of the function associated with
     *  the func node and then pass the work onto ExecMakeFunctionResult
     *  which evaluates the arguments and returns the result of
     *  calling the function on the evaluated arguments.
     *
     *  this is nearly identical to the ExecEvalOper code.
     * ----------------
     */
    func =	(Func) get_function(funcClause);
    argList =   (List) get_funcargs(funcClause);

    /*
     * get the fcache from the Func node.
     * If it is NULL, then initialize it
     */
    fcache = get_func_fcache(func);
    if (fcache == NULL) {
    	set_fcache(func, get_funcid(func));
    	fcache = get_func_fcache(func);
    }

    return
	ExecMakeFunctionResult(fcache, argList, econtext);
}
 
/* ----------------------------------------------------------------
 *    	ExecEvalNot
 *    	ExecEvalOr
 *    
 *    	Evaluate boolean expressions.  Evaluation of 'or' is
 *	short-circuited when the first true (or null) value is found.
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalNot(notclause, econtext, isNull)
    List 	notclause;
    ExprContext econtext;
    Boolean 	*isNull;
{
    Datum expr_value;
    Datum const_value;
    List  clause;

    clause = 	 (List) get_notclausearg(notclause);
    expr_value = ExecEvalExpr((Node) clause, econtext, isNull);
 
    /* ----------------
     *	if the expression evaluates to null, then we just
     *  cascade the null back to whoever called us.
     * ----------------
     */
    if (*isNull)
	return expr_value;
    
    /* ----------------
     *  evaluation of 'not' is simple.. expr is false, then
     *  return 'true' and vice versa.
     * ----------------
     */
    if (ExecCFalse(DatumGetInt32(expr_value)))
	return (Datum) true;
   
    return (Datum) false;
}
 
/* ----------------------------------------------------------------
 *	ExecEvalOr
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalOr(orExpr, econtext, isNull)
    List 	orExpr;
    ExprContext econtext;
	Boolean *isNull;
{
    List   clauses;
    List   clause;
    Datum  const_value;
 
    clauses = (List) get_orclauseargs(orExpr);
 
    /* ----------------
     *	we evaluate each of the clauses in turn,
     *  as soon as one is true we return that
     *  value.  If none are true then we return
     *  the value of the last clause evaluated, which
     *  should be false. -cim 8/31/89
     * ----------------
     */
    foreach (clause, clauses) {
	const_value = ExecEvalExpr((Node) CAR(clause), econtext, isNull);
	
	/* ----------------
	 *  if the expression evaluates to null, then we just
	 *  cascade the null back to whoever called us.
	 * ----------------
	 */
	if (*isNull)
	    return const_value;
	
	/* ----------------
	 *   if we have a non-false result, then we return it.
	 * ----------------
	 */
	if (! ExecCFalse(DatumGetInt32(const_value)))
	    return const_value;
    }
    
    return const_value;
}
 
/* ----------------------------------------------------------------  
 *    	ExecEvalExpr
 *    
 *    	Recursively evaluate a targetlist or qualification expression.
 *
 *    	This routine is an inner loop routine and should be as fast
 *      as possible.
 *
 *      Node comparison functions were replaced by macros for speed and to plug
 *      memory leaks incurred by using the planner's Lispy stuff for
 *      comparisons.  Order of evaluation of node comparisons IS IMPORTANT;
 *      the macros do no checks.  Order of evaluation:
 *      
 *      o an isnull check, largely to avoid coredumps since greg doubts this
 *        routine is called with a null ptr anyway in proper operation, but is
 *        not completely sure...
 *      o ExactNodeType checks.
 *      o clause checks or other checks where we look at the CAR of something.
 * ----------------------------------------------------------------
 */
Datum
ExecEvalExpr(expression, econtext, isNull)
    Node 	expression;
    ExprContext econtext;
    Boolean 	*isNull;
{
    Datum retDatum;

    *isNull = false;

    /* ----------------
     *	here we dispatch the work to the appropriate type
     *  of function given the type of our expression.
     * ----------------
    */
    if (expression == NULL) {
	*isNull = true;
	retDatum = (Datum) true;
    } else if (ExactNodeType(expression,Var))
	retDatum = (Datum) ExecEvalVar((Var) expression, econtext, isNull);
   
    else if (ExactNodeType(expression,Const)) {
	if (get_constisnull((Const) expression))
	    *isNull = true;

    	retDatum = get_constvalue((Const) expression);
    }
	
    else if (ExactNodeType(expression,Param))
	retDatum = (Datum)  ExecEvalParam((Param) expression, econtext);
   
    else if (fast_is_clause(expression))	/* car is a Oper node */
	retDatum = (Datum) ExecEvalOper((List) expression, econtext, isNull);
    
    else if (fast_is_funcclause(expression)) 	/* car is a Func node */
	retDatum = (Datum) ExecEvalFunc((Func) expression, econtext, isNull);
      
    else if (fast_or_clause(expression))
	retDatum = (Datum) ExecEvalOr((List) expression, econtext, isNull);
   
    else if (fast_not_clause(expression))
	retDatum = (Datum)  ExecEvalNot((List) expression, econtext, isNull);
	
    else
	elog(WARN, "ExecEvalExpr: unknown expression type");

    return retDatum;
}

/* ----------------------------------------------------------------
 *		     ExecQual / ExecTargetList 
 * ----------------------------------------------------------------
 */

/* ----------------------------------------------------------------
 *    	ExecQualClause
 *
 *	this is a workhorse for ExecQual.  ExecQual has to deal
 *	with a list of qualifications, so it passes each qualification
 *	in the list to this function one at a time.  ExecQualClause
 *	returns true when the qualification *fails* and false if
 *	the qualification succeeded (meaning we have to test the
 *	rest of the qualification)
 * ----------------------------------------------------------------
 */
/**** xxref:
 *           ExecQual
 ****/
bool
ExecQualClause(clause, econtext)
    List 	clause;
    ExprContext econtext;
{
    Datum   expr_value;
    Datum   const_value;
    Boolean isNull;

    expr_value = (Datum)
	ExecEvalExpr((Node) clause, econtext, &isNull);
    
    /* ----------------
     *	this is interesting behaviour here.  When a clause evaluates
     *  to null, then we consider this as passing the qualification.
     *  it seems kind of like, if the qual is NULL, then there's no
     *  qual.. 
     * ----------------
     */
    if (isNull)
	return true;
    
    /* ----------------
     *  remember, we return true when the qualification fails..
     * ----------------
     */
    if (ExecCFalse(DatumGetInt32(expr_value)))
	return true;
    
    return false;
}
 
/* ----------------------------------------------------------------
 *    	ExecQual
 *    
 *    	Evaluates a conjunctive boolean expression and returns t
 *	iff none of the subexpressions are false (or null).
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecMergeJoin
 *           ExecNestLoop
 *           ExecResult
 *           ExecScan
 *	     prs2StubTestTuple
 ****/
bool
ExecQual(qual, econtext)
    List 	  qual;
    ExprContext	  econtext;
{
    List 		clause;
    bool 		result;
    MemoryContext	oldContext;
    GlobalMemory  	qualContext;
    
    /* ----------------
     *	debugging stuff
     * ----------------
     */
    EV_printf("ExecQual: qual is ");
    EV_lispDisplay(qual);
    EV_printf("\n");

    IncrProcessed();

    /* ----------------
     *	return true immediately if no qual
     * ----------------
     */
    if (lispNullp(qual))
	return true;
    
    /* ----------------
     *  a "qual" is a list of clauses.  To evaluate the
     *  qual, we evaluate each of the clauses in the list.
     *
     *  ExecQualClause returns true when we know the qualification
     *  *failed* so we just pass each clause in qual to it until
     *  we know the qual failed or there are no more clauses.
     * ----------------
     */
    result = false;
    foreach (clause, qual) {
	result = ExecQualClause( CAR(clause), econtext);
	if (result == true)
	    break;
    }

    /* ----------------
     *	if result is true, then it means a clause failed so we
     *  return false.  if result is false then it means no clause
     *  failed so we return true.
     * ----------------
     */
    if (result == true)
	return false;
    
    return true;
}
 
/* ----------------------------------------------------------------
 *    	ExecTargetList
 *    
 *    	Evaluates a targetlist with respect to the current
 *	expression context and return a tuple.
 * ----------------------------------------------------------------
 */

/**** xxref:
 *           ExecMergeJoin
 *           ExecNestLoop
 *           ExecResult
 *           ExecScan
 ****/
HeapTuple
ExecTargetList(targetlist, nodomains, targettype, values, econtext)
    List 	 	targetlist;
    int 	 	nodomains;
    TupleDescriptor 	targettype;
    Pointer 	 	values;
    ExprContext	 	econtext;
{
    char		nulls_array[64];
    char		*np, *null_head;
    List 		tl;
    List 		tle;
    int			len;
    Expr 		expr;
    Resdom    	  	resdom;
    AttributeNumber	resno;
    Const 		expr_value;
    Datum 		constvalue;
    HeapTuple 		newTuple;
    MemoryContext	oldContext;
    GlobalMemory  	tlContext;
    Boolean 		isNull;

    /* ----------------
     *	debugging stuff
     * ----------------
     */
    EV_printf("ExecTargetList: tl is ");
    EV_lispDisplay(targetlist);
    EV_printf("\n");

    /* ----------------
     * Return a dummy tuple if the targetlist is empty .
     * the dummy tuple is necessary to differentiate 
     * between passing and failing the qualification.
     * ----------------
     */
    if (lispNullp(targetlist)) {
	/* ----------------
	 *	I now think that the only time this makes
	 *	any sence is when we run a delete query.  Then
	 *	we need to return something other than nil
	 *	so we know to delete the tuple associated
	 *      with the saved tupleid.. see what ExecutePlan
	 *      does with the returned tuple.. -cim 9/21/89
	 * ----------------
	 */
	CXT1_printf("ExecTargetList: context is %d\n", CurrentMemoryContext);
	return (HeapTuple) palloc(1);
    }

    /* ----------------
     *  allocate an array of char's to hold the "null" information
     *  only if we have a really large targetlist.  otherwise we use
     *  the stack.
     * ----------------
     */
    len = length(targetlist) + 1;
    if (len > 64) {
        np = (char *) palloc(len);
    } else {
	np = &nulls_array[0];
    }
    
    null_head = np;
    bzero(np, len);

    /* ----------------
     *	evaluate all the expressions in the target list
     * ----------------
     */
    EV_printf("ExecTargetList: setting target list values\n");
    
    for (tl = targetlist; !lispNullp(tl) ; tl = CDR(tl)) {
	/* ----------------
	 *    remember, a target list is a list of lists:
	 *
	 *	((resdom expr) (resdom expr) ...)
	 *
	 *    tl is a pointer to successive cdr's of the targetlist
	 *    tle is a pointer to the target list entry in tl
	 * ----------------
	 */
	tle = CAR(tl);
	if (lispNullp(tle))
	    break;
      
	expr = 		(Expr)   tl_expr(tle);
	resdom = 	(Resdom) tl_resdom(tle);
	resno = 	get_resno(resdom);
	constvalue = 	(Datum) ExecEvalExpr((Node)expr, econtext, &isNull);

	ExecSetTLValues(resno - 1, values, constvalue);
	
	if (!isNull)
	    *np++ = ' ';
	else
	    *np++ = 'n';
    }
    *np = '\0';

    /* ----------------
     * 	form the new result tuple (in the "normal" context)
     * ----------------
     */
    newTuple = (HeapTuple)
	heap_formtuple(nodomains, targettype, values, null_head);

    /* ----------------
     *	free the nulls array if we allocated one..
     * ----------------
     */
    if (len > 64) pfree(null_head);

    return
	newTuple;
}
 
/* ----------------------------------------------------------------
 *    	ExecProject
 *    
 *	projects a tuple based in projection info and stores
 *	it in the specified tuple table slot.
 *
 *	Note: someday soon the executor can be extended to eliminate
 *	      redundant projections by storing pointers to datums
 *	      in the tuple table and then passing these around when
 *	      possible.  this should make things much quicker.
 *	      -cim 6/3/91
 * ----------------------------------------------------------------
 */
TupleTableSlot
ExecProject(projInfo)
    ProjectionInfo projInfo;
{
    TupleTableSlot	slot;
    List 	 	targetlist;
    int 	 	len;
    TupleDescriptor 	tupType;
    Pointer 	 	tupValue;
    ExprContext	 	econtext;
    HeapTuple		newTuple;
    
    /* ----------------
     *	sanity checks
     * ----------------
     */
    if (projInfo == NULL)
	return (TupleTableSlot) NULL;
    
    /* ----------------
     *  get the projection info we want
     * ----------------
     */
    slot = 		get_pi_slot(projInfo);
    targetlist = 	get_pi_targetlist(projInfo);
    len = 		get_pi_len(projInfo);
    tupType = 		ExecSlotDescriptor((Pointer) slot);
    
    tupValue = 		get_pi_tupValue(projInfo);
    econtext = 		get_pi_exprContext(projInfo);
    
    /* ----------------
     *  form a new (result) tuple
     * ----------------
     */
    newTuple = ExecTargetList(targetlist,
			      len,
			      tupType,
			      tupValue,
			      econtext);

    /* ----------------
     *	store the tuple in the projection slot and return the slot.
     * ----------------
     */
    return (TupleTableSlot)
	ExecStoreTuple((Pointer) newTuple, /* tuple to store */
		       (Pointer) slot,     /* slot to store in */
		       InvalidBuffer, 	   /* tuple has no buffer */
		       true);         	   /* ok to pfree this tuple */
}
