
/*     
 *      FILE
 *     	sortresult
 *     
 *      DECLARATION
 *      	Routines to deal with sorting nesting-level results
 *     
 */
/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		relation-sortkeys
 *     		sort-relation-paths
 *     		sort-level-result
 */

#include "internal.h"

#define TRUE  1
#define FALSE  0

extern LispValue sort_list_car();

/*    
 *    	relation-sortkeys
 *    
 *    	Scans through a target list and extracts from the target list the
 *    	relation, ordering, variables, and keys required for a sort result.
 *    	This is only done provided all sort keys belong to the same relation
 *    	and are not defined on nested attributes.
 *    
 *    	This information is all returned in the following property lists of a
 *    	single SortKey node:
 *    		Relid		the single relation
 *    		Ordering	list of reskeyops to sort with
 *    		VarKeys		list of vars to sort over
 *    		Keys		list of attnos (or attno/arrayindex pairs) for
 *    				the sorted relation
 *    	The latter three fields are returned in the order of the sort, i.e.,
 *    	in reskey order.
 *    
 *    	Returns:
 *    	a SortKey node - if valid sort keys are found
 *    	an integer N - if N sort keys are defined, but not all of them meet
 *    		       the above criteria
 *    	nil - if no sort keys were found at all
 *    
 *    	As a side effect, the reskey op in all target list entries will be
 *    	modified to contain the corresponding regproc id of the operator.
 *    
 */

/*  .. query_planner     */

LispValue
relation_sortkeys (tlist)
     LispValue tlist ;
{
     LispValue xtl = LispNil;

	foreach (xtl,tlist) {
	     int numkeys = 0;
	     Boolean allsame = TRUE;
	     LispValue relid = LispNil;
	     LispValue sort_ops = LispNil;
	     LispValue varkeys = LispNil;
	     LispValue keys = LispNil;
	     LispValue resdom = LispNil;
	     LispValue expr = LispNil;

	     /* end test  */
	     if ( null (xtl)) {
		  if(!allsame) 
		    return (numkeys);
		  else 
		    if (null (relid)) 
		      return (LispNil);
		    else 
		      return (new_sortkey (list (relid),
					   sort_list_car (sort_ops),
					   sort_list_car (varkeys),
					   sort_list_car (keys)));
	     }
	     resdom = tl_resdom (CAR (xtl));
	     expr = tl_expr (CAR (xtl));

	     /*  If reskey is non-zero, we've found a sort key of some kind. */
	     if (!(equal (0,get_reskey (resdom)))) {
		  if(!(var_p (expr)) ||
		     var_is_nested (expr) || 
		     (relid && ! (equal (relid,get_varno (expr)))))
		       allsame = FALSE;
		  else 
		    if (allsame) {
			 if ( null (relid)) 
			   relid = get_varno (expr);
			 push (list (get_reskey (resdom),
				     list (get_reskeyop (resdom))),sort_ops);
			 push (list (get_reskey (resdom),
				     list (expr)),varkeys);
			 if (get_vararrayindex(expr))
			   push (list (get_reskey (resdom),
					list (get_varattno (expr),
					      get_vararrayindex (expr))),
				 keys);
			 else 
			   push (list (get_reskey (resdom),
				       get_varattno (expr)),
				 keys);
		    } 
		  numkeys++;
		  set_reskeyop (resdom,get_opcode (get_reskeyop (resdom)));
	     }
	}
}  /* function end   */

/*    
 *    	sort-list-car
 *    
 *    	Sort 'list' by numerical comparison of the first subelements
 *    	(which, as it happens, are reskeys), then discard the reskeys.
 *    
 *    	Returns a sorted list of the second subelements of 'list'.
 *    
 *    	XXX this should be an flet in find_sortkey
 *    
 */

/*  .. relation-sortkeys	 */

LispValue
sort_list_car(list)
     LispValue list ;
{
     /* XXX for now, just do the simpliest bubble sort */
     LispValue temp1 = LispNil;
     LispValue temp2 = LispNil;
     LispValue temp = LispNil;
     LispValue t_list = LispNil;

     foreach(temp1,list)
       foreach(temp2,list)
	 if (CAR (temp1) >= CAR (temp2)) {
	      /* swap */
	      temp = temp1;
	      temp1 = temp2;
	      temp2 = temp;
	 }
     temp = LispNil;
     foreach(temp,list)
       t_list = nappend1(t_list,CADR(temp));

     return(t_list);

}  /* function end  */

/*    
 *    	sort-relation-paths
 *    
 *    	Scans through each path on a single relation entry.  If the ordering
 *    	of the path doesn't match that required by the resulting sort,
 *    	a flag is set indicating that an explicit sort must be done, and
 *    	the cost of the path is modified to reflect the sort.
 *    
 *    	'pathlist' is the list of possible paths for this relation
 *    	'sortkeys' is a SortKey node, as returned from (relation-sortkeys)
 *    	'rel' is the rel node of this relation
 *    	'width' is the width of a (typical) tuple
 *    
 *    	Returns nothing of interest.
 *    
 */

/*  .. subplanner     */
 
void
*sort_relation_paths (pathlist,sortkeys,rel,width)
LispValue pathlist,sortkeys,rel,width ;
{
     LispValue path = LispNil;
     foreach (path, pathlist) {
	  
	  /* For paths that require sorting, set 'newcost' to the additional */
	  /* sorting cost incurred.  If the ordering and keys don't match, */
	  /* sort explicitly.  For results that don't have to be explicitly */
	  /* sorted, account for the cost of putting tuples into a result */
	  /* relation. */
	  LispValue final_result = _query_result_relation_ &&
                         	    (1 == _query_max_level_);
	  int newcost = 0;

	  if(!equal_path_path_ordering (get_ordering(sortkeys),
					get_ordering (path)) ||
	     !(match_sortkeys_pathkeys (get_relid (sortkeys),
					get_sortkeys (sortkeys),
					get_keys (path)))) {
	       set_sortpath (path,sortkeys);
	       newcost = get_cost (path) + 
		 cost_sort (list(1),get_size (rel),width,final_result);
	  } 
	  else 
	    if (final_result) {
		 newcost = get_cost (path) + 
		   cost_result (get_size (rel),width);
	    }

	  /* Reset the path cost  if this path was the cheapest, readjust */
	  /* the cheapest field for the relation to reflect the new path */
	  /* cost, or if this new cost is now cheaper than that of the old */
	  /* cheapest path, change the cheapest path field. */
		
	  if (newcost) {
	       set_cost (path,newcost);
	       if(equal (path,get_cheapest_path (rel))) {
		    set_cheapest (rel,get_pathlist (rel));
	       } 
	       else 
		 if (newcost < get_cost (get_cheapest_path (rel))) {
		      set_cheapest_path (rel,path);
		 } 
	  }
     }
} /* function end  */

/*    
 *    	sort-level-result
 *    
 *    	If the result of (relation-sortkeys) was numeric, then the result must
 *    	be sorted explicitly, and a scan node and a sort node are attached to
 *    	the front of the result node.  The scan node is omitted if the result
 *    	is to be retrieved into a temporary, i.e., if *query-result-relation*
 *    	is non-nil.  That is, if there is a result relation, return
 *    		SORT <- plan ...
 *    	since this will be turned into
 *    		RESULT <- SORT <- plan ...
 *    	later, but if there is no result relation, return
 *    		SEQSCAN <- SORT <- plan ...
 *    
 *    	Returns the new plan.
 *    
 */

/*  .. create_plan, query_planner		 */

LispValue
sort_level_result (plan,numkeys)
     LispValue plan,numkeys ;
{
     LispValue new_tlist = new_unsorted_tlist (get_qptargetlist (plan));
     LispValue new_plan = LispNil;
     LispValue xtl = LispNil;

     if(_query_result_relation_)
       new_plan = make_sort (get_qptargetlist (plan),
			     _query_result_relation_,
			     plan,
			     numkeys);
     else 
       new_plan = make_seqscan (tlist_temp_references(_TEMP_RELATION_ID_,
						      new_tlist),
				LispNil,
				_TEMP_RELATION_ID_,
				make_sort (get_qptargetlist (plan),
					   _TEMP_RELATION_ID_,
					   plan,
					   numkeys));

     foreach (xtl, new_tlist) 
       set_resname (tl_resdom (xtl),LispNil);
     set_qptargetlist (plan,new_tlist);
     return (new_plan);
}  /* function end  */

