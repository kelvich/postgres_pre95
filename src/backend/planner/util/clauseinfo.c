
/*     
 *      FILE
 *     	clauseinfo
 *     
 *      DESCRIPTION
 *     	ClauseInfo node manipulation routines.
 *      $Header$ 
 */

/*     
 *      EXPORTS
 *     		valid-or-clause
 *     		get-actual-clauses
 *     		get_relattvals
 *     		get_joinvars
 *     		get_opnos
 */

#include "c.h"
#include "internal.h"
#include "relation.h"
#include "relation.a.h"
#include "clauses.h"
#include "clauseinfo.h"

/* declare (localf (get_joinvar)); */

/*    
 *    	valid-or-clause
 *    
 *    	Returns t iff the clauseinfo node contains a 'normal' 'or' clause.
 *    
 */

/*  .. create-or-index-paths, match-index-orclauses, set_rest_selec
 */

bool
valid_or_clause (clauseinfo)
	LispValue clauseinfo ;
{
     if (clauseinfo != LispNil && 
	 !single_node (get_clause (clauseinfo)) && 
	 !get_notclause (clauseinfo) &&
	 or_clause (get_clause (clauseinfo))) 
       return(true);
     else
       return(false);
}

/*    
 *    	get-actual-clauses
 *    
 *    	Returns a list containing the clauses from 'clauseinfo-list'.
 *    
 */

/*  .. create_indexscan_node, create_join_node, create_scan_node
 */
LispValue
get_actual_clauses (clauseinfo_list)
     LispValue clauseinfo_list ;
{
     LispValue temp;
     LispValue result;
     foreach(temp,clauseinfo_list) {
	  result = nappend1(result,get_clause(temp));
     }
}

/*    
 *     XXX NOTE:
 *    	The following routines must return their contents in the same order
 *    	(e.g., the first clause's info should be first, and so on) or else
 *    	get_index_sel() won't work.
 *    
 */

/*    
 *    	get_relattvals
 *    
 *    	For each member of  a list of clauseinfo nodes to be used with an
 *    	index, create a vectori-long specifying:
 *    		the attnos,
 *    		the values of the clause constants, and 
 *    		flags indicating the type and location of the constant within
 *    			each clause.
 *    	Each clause is of the form (op var some_type_of_constant), thus the
 *    	flag indicating whether the constant is on the left or right should
 *    	always be *SELEC-CONSTANT-RIGHT*.
 *    
 *    	'clauseinfo-list' is a list of clauseinfo nodes
 *    
 *    	Returns a list of vectori-longs.
 *    
 */

/*  .. create_index_path
 */
LispValue
get_relattvals (clauseinfo_list)
     LispValue clauseinfo_list ;
{
     LispValue result1 = LispNil;
     LispValue result2 = LispNil;
     LispValue result3 = LispNil;
     LispValue relattvals = LispNil;
     LispValue temp;

     foreach (temp,clauseinfo_list) {
	  relattvals = nappend1(relattvals,get_relattval (get_clause (temp)));
     }

     foreach(temp,relattvals) {
	  result1 = nappend1(result1,CADR(temp));
     }
     foreach(temp,relattvals) {
	  result2 = nappend1(result2,CADDR(temp));
     }
     foreach(temp,relattvals) {
	  result3 = nappend1(result3,CADDDR(temp));
     }
     return(list (result1,result2,result3));
}

/*    
 *    	get_joinvars 
 *    
 *    	Given a list of join clauseinfo nodes to be used with the index
 *    	of an inner join relation, return three lists consisting of:
 *    		the attributes corresponding to the inner join relation
 *    		the value of the inner var clause (always "")
 *    		whether the attribute appears on the left or right side of
 *    			the operator.
 *    
 *    	'relid' is the inner join relation
 *    	'clauseinfo-list' is a list of qualification clauses to be used with
 *    		'rel'
 *    
 */

/*  .. index-innerjoin
 */
LispValue
get_joinvars (relid,clauseinfo_list)
     LispValue relid,clauseinfo_list ;
{
     LispValue result1 = LispNil;
     LispValue result2 = LispNil;
     LispValue result3 = LispNil;
     LispValue relattvals = LispNil;
     LispValue temp;
     
     foreach(temp,clauseinfo_list) {
	  relattvals = nappend1(relattvals,get_joinvar(relid,temp));
     }
     
     foreach(temp,relattvals) {
	  result1 = nappend1(result1,CADR(temp));
     }
     foreach(temp,relattvals) {
	  result2 = nappend1(result2,CADDR(temp));
     }
     foreach(temp,relattvals) {
	  result3 = nappend1(result3,CADDDR(temp));
     }
     return(list (result1,result2,result3));
}

/*    
 *    	get_joinvar
 *    
 *    	Given a clause and a relid corresponding to one of the clause operands,
 *    	create a list consisting of:
 *    		the var's attno,
 *    		value, and
 *    		a flag indicating whether the join operand in a clause whose
 *    			relation corresponds to input 'relid' appears to the
 *    			right or left of the operator.
 *    	The operand corresponding to 'relid' is always a var node and "" is 
 *    	always returned for 'value'.
 *    
 *    	Returns a list containing the above information.
 *    
 */

/*  .. get_joinvars
 */
LispValue
get_joinvar (relid,clauseinfo)
     LispValue relid,clauseinfo ;
{
     Expr clause = get_clause (clauseinfo);
     if( var_p (get_leftop (clause)) &&
	 equal (relid,get_varno (get_leftop (clause)))) {
	  list (get_varattno (get_leftop (clause)),"",_SELEC_CONSTANT_RIGHT_);
     } else {
	  list (get_varattno (get_rightop (clause)),"",_SELEC_CONSTANT_LEFT_);
     } 
}

/*    
 *    	get_opnos
 *    
 *    	Create and return a list containing the clause operators of each member
 *    	of a list of clauseinfo nodes to be used with an index.
 *    
 */

/*  .. create_index_path, index-innerjoin
 */
LispValue
get_opnos (clauseinfo_list)
     LispValue clauseinfo_list ;
{
     LispValue temp;
     LispValue result = LispNil;

     foreach(temp,clauseinfo_list) {
	  result = nappend1(result,
			    get_opno (get_op (get_clause (temp))));
     }
}
