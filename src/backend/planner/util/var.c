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
#include "primnodes.h"
#include "primnodes.a.h"
#include "planner/clauses.h"

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

	if (IsA(clause,Var) && get_varattno(clause))
	  retval = lispCons (clause,LispNil);
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
	} else if (not_clause (clause))
	  retval = pull_var_clause (get_notclausearg (clause));
	else if (is_clause (clause)) 
	  retval = nconc (pull_var_clause (get_leftop (clause)),
			  pull_var_clause (get_rightop (clause)));
	return (retval);
}

/*    
 *    	var_equal
 *    
 *    	Returns t iff two var nodes correspond to the same attribute (and
 *    	array element, if applicable), ignoring the dot fields (making
 *    	"foo.bar.baz" equivalent to "foo.ack.hodedo.baz") unless 'dots' is
 *    	set.
 *    
 */
bool
var_equal (var1,var2,dots)
     LispValue var1,var2;
     bool dots;
{
    if (IsA (var1,Var) && IsA (var2,Var) &&
	(get_varno (var1) == get_varno (var2)) &&
	(get_varattno (var1) == get_varattno (var2))) {

/*   comment out for now since vararrayindex is always
	     nil until we get procedures working.

	if(dots != LispNil) {
	    *    Check for nested-dot equality. *
	    if (equal (get_vardotfields (var1),
			   get_vardotfields (var2)) &&
		equal (get_vararrayindex (var1),
		       get_vararrayindex (var2)));
	    return(true);
	} else {
			    
	     *    The vararrayindex field should be 
		  ignored unless 'var1' no 
		  longer has any nestings. *
	    if (var_is_nested (var1) ||
		equal (get_vararrayindex (var1),
			   get_vararrayindex (var2)));
	    return(true);
	}
*/

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
	if(varid_indexes_into_array (var)) {
		return ( length (CDR (get_varid (var))) - 1);
	} else {
		return(length (CDR ( get_varid (var))));
	}
}
