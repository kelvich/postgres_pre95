
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

#include "c.h"
#include "internal.h"
#include "clause.h"

/* XXX - remove these phoney defns once we find the appropriate defn */
extern get_varno();
extern get_varattno();
extern var_equal();
extern bool is_clause();

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
LispValue 
lambda1 (qual)
     LispValue qual ;
{
	null (pull_var_clause (qual));
}

LispValue
pull_constant_clauses (quals)
     LispValue quals ;
{
	collect ( lambda1,quals);
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
LispValue
lambda2 (qual)
     LispValue qual ;
{
	and (relation_level_clause_p (qual),not (nested_clause_p (qual)));
}

LispValue
pull_relation_level_clauses (quals)
     LispValue quals ;
{
	collect (lambda2,quals);
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

/*  .. add-clause-to-rels
 */

LispValue
clause_relids_vars (clause)
LispValue clause ;
{
	LispValue vars = pull_var_clause (clause);
	LispValue retval;
	retval = list (nreverse (remove_duplicates 
				 (mapcar ( get_varno,vars))),
		       nreverse (remove_duplicates (vars, 
						    /*test*/ var_equal)));
}

/*    
 *    	clause-relids
 *    
 *    	Returns the number of different relations referenced in 'clause'.
 *    
 */

/*  .. compute_selec
 */
LispValue
clause_relids (clause)
     LispValue clause ;
{
	length (remove_duplicates (mapcar (get_varno,
					   pull_var_clause (clause))));
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
LispValue
nested_clause_p (clause)
     LispValue clause ;
{
	LispValue retval;

	if(or (null (clause),single_node (clause))) {
		retval = LispNil;

	} 
	else if (and (var_p (clause),var_is_nested (clause))) {
		retval = LispTrue;

	} else if (or_clause (clause)) {
		retval = some ( nested_clause_p,get_orclauseargs (clause));
	} else if (is_funcclause (clause)) {
		retval = some (nested_clause_p,get_funcargs (clause));

	} 
	else if (not_clause (clause)) {
		retval = nested_clause_p (get_notclausearg (clause));

	} 
	else {
		retval = nested_clause_p (get_leftop (clause)) ||
		  nested_clause_p (get_rightop (clause));
	}
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
	      some (relation_level_clause_p ,get_or_clauseargs(clause)) ) ))
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
LispValue
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

LispValue
join_clause_p (clause)
     LispValue clause ;
{
	if (is_clause(clause) &&
	    var_p(get_leftop(clause)) &&
	    var_p(get_rightop(clause)))
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
LispValue
qual_clause_p (clause)
     LispValue clause ;
{
	if (is_clause (clause) &&
	    var_p (get_leftop (clause)) &&
	    constant_p (get_rightop (clause)))
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

/*  .. in-line-lambda%598037345
 */

LispValue
lambda3 (arg)
     LispValue arg ;
{
	/*   correct relation */
	/* declare (special (rel)); */
	LispValue rel;
	return( var_p (arg) && equal (get_relid (rel) && get_varno (arg)));
}

bool
function_index_clause_p (clause,rel,index)
     LispValue clause,rel,index ;
{
	extern LispValue lispEvery();
	if (is_clause (clause) &&
	    is_funcclause (get_leftop (clause)) &&
	    constant_p (get_rightop (clause)) ) {
		LispValue funcclause = get_leftop (clause);
		LispValue function = get_function (funcclause);
		LispValue funcargs = get_funcargs (funcclause);
		LispValue index_keys = get_indexkeys (index);
		if ( function_index_info (get_funcid (function),
					  car(get_indexid (index))) &&
		    equal (length (funcargs),length (index_keys)) &&
		    lispEvery(lambda3,funcargs) &&
		    lispEvery(equal, mapcar (get_varattno,funcargs),
			  index_keys) &&
		    set_funcisindex (function, true) )
		  return(true);
		else
		  return(false);
	} else
	  return(false);

}

/*    
 *    	fix-opid
 *    
 *    	Replace all opnos within a clause with the corresponding regproc id.
 *    
 *    	Returns nothing.
 *    
 */

/*  .. fix-opid, fix-opids
 */
void *
fix_opid (clause)
     LispValue clause ;
{

	if(single_node (clause)) {
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
		replace_opid (get_op (clause));
		fix_opid (get_leftop (clause));
		fix_opid (get_rightop (clause));
	}

} /* fix_opid */

/*    
 *    	fix-opids
 *    
 *    	Replaces the opno field in all oper nodes with the corresponding
 *    	regproc id.
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
	  fix_opid(clause);
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
 *    	If 'clause' is not of the format (op var node) or (op node var),
 *    	or if the var refers to a nested attribute, then -1's are returned for
 *    	everything but the value  a blank string "" (pointer to \0) is
 *    	returned for the value if it is unknown or null.
 *    
 */

/*  .. compute_selec, get_relattvals
 */
LispValue
get_relattval (clause)
     LispValue clause ;
{
	LispValue left = get_leftop (clause);
	LispValue right = get_rightop (clause);
	
	if(is_clause (clause) && var_p (left) && 
	   !var_is_mat (left) && constant_p (right)) {
		if(non_null (right)) {
			list (get_varno (left),
			      get_varattno (left),
			      get_constvalue (right),
			      logior (_SELEC_CONSTANT_RIGHT_,
				      _SELEC_IS_CONSTANT_));
		} else {
			list (get_varno (left),
			      get_varattno (left),
			      "",
			      logior (_SELEC_CONSTANT_RIGHT_,
				      _SELEC_NOT_CONSTANT_));
		} 
	} /* if (is_clause(clause . . . */ 

	else if (is_clause (clause) && var_p (right) &&
		 ! var_is_mat (right) && constant_p (left)) {
		if (non_null (left)) {
			list (get_varno (right),
			      get_varattno (right),
			      get_constvalue (left),
			      _SELEC_IS_CONSTANT_);
		} else {
			list (get_varno (right),
			      get_varattno (right),"",
			      _SELEC_NOT_CONSTANT_);
		} 
	} else  {
		/*    One or more of the operands are expressions 
		      (e.g., oper or func clauses
		      */
		list (_SELEC_VALUE_UNKNOWN_,
		      _SELEC_VALUE_UNKNOWN_,"",
		      _SELEC_NOT_CONSTANT_);
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

/*  .. compute_selec
 */
LispValue
get_relsatts (clause)
     LispValue clause ;
{
	LispValue left = get_leftop (clause);
	LispValue right = get_rightop (clause);

	bool isa_clause = is_clause (clause);
	bool var_left = var_p (left) && !var_is_mat (left) ;
	bool var_right = var_p (right) && !var_is_mat (right) ;
	bool varexpr_left = ((func_p (left) || oper_p (left)) &&
			     pull_var_clause (left) );
	bool varexpr_right = (( func_p (right) || oper_p (right)) &&
			      pull_var_clause (right));

	if(isa_clause && var_left && var_right) {
		list (get_varno (left),get_varattno (left),
		      get_varno (right),get_varattno (right));
	} else if ( isa_clause && var_left && varexpr_right ) {
		list (get_varno (left),get_varattno (left),
		      _SELEC_VALUE_UNKNOWN_,_SELEC_VALUE_UNKNOWN_);
	} else if ( isa_clause && varexpr_left && var_right) {
		list (_SELEC_VALUE_UNKNOWN_,_SELEC_VALUE_UNKNOWN_,
		      get_varno (right),get_varattno (right));
	} else {
		list (_SELEC_VALUE_UNKNOWN_,_SELEC_VALUE_UNKNOWN_,
		      _SELEC_VALUE_UNKNOWN_,_SELEC_VALUE_UNKNOWN_);
	}
}
