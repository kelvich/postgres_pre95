
/*     
 *      FILE
 *     	mergeutils
 *     
 *      DESCRIPTION
 *     	Utilities for finding applicable merge clauses and pathkeys
 *     
 */

/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		group-clauses-by-order
 *     		match-order-mergeinfo
 */

#include "pg_lisp.h"
#include "planner/internal.h"
#include "relation.h"
#include "relation.a.h"
#include "planner/mergeutils.h"
#include "planner/clauses.h"



/*    
 *    	group-clauses-by-order
 *    
 *    	If a join clause node in 'clauseinfo-list' is mergesortable, store
 *    	it within a mergeinfo node containing other clause nodes with the same
 *    	mergesort ordering.
 *    
 *    	'clauseinfo-list' is the list of clauseinfo nodes
 *    	'inner-relid' is the relid of the inner join relation
 *    
 *    	Returns the new list of mergeinfo nodes.
 *    
 */
LispValue
group_clauses_by_order (clauseinfo_list,inner_relid)
     LispValue clauseinfo_list,inner_relid ;
{
     LispValue mergeinfo_list = LispNil;
     LispValue clauseinfo = LispNil;
	
     foreach (clauseinfo, clauseinfo_list) {
	  MergeOrder merge_ordering = get_mergesortorder (clauseinfo);
	  
	  if ( merge_ordering) {
	       /*    Create a new mergeinfo node and add it to */
	       /*    'mergeinfo-list' if one does not yet exist for this */
	       /*    merge ordering. */
	       CInfo xmergeinfo = 
		 match_order_mergeinfo (merge_ordering,mergeinfo_list);
	       Expr clause = get_clause (clauseinfo);
	       Var leftop = get_leftop (clause);
	       Var rightop = get_rightop (clause);
	       JoinKey keys;
	       
	       if(equal (inner_relid,get_varno (leftop))) {
		    keys = MakeJoinKey (rightop,leftop);
	       } 
	       else {
		    keys = MakeJoinKey (leftop,rightop);
	       } 

	       if ( null (xmergeinfo)) {
		    xmergeinfo = CreateNode (CInfo);
		    set_mergesortorder(xmergeinfo,merge_ordering);
		    push (xmergeinfo,mergeinfo_list);
	       }
	       push (clause,joinmethod_clauses (xmergeinfo));
	       push (keys,joinmethod_keys (xmergeinfo));
	  }
     }
     return(mergeinfo_list);

}  /* function end  */



/*    
 *    	match-order-mergeinfo
 *    
 *    	Searches the list 'mergeinfo-list' for a mergeinfo node whose order
 *    	field equals 'ordering'.
 *    
 *    	Returns the node if it exists.
 *    
 */

/*  .. group-clauses-by-order, match-unsorted-inner, match-unsorted-outer
 */
CInfo
match_order_mergeinfo (ordering,mergeinfo_list)
     LispValue ordering,mergeinfo_list ;
{
     MergeOrder xmergeorder;
     LispValue xmergeinfo = LispNil;
     bool temp1 ;
     bool temp2 ;
     foreach(xmergeinfo, mergeinfo_list) {
	  xmergeorder = get_mergesortorder(xmergeinfo);
	  temp1 =(bool)( IsA(ordering,MergeOrder) &&
			equal_merge_merge_ordering(ordering,xmergeorder));
	  temp2 = (bool) (!IsA(ordering,MergeOrder) &&
			  equal_path_merge_ordering(ordering,xmergeorder));
	  if (temp1 || temp2)
	    return((CInfo)xmergeinfo);
	  
     }
     return((CInfo) LispNil);
}
