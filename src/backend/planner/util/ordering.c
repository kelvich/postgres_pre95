
/*     
 *      FILE
 *     	ordering
 *     
 *      DESCRIPTION
 *     	Routines to manipulate and compare merge and path orderings
 *     
 *      EXPORTS
 *     		equal-path-path-ordering
 *     		equal-path-merge-ordering
 *     		equal-merge-merge-ordering
 *
 *	$Header$
 */

#include "internal.h"

/*    
 *    	equal-path-path-ordering
 *    
 *    	Returns t iff two path orderings are equal.
 *    
 */

/*  .. find-index-paths, in-line-lambda%598037446, in-line-lambda%598037501
 *  .. sort-relation-paths
 */
bool
equal_path_path_ordering (path_ordering1,path_ordering2)
     LispValue path_ordering1,path_ordering2 ;
{
	if ( equal (path_ordering1,path_ordering2) )
	  return(true);
	else
	  return(false);
}

/*    
 *    	equal-path-merge-ordering
 *    
 *    	Returns t iff a path ordering is usable for ordering a merge join.
 *     XXX	Presently, this means that the first sortop of the path matches
 *    	either of the merge sortops.  Is there a "right" and "wrong"
 *    	sortop to match?
 *    
 */

/*  .. in-line-lambda%598037346, in-line-lambda%598037477
 */
bool
equal_path_merge_ordering (path_ordering,merge_ordering)
     LispValue path_ordering,merge_ordering ;
{
	/* XXX - let form, maybe incorrect */
	LispValue path_sortop = CAR (path_ordering);
	if (equal (path_sortop,mergeorder_left_operator (merge_ordering)) ||
	    equal (path_sortop,mergeorder_right_operator (merge_ordering)))
	  return(true);
	else
	  return(false);
}

/*    
 *    	equal-merge-merge-ordering
 *    
 *    	Returns t iff two merge orderings are equal.
 *    
 */

/*  .. in-line-lambda%598037477
 */
bool
equal_merge_merge_ordering (merge_ordering1,merge_ordering2)
     LispValue merge_ordering1,merge_ordering2 ;
{
	if (equal (merge_ordering1,merge_ordering2))
	  return(true);
	else
	  return(false);
}
