
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

#include "tmp/c.h"

#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "planner/internal.h"
#include "planner/clauses.h"
#include "planner/clauseinfo.h"

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
	 !single_node (get_clause ((CInfo)clauseinfo)) && 
	 !get_notclause ((CInfo)clauseinfo) &&
	 or_clause ((LispValue)get_clause ((CInfo)clauseinfo))) 
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
  LispValue temp = LispNil;
  LispValue result = LispNil;
  CInfo clause = (CInfo)NULL;

  foreach(temp,clauseinfo_list) {
    clause = (CInfo)CAR(temp);
    result = nappend1(result,(LispValue)get_clause(clause));
  }
  return(result);
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
     CInfo temp = (CInfo)NULL;
     LispValue i = LispNil;
     LispValue temp2 = LispNil;
     
     foreach (i,clauseinfo_list) {
	 temp = (CInfo)CAR(i);
	 relattvals = nappend1(relattvals,
			       (LispValue)get_relattval (get_clause (temp)));
     }

     foreach(i,relattvals) {
	 temp2 = CAR(i);
	 result1 = nappend1(result1,CADR(temp2));
     }
     foreach(i,relattvals) {
	 temp2 = CAR(i);
	 result2 = nappend1(result2,CADDR(temp2));
     }
     foreach(i,relattvals) {
	 temp2 = CAR(i);
	 result3 = nappend1(result3,CADDR(CDR(temp2)));
     }
     return(lispCons(result1,
		     lispCons(result2,
			      lispCons(result3, LispNil))));
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
	  relattvals = nappend1(relattvals,
				get_joinvar((ObjectId)CInteger(relid),
					    (CInfo)CAR(temp)));
     }
     
     foreach(temp,relattvals) {
	  result1 = nappend1(result1,CAR(CAR(temp)));
     }
     foreach(temp,relattvals) {
	  result2 = nappend1(result2,CADR(CAR(temp)));
     }
     foreach(temp,relattvals) {
	  result3 = nappend1(result3,CADDR(CAR(temp)));
     }
     return(lispCons (result1,
		      lispCons(result2,
			       lispCons(result3,LispNil))));
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
     ObjectId relid;
     CInfo clauseinfo ;
{
    Expr clause = get_clause (clauseinfo);

    if( IsA (get_leftop ((LispValue)clause),Var) &&
       (relid == get_varno( get_leftop( (LispValue)clause)))) {

	return(lispCons(lispInteger(get_varattno(get_leftop((LispValue)clause))),
			  lispCons(lispString(""),
				   lispCons(lispInteger
					    (_SELEC_CONSTANT_RIGHT_),
					    LispNil))));
     } else {
	 return(lispCons (lispInteger(get_varattno(get_rightop( (LispValue)clause))),
			  lispCons(lispString(""),
				   lispCons(lispInteger(_SELEC_CONSTANT_LEFT_),
					    LispNil))));
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
    CInfo temp = (CInfo)NULL;
    LispValue result = LispNil;
    LispValue i = LispNil;

     foreach(i,clauseinfo_list) {
	 temp = (CInfo)CAR(i);
	  result =
	    nappend1(result,
		     (LispValue)get_opno(get_op( (LispValue)get_clause(temp))));
     }
    return(result);
}
