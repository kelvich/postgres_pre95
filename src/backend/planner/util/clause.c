
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
    if (relation_level_clause_p (qual) && !(nested_clause_p (qual)))
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
      tvarlist = nappend1(tvarlist,lispInteger(get_varno(CAR(i))));
    
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

    foreach (i,pull_var_clause(clause)) {
	var = (Var)CAR(i);
	t_list = nappend1(t_list,lispInteger(get_varno(var)));
    }
    t_list = remove_duplicates(t_list,equal);

    return(length(t_list));

}

/*    
 *    	nested-clause-p
 *    
 *    	Returns t if any of the vars in the qualification clause 'clause'
 *    	contain dot fields, indicating that the clause should be ignored
 *    	at this nesting level.
 *    
 */

/*  .. add-clause-to-rels, in-line-lambda%598037258, nested-clause-p
 */
bool
nested_clause_p (clause)
     LispValue clause ;
{
    bool retval = false;

/*
 *  XXX turn it off for now since it always return false
 * until we support dotted queries.
 
    if(null (clause) ||single_node (clause)) {
    } 
    else if (IsA(clause,Var) && var_is_nested (clause)) {
	retval = true;
    } else if (or_clause (clause)) {
	LispValue i = LispNil;
	LispValue orclause = LispNil;

	foreach (i,get_orclauseargs (clause)) {
	    orclause = CAR(i);
	    if (nested_clause_p (orclause)) {
	      retval = true;
	      break;
	  }
	}
    } else if (is_funcclause (clause)) {
	if (some (nested_clause_p,get_funcargs (clause)))
	  retval = true;
    } 
    else if (not_clause (clause)) {
	retval = nested_clause_p (get_notclausearg (clause));
    } 
    else {
	retval = (bool)(nested_clause_p (get_leftop (clause)) ||
			nested_clause_p (get_rightop (clause)));
    }
 */
    return(retval);
}

/*    
 *    	relation-level-clause-p
 *    
 *    	Returns t iff the qualification clause 'clause' is a relation-level
 *    	clause.
 *    
 */

/*  .. add-clause-to-rels, in-line-lambda%598037258, relation-level-clause-p
 */

bool
relation_level_clause_p (clause)
     LispValue clause ;
{
	if (!single_node(clause) &&
	    (( is_clause(clause) && get_oprelationlevel(get_op(clause))) ||
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
	if (is_clause(clause) &&
	    IsA(get_leftop(clause),Var) &&
	    IsA(get_rightop(clause),Var))
	  return(true);
	else
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
	if (is_clause (clause) &&
	    IsA (get_leftop (clause),Var) &&
	    IsA (get_rightop (clause),Const))
	  return(true);
	else
	  return(false);
}

/*    
 *    	function-index-clause-p		XXX FUNCS
 *    
 *    	Returns t iff 'clause' is a valid index-function qualification clause.
 *    
 */

bool
function_index_clause_p (clause,rel,index)
     LispValue clause,rel;
     Rel index;
{
    
  if (is_clause (clause) &&
      is_funcclause (get_leftop (clause)) &&
      constant_p (get_rightop (clause)) ) {
    Var funcclause = get_leftop (clause);
    LispValue function = get_function (funcclause);
    LispValue funcargs = get_funcargs (funcclause);
    List index_keys = get_indexkeys (index);
    bool result1 = true;
    bool result2 = true;
    LispValue arg = LispNil;
    LispValue t_list = LispNil;
    LispValue var = LispNil;
    LispValue temp = LispNil;
    LispValue temp1 = LispNil;

    /*  XXX  was a lisp every function  */
    foreach (arg, funcargs) {
      if ( ! ((IsA (arg,Var) && 
	       equal (get_relid (rel) && get_varno (arg)))))
	result1 = false;
    }        
    
    /*  XXX was mapcar  */
    foreach (var, funcargs) 
      t_list = nappend1 (t_list, get_varattno(var));
  
    /* XXX was lisp every   */
    foreach (temp, t_list) {
      temp1 = CAR (index_keys);
      index_keys = CDR (index_keys);
      if (! equal(temp,temp1))
	result2 = false;
      if (index_keys == LispNil) {
	result2 = false;
	break;
      }
    }



    set_funcisindex (function, true);
    
    return ((bool) (function_index_info (get_funcid (function),
					 /* XXX used to be CAR(get_indexid ..*/
					 get_indexid (index)) &&
		    (length (funcargs) == length (index_keys)) &&
		    result1 &&
		    result2 ));

  }
  else
    return (false);

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
	else if (not_clause (clause)) {
		fix_opid (get_notclausearg (clause));
	} 
	else if (is_clause (clause)) {
		replace_opid(get_op(clause));
		fix_opid (get_leftop (clause));
		fix_opid (get_rightop (clause));
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
	    return(lispCons (lispInteger(get_varno (left)),
		      lispCons(lispInteger(get_varattno (left)),
			       lispCons(lispInteger(get_constvalue (right)),
					lispCons(lispInteger(
						  (_SELEC_CONSTANT_RIGHT_ |
						   _SELEC_IS_CONSTANT_)),
						 LispNil)))));
	} else {
	    return(lispCons (lispInteger(get_varno (left)),
		  lispCons(lispInteger(get_varattno (left)),
			   /* lispCons(lispString(""), */
			   lispCons(lispInteger(0),
				    lispCons((lispInteger
					      (_SELEC_CONSTANT_RIGHT_ |
					       _SELEC_NOT_CONSTANT_)),
					     LispNil)))));
	} 
    } /* if (is_clause(clause . . . */ 
    
    else if (is_clause (clause) && IsA (right,Var) &&
	     ! var_is_mat (right) && IsA (left,Const)) {
	if (!null (left)) {
	    return(lispCons(lispInteger(get_varno (right)),
		   lispCons(lispInteger(get_varattno(right)),
			    lispCons(lispInteger(get_constvalue (left)),
				     lispCons(lispInteger(_SELEC_IS_CONSTANT_),
					      LispNil)))));
	} else {
	    return(lispCons (lispInteger(get_varno (right)),
		    lispCons(lispInteger(get_varattno (right)),
			     /* lispCons(lispString(""), */
			     lispCons(lispInteger(0),
				      lispCons(lispInteger(_SELEC_NOT_CONSTANT_),
					       LispNil)))));
	} 
    } else  {
	/*    One or more of the operands are expressions 
	      (e.g., oper or func clauses
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
			       pull_var_clause (left) );
    bool varexpr_right = (bool)(( IsA(right,Func) || IsA (right,Oper)) &&
				pull_var_clause (right));
    
    isa_clause = is_clause (clause);
    if(isa_clause && var_left
       && var_right) {
	return(lispCons (lispInteger(get_varno (left)),
			 lispCons(lispInteger(get_varattno (left)),
				  lispCons(lispInteger(get_varno (right)),
					   lispCons(lispInteger
						    (get_varattno (right)),
						    LispNil)))));
    } else if ( isa_clause && var_left && varexpr_right ) {
	return(lispCons(get_varno (left),
			lispCons(get_varattno (left),
				 lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
					  lispCons(lispInteger
						   (_SELEC_VALUE_UNKNOWN_),
						   LispNil)))));
    } else if ( isa_clause && varexpr_left && var_right) {
	return(lispCons (lispInteger(_SELEC_VALUE_UNKNOWN_),
			 lispCons(lispInteger(_SELEC_VALUE_UNKNOWN_),
				  lispCons(get_varno (right),
					   lispCons(get_varattno (right),
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
