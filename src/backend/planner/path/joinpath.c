

/*     
 *      FILE
 *     	joinpath
 *     
 *      DESCRIPTION
 *     	Routines to find all possible paths for processing a set of joins
 *     
 */
/*  RcsId("$Header$");  */

/*     
 *      EXPORTS
 *     		find-all-join-paths
 */

#include "pg_lisp.h"
#include "internal.h"
#include "relation.h"
#include "relation.a.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "joinpath.h"
#include "relnode.h"
#include "mergeutils.h"
#include "hashutils.h"
#include "pathnode.h"
#include "joinutils.h"

#define    OUTER   1   /* These should be moved */
#define    INNER   0

extern bool _enable_hashjoin_;
extern bool _enable_mergesort_;



/*    
 *    	find-all-join-paths
 *    
 *    	Creates all possible ways to process joins for each of the join
 *    	relations in the list 'joinrels.'  Each unique path will be included
 *    	in the join relation's 'pathlist' field.
 *    
 *    	In reality, n-way joins are handled left-only (permuting clauseless
 *    	joins doesn't usually win much).
 *    
 *    	'joinrels' is the list of relation entries to be joined
 *    	'previous-level-rels' is the list of (outer) relation entries from
 *    		the previous join processing level
 *    	'nest-level' is the current attribute nesting level
 *    
 *    	Modifies the pathlist field of the appropriate rel node to contain
 *    	the unique join paths.
 *   
 *      Returns nothing of interest. (?) 
 *      It does a destructive modification.
 */

/*  .. find-all-join-paths, find-join-paths    */

void
find_all_join_paths (joinrels,previous_level_rels,nest_level)
     LispValue joinrels,previous_level_rels;
     int nest_level;
{
     LispValue mergeinfo_list = LispNil;
     LispValue hashinfo_list = LispNil;
     LispValue temp_list = LispNil;
     LispValue path = LispNil;

     if ( consp (joinrels) ) {
	  LispValue joinrel = CAR (joinrels);
	  LispValue innerrelid = 	  /*   grow left-only plan trees */
	    last_element (get_relids (joinrel));
	  Rel innerrel = get_rel (innerrelid);
	  Rel outerrel = rel_member (remove 
					   (innerrelid,
					    get_relids (joinrel)),
					   previous_level_rels);
	  LispValue bestinnerjoin = best_innerjoin(get_innerjoin
						   (innerrel),
						   get_relids(outerrel));
	  LispValue pathlist = LispNil;
	  
	  if ( _enable_mergesort_ ) {
	       mergeinfo_list = 
		 group_clauses_by_order (get_clauseinfo (joinrel),
					 get_relid (innerrel));
	  } 
	  
	  if ( _enable_hashjoin_ ) {
	       hashinfo_list = 
		 group_clauses_by_hashop(get_clauseinfo (joinrel),
					 get_relid (innerrel));
	  } 



/*    1. Consider mergesort paths where both relations must be explicitly 
 *       sorted. 
 */

	  pathlist = sort_inner_and_outer (joinrel,outerrel,
					   innerrel,mergeinfo_list);

/*    2. Consider paths where the outer relation need not be explicitly  
 *       sorted.  This may include either nestloops and mergesorts where 
 *       the outer path is already ordered. 
 */

	  pathlist = add_pathlist(joinrel,pathlist,
				  match_unsorted_outer(joinrel,
						       outerrel,
						       innerrel,
						       get_pathlist(outerrel),
						       get_cheapestpath 
						                  (innerrel),
						       bestinnerjoin,
						       mergeinfo_list));

/*    3. Consider paths where the inner relation need not be explicitly  
 *       sorted.  This may include nestloops and mergesorts  the actual
 *       nestloop nodes were constructed in (match-unsorted-outer). 
 *       Thus, this call is unnecessary for higher attribute nesting
 *       levels since index scans can't be used (all scans are on 
 *       temporaries).
 */

	  if ( 1 == nest_level) {
	       pathlist = 
		 add_pathlist (joinrel,pathlist,
			       match_unsorted_inner(joinrel,outerrel,
						    innerrel,
						    get_pathlist(innerrel),
						    mergeinfo_list));
	  }

/*    4. Consider paths where both outer and inner relations must be 
 *       hashed before being joined.
 */

	  pathlist = 
	    add_pathlist (joinrel,pathlist,
			  hash_inner_and_outer(joinrel,outerrel,
					       innerrel,hashinfo_list));

	  set_pathlist (joinrel,pathlist);

/*    'OuterJoinCost is only valid when calling (match-unsorted-inner) 
 *    with the same arguments as the previous invokation of 
 *    (match-unsorted-outer), so clear the field before going on. 
 */

	  temp_list = get_pathlist(innerrel);
	  foreach (path, temp_list) {
	       set_outerjoincost (path,LispNil);
	  }
	  find_all_join_paths (CDR (joinrels),
			       previous_level_rels,nest_level);
     }
}  /* function end */

/*    
 *    	best-innerjoin
 *    
 *    	Find the cheapest index path that has already been identified by
 *    	(indexable_joinclauses) as being a possible inner path for the given
 *    	outer relation in a nestloop join.
 *    
 *    	'join-paths' is a list of join nodes
 *    	'outer-relid' is the relid of the outer join relation
 *    
 *    	Returns the pathnode of the selected path.
 *    
 */

/*  .. find-all-join-paths    */

LispValue
best_innerjoin (join_paths,outer_relid)
     LispValue join_paths,outer_relid ;
{
     /* XXX - let form, maybe incorrect */
     LispValue cheapest = LispNil;
     LispValue join_path = LispNil;
     
     foreach (join_path, join_paths) {
	  if ( same(get_joinid(join_path),outer_relid)
		&& ((null (cheapest) 
		     || path_is_cheaper(join_path,cheapest)))) {
	       cheapest = join_path;
	  }
     }
     return(cheapest);

}  /*  function end   */

/*    
 *    	sort-inner-and-outer
 *    
 *    	Create mergesort join paths by explicitly sorting both the outer and
 *    	inner join relations on each available merge ordering.
 *    
 *    	'joinrel' is the join relation
 *    	'outerrel' is the outer join relation
 *    	'innerrel' is the inner join relation
 *    	'mergeinfo-list' is a list of nodes containing info on (mergesortable)
 *    		clauses for joining the relations
 *    	
 *    	Returns a list of mergesort paths.
 *    
 */

/*  .. find-all-join-paths     */

LispValue
sort_inner_and_outer (joinrel,outerrel,innerrel,mergeinfo_list)
     LispValue joinrel,outerrel,innerrel,mergeinfo_list ;
{
     LispValue ms_list = LispNil;
     LispValue xmergeinfo = LispNil;
     MergePath temp_node ;

     foreach (xmergeinfo,mergeinfo_list) {

	  LispValue outerkeys = 
	    extract_path_keys (joinmethod_keys(xmergeinfo),
			       get_targetlist (outerrel),
			       OUTER);

	  LispValue innerkeys = 
	    extract_path_keys (joinmethod_keys(xmergeinfo),
			       get_targetlist(innerrel),
			       INNER);

	  LispValue merge_pathkeys = 
	    new_join_pathkeys (outerkeys,get_targetlist(joinrel),
			       joinmethod_clauses (xmergeinfo));

	  temp_node = create_mergesort_path(joinrel,
					    get_size (outerrel),
					    get_size (innerrel),
					    get_width (outerrel),
					    get_width (innerrel),
					    get_cheapestpath(outerrel),
					    get_cheapestpath(innerrel),
					    merge_pathkeys,
					    get_mergesortorder(xmergeinfo),
					    joinmethod_clauses(xmergeinfo),
					    outerkeys,innerkeys);

	  ms_list = nappend1(ms_list,temp_node);
     }
     return(ms_list);

}  /*  function end */

/*    
 *    	match-unsorted-outer
 *    
 *    	Creates possible join paths for processing a single join relation
 *    	'joinrel' by employing either iterative substitution or
 *    	mergesorting on each of its possible outer paths (assuming that the
 *    	outer relation need not be explicitly sorted).
 *    
 *    	1. The inner path is the cheapest available inner path.
 *    	2. Mergesort wherever possible.  Mergesorts are considered if there
 *    	   are mergesortable join clauses between the outer and inner join
 *    	   relations such that the outer path is keyed on the variables
 *    	   appearing in the clauses.  The corresponding inner merge path is
 *    	   either a path whose keys match those of the outer path (if such a
 *    	   path is available) or an explicit sort on the appropriate inner
 *    	   join keys, whichever is cheaper.
 *    
 *    	'joinrel' is the join relation
 *    	'outerrel' is the outer join relation
 *    	'innerrel' is the inner join relation
 *    	'outerpath-list' is the list of possible outer paths
 *    	'cheapest-inner' is the cheapest inner path
 *    	'best-innerjoin' is the best inner index path (if any)
 *    	'mergeinfo-list' is a list of nodes containing info on mergesortable
 *    		clauses
 *    
 *    	Returns a list of possible join path nodes.
 *    
 */

/*  .. find-all-join-paths     */

List
match_unsorted_outer (joinrel,outerrel,innerrel,outerpath_list,
		      cheapest_inner,best_innerjoin,mergeinfo_list)
     Rel joinrel,outerrel,innerrel;
     List outerpath_list;
     Path cheapest_inner,best_innerjoin;
     List mergeinfo_list ;
{
     /*   XXX  was mapcan   */

     List outerpath;
     LispValue jp_list = LispNil;
     LispValue temp_node = LispNil;
     CInfo xmergeinfo;
     Expr clauses ;
     LispValue keyquals = LispNil;
     LispValue merge_pathkeys = LispNil;
     Path nestinnerpath;
     LispValue paths = LispNil;

     foreach(outerpath,outerpath_list) {
	  LispValue outerpath_ordering = 
	    get_p_ordering ((Path)CAR(outerpath));

	  if ( outerpath_ordering ) {
	       xmergeinfo = 
		 match_order_mergeinfo (outerpath_ordering,mergeinfo_list);
	  } 
	  
	  if (xmergeinfo ) {
	       clauses = get_clause (xmergeinfo);
	  } 

	  if ( clauses ) {
	       keyquals = 
		 match_pathkeys_joinkeys (get_keys (outerpath),
					  joinmethod_keys (xmergeinfo),
					  joinmethod_clauses (xmergeinfo),
					  OUTER);

	       if ( clauses ) {
		    merge_pathkeys = 
		      new_join_pathkeys (get_keys (outerpath),
					 get_targetlist (joinrel),clauses);
	       } 
	       else {
		    merge_pathkeys = get_keys (outerpath);
	       } 
	       
	       if (best_innerjoin &&
		   path_is_cheaper(best_innerjoin,cheapest_inner)) {
		    nestinnerpath = best_innerjoin;
	       } 
	       else {
		    nestinnerpath = cheapest_inner;
	       } 
	       
	       paths = 
		 lispCons (create_nestloop_path (joinrel,outerrel,
						 outerpath,nestinnerpath,
						 merge_pathkeys),
			   LispNil);
	       

	       if ( clauses && keyquals) {
		    bool path_is_cheaper_than_sort;
		    LispValue varkeys = LispNil;
		    
		    Path mergeinnerpath = 
		      match_paths_joinkeys (nth (0,keyquals),
					    outerpath_ordering,
					    get_pathlist (innerrel),
					    INNER);
		    path_is_cheaper_than_sort = 
		      (bool) (mergeinnerpath && 
			      (get_cost (mergeinnerpath) < 
			       (get_cost (cheapest_inner) +
				cost_sort (nth (0,keyquals)
					   ,get_size(innerrel),
					   get_width(innerrel),
					   LispNil))));
		    if (!path_is_cheaper_than_sort)  {
			 varkeys = 
			   extract_path_keys (nth (0,keyquals),
					      get_targetlist (innerrel),
					      INNER);
		    } 
		    

		    /*    Keep track of the cost of the outer path used with 
		     *    this ordered inner path for later processing in 
		     *    (match-unsorted-inner), since it isn't a sort and 
		     *    thus wouldn't otherwise be considered. 
		     */

		    if(path_is_cheaper_than_sort) {
			 set_outerjoincost (mergeinnerpath,
					    get_cost (outerpath));
		    } 
		    else {
			 mergeinnerpath = cheapest_inner;
		    } 
		    
		    temp_node = lispCons(create_mergesort_path(joinrel,
							   get_size (outerrel),
							   get_size (innerrel),
							   get_width (outerrel),
							   get_width (innerrel),
							   outerpath,
							   mergeinnerpath,
							   merge_pathkeys,
							   outerpath_ordering,
							   nth (1,keyquals),
							   LispNil,varkeys),
					 paths);
		    
	       } 
	       else {
		    temp_node = paths;
	       } 
	       jp_list = nconc(jp_list,temp_node);
	  }
     }
     return(jp_list);
}  /* function end  */

/*    
 *    	match-unsorted-inner 
 *    
 *    	Find the cheapest ordered join path for a given (ordered, unsorted)
 *    	inner join path.
 *    
 *    	Scans through each path available on an inner join relation and tries
 *    	matching its ordering keys against those of mergejoin clauses.
 *    	If 1. an appropriately-ordered inner path and matching mergeclause are
 *    	      found, and
 *    	   2. sorting the cheapest outer path is cheaper than using an ordered
 *    	      but unsorted outer path (as was considered in
 *    	      (match-unsorted-outer)),
 *    	then this merge path is considered.
 *    
 *    	'joinrel' is the join result relation
 *    	'outerrel' is the outer join relation
 *    	'innerrel' is the inner join relation
 *    	'innerpath-list' is the list of possible inner join paths
 *    	'mergeinfo-list' is a list of nodes containing info on mergesortable
 *    		clauses
 *    
 *    	Returns a list of possible merge paths.
 *    
 */

/*  .. find-all-join-paths    */

LispValue
match_unsorted_inner (joinrel,outerrel,innerrel,innerpath_list,mergeinfo_list)
     LispValue joinrel,outerrel,innerrel,innerpath_list,mergeinfo_list ;
{
     
     /*  XXX   was mapcan   */

     LispValue innerpath = LispNil;
     LispValue mp_list = LispNil;
     LispValue temp_node = LispNil;
     CInfo xmergeinfo;
     LispValue innerpath_ordering = get_ordering (innerpath);
     Expr clauses ;
     LispValue keyquals = LispNil;
     Cost temp1 ;
     bool temp2 ;

     if ( innerpath_ordering ) {
	  xmergeinfo = 
	    match_order_mergeinfo (innerpath_ordering,mergeinfo_list);
     } 

     if ( xmergeinfo ) {
	clauses = get_clause (xmergeinfo);
   } 

     if ( clauses ) {
	  keyquals = 
	    match_pathkeys_joinkeys (get_keys (innerpath),
				     joinmethod_keys (xmergeinfo),
				     joinmethod_clauses (xmergeinfo),
				     INNER);
     } 

     temp1 = get_cost(get_cheapestpath(outerrel)) + 
             cost_sort(nth(0,keyquals), get_size(outerrel),
		       get_width(outerrel), LispNil);

     temp2 = (bool) (null(get_outerjoincost(innerpath))
		     || (get_outerjoincost(innerpath) > temp1));

     if ( clauses        /*    'OuterJoinCost was set above in  */
	 && keyquals	 /*    (match-unsorted-outer) if it is applicable. */
	 && temp2) {

	  LispValue outerkeys = 
	    extract_path_keys(nth (0,keyquals),
			      get_targetlist (outerrel),
			      OUTER);
	  LispValue merge_pathkeys = 
	    new_join_pathkeys (outerkeys,get_targetlist (joinrel),clauses);

	  temp_node = lispCons(create_mergesort_path(joinrel,
						     get_size (outerrel),
						     get_size (innerrel),
						     get_width (outerrel),
						     get_width (innerrel),
						    get_cheapestpath(outerrel),
						     innerpath,merge_pathkeys,
						     innerpath_ordering,
						     nth (1,keyquals),
						     outerkeys,LispNil),
			       LispNil);

	  mp_list = nconc(mp_list,temp_node);
     } 
     return(mp_list);

}  /* function end  */

/*    
 *    	hash-inner-and-outer		XXX HASH
 *    
 *    	Create hashjoin join paths by explicitly hashing both the outer and
 *    	inner join relations on each available hash op.
 *    
 *    	'joinrel' is the join relation
 *    	'outerrel' is the outer join relation
 *    	'innerrel' is the inner join relation
 *    	'hashinfo-list' is a list of nodes containing info on (hashjoinable)
 *    		clauses for joining the relations
 *    	
 *    	Returns a list of hashjoin paths.
 *    
 */

/*  .. find-all-join-paths      */

LispValue
hash_inner_and_outer (joinrel,outerrel,innerrel,hashinfo_list)
     LispValue joinrel,outerrel,innerrel,hashinfo_list ;
{
     /* XXX  was mapCAR  */

     LispValue xhashinfo = LispNil;
     LispValue hjoin_list = LispNil;
     HashPath temp_node;
     
     foreach(xhashinfo,hashinfo_list) {
	  LispValue outerkeys = 
	    extract_path_keys(joinmethod_keys(xhashinfo),
			      get_targetlist (outerrel),OUTER);
	  LispValue innerkeys = 
	    extract_path_keys(joinmethod_keys(xhashinfo),
			      get_targetlist (innerrel),INNER);
	  LispValue hash_pathkeys = 
	    new_join_pathkeys(outerkeys,
			      get_targetlist(joinrel),
			      joinmethod_clauses (xhashinfo));

	  temp_node = create_hashjoin_path(joinrel,
					   get_size (outerrel),
					   get_size (innerrel),
					   get_width (outerrel),
					   get_width (innerrel),
					   get_cheapestpath (outerrel),
					   get_cheapestpath (innerrel),
					   hash_pathkeys,
					   get_hashjoinoperator (xhashinfo),
					   joinmethod_clauses (xhashinfo),
					   outerkeys,innerkeys);
	  hjoin_list = nappend1(hjoin_list,temp_node);
     }
     return(hjoin_list);

}   /* function end  */
