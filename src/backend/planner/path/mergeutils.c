
/*     
 *      FILE
 *     	mergeutils
 *     
 *      DESC<RIPTION
 *     	Utilities for finding applicable merge clauses and pathkeys
 *     
 */

/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		group-clauses-by-order
 *     		match-order-mergeinfo
 */

#include "internal.h"

extern LispValue match_order_mergeinfo();

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
     /* XXX - let form, maybe incorrect */
     LispValue mergeinfo_list = LispNil;
     LispValue clauseinfo = LispNil;
	
     foreach (clauseinfo, clauseinfo_list) {
	  /* XXX - let form, maybe incorrect */
	  LispValue merge_ordering = get_mergesortorder (clauseinfo);
	  
	  if ( merge_ordering) {
	       /*    Create a new mergeinfo node and add it to */
	       /*    'mergeinfo-list' if one does not yet exist for this */
	       /*    merge ordering. */
	       LispValue xmergeinfo = 
		 match_order_mergeinfo (merge_ordering,mergeinfo_list);
	       LispValue clause = get_clause (clauseinfo);
	       LispValue leftop = get_leftop (clause);
	       LispValue rightop = get_rightop (clause);
	       LispValue keys = LispNil;
	       
	       if(equal (inner_relid,get_varno (leftop))) {
		    keys = make_joinkey (outer(rightop),inner(leftop));
	       } 
	       else {
		    keys = make_joinkey (outer(leftop),inner(rightop));
	       } 

	       if ( null (xmergeinfo)) {
		    xmergeinfo = make_mergeinfo (ordering(merge_ordering));
		    push (xmergeinfo,mergeinfo_list);
	       }
	       push (clause,joinmethod_clauses (xmergeinfo));
	       push (keys,joinmethod_keys (xmergeinfo));
	  }
     }
     return(mergeinfo_list);

}  /* function end  */

/*
 * lambda function used to search the list 
 * mergeinfo_list for a mergeinfo node whose order field
 * equals 'ordering'
 */

bool
lambda_fun (xmergeinfo,ordering)
     LispValue xmergeinfo,ordering;
{
     LispValue xmergeorder = mergeinfo_ordering(xmergeinfo);
     
     return (( mergeorder_p(ordering) &&
	       equal_merge_merge_ordering(ordering,xmergeorder))
	     || (!mergeorder_p(ordering) &&
		 equal_path_merge_ordering(ordering,xmergeorder)));
}

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
LispValue
match_order_mergeinfo (ordering,mergeinfo_list)
     LispValue ordering,mergeinfo_list ;
{
     /* XXX  lisp find_if function */

     return (find_if (lambda_fun(mergeinfo_list,ordering)));  /* XXX fix me  */

}
