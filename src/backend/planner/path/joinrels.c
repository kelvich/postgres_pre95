
/*     
 *      FILE
 *     	joinrels
 *     
 *      DESCRIPTION
 *     	Routines to determine which relations should be joined
 *     
 */

/*  RcsId("$Header$");  */

/*     
 *      EXPORTS
 *     		find-join-rels
 */

#include "pg_lisp.h"
#include "internal.h"
#include "relation.h"
#include "relation.a.h"
#include "joinrels.h"
#include "joininfo.h"


/*  These are functions defined in relation.l but not in relation.c.
 *  They need to either be converted, or replaced by their 
 *  equivalent new functions.
 */
extern LispValue get_other_rels();  /*
				     * The OtherRels field is a list of 
				     * relids used in the join clause
				     */
extern LispValue get_join_list();    /* returns a list of relids to which
				      
/*    
 *    	find-join-rels
 *    
 *    	Find all possible joins for each of the outer join relations in
 *    	'outer-rels'.  A rel node is created for each possible join relation,
 *    	and the resulting list of nodes is returned.  If at all possible, only
 *    	those relations for which join clauses exist are considered.  If none
 *    	of these exist for a given relation, all remaining possibilities are
 *    	considered.
 *    
 *    	'outer-rels' is the list of rel nodes
 *    
 *    	Returns a list of rel nodes corresponding to the new join relations.
 *    
 */

/*  .. find-join-paths    */

LispValue
find_join_rels (outer_rels)
     LispValue outer_rels ;
{
     LispValue outer_rel = LispNil;
     LispValue temp = LispNil;
     LispValue t_list = LispNil;
     
     foreach (outer_rel,outer_rels) {
	  if (find_clause_joins (outer_rel,get_join_info (outer_rel)))
	    temp = find_clause_joins (outer_rel,get_join_info (outer_rel));
	  else
	    temp = find_clauseless_joins (outer_rel,_query_relation_list_);
	  t_list = nconc(t_list,temp);   /*  XXX is this right?  */
     }
     return(t_list);

}  /* function end  */

/*    
 *    	find-clause-joins
 *    
 *    	Determines whether joins can be performed between an outer relation
 *    	'outer-rel' and those relations within 'outer-rel's joininfo nodes
 *    	(i.e., relations that participate in join clauses that 'outer-rel'
 *    	participates in).  This is possible if all but one of the relations
 *    	contained within the join clauses of the joininfo node are already
 *    	contained within 'outer-rel'.
 *    
 *    	'outer-rel' is the relation entry for the outer relation
 *    	'joininfo-list' is a list of join clauses which 'outer-rel' 
 *    		participates in
 *    
 *    	Returns a list of new join relations.
 *    
 */

/*  .. find-join-rels    */

LispValue
find_clause_joins (outer_rel,joininfo_list)
LispValue outer_rel,joininfo_list ;
{
     /*  XXX  was mapcan   */
     LispValue joininfo;
     LispValue temp_node = LispNil;
     LispValue t_list = LispNil;
     
     foreach(joininfo,joininfo_list) {
	  LispValue other_rels = get_other_rels (joininfo);
	  if ( 1 == length (other_rels) ) {
	       temp_node = list (init_join_rel(outer_rel,
					       get_rel(car (other_rels)),
					       joininfo));
	       t_list = nconc(t_list,temp_node);
	  } 
	  else {
	       t_list = nconc(t_list,LispNil);
	  } 
     }
     return(t_list);

}  /* function end  */

/*    
 *    	find-clauseless-joins
 *    
 *    	Given an outer relation 'outer-rel' and a list of inner relations
 *    	'inner-rels', create a join relation between 'outer-rel' and each
 *    	member of 'inner-rels' that isn't already included in 'outer-rel'.
 *    
 *    	Returns a list of new join relations.
 *    
 */

/*  .. find-join-rels    */

LispValue
find_clauseless_joins (outer_rel,inner_rels)
     LispValue outer_rel,inner_rels ;
{
     /*  XXX was mapcan   */
     LispValue inner_rel = LispNil;
     LispValue t_list = LispNil;
     LispValue temp_node = LispNil;

     foreach(inner_rel,inner_rels) {
	  if ( !member(get_relid(inner_rel),get_relids (outer_rel))) {
	       temp_node = list (init_join_rel(outer_rel,
					       get_rel(get_relid (inner_rel)),
					       LispNil));
	       t_list = nconc(t_list,temp_node);
	  } 
	  else {
	       t_list = nconc(t_list,LispNil);
	  } 
     }
     return(t_list);

}  /*  function end   */

/*    
 *    	init-join-rel
 *    
 *    	Creates and initializes a new join relation.
 *    
 *    	'outer-rel' and 'inner-rel' are relation nodes for the relations to be
 *    		joined
 *    	'joininfo' is the joininfo node (join clause) containing both
 *    		'outer-rel' and 'inner-rel', if any exists
 *    
 *    	Returns the new join relation node.
 *    
 */

/*  .. find-clause-joins, find-clauseless-joins   */

Rel
init_join_rel (outer_rel,inner_rel,joininfo)
     LispValue outer_rel,inner_rel,joininfo ;
{
     Rel joinrel = CreateNode (Rel);
     LispValue joinrel_joininfo_list;

	/*    Create a new tlist by removing irrelevant elements from both */
	/*    tlists of the outer and inner join relations and then merging */
	/*    the results together. */

     LispValue new_outer_tlist = 
       new_join_tlist (get_tlist (outer_rel)	    /*   XXX 1-based attnos */
	               ,get_relids (inner_rel),1);
     LispValue new_inner_tlist = 
       new_join_tlist (get_tlist (inner_rel)    /*   XXX 1-based attnos */
		       ,get_relids (outer_rel),
		       length (new_outer_tlist) - 1);

     set_relids (joinrel,append1 (get_relids (outer_rel),
				  get_relid (inner_rel)));
     set_tlist (joinrel,nconc (new_outer_tlist,new_inner_tlist));

     if ( joininfo) {
	  set_clause_info (joinrel,get_clause_info (joininfo));
     };

     /* XXX - let form, maybe incorrect */
     joinrel_joininfo_list = 
       new_joininfo_list (new_joininfo_list(LispNil,
					    get_join_info (outer_rel),
					    get_relids (inner_rel)),
			  get_join_info (inner_rel),
			  get_relids (outer_rel));
     set_join_info (joinrel,joinrel_joininfo_list);

     return(joinrel);

}  /* function end  */

/*    
 *    	new-join-tlist
 *    
 *    	Builds a join relations's target list by keeping those elements that 
 *    	will be in the final target list and any other elements that are still 
 *    	needed for future joins.  For a target list entry to still be needed 
 *    	for future joins, its 'joinlist' field must not be empty after removal 
 *    	of all relids in 'other-relids'.
 *    
 *    	'tlist' is the target list of one of the join relations
 *    	'other-relids' is a list of relids contained within the other
 *    		join relation
 *    	'first-resdomno' is the resdom number to use for the first created
 *    		target list entry
 *    
 *    	Returns the new target list.
 *    
 */

/*  .. init-join-rel    */

LispValue
new_join_tlist (tlist,other_relids,first_resdomno)
     LispValue tlist,other_relids,first_resdomno ;
{
     /*  XXX was mapcan  */
	LispValue resdomno = first_resdomno - 1;
	LispValue xtl = LispNil;
	LispValue temp_node = LispNil;
	LispValue t_list = LispNil;

	LispValue join_list = get_join_list (xtl);
	bool in_final_tlist = null (join_list);
	LispValue future_join_list = LispNil;
	if ( join_list ) {
	     future_join_list = set_difference (join_list,other_relids);
	} 
	foreach(xtl,tlist) {
	     if ( or (in_final_tlist,future_join_list) ) {
		  temp_node = 
		    list (create_tl_element (tl_expr(get_tlelement (xtl)),
					     incf (resdomno),
					     future_join_list));
		  t_list = nconc(t_list,temp_node);
	     } 
	     else {
		  t_list = nconc(t_list,LispNil);
	     } 
	}
	return(t_list);
   }

/*    
 *    	new-joininfo-list
 *    
 *    	Builds a join relation's joininfo list by checking for join clauses
 *    	which still need to used in future joins involving this relation.  A
 *    	join clause is still needed if there are still relations in the clause
 *    	not contained in the list of relations comprising this join relation.
 *    	New joininfo nodes are only created and added to
 *    	'current-joininfo-list' if a node for a particular join hasn't already
 *    	been created.
 *    
 *    	'current-joininfo-list' contains a list of those joininfo nodes that 
 *    		have already been built 
 *    	'joininfo-list' is the list of join clauses involving this relation
 *    	'join-relids' is a list of relids corresponding to the relations 
 *    		currently being joined
 *    
 *    	Returns a list of joininfo nodes, new and old.
 *    
 */

/*  .. init-join-rel    */

LispValue
new_joininfo_list (current_joininfo_list,joininfo_list,join_relids)
     LispValue current_joininfo_list,joininfo_list,join_relids ;
{
     LispValue joininfo = LispNil;
	foreach (joininfo, joininfo_list) {
	     LispValue new_otherrels = 
	       set_difference (get_other_rels (joininfo),join_relids);
	     JInfo other_joininfo;
	     if ( new_otherrels ) {
		  other_joininfo = 
		    joininfo_member (new_otherrels,current_joininfo_list);
	     } 

	     if (other_joininfo) {
		  set_clause_info (other_joininfo,
				   Lispunion(get_clause_info (joininfo),
					  get_clause_info (other_joininfo)));
	     } 
	     else {
		  other_joininfo = CreateNode (JInfo);
		  set_other_rels (other_joininfo,new_otherrels);
		  set_clause_info (other_joininfo,get_clause_info (joininfo));
		  set_mergesortable (other_joininfo,
				     get_mergesortable (joininfo));
		  set_hashjoinable (other_joininfo,
				    get_hashjoinable (joininfo));
		  push (other_joininfo,current_joininfo_list);
	     } 
	}
	return(current_joininfo_list);  /* XXX is this right  */
}  /* function end  */

