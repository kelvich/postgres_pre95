
/*     
 *      FILE
 *     	path
 *     
 *      DESCRIPTION
 *     	Routines to find possible search paths for processing a query
 *     
 */
/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		find-paths
 */

#include "pg_lisp.h"

extern LispValue find_rel_paths();
extern LispValue find_join_paths();

/*    
 *    	find-paths
 *    
 *    	Finds all possible access paths for executing a query, returning the
 *    	top level list of relation entries.  
 *    
 *    	'rels' is the list of single relation entries appearing in the query
 *    	'nest-level' is the current attribute nesting level being processed
 *    	'sortkeys' contains info on sorting of the result
 *    
 */

/*  .. subplanner   */

LispValue
find_paths (rels,nest_level,sortkeys)
     LispValue rels,nest_level,sortkeys ;
{
  if ( and (length (rels) > 0,nest_level > 0) ) {
    /* Set the number of join (not nesting) levels yet to be processed. */
    /* XXX - let form, maybe incorrect */
    LispValue levels_left = length (rels);

    /* Find the base relation paths. */
    find_rel_paths (rels,nest_level,sortkeys);
    if ( and ( not (sortkeys), (1 == levels_left))) {
      /* Unsorted single relation, no more processing is required. */
      rels;       /* returns rels */
    } 
    else {
      
      /* Joins or sorts are required. */
      /* Compute the sizes, widths, and selectivities required for */
      /* further join processing and/or sorts. */
      LispValue rel;
      set_rest_relselec (rels);
      for (rel = car(rels); rels != LispNil; rels = cdr(rels)) {
	set_size (rel,compute_rel_size (rel));
	set_width (rel,compute_rel_width (rel));
      }
      if(1 == levels_left) {
	rels;         /* returns rels */
      } 
      else {
	find_join_paths (rels,levels_left - 1,nest_level);
      }
    }
  }
}  /* end find_paths */

/*    
 *    	find-rel-paths
 *    
 *    	Finds all paths available for scanning each relation entry in 
 *    	'rels'.  Sequential scan and any available indices are considered
 *    	if possible (indices are not considered for lower nesting levels).
 *    	All unique paths are attached to the relation's 'pathlist' field.
 *    
 *    	'level' is the current attribute nesting level, where 1 is the first.
 *    	'sortkeys' contains sort result info.
 *    
 *    	Returns 'rels'.
 *    
 */

/*  .. find-paths
 */
LispValue
find_rel_paths (rels,level,sortkeys)
     LispValue rels,level,sortkeys ;
{
  LispValue rel;
  /* XXX Lisp Dolist - may be wrong. need to return rels */
  for (rel = car(rels) ; rels != LispNil ; rels = cdr(rels)) {
    /* XXX - let form, maybe incorrect */
    LispValue sequential_scan_list = list (create_seqscan_path (rel));
    if (level > 1) {
      set_pathlist (rel,sequential_scan_list);
      set_unordered_path (rel,sequential_scan_list);
      set_cheapest_path (rel,sequential_scan_list);
      
    } 
    else {
      
      /* XXX Because these routines destructively modify the */
      /* relation data structures, the "let*" here is significant. */
      /* XXX - let form, may be incorrect */

      LispValue rel_index_scan_list = 
	find_index_paths (rel,find_relation_indices(rel),get_clause_info(rel),
			  get_join_info(rel),sortkeys);
      LispValue or_index_scan_list = 
	create_or_index_paths (rel,get_clause_info (rel));

      set_pathlist (rel,add_pathlist (rel,sequential_scan_list,
		    append (rel_index_scan_list,or_index_scan_list)));
    } 
    
    /* The unordered path is always the last in the list.  If it is not */
    /* the cheapest path, prune it. */
    
    prune_rel_path (rel,last_element (get_pathlist (rel)));
  }
}  /* end find_rel_path */

/*    
 *    	find-join-paths
 *    
 *    	Find all possible joinpaths for a query by successively finding ways
 *    	to join single relations into join relations.  
 *    
 *    	'outer-rels' is	the current list of relations for which join paths 
 *    		are to be found	
 *    	'levels-left' is the current join level being processed, where '1' is
 *    		the "last" level
 *    	'nest-level' is the current attribute nesting level
 *    
 *    	Returns the final level of join relations.
 *    
 */

/*  .. find-join-paths, find-paths
 */
LispValue
find_join_paths (outer_rels,levels_left,nest_level)
     LispValue outer_rels,levels_left,nest_level ;
{
  LispValue rel;

  /* Determine all possible pairs of relations to be joined at this level. */
  /* Determine paths for joining these relation pairs and modify 'new-rels' */
  /* accordingly, then eliminate redundant join relations. */
  /* XXX - let form, maybe incorrect */

  LispValue new_rels = find_join_rels (outer_rels);

  find_all_join_paths (new_rels,outer_rels,nest_level);
  new_rels = prune_joinrels (new_rels);

  /* Compute cost and sizes, then go on to next level of join processing. */

  prune_rel_paths (new_rels);
  for (rel= car(new_rels); new_rels != LispNil; new_rels = cdr(new_rels)) {
    set_width (rel,compute_rel_width (rel));
  }
  if(levels_left == 1) {
      new_rels;
    } 
  else {
    find_join_paths (new_rels,levels_left - 1,nest_level);
  } 
}
