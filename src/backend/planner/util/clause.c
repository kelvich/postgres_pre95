
/*     
 *      FILE
 *     	clause
 *     
 *      DESCRIPTION
 *     	Routines to manipulate qualification clauses
 *      $Header$
 */

/*     
 *      EXPORTS
 *     		pull-constant-clauses
 *     		pull-relation-level-clauses
 *     		clause-relids-vars
 *     		clause-relids
 *     		nested-clause-p
 *     		relation-level-clause-p
 *     		contains-not
 *     		join-clause-p
 *     		qual-clause-p
 *     		function-index-clause-p
 *     		fix-opids
 *     		get_relattval
 *     		get_relsatts
 */

#include "tmp/c.h"

#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "planner/internal.h"
#include "planner/clause.h"
#include "planner/var.h"
#include "planner/clauses.h"

/*    
 *    	pull-constant-clauses
 *    
 *    	Scans through a list of qualifications and find those that
 *    	contain no variables.
 *    
 *    	Returns a list of the appropriate clauses.
 *    
 */

/*  .. query_planner
 */
bool
lambda1 (qual)
     LispValue qual ;
{
    return(null (pull_var_clause (qual)));
}

LispValue
pull_constant_clauses (quals)
     LispValue quals ;
{
    return(collect ( lambda1,quals));
}

/*    
 *    	pull-relation-level-clauses
 *    
 *    	Retrieve relation level clauses from a list of clauses.
 *    
 *    	'quals' is the list of clauses
 *    
 *    	Returns a list of all relation level clauses found.
 *    
 */

/*  .. query_planner
 */
static bool
lambda2 (qual)
     LispValue qual ;
{
    if (relation_level_clause_p (qual))
      return(true);
    else
      return(false);
}

LispValue
pull_relation_level_clauses (quals)
     LispValue quals ;
{
    return(collect (lambda2,quals));
}

/*    
 *    	clause-relids-vars
 *    
 *    	Retrieves relids and vars appearing within a clause.
 *    	Returns ((relid1 relid2 ... relidn) (var1 var2 ... varm)) where 
 *    	vars appear in the clause  this is done by recursively searching
 *    	through the left and right operands of a clause.
 *    
 *    	Returns the list of relids and vars.
 *    
 *    	XXX take the nreverse's out later
 *    
 */

/*  .. add-clause-to-rels   */


LispValue
clause_relids_vars (clause)
     LispValue clause ;
{
    LispValue vars = pull_var_clause (clause);
    LispValue retval = LispNil;
    LispValue var_list = LispNil;
    LispValue varno_list = LispNil;
    LispValue tvarlist = LispNil;
    LispValue i = LispNil;

    foreach (i,vars) 
      tvarlist = nappend1(tvarlist,lispInteger((int)get_varno((Var)CAR(i))));
    
    varno_list = nreverse(remove_duplicates (tvarlist,equal));
    var_list = nreverse (remove_duplicates (vars, var_equal));

    retval = lispCons (varno_list,
		       lispCons(var_list,LispNil));
    
    return(retval);
}

/*    
 *	NumRelids
 *    	(formerly clause-relids)
 *    
 *    	Returns the number of different relations referenced in 'clause'.
 */

/*  .. compute_selec   */

int
NumRelids (clause)
     Expr clause ;
{
    LispValue t_list = LispNil;
    LispValue temp = LispNil;
    Var var = (Var)NULL;
    LispValue i = LispNil;

    foreach (i,pull_var_clause((LispValue)clause)) {
	var = (Var)CAR(i);
	t_list = nappend1(t_list,lispInteger(get_varno(var)));
    }
    t_list = remove_duplicates(t_list,equal);

    return(length(t_list));

}

/*  .. add-clause-to-rels, in-line-lambda%598037258, relation-level-clause-p
 */

bool
relation_level_clause_p (clause)
     LispValue clause ;
{
	if (!single_node(clause) &&
	    (( is_clause(clause) &&
	       get_oprelationlevel((Oper)get_op(clause))) ||
	     (not_clause(clause) && 
	      relation_level_clause_p(get_notclausearg(clause))) ||
	     (or_clause (clause) && 
	      some (relation_level_clause_p ,get_orclauseargs(clause)) ) ))
	  return(true);
	else
	  return(false);
}

/*    
 *    	contains-not
 *    
 *    	Returns t iff the clause is a 'not' clause or if any of the
 *    	subclauses within an 'or' clause contain 'not's.
 *    
 */

/*  .. add-clause-to-rels
 */
bool
contains_not (clause)
     LispValue clause ;
{
	if (!single_node(clause) &&
	    (not_clause(clause) || 
	     (or_clause(clause) && some( contains_not,
					get_orclauseargs(clause)))))
	  return(true);
	else
	  return(false);
}

/*    
 *    	join-clause-p
 *    
 *    	Returns t iff 'clause' is a valid join clause.
 *    
 */

/*  .. in-line-lambda%598037345, in-line-lambda%598037346
 *  .. initialize-join-clause-info, other-join-clause-var
 */

bool
join_clause_p (clause)
     LispValue clause ;
{
	LispValue leftop = (LispValue) get_leftop(clause);
	LispValue rightop = (LispValue) get_rightop(clause);

	if (!is_clause(clause))
		return false;
	
	/*
	 * One side of the clause (i.e. left or right operands)
	 * must either be a var node ...
	 */
	if (IsA(leftop,Var) || IsA(rightop,Var))
		return true;

	/*
	 * ... or a func node.
	 */
	if (consp(leftop) && IsA(CAR(leftop),Func))
		return(true);
	if (consp(rightop) && IsA(CAR(rightop),Func))
		return true;
	  return(false);
}

/*    
 *    	qual-clause-p
 *    
 *    	Returns t iff 'clause' is a valid qualification clause.
 *    
 */

/*  .. create_nestloop_node, in-line-lambda%598037345
 */
bool
qual_clause_p (clause)
     LispValue clause ;
{
    if (!is_clause(clause))
	return false;

    if (IsA (get_leftop (clause),Var) &&
	IsA (get_rightop (clause),Const))
    {
	return(true);
    }
    else if (IsA (get_rightop(clause),Var) &&
	     IsA (get_leftop(clause),Const))
    {
	return(true);
    }
    return(false);
}

/*    
 *    	fix-opid
 *    
 *	Calculate the opfid from the opno...
 *    
 *    	Returns nothing.
 *    
 */

/*  .. fix-opid, fix-opids
 */
void
fix_opid (clause)
     LispValue clause ;
{

	Oper oper;

	if(null(clause) || single_node (clause)) {
		;
	} 
	else if (or_clause (clause)) {
		fix_opids (get_orclauseargs (clause));
	} 
	else if (is_funcclause (clause)) {
		fix_opids (get_funcargs (clause));
	} 
	else if (IsA(clause,ArrayRef)) {
		fix_opid(get_refindexpr((ArrayRef)clause));
		fix_opid(get_refexpr((ArrayRef)clause));
	}
	else if (not_clause (clause)) {
		fix_opid (get_notclausearg (clause));
	} 
	else if (is_clause (clause)) {
		replace_opid(get_op(clause));
		fix_opid ((LispValue)get_leftop (clause));
		fix_opid ((LispValue)get_rightop (clause));
	}

} /* fix_opid */

/*    
 *    	fix-opids
 *    
 *	Calculate the opfid from the opno for all the clauses...
 *    
 *    	Returns its argument.
 *    
 */

/*  .. create_scan_node, fix-opid, preprocess-targetlist, query_planner
 */
LispValue
fix_opids (clauses)
     LispValue clauses ;
{
	LispValue clause;
	for (clause = clauses ; clause != LispNil ; clause = CDR(clause) ) 
	  fix_opid(CAR(clause));
	return(clauses);
}

/*    
 *    	get_relattval
 *    
 *    	For a non-join clause, returns a list consisting of the 
 *    		relid,
 *    		attno,
 *    		value of the CONST node (if any), and a
 *    		flag indicating whether the value appears on the left or right
 *    			of the operator and whether the value varied.
 *
 * OLD OBSOLETE COMMENT FOLLOWS:
 *    	If 'clause' is not of the format (op var node) or (op node var),
 *    	or if the var refers to a nested attribute, then -1's are returned for
 *    	everything but the value  a blank string "" (pointer to \0) is
 *    	returned for the value if it is unknown or null.
 * END OF OLD OBSOLETE COMMENT.
 * NEW COMMENT:
 * when defining rules one of the attibutes of the operator can
 * be a Param node (which is supposed to be treated as a constant).
 * However as there is no value specified for a parameter until run time
 * this routine used to return "" as value, which made 'compute_selec'
 * to bomb (because it was expecting a lisp integer and got back a lisp
 * string). Now the code returns a plain old good "lispInteger(0)".
 *    
 */

/*  .. compute_selec, get_relattvals
 */
LispValue
get_relattval (clause)
     LispValue clause ;
{
    Var left = get_leftop (clause);
    Var right = get_rightop (clause);
    
    if(is_clause (clause) && IsA (left,Var) && 
       !var_is_mat (left) && IsA (right,Const)) {
	if(!null(right)) {
	    return(lispCons (lispCopy(CAR(get_varid (left))),
		      lispCons(lispInteger(get_varattno (left)),
			     lispCons(lispInteger(get_constvalue((Const)right)),
				      lispCons(lispInteger(
						(_SELEC_CONSTANT_RIGHT_ |
						   _SELEC_IS_CONSTANT_)),
						 LispNil)))));
	} else {
	    return(lispCons (lispCopy(CAR(get_varid (left))),
		  lispCons(lispInteger(get_varattno (left)),
			   /* lispCons(lispString(""), */
			   lispCons(lispInteger(0),
				    lispCons((lispInteger
					      (_SELEC_CONSTANT_RIGHT_ |
					       _SELEC_NOT_CONSTANT_)),
					     LispNil)))));
	} 
    } /* if (is_clause(clause . . . */ 

    else if (is_clause(clause) &&
	     is_funcclause((LispValue)left) &&
	     IsA(right,Const))
    {
	    return(lispCons(lispCopy(CAR(get_varid((Var)CAR(get_funcargs((LispValue)left))))),
		      lispCons(lispInteger(InvalidAttributeNumber),
			     lispCons(lispInteger(get_constvalue((Const)right)),
					lispCons(lispInteger(
						  (_SELEC_CONSTANT_RIGHT_ |
						   _SELEC_IS_CONSTANT_)),
						 LispNil)))));
    }
    
    /*
     * XXX both of these func clause handling if's seem wrong to me.
     *     they assume that the first argument is the Var.  It could
     *     not handle (for example) f(1, emp.name).  I think I may have
     *     been assuming no constants in functional index scans when I
     *     implemented this originally (still currently true).
     *     -mer 10 Aug 1992
     */
    else if (is_clause(clause) &&
	     is_funcclause((LispValue)right) &&
	     IsA(left,Const))
    {
	    return(lispCons(lispCopy(CAR(get_varid((Var)CAR(get_funcargs((LispValue)right))))),
		      lispCons(lispInteger(InvalidAttributeNumber),
			     lispCons(lispInteger(get_constvalue ((Const)left)),
					lispCons(lispInteger(
						  (_SELEC_IS_CONSTANT_)),
						 LispNil)))));
    }
    
    else if (is_clause (clause) && IsA (right,Var) &&
	     ! var_is_mat (right) && IsA (left,Const)) {
	if (!null (left)) {
	    return(lispCons(lispCopy(CAR(get_varid (right))),
		   lispCons(lispInteger(get_varattno(right)),
			    lispCons(lispInteger(get_constvalue ((Const)left)),
				     lispCons(lispInteger(_SELEC_IS_CONSTANT_),
					      LispNil)))));
	} else {
	    return(lispCons (lispCopy(CAR(get_varid (right))),
		    lispCons(lispInteger(get_varattno (right)),
			     /* lispCons(lispString(""), */
			     lispCons(lispInteger(0),
				      lispCons(lispInteger(_SELEC_NOT_CONSTANT_),
					       LispNil)))));
	} 
    } else  {
	/*    One or more of the operands are expressions 
	      (e.g., oper clauses
	      */
	return(lispCons (lispInteger(_SELEC_VALUE_UNKNOWN_),
		  lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
			   /* lispCons(lispString(""), */
			   lispCons(lispInteger(0),
				    lispCons(lispInteger(_SELEC_NOT_CONSTANT_),
					     LispNil)))));
    }		       
}

/*    
 *    	get_relsatts
 *    
 *    	Returns a list 
 *    		( relid1 attno1 relid2 attno2 )
 *    	for a joinclause.
 *    
 *    	If the clause is not of the form (op var var) or if any of the vars
 *    	refer to nested attributes, then -1's are returned.
 *    
 */

/*  .. compute_selec   */

LispValue
get_relsatts (clause)
     LispValue clause ;
{
    Var left = get_leftop (clause);
    Var right = get_rightop (clause);
    bool isa_clause = false;
    
    bool var_left = ( (IsA(left,Var) && !var_is_mat (left) ) ?true : false);
    bool var_right = ( (IsA(right,Var) && !var_is_mat(right)) ? true:false);
    bool varexpr_left = (bool)((IsA(left,Func) || IsA (left,Oper)) &&
			       pull_var_clause ((LispValue)left) );
    bool varexpr_right = (bool)(( IsA(right,Func) || IsA (right,Oper)) &&
				pull_var_clause ((LispValue)right));
    
    isa_clause = is_clause (clause);
    if(isa_clause && var_left
       && var_right) {
	return(lispCons (lispCopy(CAR(get_varid (left))),
			 lispCons(lispCopy(CADR(get_varid (left))),
				  lispCons(lispCopy(CAR(get_varid (right))),
					   lispCons(lispCopy
						    (CADR(get_varid (right))),
						    LispNil)))));
    } else if ( isa_clause && var_left && varexpr_right ) {
	return(lispCons(lispCopy(CAR(get_varid (left))),
			lispCons(lispCopy(CADR(get_varid (left))),
				 lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
					  lispCons(lispInteger
						   (_SELEC_VALUE_UNKNOWN_),
						   LispNil)))));
    } else if ( isa_clause && varexpr_left && var_right) {
	return(lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
			lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
				 lispCons(lispCopy(CAR(get_varid (right))),
					  lispCons(lispCopy(CADR(get_varid(right))),
						   LispNil)))));
    } else {
	return(lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
			lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
				 lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
					  lispCons(lispInteger
						   (_SELEC_VALUE_UNKNOWN_),
						   LispNil)))));
    }
}

bool
is_clause(clause)
     LispValue clause;
{
    if (consp (clause)) {
	LispValue oper = clause_head(clause);

	if (IsA(oper,Oper))
	  return(true);
    }
    
    return(false);
    
}

void
CommuteClause(clause)
    LispValue clause;
{
    LispValue temp;
    Oper commu;
    OperatorTupleForm commuTup;
    HeapTuple heapTup;

    if (!(is_clause(clause) &&
	  IsA(CAR(clause),Oper)))
	return;

    heapTup = (HeapTuple)
	get_operator_tuple( get_commutator(get_opno((Oper)CAR(clause))) );

    if (heapTup == (HeapTuple)NULL)
	return;

    commuTup = (OperatorTupleForm)GETSTRUCT(heapTup);

    commu = MakeOper(heapTup->t_oid,
	     InvalidObjectId, 
	     get_oprelationlevel((Oper)CAR(clause)),
	     commuTup->oprresult,
	     get_opsize((Oper)CAR(clause)),
	     NULL);

    /*
     * reform the clause -> (operator func/var constant)
     */
    CAR(clause) = (LispValue)commu;
    temp = CADR(clause);
    CADR(clause) = CADDR(clause);
    CADDR(clause) = temp;
}
