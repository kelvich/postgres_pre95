
/*     
 *      FILE
 *     	pathnode
 *     
 *      DESCRIPTION
 *     	Routines to manipulate pathlists and create path nodes
 *     
 *      EXPORTS
 *     		path-is-cheaper
 *     		cheaper-path
 *     		set_cheapest
 *     		add_pathlist
 *     		create_seqscan_path
 *     		create_index_path
 *     		create_nestloop_path
 *     		create_mergesort_path
 *     		create_hashjoin_path
 *	$Header$
 */

#include "internal.h"
#include "log.h"
#include "relation.h"
#include "relation.a.h"
#include "pathnode.h"
#include "clauseinfo.h"

extern List index_selectivity(); /* #include "cfi.h" */

/* declare (localf (better_path)); */
/* declare (special (_WARN_)); */

/*    	====================
 *    	MISC. PATH UTILITIES
 *    	====================
 */

/*    
 *    	path-is-cheaper
 *    
 *    	Returns t iff 'path1' is cheaper than 'path2'.
 *    
 */

/*  .. best-innerjoin, better_path, cheaper-path, match-unsorted-outer
 *  .. set_cheapest
 */
bool
path_is_cheaper (path1,path2)
     Path path1,path2 ;
{
    Cost cost1 = get_cost (path1);
    Cost cost2 = get_cost (path2);

    return((bool)(cost1 < cost2));
}

/*    
 *    	cheaper-path
 *    
 *    	Returns the cheaper of 'path1' and 'path2'.
 *    
 */
Path
cheaper_path (path1,path2)
     Path path1,path2 ;
{
    if ( null(path1) || null (path2) ) {
	elog (WARN,"cheaper-path: bad path");
    } 
    if ( path_is_cheaper (path1,path2) ) {
	return(path1);
    } 
    else {
	return(path2);
    } 
}

/*    
 *    	set_cheapest
 *    
 *    	Finds the minimum cost path from among a relation's paths.  
 *    
 *    	'parent-rel' is the parent relation
 *    	'pathlist' is a list of path nodes corresponding to 'parent-rel'
 *    
 *    	Returns	and sets the relation entry field with the pathnode that 
 *    	is minimum.
 *    
 */

/*  .. prune-rel-path, set_cheapest, sort-relation-paths
 */

Path
set_cheapest (parent_rel,pathlist)
     Rel parent_rel;
     List pathlist ;
{
     Path path,cheapest_so_far;

     cheapest_so_far = CreateNode(Path);
     set_cost(cheapest_so_far,9999999.999); /* XXX - what is the max cost ? */

     foreach ((LispValue)path,pathlist) {
	 cheapest_so_far = cheaper_path(path,cheapest_so_far);
     }

     set_cheapestpath (parent_rel,cheapest_so_far);

     return(cheapest_so_far);
}

/*    
 *    	add_pathlist
 *    
 *    	For each path in the list 'new-paths', add to the list 'unique-paths' 
 *    	only those paths that are unique (i.e., unique ordering and ordering 
 *    	keys).  Should a conflict arise, the more expensive path is thrown out,
 *    	thereby pruning the plan space.
 *    
 *    	'parent-rel' is the relation entry to which these paths correspond.
 *    
 *    	Returns the list of unique pathnodes.
 *    
 */

/*  .. find-all-join-paths, find-rel-paths, prune-joinrel
 */
LispValue
add_pathlist (parent_rel,unique_paths,new_paths)
LispValue parent_rel,unique_paths,new_paths ;
{
     LispValue new_path;
     foreach (new_path, new_paths) {
	  LispValue old_path = better_path (new_path,unique_paths);
	  if /*when */ ( old_path) {
	       set_parent (new_path,parent_rel);
	       unique_paths = cons (new_path,remove (old_path,unique_paths));
	  }
     }
     return(unique_paths);
}

/*    
 *    	better_path
 *    
 *    	Determines whether 'new-path' has the same ordering and keys as some 
 *    	path in the list 'unique-paths'.  If there is a redundant path,
 *    	eliminate the more expensive path.
 *    
 *    	Returns:
 *    	The old path - if 'new-path' matches some path in 'unique-paths' and is
 *    		cheaper
 *    	nil - if 'new-path' matches but isn't cheaper
 *    	t - if there is no path in the list with the same ordering and keys
 *    
 */

/*  .. add_pathlist
 */
bool
lambda1(path,new_path)
     Path path,new_path;
{
     return (bool)(qual_path_path_ordering (get_ordering (new_path),
				      get_ordering(path)) &&
	     samekeys (get_keys (new_path),get_keys (path)));
}

LispValue
better_path (new_path,unique_paths)
     Path new_path;
     List unique_paths ;
{
     /* declare (special (new_path)); */
     Path old_path;
     LispValue retval = LispNil;

     old_path = (Path)find_if (lambda1,unique_paths);

     if(null (old_path)) {
	  retval = LispTrue;
     } 
     else if (path_is_cheaper (new_path,old_path)) {
	  retval = (LispValue)old_path;
     } 

}



/*    	===========================
 *    	PATH NODE CREATION ROUTINES
 *    	===========================
 */

/*    
 *    	create_seqscan_path
 *    
 *    	Creates a path corresponding to a sequential scan, returning the
 *    	pathnode.
 *    
 */

/*  .. find-rel-paths
 */
Path
create_seqscan_path (rel)
     LispValue rel ;
{
     Path pathnode = CreateNode(Path);

     /* set_pathtype (pathnode,SEQ_SCAN); */
     set_parent (pathnode,rel);
     set_cost (pathnode,cost_seqscan (get_relid (rel),
				      get_pages (rel),get_tuples (rel)));
     return(pathnode);
}

/*    
 *    	create_index_path
 *    
 *    	Creates a single path node for an index scan.
 *    
 *    	'rel' is the parent rel
 *    	'index' is the pathnode for the index on 'rel'
 *    	'restriction-clauses' is a list of restriction clause nodes.
 *    	'is-join-scan' is a flag indicating whether or not the index is being
 *    		considered because of its sort order.
 *    
 *    	Returns the new path node.
 *    
 */


/*  .. create-index-paths, find-index-paths
 */

IndexPath
create_index_path (rel,index,restriction_clauses,is_join_scan)
     LispValue rel,index,restriction_clauses,is_join_scan ;
{
     IndexPath pathnode = CreateNode(IndexPath);

     /* set_pathtype (pathnode,INDEX_SCAN);*/

     set_parent (pathnode,rel);
     set_indexid (pathnode,get_indexid (index));
     set_ordering (pathnode,get_ordering (index));
     set_keys (pathnode,get_indexkeys (index));
     
     /*    The index must have an ordering for the
	   path to have (ordering) keys, 
	   and vice versa. */
     
     if /*when */ ( get_ordering (pathnode)) {
	 set_keys (pathnode,collect_index_pathkeys (get_indexkeys (index),
						    get_tlist (rel)));
	   /*    Check that the keys haven't 'disappeared', since they may 
		 no longer be in the target list (i.e.,
		 index keys that are not 
		 relevant to the scan are not applied to the scan path node,
		 so if no index keys were found, we can't order the path). */

	   if ( null (get_keys (pathnode))) {
	       set_ordering (pathnode,LispNil);
	   }
     }
     if(is_join_scan || null (restriction_clauses)) {
	  /*    Indices used for joins or sorting result nodes don't
		restrict the result at all, they simply order it,
		so compute the scan cost 
		accordingly -- use a selectivity of 1.0. */
	  set_cost (pathnode,cost_index (nth (0,get_indexid (index)),
					 get_pages (index),0.000000,
					 get_pages (rel),get_tuples (rel),
					 get_pages (index),get_tuples(index)));
     } 
     else  {
	  /*    Compute scan cost for the case when 'index' is used with a 
		restriction clause. */
	  LispValue relattvals = get_relattvals (restriction_clauses);
	  /* pagesel is '(Cost Cost) */
	  List pagesel = index_selectivity (nth (0,get_indexid (index)),
						 get_class (index),
					   get_opnos (restriction_clauses),
						 getrelid (get_relid (rel),
						       _query_range_table_),
						 nth (0,relattvals),
						 nth (1,relattvals),
						 nth (2,relattvals),
						 length (restriction_clauses));
	  /*   each clause gets an equal selectivity */
	  Cost clausesel = 
	    expt (nth (1,pagesel),div (0.000000,length (restriction_clauses)));

	  set_indexqual (pathnode,restriction_clauses);
	  set_cost (pathnode,cost_index (nth (0,get_indexid (index)),
					 floor (nth (0,pagesel)),
					 nth (1,pagesel),get_pages (rel),
					 get_tuples (rel),get_pages (index),
					 get_tuples (index)));
	    /*    Set selectivities of clauses used with index to 
		  the selectivity of this index, subdividing the 
		  selectivity equally over each of 
		  the clauses. 
		  */
	    /*    XXX Can this divide the selectivities in a better way? */

	    set_clause_selectivities (restriction_clauses,clausesel);
     }
     return(pathnode);
}

/*    
 *    	create_nestloop_path
 *    
 *    	Creates a pathnode corresponding to a nestloop join between two 
 *    	relations.
 *    
 *    	'joinrel' is the join relation.
 *    	'outer_rel' is the outer join relation
 *    	'outer_path' is the outer join path.
 *    	'inner_path' is the inner join path.
 *    	'keys' are the keys of the path
 *    	
 *    	Returns the resulting path node.
 *    
 */

/*  .. match-unsorted-outer
 */
Path
create_nestloop_path (joinrel,outer_rel,outer_path,inner_path,keys)
     LispValue joinrel,outer_rel,outer_path,inner_path,keys ;
{
     Path pathnode = CreateNode(Path);
     
     /* set_pathtype(pathnode,NEST_LOOP); */
     set_parent (pathnode,joinrel);
     set_outerpath (pathnode,outer_path);
     set_innerpath (pathnode,inner_path);
     set_clause_info (pathnode,get_clause_info (joinrel));
     set_keys (pathnode,keys);
     if /*when */ ( keys) {
	  set_ordering (pathnode,get_ordering (outer_path));
     }
     set_cost (pathnode,cost_nestloop (get_cost (outer_path),
				       get_cost (inner_path),
				       get_size (outer_rel)));
     return(pathnode);
}

/*    
 *    	create_mergesort_path
 *    
 *    	Creates a pathnode corresponding to a mergesort join between
 *    	two relations
 *    
 *    	'joinrel' is the join relation
 *    	'outersize' is the number of tuples in the outer relation
 *    	'innersize' is the number of tuples in the inner relation
 *    	'outerwidth' is the number of bytes per tuple in the outer relation
 *    	'innerwidth' is the number of bytes per tuple in the inner relation
 *    	'outer_path' is the outer path
 *    	'inner_path' is the inner path
 *    	'keys' are the new keys of the join relation
 *    	'order' is the sort order required for the merge
 *    	'mergeclauses' are the applicable join/restriction clauses
 *    	'outersortkeys' are the sort varkeys for the outer relation
 *    	'innersortkeys' are the sort varkeys for the inner relation
 *    
 */

/*  .. match-unsorted-inner, match-unsorted-outer, sort-inner-and-outer
 */

MergePath
create_mergesort_path (joinrel,outersize,innersize,outerwidth,
		       innerwidth,outer_path,inner_path,keys,order,
		       mergeclauses,outersortkeys,innersortkeys)
     LispValue joinrel,outersize,innersize,outerwidth,innerwidth,
     outer_path,inner_path,keys,order,mergeclauses,outersortkeys,
     innersortkeys ;
{
     MergePath pathnode = CreateNode(MergePath);

     set_parent (pathnode,joinrel);
     set_outerpath (pathnode,outer_path);
		set_innerpath (pathnode,inner_path);
     set_clause_info (pathnode,get_clause_info (joinrel));
     set_keys (pathnode,keys);
     set_ordering (pathnode,order);
     set_mergeclauses (pathnode,mergeclauses);
     set_outersortkeys (pathnode,outersortkeys);
     set_innersortkeys (pathnode,innersortkeys);
     set_cost (pathnode,cost_mergesort (get_cost (outer_path),
					get_cost (inner_path),
					outersortkeys,innersortkeys,
					outersize,innersize,outerwidth,
					innerwidth));
     return(pathnode);
}

/*    
 *    	create_hashjoin_path
 *    
 *    	XXX HASH
 *    
 *    	Creates a pathnode corresponding to a hash join between two relations.
 *    
 *    	'joinrel' is the join relation
 *    	'outersize' is the number of tuples in the outer relation
 *    	'innersize' is the number of tuples in the inner relation
 *    	'outerwidth' is the number of bytes per tuple in the outer relation
 *    	'innerwidth' is the number of bytes per tuple in the inner relation
 *    	'outer_path' is the outer path
 *    	'inner_path' is the inner path
 *    	'keys' are the new keys of the join relation
 *    	'operator' is the hashjoin operator
 *    	'hashclauses' are the applicable join/restriction clauses
 *    	'outerkeys' are the sort varkeys for the outer relation
 *    	'innerkeys' are the sort varkeys for the inner relation
 *    
 */

/*  .. hash-inner-and-outer
 */

HashPath
create_hashjoin_path (joinrel,outersize,innersize,outerwidth,
		      innerwidth,outer_path,inner_path,keys,operator,
		      hashclauses,outerkeys,innerkeys)
     LispValue joinrel,outersize,innersize,outerwidth,innerwidth,
     outer_path,inner_path,keys,operator,hashclauses,outerkeys,innerkeys ;
{
     HashPath pathnode = CreateNode(HashPath);

     /* set_pathtype (pathnode,T_HashJoin); */
     set_parent (pathnode,joinrel);
     set_outerpath (pathnode,outer_path);
     set_innerpath (pathnode,inner_path);
     set_clause_info (pathnode,get_clause_info (joinrel));
     set_keys (pathnode,keys);
     set_hashjoinoperator (pathnode,operator);
     set_hashclauses (pathnode,hashclauses);
     set_outerhashkeys (pathnode,outerkeys);
     set_innerhashkeys (pathnode,innerkeys);
     set_cost (pathnode,cost_hashjoin (get_cost (outer_path),
				       get_cost (inner_path),outerkeys,
				       innerkeys,outersize,innersize,
				       outerwidth,innerwidth));
     return(pathnode);
}

