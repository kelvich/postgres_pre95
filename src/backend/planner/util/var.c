/*     
 *      FILE
 *     	var
 *     
 *      DESCRIPTION
 *     	Var node manipulation routines
 *     $Header$
 */

/*
 *      EXPORTS
 *     		pull_var_clause
 *     		var_equal
 *     		numlevels
 */

#include "planner/internal.h"
#include "planner/var.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "planner/clauses.h"

LispValue
pull_agg_clause(clause)
	 LispValue clause;
{
	LispValue retval = LispNil;
	if(null(clause))
	    retval=LispNil;
	else if(agg_clause(clause))
	    retval = CDR(clause);
	    /* this brings the clause down to
	     * (aggname, query, tlist, -1)
	     */
        else if (single_node (clause)) 
	    retval = LispNil;
	else if (or_clause(clause)) {
		LispValue temp;
		LispValue result = LispNil;
		/* mapcan */
		foreach (temp,get_orclauseargs(clause) )
		  retval = nconc(retval, pull_agg_clause(CAR(temp)));
	    }
	else if (is_funcclause (clause)) {
	    LispValue temp;
	    /* mapcan */
	    foreach(temp,get_funcargs(clause))
	      retval = nconc (retval,pull_agg_clause(CAR(temp)));
	    }
	else if (IsA(clause,ArrayRef)) {
	    retval = pull_agg_clause(get_refindexpr((ArrayRef)clause));
	  }
	else if (not_clause (clause))
	    retval = pull_agg_clause(get_notclausearg(clause));
	else if (is_clause(clause))
	    retval = nconc (pull_agg_clause ((LispValue)get_leftop (clause)),
			pull_agg_clause ((LispValue)get_rightop (clause)));
	else
	    retval=  LispNil;
	return (retval);
}

/*
 *	find_varno
 *
 *	Descends down part of a parsetree (qual or tlist), looking for a VAR
 *	node that contains 'varno'.
 *
 *	Returns 1 if one is found, 0 if not.
 *
 *	XXX this is actually called by the executor, in InitPlan().
 */
find_varno(me, varno)
	LispValue me;
{
	LispValue i;
	int result = 0;
	
	if (lispNullp(me))
		return(0);

	switch (NodeType(me)) {
	case classTag(LispList):
		foreach (i, me) {
			if (find_varno(CAR(i), varno)) {
				result = 1;
				break;
			}
		}
		break;
	case classTag(ArrayRef):
		result = find_varno(get_refindexpr((ArrayRef) me), varno);
		break;
	case classTag(Var):
		result = (get_varno((Var) me) == varno);
		break;
	}
	return(result);
}

/*    
 *    	pull_var_clause
 *    
 *    	Recursively pulls all var nodes from a clause by pulling vars from the
 *    	left and right operands of the clause.  
 *    
 *    	Returns list of varnodes found.
 *    
 */

LispValue
pull_var_clause (clause)
     LispValue clause ;
{
	LispValue retval = LispNil;

	if (null (clause) ) 
	  return(LispNil);
	else if (IsA(clause,Var))
	  retval = lispCons (clause,LispNil);
	else if ( IsA(clause,Iter) )
		  retval = pull_var_clause(get_iterexpr((Iter)clause));
	else if (single_node (clause)) 
	  retval = LispNil;
	else if (or_clause (clause)) {
		LispValue temp;
		LispValue result = LispNil;
		/* mapcan */
		foreach (temp,get_orclauseargs(clause) )
		  retval = nconc(retval, pull_var_clause(CAR(temp)));
	} else if (is_funcclause (clause)) {
		LispValue temp;
		/* mapcan */
		foreach(temp,get_funcargs(clause)) 
		  retval = nconc (retval,pull_var_clause(CAR(temp)));
	} else if (IsA(clause,ArrayRef)) {
	    retval = nconc(pull_var_clause(get_refindexpr((ArrayRef)clause)),
			   pull_var_clause(get_refexpr((ArrayRef)clause)));
	} else if (not_clause (clause))
	  retval = pull_var_clause (get_notclausearg (clause));
	else if (is_clause (clause)) 
	  retval = nconc (pull_var_clause ((LispValue)get_leftop (clause)),
			  pull_var_clause ((LispValue)get_rightop (clause)));
	else retval = LispNil;
	return (retval);
}

/*    
 *    	var_equal
 *    
 *    	Returns t iff two var nodes correspond to the same attribute.
 */
bool
var_equal (var1,var2)
     LispValue var1,var2;
{
    if (IsA (var1,Var) && IsA (var2,Var) &&
	(get_varno ((Var)var1) == get_varno ((Var)var2)) &&
	(get_vartype ((Var)var1) == get_vartype ((Var)var2)) && 
	(get_varattno ((Var)var1) == get_varattno ((Var)var2))) {

         return(true);
    } else 
      return(false);
    
} /* var_equal */


/*    
 *    	numlevels
 *    
 *    	Returns the number of nesting levels in a var-node
 *    
 */

/*  .. replace-resultvar-refs
 */

int
numlevels (var)
     Var var;
{
    return(length (CDR ( get_varid (var))));
}

ObjectId
var_getrelid(var)
    Var var;
{
    int rt_id;

    rt_id = CInteger(CAR(get_varid(var)));
    return (ObjectId) CInteger(getrelid(rt_id, _query_range_table_));
}
