
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

#include "planner/internal.h"

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
    if (path_ordering1 == path_ordering2)
	return true;
    if (!path_ordering1 || !path_ordering2)
	return false;
    if ((IsA(path_ordering1,MergeOrder) && IsA(path_ordering2,MergeOrder)) ||
	(!IsA(path_ordering1,MergeOrder) && !IsA(path_ordering2,MergeOrder)))
	return equal((Node)path_ordering1, (Node)path_ordering2);
    if (IsA(path_ordering1,MergeOrder) && !IsA(path_ordering2,MergeOrder))
	return path_ordering2 && get_left_operator((MergeOrder)path_ordering1) == 
				 CInteger(CAR(path_ordering2));
    return path_ordering1 && CInteger(CAR(path_ordering1)) ==
			     get_left_operator((MergeOrder)path_ordering2);
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
	if (merge_ordering == LispNil) return(false);
	if ((CInteger(path_sortop) == get_left_operator((MergeOrder)merge_ordering)) ||
	    (CInteger(path_sortop) == get_right_operator((MergeOrder)merge_ordering)))
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
	if (equal ((Node)merge_ordering1,(Node)merge_ordering2))
	  return(true);
	else
	  return(false);
}
