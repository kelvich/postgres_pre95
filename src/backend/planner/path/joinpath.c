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
#include "planner/internal.h"
#include "relation.h"
#include "relation.a.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "planner/joinpath.h"
#include "planner/relnode.h"
#include "planner/mergeutils.h"
#include "planner/hashutils.h"
#include "planner/pathnode.h"
#include "planner/joinutils.h"
#include "planner/keys.h"
#include "planner/costsize.h"

extern bool _enable_hashjoin_;
extern bool _enable_mergesort_;



/*    
 *    	find-all-join-paths
 *    
 *    	Creates all possible ways to process joins for each of the join
 *    	relations in the list 'joinrels.'  Each unique path will be included
 *    	in the join relation's 'pathlist' field.
 *    
 *    	In postgres, n-way joins are handled left-only (permuting clauseless
 *    	joins doesn't usually win much).
 *    
 *	In xprs, bushy-tree joins are cosidered for parallelism's sake
 *
 *    	'joinrels' is the list of relation entries to be joined
 *    	'previous-level-rels' is the list of (outer) relation entries from
 *    		the previous join processing level
 *    	'nest-level' is the current attribute nesting level
 *    
 *    	Modifies the pathlist field of the appropriate rel node to contain
 *    	the unique join paths.
 *	If bushy trees are considered, may modify the relid field of the
 *	join rel nodes to flatten the lists.
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
#ifdef _xprs_
     LispValue form_relid();
#endif /* _xprs_ */


     if ( consp (joinrels) ) {
	  LispValue joinrel = CAR (joinrels);
#ifdef _xprs_
          LispValue innerrelids = CADR (get_relids (joinrel));
          LispValue outerrelids = CAR (get_relids (joinrel));
          LispValue innerrelid =          /*   grow bushy plan trees */
            form_relid (innerrelids);
          LispValue outerrelid =
            form_relid (outerrelids);
          Rel innerrel = rel_member (innerrelids,previous_level_rels);
          Rel outerrel = rel_member (outerrelids,previous_level_rels);
#else /* _xprs_ */
	  LispValue innerrelid = 	  /*   grow left-only plan trees */
	    last_element (get_relids (joinrel));
	  Rel innerrel = get_rel (innerrelid);
	  Rel outerrel = rel_member (remove 
					   (innerrelid,
					    get_relids (joinrel)),
					   previous_level_rels);
#endif /* _xprs_ */
	  Path bestinnerjoin = best_innerjoin(get_innerjoin
						   (innerrel),
						   get_relids(outerrel));
	  LispValue pathlist = LispNil;
	  
	  if ( _enable_mergesort_ ) {
	       mergeinfo_list = 
		 group_clauses_by_order (get_clauseinfo (joinrel),
					 CAR(get_relids (innerrel)));
	  } 
	  
	  if ( _enable_hashjoin_ ) {
	      hashinfo_list = 
		group_clauses_by_hashop(get_clauseinfo (joinrel),
					CAR(get_relids (innerrel)));
	  } 
	  
#ifdef _xprs_
          set_relids (joinrel, append (outerrelids,innerrelids));
#endif /* _xprs_ */


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
	      if (IsA(path,JoinPath))
		set_outerjoincost (CAR(path),LispNil);
	      /* do it iff it is a join path, which is not always
		 true, esp since the base level */
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

Path
best_innerjoin (join_paths,outer_relid)
     LispValue join_paths,outer_relid ;
{
    Path cheapest = (Path)NULL;
    LispValue join_path = LispNil;
    
    foreach (join_path, join_paths) {
	if ( same(get_joinid(CAR(join_path)),outer_relid)
	    && ((null (cheapest) 
		 || path_is_cheaper(CAR(join_path),cheapest)))) {
	    cheapest = (Path)CAR(join_path);
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
     MInfo xmergeinfo = (MInfo)NULL;
     MergePath temp_node = (MergePath)NULL;
     LispValue i = LispNil;
     LispValue outerkeys = LispNil;
     LispValue innerkeys = LispNil;
     LispValue merge_pathkeys = LispNil;
     
     foreach (i,mergeinfo_list) {
	 xmergeinfo = (MInfo)CAR(i);

	 outerkeys = 
	   extract_path_keys (joinmethod_keys(xmergeinfo),
			      get_targetlist (outerrel),
			      OUTER);

	 innerkeys = 
	   extract_path_keys (joinmethod_keys(xmergeinfo),
			      get_targetlist(innerrel),
			      INNER);

	 merge_pathkeys = 
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
					    get_m_ordering(xmergeinfo),
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

    Path outerpath = (Path)NULL;
    LispValue jp_list = LispNil;
    LispValue temp_node = LispNil;
    LispValue merge_pathkeys = LispNil;
    Path nestinnerpath =(Path)NULL;
    LispValue paths = LispNil;
    LispValue i = LispNil;
    LispValue outerpath_ordering = LispNil;

    foreach(i,outerpath_list) {
	List clauses = LispNil;
	LispValue keyquals = LispNil;
        MInfo xmergeinfo = (MInfo)NULL;
	outerpath = (Path)CAR(i);
	outerpath_ordering = get_p_ordering (outerpath);
	
	if ( outerpath_ordering ) {
	    xmergeinfo = 
	      match_order_mergeinfo (outerpath_ordering,mergeinfo_list);
	} 
	
	if (xmergeinfo ) {
	    clauses = get_clauses (xmergeinfo);
	} 

	if ( clauses ) {
	    keyquals = 
	      match_pathkeys_joinkeys (get_keys (outerpath),
				       joinmethod_keys (xmergeinfo),
				       joinmethod_clauses (xmergeinfo),
				       OUTER);
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
		      (get_path_cost (mergeinnerpath) < 
		       (get_path_cost (cheapest_inner) +
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
				   get_path_cost (outerpath));
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
						     get_m_ordering(xmergeinfo),
						       nth (1,keyquals),
							   LispNil,varkeys),
				 paths);
	    
	} 
	else {
	    temp_node = paths;
	} 
	jp_list = nconc(jp_list,temp_node);
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
     Rel joinrel,outerrel,innerrel;
     List innerpath_list,mergeinfo_list ;
{
     
    Path innerpath = (Path)NULL;
    LispValue mp_list = LispNil;
    LispValue temp_node = LispNil;
    LispValue innerpath_ordering = LispNil;
    Cost temp1  = 0.0;
    bool temp2 = false;
    LispValue i = LispNil;

    
    foreach (i,innerpath_list) {
        MInfo xmergeinfo = (MInfo)NULL;
        List clauses = LispNil;
        LispValue keyquals = LispNil;
	innerpath = (Path)CAR(i);
	innerpath_ordering = get_p_ordering (innerpath);
	if ( innerpath_ordering ) {
	    xmergeinfo = 
	      match_order_mergeinfo (innerpath_ordering,mergeinfo_list);
	} 
	
	if ( xmergeinfo ) {
	    clauses = get_clauses (xmergeinfo);
	} 
	
	if ( clauses ) {
	    keyquals = 
	      match_pathkeys_joinkeys (get_keys (innerpath),
				       joinmethod_keys (xmergeinfo),
				       joinmethod_clauses (xmergeinfo),
				       INNER);
	} 
	
	/*    (match-unsorted-outer) if it is applicable. */
	/*    'OuterJoinCost was set above in  */
	if ( clauses && keyquals) {
	    temp1 = get_path_cost(get_cheapestpath(outerrel)) + 
	      cost_sort(nth(0,keyquals), get_size(outerrel),
			get_width(outerrel), LispNil);
	    
	    temp2 = (bool) (null(get_outerjoincost(innerpath))
			    || (get_outerjoincost(innerpath) > temp1));
	
	    if (temp2) {
		
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
							   get_cheapestpath
							   (outerrel),
							   innerpath,merge_pathkeys,
						     get_m_ordering(xmergeinfo),
							   nth (1,keyquals),
							   outerkeys,LispNil),
				     LispNil);
		
		mp_list = nconc(mp_list,temp_node);

	    } /* if temp2 */
	} /* if clauses */
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
     Rel joinrel,outerrel,innerrel;
     LispValue hashinfo_list ;
{
    /* XXX  was mapCAR  */
    
    HInfo xhashinfo = (HInfo)NULL;
    LispValue hjoin_list = LispNil;
    HashPath temp_node = (HashPath)NULL;
    LispValue i = LispNil;
    LispValue outerkeys = LispNil;
    LispValue innerkeys = LispNil;
    LispValue hash_pathkeys = LispNil;
    
    foreach(i,hashinfo_list) {
	xhashinfo = (HInfo)CAR(i);
	outerkeys = 
	  extract_path_keys(get_jmkeys(xhashinfo),
 			    get_targetlist (outerrel),OUTER);
	innerkeys = 
	  extract_path_keys(get_jmkeys(xhashinfo),
 			    get_targetlist (innerrel),INNER);
	hash_pathkeys = 
	  new_join_pathkeys(outerkeys,
			    get_targetlist(joinrel),
			    get_clauses (xhashinfo));
	
	temp_node = create_hashjoin_path(joinrel,
					 get_size (outerrel),
					 get_size (innerrel),
					 get_width (outerrel),
					 get_width (innerrel),
					 get_cheapestpath (outerrel),
					 get_cheapestpath (innerrel),
					 hash_pathkeys,
					 get_hashop (xhashinfo),
					 get_clauses (xhashinfo),
					 outerkeys,innerkeys);
	hjoin_list = nappend1(hjoin_list,temp_node);
    }
    return(hjoin_list);
    
}   /* function end  */

#ifdef _xprs_
/*
 *      form_relid
 *
 */

LispValue
form_relid (relids)
        LispValue relids;
{
  if (length (relids) == 1)
    return (CAR (relids));
  else
    return (relids);
}
#endif /* _xprs_ */
