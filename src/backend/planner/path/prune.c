
/*     
 *      FILE
 *     	prune
 *     
 *      DESCRIPTION
 *     	Routines to prune redundant paths and relations
 *     
 */

/* RcsId("$Header$"); */

/*     
 *      EXPORTS
 *     		prune-joinrels
 *     		prune-rel-paths
 *     		prune-rel-path
 */

#include "relation.h"
#include "pathnode.h"
#include "prune.h"

#define foreach(elt,list)     for(elt=list;elt!=LispNil;elt=CDR(elt))



/*    
 *    	prune-joinrels
 *    
 *    	Removes any redundant relation entries from a list of rel nodes
 *    	'rel-list'.
 *    
 *    	Returns the resulting list. 
 *    
 */

/*  .. find-join-paths, prune-joinrels    */

LispValue
prune_joinrels (rel_list)
     LispValue rel_list ;
{
     LispValue temp_list = LispNil;
     if ( consp (rel_list) ) {
	  temp_list = lispCons (CAR (rel_list),
			    prune_joinrels (prune_joinrel(CAR (rel_list),
							  CDR (rel_list))));
     } 
     return(temp_list);

}  /* function end  */


/*    
 *    	prune-joinrel
 *    
 *    	Prunes those relations from 'other-rels' that are redundant with
 *    	'rel'.  A relation is redundant if it is built up of the same
 *    	relations as 'rel'.  Paths for the redundant relation are merged into
 *    	the pathlist of 'rel'.
 *    
 *    	Returns a list of non-redundant relations, and sets the pathlist field
 *    	of 'rel' appropriately.
 *    
 */

/*  .. prune-joinrels */

LispValue
prune_joinrel (rel,other_rels)
     LispValue rel,other_rels ;
{
     /*  XXX was mapcan   */
     
     LispValue other_rel = LispNil;
     LispValue t_list = LispNil;
     LispValue temp_node = LispNil;

     foreach(other_rel,other_rels) {
	  if(same (get_relids (rel),get_relids (other_rel))) {
	       set_pathlist (rel,add_pathlist (rel,
					       get_pathlist (rel),
					       get_pathlist (other_rel)));
	       t_list = nconc(t_list, LispNil);  /* XXX is this right ? */
	  } 
	  else {
	       temp_node = list (other_rel);
	       t_list = nconc(t_list,temp_node);
	  } 
     }
     return(t_list);
}  /* function end  */

/*    
 *    	prune-rel-paths
 *    
 *    	For each relation entry in 'rel-list' (which corresponds to a join
 *    	relation), set pointers to the unordered path and cheapest paths
 *    	(if the unordered path isn't the cheapest, it is pruned), and
 *    	reset the relation's size field to reflect the join.
 *    
 *    	Returns nothing of interest.
 *    
 */

/*  .. find-join-paths   */

void
*prune_rel_paths (rel_list)
LispValue rel_list ;
{
     LispValue rel = LispNil;
     foreach (rel, rel_list) {
	  /* XXX - let form, maybe incorrect */
	  /* XXX Lisp find_if_not function used  */
	  Path cheapest = 
	    prune_rel_path (rel,find_if_not (get_ordering(get_pathlist(rel))));
	  set_size (rel,compute_joinrel_size (cheapest));
     }
}  /* function end  */

/*    
 *    	prune-rel-path
 *    
 *    	Compares the unordered path for a relation with the cheapest path  if
 *    	the unordered path is not cheapest, it is pruned.
 *    
 *    	Resets the pointers in 'rel' for unordered and cheapest paths.
 *    
 *    	Returns the cheapest path.
 *    
 */

/*  .. find-rel-paths, prune-rel-paths	 */

Path
prune_rel_path (rel,unordered_path)
     LispValue rel,unordered_path ;
{
     /* XXX - let form, maybe incorrect */
     Path cheapest = set_cheapest (rel,get_pathlist (rel));
     if (!(eq (unordered_path,cheapest))) {
	  set_unordered_path (rel,LispNil);
	  set_pathlist (rel,LispDelete(unordered_path,get_pathlist (rel)));
     } 
     else {
	  set_unordered_path (rel,unordered_path);
     } 
     
     return(cheapest);

}  /* function end  */
