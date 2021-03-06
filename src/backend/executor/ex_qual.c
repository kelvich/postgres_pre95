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

#include "parser/parsetree.h"
#include "parse.h" /* for NOT used in macros in ExecEvalExpr */
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "planner/keys.h"
#include "planner/clauses.h"
#include "nodes/mnodes.h"
#include "catalog/pg_language.h"
#include "executor/executor.h"

#include "access/heapam.h"

#include "utils/memutils.h"
#include "utils/palloc.h"

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
 *    ExecEvalArrayRef
 *
 *     This function takes an ArrayRef and returns a Const Node if it
 *     is an array reference or returns the changed Array Node if it is
 *	   an array assignment.
 *
 * --------------------------------
 */

Datum
ExecEvalArrayRef(arrayRef, econtext, isNull, isDone)
    ArrayRef    arrayRef;
    ExprContext econtext;
    Boolean     *isNull;
    Boolean     *isDone;
{
    bool	dummy;
    int 	i = 0, j = 0;
    char 	*array_scanner;
    char 	*retval;
	LispValue elt, upperIndexpr, 
		lowerIndexpr, assgnexpr;
	IntArray 	upper, lower;
	int *lIndex;
	struct varlena *dataPtr;
	extern char * array_set(), * array_assgn();
	extern char * array_ref(), * array_clip();
    
	execConstByVal = get_refelembyval(arrayRef);
    *isNull       =	false; 
    array_scanner =	(char *)ExecEvalExpr((Node)get_refexpr(arrayRef),
					     econtext,
					     isNull,
					     isDone);
    if (*isNull) return (Datum)NULL;

	upperIndexpr = get_refupperindexpr(arrayRef);
	foreach (elt, upperIndexpr) {
    	upper.indx[i++] = (int32)ExecEvalExpr((Node)CAR(elt),
				      econtext,
				      isNull,
				      &dummy);
    	if (*isNull) return (Datum)NULL;
	}

	lowerIndexpr = get_reflowerindexpr(arrayRef);
	lIndex = NULL;
	if (lowerIndexpr != LispNil) {
	foreach (elt, lowerIndexpr) {
    	lower.indx[j++] = (int32)ExecEvalExpr((Node)CAR(elt),
				      econtext,
				      isNull,
				      &dummy);
    	if (*isNull) return (Datum)NULL;
		}
		if (i != j) elog(WARN, 
		"ExecEvalArrayRef: upper and lower indices mismatch");
		lIndex = lower.indx;
	}

	assgnexpr    = get_refassgnexpr(arrayRef);
	if (assgnexpr != LispNil) {
   		dataPtr = (struct varlena *)ExecEvalExpr((Node)
					  assgnexpr, econtext,
				      isNull, &dummy);
   		if (*isNull) return (Datum)NULL;
		if (lIndex == NULL)
			return (Datum) array_set(array_scanner, i, upper.indx, dataPtr, 
					get_refelembyval(arrayRef), get_refelemlength(arrayRef), 
					get_refattrlength(arrayRef), isNull);
		return (Datum) array_assgn(array_scanner, i, upper.indx,
				lower.indx, dataPtr, get_refelembyval(arrayRef), 
				get_refelemlength(arrayRef), isNull);
	}
	if (lIndex == NULL) 
		return (Datum) array_ref(array_scanner, i, upper.indx,
				get_refelembyval(arrayRef), get_refelemlength(arrayRef), 
				get_refattrlength(arrayRef), isNull);
	return (Datum) array_clip(array_scanner, i, upper.indx, lower.indx,
            get_refelembyval(arrayRef), get_refelemlength(arrayRef), isNull);
}

Datum
ExecExtractResult(slot, attnum, isNull)
    TupleTableSlot  slot;
    AttributeNumber attnum;
    Boolean         *isNull;
{
    ExecTupDescriptor	execTd;
    TupleDescriptor	td;
    HeapTuple		tuple;
    Buffer		buffer;
    DatumPtr		values;
    char		*nullVect;  
    int			count;
    int			subTupNatts;
    AttributeNumber	subTupAtt;
    TupleTableSlot	resultSlot;

    execTd = SlotExecTupDescriptor(slot);

    *isNull = false;
    subTupNatts = execTd->data[attnum-1]->len;
    td = ExecSlotDescriptor(slot);
    tuple = (HeapTuple)ExecFetchTuple(slot);
    buffer = ExecSlotBuffer(slot);

    values = (DatumPtr)palloc(subTupNatts*sizeof(Datum));
    nullVect = (char *)palloc(subTupNatts*sizeof(char));

    /*
     * Figure out where in the context of the larger tuple to begin
     * searching for the sub tuples attributes.  Note that attribute
     * numbers are 1 based.
     */
    count = 0;
    subTupAtt = 1;
    while (count < (attnum-1))
	subTupAtt += execTd->data[count++]->len;

    count = 0;
    while (count < subTupNatts)
    {
	bool attrIsNull;

	values[count] = (Datum)
	    heap_getattr(tuple,
			 buffer,
			 subTupAtt,
			 (struct attribute **)&td->data[0],
			 &attrIsNull);
	
	if (attrIsNull)
	    nullVect[count] = 'n';
	else
	    nullVect[count] = ' ';
	
	count++;
	subTupAtt++;
    }

    td = execTd->data[attnum-1]->attdesc;
    tuple = heap_formtuple(subTupNatts, td, values, nullVect);
    resultSlot = MakeTupleTableSlot(false,
				    true,
				    ExecCopyTupType(td, subTupNatts),
				    NULL,
				    InvalidBuffer,
				    -1);
    return (Datum)
	ExecStoreTuple((Pointer) tuple,
		       (Pointer) resultSlot,
		       InvalidBuffer,
		       true);
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
    ExecTupDescriptor	execTupDesc;
    HeapTuple		heapTuple;
    TupleDescriptor	tuple_type;
    Buffer 		buffer;
    bool		byval;
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
	    
    /*
     * If the attribute number is invalid, then we are supposed to
     * return the entire tuple, we give back a whole slot so that
     * callers know what the tuple looks like.
     */
    if (attnum == InvalidAttributeNumber)
    {
	TupleTableSlot  tempSlot;
	TupleDescriptor td;
	HeapTuple       tup;

	tempSlot = MakeTupleTableSlot(false,
				      true,
				      (TupleDescriptor)NULL,
				      (ExecTupDescriptor)NULL,
				      InvalidBuffer,
				      -1);

	tup = (HeapTuple)heap_copysimple(ExecFetchTuple(slot));
	td = (TupleDescriptor)
		ExecCopyTupType(ExecSlotDescriptor(slot), tup->t_natts);
	ExecSetSlotDescriptor((Pointer)tempSlot, td);

	ExecStoreTuple((Pointer)tup, (Pointer)tempSlot, InvalidBuffer, true);
	return (Datum) tempSlot;
    }

    /* ----------------
     *	get the attribute our of the tuple.  Note that
     *  now the attribute could be much more than a simple datum.
     * ----------------
     */
    execTupDesc = SlotExecTupDescriptor(slot);
    if (execTupDesc != (ExecTupDescriptor)NULL &&
	AttributeNumberIsForUserDefinedAttribute(attnum) &&
	execTupDesc->data[attnum-1]->tag == ATTTUP)
    {
	result = ExecExtractResult(slot, attnum, isNull);
    }
    else
    {
	result =  (Datum)
	    heap_getattr(heapTuple,  /* tuple containing attribute */
		         buffer,     /* buffer associated with tuple */
		         attnum,     /* attribute number of desired attribute */
		         tuple_type, /* tuple descriptor of tuple */
		         isNull);    /* return: is attribute null? */
    }

    /* ----------------
     *	return null if att is null
     * ----------------
     */
    if (*isNull)
	return (Datum) NULL;
    
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

    execConstByVal = byval;
    execConstLen =   len;

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
ExecEvalParam(expression, econtext, isNull)
    Param 	expression;
    ExprContext econtext;
    Boolean     *isNull;
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
    
    *isNull = false;
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
    if (paramList->isnull)
    {
	*isNull = true;
	return (Datum)NULL;
    }

    if (get_param_tlist(expression) != (List)NULL)
    {
	HeapTuple      tup;
	Datum          value;
	List           tlist = get_param_tlist(expression);
	List           tle  = CAR(tlist);
	TupleTableSlot slot = (TupleTableSlot)paramList->value;

	tup = (HeapTuple)ExecFetchTuple((Pointer)slot);
	value = ProjectAttribute(ExecSlotDescriptor(slot), tle, tup, isNull);
	return value;
    }
    return(paramList->value);
}
 

/* ----------------------------------------------------------------
 *    	ExecEvalOper / ExecEvalFunc support routines
 * ----------------------------------------------------------------
 */
 
/* ----------------
 *	GetAttributeByName
 *	GetAttributeByNum
 *
 *	These are functions which return the value of the
 *	named attribute out of the tuple from the arg slot.  User defined
 *	C functions which take a tuple as an argument are expected
 *	to use this.  Ex: overpaid(EMP) might call GetAttributeByNum().
 * ----------------
 */

char *
GetAttributeByNum(slot, attrno, isNull)
    TupleTableSlot slot;
    AttributeNumber attrno;
    Boolean *isNull;
{
    Datum retval;

    if (!AttributeNumberIsValid(attrno))
	elog(WARN, "GetAttributeByNum: Invalid attribute number");
    
    if (!AttributeNumberIsForUserDefinedAttribute(attrno))
	elog(WARN, "GetAttributeByNum: cannot access system attributes here");

    if (isNull == (Boolean *)NULL)
	elog(WARN, "GetAttributeByNum: a NULL isNull flag was passed");
    
    if (TupIsNull((Pointer)slot))
    {
    	*isNull = true;
	return (char *) NULL;
    }

    retval = (Datum)
	heap_getattr(ExecFetchTuple(slot),
		     ExecSlotBuffer(slot),
		     attrno,
		     ExecSlotDescriptor(slot),
		     isNull);
    if (*isNull)
	return (char *) NULL;
    return (char *) retval;
}

/* XXX char16 name for catalogs */
char *
att_by_num(slot, attrno, isNull)
    TupleTableSlot slot;
    AttributeNumber attrno;
    Boolean *isNull;
{
	return(GetAttributeByNum(slot, attrno, isNull));
}

char *
GetAttributeByName(slot, attname, isNull)
    TupleTableSlot slot;
    Name    attname;
    Boolean *isNull;
{
    AttributeNumber attrno;
    TupleDescriptor tupdesc;
    HeapTuple       tuple;
    ObjectId relid;
    Datum retval;
    int natts;
    int i;

    if (attname == (Name)NULL)
	elog(WARN, "GetAttributeByName: Invalid attribute name");
    
    if (isNull == (Boolean *)NULL)
	elog(WARN, "GetAttributeByName: a NULL isNull flag was passed");
    
    if (TupIsNull((Pointer)slot))
    {
    	*isNull = true;
	return (char *) NULL;
    }

    tupdesc = ExecSlotDescriptor(slot);
    tuple = (HeapTuple)ExecFetchTuple(slot);

    natts = tuple->t_natts;

    i = 0;
    attrno = InvalidAttributeNumber;
    while (i < natts)
    {
	/*
	 * Given a slot we really don't know how many attributes
	 * we have in the tuple, so I chose to use a completely
	 * bogus check looking right into bogus memory.  I can't
	 * think of a reasonable way to do this and it is getting
	 * late. I apologize to future post-boys and post-girls -mer
	 */
	if (NameIsEqual(attname, &(tupdesc->data[i]->attname)))
	{
	    attrno = tupdesc->data[i]->attnum;
	    break;
	}
	i++;
    }
	
    if (attrno == InvalidAttributeNumber)
	elog(WARN, "GetAttributeByName: attribute %s not found", attname);

    retval = (Datum)
	heap_getattr(ExecFetchTuple(slot),
		     ExecSlotBuffer(slot),
		     attrno,
		     tupdesc,
		     isNull);
    if (*isNull)
	return (char *) NULL;
    return (char *) retval;
}

/* XXX char16 name for catalogs */
char *
att_by_name(slot, attname, isNull)
    TupleTableSlot slot;
    Name    attname;
    Boolean *isNull;
{
	return(GetAttributeByName(slot, attname, isNull));
}

void
ExecEvalFuncArgs(fcache, econtext, argList, argV, argIsDone)
    FunctionCachePtr fcache;
    ExprContext econtext;
    List argList;
    Datum argV[];
    Boolean *argIsDone;
{
    int i;
    bool      argIsNull, *nullVect;
    LispValue arg;

    nullVect = fcache->nullVect;

    i = 0;
    foreach (arg, argList) {
	/* ----------------
	 *   evaluate the expression, in general functions cannot take
	 *   sets as arguments but we make an exception in the case of
	 *   nested dot expressions.  We have to watch out for this case
	 *   here.
	 * ----------------
	 */
	argV[i] = (Datum)
	    ExecEvalExpr((Node) CAR(arg),
		     	econtext,
		     	&argIsNull,
		     	argIsDone);
	if (! (*argIsDone))
	{
	    Assert(i == 0);
	    fcache->setArg = (char *)argV[0];
	    fcache->hasSetArg = true;
	}
	if (argIsNull)
	    nullVect[i] = true;
	else
	    nullVect[i] = false;
	i++;
    }
}

/* ----------------
 *	ExecMakeFunctionResult
 * ----------------
 */
Datum
ExecMakeFunctionResult(node, arguments, econtext, isNull, isDone)
    Node	node;
    List 	arguments;
    ExprContext econtext;
    Boolean *isNull;
    Boolean *isDone;
{
    List	arg;
    Datum	argv[MAXFMGRARGS];
    FunctionCachePtr fcache;
    Func	funcNode = NULL;
    Oper	operNode = NULL;
    bool        funcisset = false;

    /*
     * This is kind of ugly, Func nodes now have targetlists so that
     * we know when and what to project out from postquel function results.
     * This means we have to pass the func node all the way down instead
     * of using only the fcache struct as before.  ExecMakeFunctionResult
     * becomes a little bit more of a dual personality as a result.
     */
    if (ExactNodeType(node,Func))
    {
	funcNode = (Func)node;
	fcache = get_func_fcache(funcNode);
    }
    else
    {
	operNode = (Oper)node;
	fcache = get_op_fcache(operNode);
    }

    /* ----------------
     *	arguments is a list of expressions to evaluate
     *  before passing to the function manager.
     *  We collect the results of evaluating the expressions
     *  into a datum array (argv) and pass this array to arrayFmgr()
     * ----------------
     */
    if (fcache->nargs != 0) {
	bool argDone;

	if (fcache->nargs > MAXFMGRARGS) 
	    elog(WARN, "ExecMakeFunctionResult: too many arguments");

	/*
	 * If the setArg in the fcache is set we have an argument
	 * returning a set of tuples (i.e. a nested dot expression).  We
	 * don't want to evaluate the arguments again until the function
	 * is done. hasSetArg will always be false until we eval the args
	 * for the first time. We should set this in the parser.
	 */
	if ((fcache->hasSetArg) && fcache->setArg != NULL)
	{
	    argv[0] = (Datum)fcache->setArg;
	    argDone = false;
	}
	else
	    ExecEvalFuncArgs(fcache, econtext, arguments, argv, &argDone);

	if ((fcache->hasSetArg) && (argDone)) {
	    if (isDone) *isDone = true;
	    return (Datum)NULL;
	}
    }

    /* If this function is really a set, we have to diddle with things.
     * If the function has already been called at least once, then the 
     * setArg field of the fcache holds
     * the OID of this set in pg_proc.  (This is not quite legit, since
     * the setArg field is really for functions which take sets of tuples
     * as input - set functions take no inputs at all.  But it's a nice
     * place to stash this value, for now.)
     *
     * If this is the first call of the set's function, then
     * the call to ExecEvalFuncArgs above just returned the OID of 
     * the pg_proc tuple which defines this set.  So replace the existing
     * funcid in the funcnode with the set's OID.  Also, we want a new
     * fcache which points to the right function, so get that, now that
     * we have the right OID.  Also zero out the argv, since the real
     * set doesn't take any arguments.
     */
    if (get_funcid((Func)node) == SetEvalRegProcedure) {
	 funcisset = true;
	 if (fcache->setArg) {
	      argv[0] = 0;
	      set_funcid((Func) node, 
			 (ObjectId) PointerGetDatum(fcache->setArg));
	 } else {
	      set_funcid((Func)node, (ObjectId) argv[0]);
	      set_fcache(node, argv[0], LispNil,econtext);
	      fcache = get_func_fcache((Func)node);
	      fcache->setArg = (char*)argv[0];
	      argv[0] = (Datum)0;
	 }
    }

    /* ----------------
     *   now return the value gotten by calling the function manager, 
     *   passing the function the evaluated parameter values. 
     * ----------------
     */
    if (fcache->language == POSTQUELlanguageId) {
	Datum result;

	Assert(funcNode);
	result = postquel_function (funcNode, (char **) argv, isNull, isDone);
	/*
	 * finagle the situation where we are iterating through all results
	 * in a nested dot function (whose argument function returns a set
	 * of tuples) and the current function finally finishes.  We need to
	 * get the next argument in the set and run the function all over
	 * again.  This is getting unclean.
	 */
	if ((*isDone) && (fcache->hasSetArg)) {
	    Boolean argDone;

	    ExecEvalFuncArgs(fcache, econtext, arguments, argv, &argDone);

	    if (argDone) {
	        fcache->setArg = (char *)NULL;
		*isDone = true;
		result = (Datum)NULL;
	    }
	    else
		result = postquel_function(funcNode,
					   (char **) argv,
					   isNull,
					   isDone);
	}
	if (funcisset) {
	     /* reset the funcid so that next call to this routine will
	      * still recognize this func as a set.
	      * Note that for now we assume that the set function in
	      * pg_proc must be a Postquel function - the funcid is
	      * not reset below for C functions.
	      */
	     set_funcid((Func)node, SetEvalRegProcedure);
	     /* If we're done with the results of this function, get rid
	      * of its func cache.
	      */
	     if (*isDone) {
		  set_func_fcache((Func)node, NULL);
	     }
	}
	return result;
    }
    else 
    {
	int i;

	if (isDone) *isDone = true;
	for (i = 0; i < fcache->nargs; i++) 
	    if (fcache->nullVect[i] == true) *isNull = true;

	return((Datum) fmgr_c(fcache->func, fcache->foid, fcache->nargs,
			      (FmgrValues *) argv, isNull));
    }
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
    bool	isDone;
    
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
        set_fcache(op, get_opid(op), cdr(opClause), econtext);
    	fcache = get_op_fcache(op);
    }
    
    /* -----------
     *  call ExecMakeFunctionResult() with a dummy isDone that we ignore.
     *  We don't have operator whose arguments are sets.
     * -----------
     */
    return
	ExecMakeFunctionResult((Node)op, argList, econtext, isNull, &isDone);
}
 
/* ----------------------------------------------------------------
 *	ExecEvalFunc
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalFunc(funcClause, econtext, isNull, isDone)
    LispValue   funcClause;
    ExprContext econtext;
    Boolean 	*isNull;
    Boolean 	*isDone;
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
    	set_fcache(func, get_funcid(func), CDR(funcClause), econtext);
    	fcache = get_func_fcache(func);
    }

    return
	ExecMakeFunctionResult((Node)func, argList, econtext, isNull, isDone);
}
 
/* ----------------------------------------------------------------
 *    	ExecEvalNot
 *    	ExecEvalOr
 *	ExecEvalAnd
 *    
 *    	Evaluate boolean expressions.  Evaluation of 'or' is
 *	short-circuited when the first true (or null) value is found.
 *
 *	The query planner reformulates clause expressions in the
 *	qualification to conjunctive normal form.  If we ever get
 *	an AND to evaluate, we can be sure that it's not a top-level
 *	clause in the qualification, but appears lower (as a function
 *	argument, for example), or in the target list.  Not that you
 *	need to know this, mind you...
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
    bool  isDone;

    clause = 	 (List) get_notclausearg(notclause);

    /* ----------------
     *  We don't iterate over sets in the quals, so pass in an isDone
     *  flag, but ignore it.
     * ----------------
     */
    expr_value = ExecEvalExpr((Node) clause, econtext, isNull, &isDone);
 
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
    bool   isDone;
	Boolean IsNull;

	IsNull = false; 
    clauses = (List) get_orclauseargs(orExpr);
 
    /* ----------------
	 *  we use three valued logic functions here...
     *	we evaluate each of the clauses in turn,
     *  as soon as one is true we return that
     *  value.  If none is true and  none of the
	 *  clauses evaluate to NULL we return
     *  the value of the last clause evaluated (which
     *  should be false) with *isNull set to false else 
	 *  if none is true and at least one clause evaluated
	 *  to NULL we set *isNull flag to true -  
     * ----------------
     */
    foreach (clause, clauses) {

	/* ----------------
	 *  We don't iterate over sets in the quals, so pass in an isDone
	 *  flag, but ignore it.
	 * ----------------
	 */
	const_value = ExecEvalExpr((Node) CAR(clause),
				   econtext,
				   isNull,
				   &isDone);
	
	/* ----------------
	 *  if the expression evaluates to null, then we 
	 *  remember it in the local IsNull flag, if none of the
	 *  clauses are true then we need to set *isNull
	 *  to true again.
	 * ----------------
	 */
	if (*isNull)
		IsNull = *isNull;
	
	/* ----------------
	 *   if we have a true result, then we return it.
	 * ----------------
	 */
	if (! ExecCFalse(DatumGetInt32(const_value)))
	    return const_value;
    }

	/* IsNull is true if at least one clause evaluated to NULL */	
	*isNull = IsNull;
    return const_value;
}

/* ----------------------------------------------------------------
 *	ExecEvalAnd
 * ----------------------------------------------------------------
 */
 
/**** xxref:
 *           ExecEvalExpr
 ****/
Datum
ExecEvalAnd(andExpr, econtext, isNull)
    List 	andExpr;
    ExprContext econtext;
    Boolean *isNull;
{
    List   clauses;
    List   clause;
    Datum  const_value;
    bool   isDone;
	Boolean IsNull;

	IsNull = false;
 
    clauses = (List) get_andclauseargs(andExpr);
 
    /* ----------------
     *	we evaluate each of the clauses in turn,
     *  as soon as one is false we return that
     *  value.  If none are false or NULL then we return
     *  the value of the last clause evaluated, which
     *  should be true. 
     * ----------------
     */
    foreach (clause, clauses) {

	/* ----------------
	 *  We don't iterate over sets in the quals, so pass in an isDone
	 *  flag, but ignore it.
	 * ----------------
	 */
	const_value = ExecEvalExpr((Node) CAR(clause),
				   econtext,
				   isNull,
				   &isDone);
	
	/* ----------------
	 *  if the expression evaluates to null, then we 
	 *  remember it in IsNull, if none of the clauses after
	 *  this evaluates to false we will have to set *isNull
	 *  to true again.
	 * ----------------
	 */
	if (*isNull)
	    IsNull = *isNull;
	
	/* ----------------
	 *   if we have a false result, then we return it, since the
	 *   conjunction must be false.
	 * ----------------
	 */
	if (ExecCFalse(DatumGetInt32(const_value)))
	    return const_value;
    }

	*isNull = IsNull;
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
ExecEvalExpr(expression, econtext, isNull, isDone)
    Node 	expression;
    ExprContext econtext;
    Boolean 	*isNull;
    Boolean 	*isDone;
{
    Datum retDatum;

    *isNull = false;

    /*
     * Some callers don't care about is done and only want 1 result.  They
     * indicate this by passing NULL
     */
    if (isDone)
	*isDone = true;

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
	retDatum = (Datum)  ExecEvalParam((Param) expression, econtext, isNull);
   
    else if (ExactNodeType(expression,Iter))
	retDatum = (Datum)  ExecEvalIter((Iter) expression,
					 econtext,
					 isNull,
					 isDone);
   
    else if (ExactNodeType(expression,ArrayRef))
	retDatum = (Datum)  ExecEvalArrayRef((ArrayRef) expression,
					 econtext,
					 isNull,
					 isDone);
    else if (fast_is_clause(expression))	/* car is a Oper node */
	retDatum = (Datum) ExecEvalOper((List) expression, econtext, isNull);
    
    else if (fast_is_funcclause(expression)) 	/* car is a Func node */
	retDatum = (Datum) ExecEvalFunc((LispValue) expression,
					econtext,
					isNull,
					isDone);
      
    else if (fast_or_clause(expression))
	retDatum = (Datum) ExecEvalOr((List) expression, econtext, isNull);
   
    else if (fast_and_clause(expression))
	retDatum = (Datum)  ExecEvalAnd((List) expression, econtext, isNull);

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
    Boolean isDone;

    /*
     * pass isDone, but ignore it.  We don't iterate over multiple
     * returns in the qualifications.
     */
    expr_value = (Datum)
	ExecEvalExpr((Node) clause, econtext, &isNull, &isDone);
    
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

HeapTuple
ExecFormComplexResult(tlist, natts, tdesc, values, nulls)
    List            tlist;
    int             natts;
    TupleDescriptor tdesc;
    Datum           *values;
    char            *nulls;
{
    TupleDescriptor flatTd;
    Datum           *flatValues;
    char            *flatNulls;
    int             flatInd;
    int             flatNatts;
    int             complexInd;
    TupleTableSlot  *foundslots;
    TupleTableSlot  *slotbase;
    TupleTableSlot  slot;
    HeapTuple       resultTup;
    List            tlistP;

    slotbase = (TupleTableSlot *)palloc ((natts+1)*sizeof(TupleTableSlot));
    bzero(slotbase, ((natts+1)*sizeof(TupleTableSlot)));
    foundslots = slotbase;

    /*
     * we make two passes through the target list - one to figure
     * out just how many attributes we are dealing with and another
     * to get all the aforementioned attributes.
     */
    flatNatts = 0;
    complexInd = 0;
    foreach(tlistP, tlist)
    {
	List tle = CAR(tlistP);
	/*
	 * Since we are dealing with a complex result we know that we
	 * are not at the top of the plan tree and therefore we know
	 * that we have only resdoms and expressions (i.e. no Fjoins)
	 */
/*	if ( get_rescomplex((Resdom)tl_resdom(tle)) )
	{
	    HeapTuple      tuple;

	    slot  = (TupleTableSlot)values[complexInd];
	    tuple =  (HeapTuple)ExecFetchTuple(slot);
	    flatNatts += tuple->t_natts;
	}
	else
*/
	    flatNatts++;

	complexInd++;
    }
    flatTd = CreateTemplateTupleDesc(flatNatts);
    flatValues = (Datum *)palloc(flatNatts*sizeof(Datum));
    flatNulls = (char *)palloc(flatNatts*sizeof(char));

    complexInd = flatInd = 0;
    foreach(tlistP, tlist)
    {
	
	List tle = CAR(tlistP);
	/*
	 * Since we are dealing with a complex result we know that we
	 * are not at the top of the plan tree and therefore we know
	 * that we have only resdoms and expressions (i.e. no Fjoins)
	 */
	if ( get_rescomplex((Resdom)tl_resdom(tle)) )
	{
	    int             i;
	    HeapTuple       tuple;
	    TupleDescriptor subDesc;
	    Resdom          res   = (Resdom)tl_resdom(tle);

	    slot  = (TupleTableSlot)values[complexInd];
	    tuple =  (HeapTuple)ExecFetchTuple(slot);
	    subDesc = ExecSlotDescriptor(slot);
	    heap_deformtuple(tuple,
			     subDesc,
			     &flatValues[flatInd],
			     &flatNulls[flatInd]);
	    *foundslots++ = slot;
	    for (i = 0; i < tuple->t_natts; i++, flatInd++)
	    {
		flatTd->data[flatInd] = (AttributeTupleForm)
		    palloc(sizeof(AttributeTupleFormD));
		bcopy(subDesc->data[i],
		      flatTd->data[flatInd],
		      sizeof(AttributeTupleFormD));
	    }
	}
	else
	{
	    flatTd->data[flatInd] = (AttributeTupleForm)
		    palloc(sizeof(AttributeTupleFormD));

	    bcopy(tdesc->data[complexInd],
		  flatTd->data[flatInd],
		  sizeof(AttributeTupleFormD));

	    flatValues[flatInd] = values[complexInd];
	    flatInd++;
	}
	complexInd++;
    }

    resultTup = (HeapTuple)heap_formtuple(flatNatts,
					  flatTd,
					  flatValues,
					  flatNulls);
    /*
     * Be tidy and free up our constructed tuple descriptor and all slots
     * that we found.
     */
    for (flatInd = 0; flatInd < flatNatts; flatInd++)
	pfree(flatTd->data[flatInd]);
    pfree (flatTd);
    foundslots = slotbase;
    while ((slot = *foundslots) != (TupleTableSlot)NULL)
    {
	pfree(ExecFetchTuple(slot));
	pfree(slot);
	foundslots++;
    }
    pfree (slotbase);
    return resultTup;
}
 
int
ExecTargetListLength(targetlist)
    List targetlist;
{
    int len;
    List tl, curTle;

    len = 0;
    foreach (tl, targetlist) {
	curTle = CAR(tl);

	if (tl_is_resdom(curTle))
	    len++;
	else
	    len += get_fj_nNodes((Fjoin)tl_node(curTle));
    }
    return len;
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
ExecTargetList(targetlist, nodomains, targettype, values, econtext, isDone)
    List 	 	targetlist;
    int 	 	nodomains;
    TupleDescriptor 	targettype;
    Pointer 	 	values;
    ExprContext	 	econtext;
    bool		*isDone;
{
    char		nulls_array[64];
    bool		fjNullArray[64];
    bool		*fjIsNull;
    char		*np, *null_head;
    List 		tl;
    List 		tle;
    int			len;
    Expr 		expr;
    Resdom    	  	resdom;
    AttributeNumber	resind;
    Const 		expr_value;
    Datum 		constvalue;
    HeapTuple 		newTuple;
    MemoryContext	oldContext;
    GlobalMemory  	tlContext;
    Boolean		complexResult;
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
	 *
	 *      It could also happen in queries like:
	 *	    retrieve (foo.all) where bar.a = 3
	 *
	 *      is this a new phenomenon? it might cause bogus behavior
	 *      if we try to free this tuple later!! I put a hook in
	 *      ExecProject to watch out for this case -mer 24 Aug 1992
	 * ----------------
	 */
	CXT1_printf("ExecTargetList: context is %d\n", CurrentMemoryContext);
	*isDone = true;
	return (HeapTuple) true;
    }

    /* ----------------
     *  allocate an array of char's to hold the "null" information
     *  only if we have a really large targetlist.  otherwise we use
     *  the stack.
     * ----------------
     */
    len = ExecTargetListLength(targetlist);
	
    if (len > 64) {
        np = (char *) palloc(len+1);
	fjIsNull = (bool *) palloc(len+1);
    } else {
	np = &nulls_array[0];
	fjIsNull = &fjNullArray[0];
    }
    
    null_head = np;
    bzero(np, len+1);

    /* ----------------
     *	evaluate all the expressions in the target list
     * ----------------
     */
    EV_printf("ExecTargetList: setting target list values\n");

    *isDone = true;
    complexResult = false;
    foreach (tl, targetlist) {
	/* ----------------
	 *    remember, a target list is a list of lists:
	 *
	 *	((<resdom | fjoin> expr) (<resdom | fjoin> expr) ...)
	 *
	 *    tl is a pointer to successive cdr's of the targetlist
	 *    tle is a pointer to the target list entry in tl
	 * ----------------
	 */
	tle = CAR(tl);
	if (lispNullp(tle))
	    break;
      
	if (tl_is_resdom(tle)) {
	    expr       = (Expr)   tl_expr(tle);
	    resdom     = (Resdom) tl_resdom(tle);
	    resind     = get_resno(resdom) - 1;
	    constvalue = (Datum) ExecEvalExpr((Node)expr,
					      econtext,
					      &isNull,
					      isDone);

	    if ((ExactNodeType(expr,Iter)) && (*isDone))
		return (HeapTuple)NULL;

	    if (get_rescomplex(resdom))
		complexResult = true;

	    ExecSetTLValues(resind, values, constvalue);
	
	    if (!isNull)
		null_head[resind] = ' ';
	    else
		null_head[resind] = 'n';
	}
	else {
	    int      curNode;
	    Resdom   fjRes;
	    List     fjTlist   = CDR(tle);
	    Fjoin    fjNode    = (Fjoin)tl_node(tle);
 	    int      nNodes    = get_fj_nNodes(fjNode);
	    DatumPtr results   = get_fj_results(fjNode);
 
	    ExecEvalFjoin(tle, econtext, fjIsNull, isDone);
	    if (*isDone)
		return (HeapTuple)NULL;

	    /*
	     * get the result from the inner node
	     */
	    fjRes = (Resdom)CAR(get_fj_innerNode(fjNode));
	    resind = get_resno(fjRes) - 1;
	    if (fjIsNull[0])
		null_head[resind] = 'n';
	    else
	    {
		null_head[resind] = ' ';
		ExecSetTLValues(resind, values, results[0]);
	    }

	    /*
	     * Get results from all of the outer nodes
	     */
	    for (curNode = 1;
		 curNode < nNodes;
		 curNode++, fjTlist = CDR(fjTlist))
	    {
		fjRes = (Resdom)CAAR(fjTlist);

		resind = get_resno(fjRes) - 1;
		if (fjIsNull[curNode]) {
		    null_head[resind] = 'n';
		}
		else
		{
		    null_head[resind] = ' ';
	    	    ExecSetTLValues(resind, values, results[curNode]);
		}
	    }
	}
    }

    /* ----------------
     * 	form the new result tuple (in the "normal" context)
     * ----------------
     */
/*    if (complexResult)
	newTuple = (HeapTuple) ExecFormComplexResult(targetlist,
						     nodomains,
						     targettype,
						     values,
						     null_head);
    else
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
ExecProject(projInfo, isDone)
    ProjectionInfo projInfo;
    bool           *isDone;
{
    TupleTableSlot	slot;
    List 	 	targetlist;
    List 	 	tle;
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
			      econtext,
			      isDone);

    /* ----------------
     *	store the tuple in the projection slot and return the slot.
     *
     *  If there's no projection target list we don't want to pfree
     *  the bogus tuple that ExecTargetList passes back to us.
     *       -mer 24 Aug 1992
     * ----------------
     */
    return (TupleTableSlot)
	ExecStoreTuple((Pointer) newTuple, /* tuple to store */
		       (Pointer) slot,     /* slot to store in */
		       InvalidBuffer, 	   /* tuple has no buffer */
					   /* ok to pfree this tuple */
		       get_pi_targetlist(projInfo) ? true : false);
}








