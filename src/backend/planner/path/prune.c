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

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "planner/internal.h"
#include "planner/pathnode.h"
#include "planner/prune.h"

extern int testFlag;
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
prune_joinrels(rel_list)
     LispValue rel_list ;
{
     LispValue temp_list = LispNil;
     if( consp(rel_list) ) {
	  temp_list = lispCons(CAR(rel_list),
			    prune_joinrels(prune_joinrel((Rel)CAR(rel_list),
							  CDR(rel_list))));
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
prune_joinrel(rel,other_rels)
     Rel rel;
     List other_rels ;
{
     /*  XXX was mapcan   */
     
     LispValue i = LispNil;
     LispValue t_list = LispNil;
     LispValue temp_node = LispNil;
     Rel other_rel = (Rel)NULL;
     
     foreach(i,other_rels) {
	 other_rel = (Rel)CAR(i);
	 if(same(get_relids(rel),get_relids(other_rel))) {
	     set_pathlist(rel,add_pathlist(rel,
					     get_pathlist(rel),
					     get_pathlist(other_rel)));
	     t_list = nconc(t_list, LispNil);  /* XXX is this right ? */
	 } 
	 else {
	     temp_node = lispCons((LispValue)other_rel,LispNil);
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
prune_rel_paths(rel_list)
     List rel_list ;
{
    LispValue x = LispNil;
    LispValue y = LispNil;
    Path path;
    Rel rel = (Rel)NULL;
    JoinPath cheapest = (JoinPath)NULL;
    
    foreach(x, rel_list) {
	rel = (Rel)CAR(x);
	foreach(y, get_pathlist(rel)) {
	    path = (Path)CAR(y);
	    if(!get_p_ordering(path)) {
		break;
	      }	    
	  }
	cheapest = (JoinPath)prune_rel_path(rel, path);
	if(IsA(cheapest,JoinPath))
	{
	  set_size(rel,compute_joinrel_size(cheapest));
	}
	else
	  printf( "WARN: non JoinPath called. \n");
    }
}  /* function end  */

/* ------------------------------
 *	mergepath_contain_redundant_sorts
 *
 *	return true if path contains redundant sorts, i.e.,
 *	a sort on an already ordered relation
 *	note: current this routine is not used because
 *	we forbidden any explicit sorts since hashjoins
 *	can always do better.
 * -------------------------------
 */
bool
mergepath_contain_redundant_sorts(path)
MergePath path;
{
    MergeOrder morder;
    Path innerpath, outerpath;
    List outerorder, innerorder;
    List outerkeys, innerkeys;

    if(get_outersortkeys(path) || get_innersortkeys(path))
	return true;  /* a temporary hack to reduce plan space */
    else return false;

#if 0
    morder = (MergeOrder)get_p_ordering(path);
    outerpath = get_outerjoinpath(path);
    outerorder = get_p_ordering(outerpath);
    outerkeys = get_keys(outerpath);
    if(outerorder && 
	get_left_operator(morder) ==
	(IsA(outerorder,MergeOrder)?get_left_operator(outerorder):
	 CInteger(CAR(outerorder))) &&
	get_outersortkeys(path) &&
	outerkeys &&
	CAR(outerkeys) &&
	member(CAR(CAR(get_outersortkeys(path))),
	      CAR(outerkeys)))
	return true;
    innerpath = get_innerjoinpath(path);
    innerorder = get_p_ordering(innerpath);
    innerkeys = get_keys(innerpath);
    if(innerorder &&
	get_right_operator(morder) ==
	(IsA(innerorder,MergeOrder)?get_right_operator(innerorder):
	 CInteger(CAR(innerorder))) &&
	get_innersortkeys(path) &&
	innerkeys &&
	CAR(innerkeys) &&
	member(CAR(CAR(get_innersortkeys(path))),
	      CAR(innerkeys)))
	return true;
    return false;
#endif
}

/* --------------------------------------
 *	path_contain_rotated_mergepaths
 *
 *	return true if two paths are virtually the same except on
 *	the order of a mergejoin, such two paths should always
 *	have the same cost.
 * --------------------------------------
 */
bool
path_contain_rotated_mergepaths(p1, p2)
Path p1, p2;
{
    Path p;
    if(p1 == NULL || p2 == NULL) return NULL;
    if(IsA(p1,MergePath) && IsA(p2,MergePath)) {
       if(equal((Node)get_outerjoinpath((JoinPath)p1),
		(Node)get_innerjoinpath((JoinPath)p2)) &&
           equal((Node)get_innerjoinpath((JoinPath)p1),
		 (Node)get_outerjoinpath((JoinPath)p2)))
          return true;
      }
    if(IsA(p1,JoinPath) && IsA(p2,JoinPath)) {
       return path_contain_rotated_mergepaths(get_outerjoinpath((JoinPath)p1),
                                              get_outerjoinpath((JoinPath)p2)) ||
              path_contain_rotated_mergepaths(get_innerjoinpath((JoinPath)p1),
                                              get_innerjoinpath((JoinPath)p2));
      }
    return false;
}


/* --------------------------------------
 *	path_contain_redundant_indexscans
 *
 *	return true if path contains obviously useless indexscans, i.e.,
 *	if indxqual is nil and the order is not later utilized
 * --------------------------------------
 */
bool
path_contain_redundant_indexscans(path, order_expected)
Path path;
bool order_expected;
{
    if(IsA(path,MergePath)) {
        return path_contain_redundant_indexscans(get_outerjoinpath((JoinPath)path), 
				!get_outersortkeys((MergePath)path)) ||
               path_contain_redundant_indexscans(get_innerjoinpath((JoinPath)path), 
				!get_innersortkeys((MergePath)path));
      }
    if(IsA(path,HashPath)) {
        return path_contain_redundant_indexscans(get_outerjoinpath((JoinPath)path), 
						 false) ||
               path_contain_redundant_indexscans(get_innerjoinpath((JoinPath)path), 
						 false);
      }
    if(IsA(path,JoinPath)) {
        if(!IsA(get_innerjoinpath((JoinPath)path),IndexPath) &&
            !IsA(get_innerjoinpath((JoinPath)path),JoinPath) &&
            length(get_relids(get_parent((Path)get_innerjoinpath((JoinPath)path)))) == 1)
           return true;
        return path_contain_redundant_indexscans(get_outerjoinpath((JoinPath)path), 
						 order_expected) ||
               path_contain_redundant_indexscans(get_innerjoinpath((JoinPath)path), 
						 false);
      }
    if(IsA(path,IndexPath)) {
        return lispNullp(get_indexqual((IndexPath)path)) && !order_expected;
      }
    return false;
}

/* ----------------------------------------------
 *	test_prune_unnecessary_paths
 *
 *	prune paths of rel for tests, only meant to be called with
 *	testFlag is set.
 * -----------------------------------------------
 */
void
test_prune_unnecessary_paths(rel)
Rel rel;
{
     LispValue x, y;
     Path path, path1;
     List newpathlist = LispNil;
     List prunelist = LispNil;
     /* done in path generation, match_unsorted_outer and match_unsorted_inner
     foreach(x, get_pathlist(rel)) {
	 path = (Path)CAR(x);
	 if(IsA(path,MergePath)) {
	     if(mergepath_contain_redundant_sorts(path))
	     continue;
	   }
	 newpathlist = nappend1(newpathlist, path);
      }
     */
     foreach(x, get_pathlist(rel)) {
	 path = (Path)CAR(x);
	 if(!path_contain_redundant_indexscans(path,
			    (length(get_relids(get_parent(path))) !=
			     length(_base_relation_list_)))) {
	     newpathlist = nappend1(newpathlist, (LispValue)path);
	   }
       }
     foreach(x, newpathlist) {
	 path = (Path)CAR(x);
	 foreach(y, CDR(x)) {
	     path1 = (Path)CAR(y);
	     if(equal((Node)path, (Node)path1) ||
		 path_contain_rotated_mergepaths(path, path1))
		 prunelist = nappend1(prunelist, (LispValue)path1);
	   }
       }
     newpathlist = nset_difference(newpathlist, prunelist);
     set_pathlist(rel, newpathlist);
}

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
prune_rel_path(rel,unorderedpath)
     Rel rel ;
     Path unorderedpath ;
{
     Path cheapest = set_cheapest(rel,get_pathlist(rel));

     /* don't prune if not pruneable  -- JMH, 11/23/92 */
     if(!(eq(unorderedpath,cheapest)) && !testFlag
	&& get_pruneable(rel)) {

	  set_unorderedpath(rel,(PathPtr)NULL);
	  set_pathlist(rel,LispRemove((LispValue)unorderedpath,
				      get_pathlist(rel)));

     } else {

	  set_unorderedpath(rel,(PathPtr)unorderedpath);

     } 

     if(testFlag) {
	 test_prune_unnecessary_paths(rel);
       }
     
     return(cheapest);

}  /* function end  */


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
merge_joinrels(rel_list1,rel_list2)
LispValue rel_list1, rel_list2;
{
    LispValue xrel = LispNil;

    foreach(xrel,rel_list1) {
        Rel rel = (Rel)CAR(xrel);
        rel_list2 = prune_joinrel(rel,rel_list2);
    }
    return(append(rel_list1, rel_list2));
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
prune_oldrels(old_rels)
LispValue old_rels;
{
     Rel rel;
     LispValue joininfo_list, xjoininfo;

     if(old_rels == LispNil)
	  return(LispNil);

     rel = (Rel)CAR(old_rels);
     joininfo_list = get_joininfo(rel);
     if(joininfo_list == LispNil)
	 return(lispCons((LispValue)rel, prune_oldrels(CDR(old_rels))));
     foreach(xjoininfo, joininfo_list) {
	 JInfo joininfo = (JInfo)CAR(xjoininfo);
	 if(!joininfo_inactive(joininfo))
	      return(lispCons((LispValue)rel, prune_oldrels(CDR(old_rels))));
      }
     return(prune_oldrels(CDR(old_rels)));
}
