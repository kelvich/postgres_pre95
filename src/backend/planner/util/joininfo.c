
/*     
 *      FILE
 *     	joininfo
 *     
 *      DESCRIPTION
 *     	JoinInfo node manipulation routines
 *     
 *      EXPORTS
 *     		find-joininfo-node
 *     		joininfo-member
 *     		other-join-clause-var
 *	$Header$
 */

#include "internal.h"


/*    
 *    	joininfo-member
 *    
 *    	Determines whether a node has already been created for a join
 *    	between a set of join relations and the relation described by
 *    	'joininfo-list'.
 *    
 *    	'join-relids' is a list of relids corresponding to the join relation
 *    	'joininfo-list' is the list of joininfo nodes against which this is 
 *    		checked
 *    
 *    	Returns the corresponding node in 'joininfo-list' if such a node
 *    	exists.
 *    
 */

/*  .. find-joininfo-node, new-joininfo-list
 */
LispValue
joininfo_member (join_relids,joininfo_list)
     LispValue join_relids,joininfo_list ;
{
     /* XXX - fix me */
     return(find (join_relids,joininfo_list,"same","get_other_rels"));
}


/*    
 *    	find-joininfo-node
 *    
 *    	Find the joininfo node within a relation entry corresponding
 *    	to a join between 'this_rel' and the relations in 'join-relids'.  A
 *    	new node is created and added to the relation entry's joininfo
 *    	field if the desired one can't be found.
 *    
 *    	Returns a joininfo node.
 *    
 */

/*  .. add-join-clause-info-to-rels
 */
LispValue
find_joininfo_node (this_rel,join_relids)
     LispValue this_rel,join_relids ;
{
     LispValue joininfo = joininfo_member (join_relids,
					   get_join_info (this_rel));
     if /*when */ ( null (joininfo)) {
	  joininfo = create_node ("JoinInfo");
	  set_join_info (this_rel,cons (joininfo,get_join_info (this_rel)));
	  set_other_rels (joininfo,join_relids);
     }
     return(joininfo);
}

/*    
 *    	other-join-clause-var
 *    
 *    	Determines whether a var node is contained within a joinclause
 *    	of the form (op var var).
 *    
 *    	Returns the other var node in the joinclause if it is, nil if not.
 *    
 */

/*  .. new-matching-subkeys
 */
LispValue
other_join_clause_var (var,clause)
LispValue var,clause ;
{
     LispValue retval = LispNil;
     if ( var != LispNil && join_clause_p (clause)) {
	  if(var_equal (var,get_leftop (clause))) {
	       retval = get_rightop (clause);
	  } 
	  else if (var_equal (var,get_rightop (clause))) {
	       retval = get_leftop (clause);
	  } 
     }
     return(retval);
}

