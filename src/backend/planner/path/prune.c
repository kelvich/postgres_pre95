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

#include "pg_lisp.h"
#include "relation.h"
#include "relation.a.h"
#include "planner/pathnode.h"
#include "planner/prune.h"



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

/*  .. prune-joinrels, merge_joinrels */

LispValue
prune_joinrel (rel,other_rels)
     LispValue rel,other_rels ;
{
     /*  XXX was mapcan   */
     
     LispValue i = LispNil;
     LispValue t_list = LispNil;
     LispValue temp_node = LispNil;
     Rel other_rel = (Rel)NULL;
     
     foreach(i,other_rels) {
	 other_rel = (Rel)CAR(i);
	 if(same (get_relids (rel),get_relids (other_rel))) {
	     set_pathlist (rel,add_pathlist (rel,
					     get_pathlist (rel),
					     get_pathlist (other_rel)));
	     t_list = nconc(t_list, LispNil);  /* XXX is this right ? */
	 } 
	 else {
	     temp_node = lispCons(other_rel,LispNil);
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
prune_rel_paths (rel_list)
     List rel_list ;
{
    LispValue temp = LispNil;
    LispValue temp2 = LispNil;
    LispValue temp3 = LispNil;
    Rel rel = (Rel)NULL;
    JoinPath cheapest = (JoinPath)NULL;
    
    foreach (temp, rel_list) { 	/* XXX Lisp find_if_not function used  */
	rel = (Rel)CAR(temp);
	foreach (temp2,get_pathlist(rel)) {
	    if (!get_p_ordering(CAR(temp2))) {
		temp3 = CAR(temp2);
		break;
	    }	    
	}
	cheapest = (JoinPath)prune_rel_path (rel,temp3);
	if (IsA(cheapest,JoinPath))
	  set_size (rel,compute_joinrel_size (cheapest));
	else
	  printf( "WARN: non JoinPath called. \n");
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
prune_rel_path (rel,unorderedpath)
     Rel rel ;
     Path unorderedpath ;
{
     Path cheapest = set_cheapest (rel,get_pathlist (rel));

     if (!(eq (unorderedpath,cheapest))) {

	  set_unorderedpath (rel,LispNil);
	  set_pathlist (rel,remove(unorderedpath,get_pathlist (rel)));

     } else {

	  set_unorderedpath (rel,unorderedpath);

     } 
     
     return(cheapest);

}  /* function end  */


#ifdef _xprs_
/*
 *      merge-joinrels
 *
 *      Given two lists of rel nodes that are already
 *      pruned, merge them into one pruned rel node list
 *
 *      'rel-list1' and
 *      'rel-list2' are the rel node lists
 *
 *      Returns one pruned rel node list
 */

/* .. find-join-paths
*/
LispValue
merge_joinrels (rel_list1,rel_list2)
LispValue rel_list1, rel_list2;
{
    LispValue xrel = LispNil;

    foreach (xrel,rel_list1) {
        LispValue rel = CAR(xrel);
        rel_list2 = prune_joinrel (rel,rel_list2);
    }
    return (append (rel_list1, rel_list2));
}

/*
 *	prune_oldrels
 *
 *	If all the joininfo's in a rel node are inactive,
 *	that means that this node has been joined into
 *	other nodes in all possible ways, therefore
 *	this node can be discarded.  If not, it will cause
 *	extra complexity of the optimizer.
 *
 *	old_rels is a list of rel nodes
 *	
 *	Returns a new list of rel nodes
 */

/* .. find_join_paths
*/
LispValue
prune_oldrels (old_rels)
LispValue old_rels;
{
     Rel rel;
     LispValue joininfo_list, xjoininfo;

     if (old_rels == LispNil)
	  return (LispNil);

     rel = (Rel)CAR(old_rels);
     joininfo_list = get_joininfo(rel);
     if (joininfo_list == LispNil)
	 return (lispCons (rel, prune_oldrels (CDR(old_rels))));
     foreach (xjoininfo, joininfo_list) {
	 JInfo joininfo = (JInfo)CAR(xjoininfo);
	 if (!joininfo_inactive(joininfo))
	      return (lispCons (rel, prune_oldrels (CDR(old_rels))));
      }
     return(prune_oldrels(CDR(old_rels)));
}
#endif /* _xprs */
