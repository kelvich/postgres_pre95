
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

#include "planner/internal.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "planner/clauses.h"

/* ----------------
 *	JInfo creator declaration
 * ----------------
 */
extern JInfo RMakeJInfo();

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
JInfo
joininfo_member (join_relids,joininfo_list)
     LispValue join_relids,joininfo_list ;
{
    LispValue i = LispNil;
    List other_rels = LispNil;
    foreach (i,joininfo_list) {
	other_rels = CAR(i);
	if (same(join_relids,get_otherrels(other_rels)))
	  return((JInfo)other_rels);
    }
    return((JInfo)NULL);
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

JInfo
find_joininfo_node (this_rel,join_relids)
     LispValue this_rel,join_relids ;
{
    JInfo joininfo = joininfo_member (join_relids,
				     get_joininfo (this_rel));
    if ( joininfo == NULL ) {
	joininfo = RMakeJInfo();
	set_otherrels (joininfo,join_relids);
	set_jinfoclauseinfo (joininfo,LispNil);
	set_mergesortable (joininfo,false);
	set_hashjoinable (joininfo,false);
	set_inactive (joininfo,false);
	set_joininfo (this_rel, lispCons (joininfo,get_joininfo (this_rel)));

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
Var
other_join_clause_var (var,clause)
     Var var;
     LispValue clause ;
{
     Var retval;
     Var l, r;

     retval = (Var) NULL;

     if ( var != NULL  && join_clause_p (clause)) {

	  l = (Var) get_leftop(clause);
	  r = (Var) get_rightop(clause);

	  if (var_equal(var, l)) {
	       retval = r;
	  } else if (var_equal (var, r)) {
	       retval = l;
	  }
     }

     return(retval);
}
