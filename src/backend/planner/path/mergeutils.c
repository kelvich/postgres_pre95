
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

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "planner/internal.h"
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
     LispValue xclauseinfo = LispNil;
	
     foreach (xclauseinfo, clauseinfo_list) {
	  CInfo clauseinfo = (CInfo)CAR(xclauseinfo);
	  MergeOrder merge_ordering = get_mergesortorder (clauseinfo);
	  
	  if ( merge_ordering) {
	       /*    Create a new mergeinfo node and add it to */
	       /*    'mergeinfo-list' if one does not yet exist for this */
	       /*    merge ordering. */
	       MInfo xmergeinfo = 
		 match_order_mergeinfo (merge_ordering,mergeinfo_list);
	       LispValue clause = get_clause (clauseinfo);
	       Var leftop = get_leftop (clause);
	       Var rightop = get_rightop (clause);
	       JoinKey keys;
	       
	       if(equal ((Node)inner_relid,
			 (Node)lispInteger(get_varno (leftop)))) {
		    keys = MakeJoinKey ((LispValue)rightop,(LispValue)leftop);
	       } 
	       else {
		    keys = MakeJoinKey ((LispValue)leftop,(LispValue)rightop);
	       } 

	       if ( null (xmergeinfo)) {
		    xmergeinfo = RMakeMInfo();
		    set_m_ordering(xmergeinfo,merge_ordering);
		    mergeinfo_list = push ((LispValue)xmergeinfo,
					   mergeinfo_list);
	       }

	       set_clauses((JoinMethod)xmergeinfo,
	                push (clause,
			      joinmethod_clauses((JoinMethod)xmergeinfo)));
	       set_jmkeys((JoinMethod)xmergeinfo, 
			 push((LispValue)keys,
			      joinmethod_keys((JoinMethod)xmergeinfo)));
	  }
     }
     return(mergeinfo_list);

}  /* function end  */


dump_rel(rel)

Rel rel;

{
	printf("relids: %d\n", rel->relids);
	printf("indexed %c\n", (rel->indexed ? 't' : 'f'));
	printf("pages %d tuples %d size %d width %d\n", rel->pages, rel->tuples,
		   rel->size, rel->width);
	printf("targetlist: \n");
	lispDisplay(rel->targetlist);
	printf("\npathlist: \n");
	lispDisplay(rel->targetlist);
	printf("\nclauseinfo: \n");
	lispDisplay(rel->clauseinfo);
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
MInfo
match_order_mergeinfo (ordering,mergeinfo_list)
     MergeOrder ordering;
     List mergeinfo_list ;
{
     MergeOrder xmergeorder;
     LispValue xmergeinfo = LispNil;
     bool temp1 ;
     bool temp2 ;
     foreach(xmergeinfo, mergeinfo_list) {
	  MInfo mergeinfo = (MInfo)CAR(xmergeinfo);
	  xmergeorder = get_m_ordering(mergeinfo);
	  temp1 =(bool)( IsA(ordering,MergeOrder) &&
			equal_merge_merge_ordering(ordering,xmergeorder));
	  temp2 = (bool) (!IsA(ordering,MergeOrder) &&
			  equal_path_merge_ordering(ordering,xmergeorder));
	  if (temp1 || temp2)
	    return(mergeinfo);
	  
     }
     return((MInfo) LispNil);
}
