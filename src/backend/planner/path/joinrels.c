
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
#include "planner/internal.h"
#include "relation.h"
#include "relation.a.h"
#include "planner/joinrels.h"
#include "planner/joininfo.h"
#include "planner/relnode.h"

/* ----------------
 *	node creator declarations
 * ----------------
 */
extern JInfo RMakeJInfo();
extern Rel RMakeRel();

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
    LispValue i = LispNil;

    foreach (i,outer_rels) {
	outer_rel = CAR(i);
	if (!(temp = find_clause_joins (outer_rel,get_joininfo (outer_rel))))
#ifdef _xprs_
	  temp = find_clauseless_joins (outer_rel,outer_rels);
#else /* _xprs_ */
	  temp = find_clauseless_joins (outer_rel,_query_relation_list_);
#endif /* _xprs */
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
     LispValue temp_node = LispNil;
     LispValue t_list = LispNil;
     LispValue i = LispNil;
#ifdef _xprs_
     bool joininfo_inactive();
#endif /* _xprs_ */
     
     foreach(i,joininfo_list) {
	 JInfo joininfo = (JInfo)CAR(i);
#ifdef _xprs_
         if (!joininfo_inactive (joininfo)) {
               LispValue other_rels = get_otherrels (joininfo);
               if (!null (other_rels)) {
                  if (length (other_rels) == 1)
                     temp_node = lispCons(init_join_rel(outer_rel,
                                                       get_rel(CAR(other_rels)),
                                                       joininfo),
                                          LispNil);
                  else
                     temp_node = lispCons(init_join_rel (outer_rel,
                                                         get_rel(other_rels),
                                                         joininfo),
                                          LispNil);
                t_list = nconc(t_list,temp_node);

            }
         }
#else /* _xprs_ */
	 LispValue other_rels = get_otherrels (joininfo);
	 if ( 1 == length (other_rels) ) {
	       temp_node = lispCons(init_join_rel(outer_rel,
						  get_rel(CAR(other_rels)),
						  joininfo),
				    LispNil);
	       t_list = nconc(t_list,temp_node);
	   } 
	 else {
	     t_list = nconc(t_list,LispNil);
	 } 
#endif /* _xprs_ */
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
     LispValue i = LispNil;
#ifdef _xprs_
     bool nonoverlap_rels();
#endif /* _xprs_ */

     foreach(i,inner_rels) {
	 inner_rel = CAR(i);
#ifdef _xprs_
         if (nonoverlap_rels (inner_rel,outer_rel)) {
#else  /* _xprs_ */
	 if ( !member(CAR(get_relids(inner_rel)),get_relids (outer_rel))) {
#endif /* _xprs_ */
	     temp_node = lispCons (init_join_rel(outer_rel,
#ifdef _xprs_
                                                 inner_rel,
#else /* _xprs_ */
						 get_rel(CAR(get_relids 
							 (inner_rel))),
#endif /* _xprs_ */
						 LispNil),
				   LispNil);
	     t_list = nconc(t_list,temp_node);
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
     Rel outer_rel,inner_rel;
     JInfo joininfo ;
{
    Rel joinrel = RMakeRel();
    LispValue joinrel_joininfo_list = LispNil;
#ifdef _xprs_
    void deactivate_joininfo();
#endif /* _xprs_ */
    
    /*    Create a new tlist by removing irrelevant elements from both */
    /*    tlists of the outer and inner join relations and then merging */
    /*    the results together. */

    LispValue new_outer_tlist = 
      new_join_tlist (get_targetlist (outer_rel)   /*   XXX 1-based attnos */
		      ,get_relids (inner_rel),1);
    LispValue new_inner_tlist = 
      new_join_tlist (get_targetlist  (inner_rel)    /*   XXX 1-based attnos */
		      ,get_relids (outer_rel),
		      length (new_outer_tlist) + 1);
     
     
    set_relids (joinrel, LispNil);
    set_indexed (joinrel,false);
    set_pages (joinrel, 0);
    set_tuples (joinrel,0);
    set_width (joinrel,0);
    set_targetlist (joinrel,LispNil);
    set_pathlist (joinrel,LispNil);
    set_unorderedpath (joinrel,(Path)NULL);
    set_cheapestpath (joinrel,(Path)NULL);
    set_classlist (joinrel,(List)NULL);
    set_ordering (joinrel,LispNil);
    set_clauseinfo (joinrel,LispNil);
    set_joininfo (joinrel,LispNil);
    set_innerjoin (joinrel,LispNil);
    set_superrels (joinrel,LispNil);
    
#ifdef _xprs_
    set_relids (joinrel,lispCons (get_relids (outer_rel),
                                  lispCons ( get_relids (inner_rel),
                                             LispNil)));
#else /* _xprs_ */
    set_relids (joinrel,append1 (get_relids (outer_rel),
				  CAR(get_relids (inner_rel))));
#endif /* _xprs_ */

    set_targetlist (joinrel,nconc (new_outer_tlist,new_inner_tlist));
    
    if ( joininfo) {
	set_clauseinfo (joinrel,get_jinfoclauseinfo (joininfo));
#ifdef _xprs_
        deactivate_joininfo (joininfo);
#endif /* _xprs_ */
    }
    
    
    joinrel_joininfo_list = 
#ifdef _xprs_
      new_joininfo_list (append (get_joininfo (outer_rel),
                                 get_joininfo (inner_rel)),
                         append (get_relids (outer_rel),
                                 get_relids (inner_rel)));
#else /* _xprs_ */
      new_joininfo_list (new_joininfo_list(LispNil,
					   get_joininfo (outer_rel),
					   get_relids (inner_rel)),
			 get_joininfo (inner_rel),
			 get_relids (outer_rel));
#endif /* _xprs_ */

    set_joininfo (joinrel,joinrel_joininfo_list);

#ifdef _xprs_
    _query_relation_list_ = lispCons (joinrel,_query_relation_list_);
#endif /* _xprs_ */
    
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
     LispValue tlist,other_relids;
     int first_resdomno ;
{
    int resdomno = first_resdomno - 1;
    LispValue xtl = LispNil;
    LispValue temp_node = LispNil;
    LispValue t_list = LispNil;
    LispValue i = LispNil;
    LispValue join_list = LispNil;
    bool in_final_tlist =false;
    LispValue future_join_list = LispNil;
    

    foreach(i,tlist) {
	xtl= CAR(i);
	join_list = get_joinlist (xtl);
	in_final_tlist = null (join_list);
	resdomno += 1;
	if ( join_list ) 
	  future_join_list = set_difference (join_list,other_relids);
	if ( in_final_tlist || future_join_list)  {
	    temp_node = 
	      lispCons (create_tl_element (get_expr(get_entry (xtl)),
					   resdomno,
					   future_join_list),
			LispNil);
	    t_list = nconc(t_list,temp_node);
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

#ifdef _xprs_
LispValue
new_joininfo_list (joininfo_list,join_relids)
     LispValue joininfo_list,join_relids ;
{
   LispValue current_joininfo_list = LispNil;
   LispValue new_otherrels = LispNil;
   JInfo other_joininfo = (JInfo)NULL;
   LispValue xjoininfo = LispNil;
   bool nonoverlap_sets();

   foreach (xjoininfo, joininfo_list) {
      JInfo joininfo = (JInfo)CAR (xjoininfo);
      new_otherrels = get_otherrels (joininfo);
      if ( nonoverlap_sets (new_otherrels,join_relids) ) {
         other_joininfo = joininfo_member (new_otherrels,current_joininfo_list);
         if ( other_joininfo )
            set_clauseinfo (other_joininfo,
                            LispUnion (get_jinfoclauseinfo (joininfo),
                                   get_jinfoclauseinfo (other_joininfo)));
         else {
            other_joininfo = MakeJInfo (get_otherrels (joininfo),
                                        get_jinfoclauseinfo (joininfo),
                                        get_mergesortable (joininfo),
                                        get_hashjoinable (joininfo),
					false);
            current_joininfo_list = push(other_joininfo, current_joininfo_list);
         }
       }
    }
    return (current_joininfo_list);
} /* function end  */
#else /* _xprs_ */
LispValue
new_joininfo_list (current_joininfo_list,joininfo_list,join_relids)
     LispValue current_joininfo_list,joininfo_list,join_relids ;
{
    JInfo joininfo = (JInfo)NULL;
    LispValue i = LispNil;
    LispValue new_otherrels = LispNil;
    JInfo other_joininfo = (JInfo)NULL;

    foreach (i, joininfo_list) {
	joininfo = (JInfo)CAR(i);
	new_otherrels = 
	  set_difference (get_otherrels (joininfo),join_relids);

	if ( new_otherrels ) {
	    other_joininfo = 
	      joininfo_member (new_otherrels,current_joininfo_list);
	} 
	
	if (other_joininfo) {
	    set_jinfoclauseinfo (other_joininfo,
				 LispUnion(get_jinfoclauseinfo (joininfo),
					   get_jinfoclauseinfo
					              (other_joininfo)));
	} 
	else {
	    other_joininfo = RMakeJInfo();

	    set_otherrels (other_joininfo,new_otherrels);
	    set_jinfoclauseinfo (other_joininfo,
				 get_jinfoclauseinfo (joininfo));
	    set_mergesortable (other_joininfo,
			       get_mergesortable (joininfo));
	    set_hashjoinable (other_joininfo,
			      get_hashjoinable (joininfo));
	    current_joininfo_list = nappend1(current_joininfo_list,
					     other_joininfo);
	    current_joininfo_list = nreverse(current_joininfo_list);
	} 
    }
    return(current_joininfo_list);  /* XXX is this right  */
}  /* function end  */
#endif /* _xprs_ */

#ifdef _xprs_
/*
 *      add-new-joininfos
 *
 *      For each new join relation, create new joininfos that
 *      use the join relation as inner relation, and add
 *      the new joininfos to those rel nodes that still
 *      have joins with the join relation.
 *
 *      'joinrels' is a list of join relations.
 *
 *      Modifies the joininfo field of appropriate rel nodes.
 *
 */

/*  .. find-join-paths
 */
LispValue
add_new_joininfos (joinrels,outerrels)
LispValue joinrels,outerrels ;
{
    LispValue xjoinrel = LispNil;
    LispValue xrelid = LispNil;
    LispValue xrel = LispNil;
    LispValue xjoininfo = LispNil;
    bool nonoverlap_rels();
    void add_superrels();

    foreach (xjoinrel, joinrels) {
	LispValue joinrel = CAR (xjoinrel);
        foreach (xrelid, get_relids (joinrel)) {
	    Relid relid = (Relid)CAR (xrelid);
	    Rel rel = get_rel (relid);
	    add_superrels (rel,joinrel);
	}
    }
    foreach (xjoinrel, joinrels) {
	LispValue joinrel = CAR (xjoinrel);
	foreach (xjoininfo, get_joininfo (joinrel)) {
	    LispValue joininfo = CAR (xjoininfo);
	    LispValue other_rels = get_otherrels (joininfo);
	    LispValue clause_info = get_jinfoclauseinfo (joininfo);
	    bool mergesortable = get_mergesortable (joininfo);
	    bool hashjoinable = get_hashjoinable (joininfo);
	    foreach (xrelid, other_rels) {
		Relid relid = (Relid)CAR (xrelid);
		Rel rel = get_rel (relid);
		List super_rels = get_superrels (rel);
		LispValue xsuper_rel = LispNil;
		JInfo new_joininfo = MakeJInfo (get_relids (joinrel),
						    clause_info,
						    mergesortable,
						    hashjoinable,
						    false);
	        set_joininfo (rel,append1 (get_joininfo (rel),new_joininfo));
		foreach (xsuper_rel, super_rels) {
		    LispValue super_rel = CAR (xsuper_rel);
		    if ( nonoverlap_rels (super_rel,joinrel) ) {
			LispValue new_relids = get_relids (super_rel);
			JInfo other_joininfo = 
				  joininfo_member (new_relids,
						   get_joininfo (joinrel));
			if ( other_joininfo ) {
			    set_jinfoclauseinfo (other_joininfo,
				  LispUnion (clause_info, 
				         get_jinfoclauseinfo (other_joininfo)));
			} else {
			    JInfo new_joininfo = MakeJInfo (new_relids,
								clause_info,
								mergesortable,
								hashjoinable,
								false);
			    set_joininfo (joinrel,
			      append1 (get_joininfo (joinrel),new_joininfo));
			}
		    }
		}
	     }
	 }
    }
    foreach (xrel, outerrels)  {
	LispValue rel = CAR (xrel);
	set_superrels (rel,LispNil);
    }
}

/*
 *      final-join-rels
 *
 *      Find the join relation that includes all the original
 *      relations, i.e. the final join result.
 *
 *      'join-rel-list' is a list of join relations.
 *
 *      Returns the list of final join relations.
 *
 */

/*  .. find-join-paths
 */
LispValue
final_join_rels (join_rel_list)
LispValue join_rel_list ;
{
    LispValue xrel = LispNil;
    LispValue temp = LispNil;
    LispValue t_list = LispNil;

/*    find the relations that has no further joins, */
/*    i.e., its joininfos all have otherrels nil. */
    foreach (xrel,join_rel_list)  {
	LispValue rel = CAR (xrel);
	LispValue xjoininfo = LispNil;
	bool final = true;
	foreach (xjoininfo,get_joininfo (rel)) {
	    LispValue joininfo = CAR (xjoininfo);
	    if (get_otherrels (joininfo) != LispNil)  {
	       final = false;
	       break;
	    }
	}
	if (final)  {
	   temp = lispCons (rel,LispNil);
	   t_list = nconc (t_list, temp);
	}
    }
    return (t_list);
}  /* function end */

/*
 *      add_superrels
 *
 *      add rel to the temporary property list superrels.
 *
 *      'rel' a rel node
 *      'super-rel' rel node of a join relation that includes rel
 *
 *      Modifies the superrels field of rel
 *
 */

/*  .. init-join-rel
 */
void
add_superrels (rel,super_rel)
LispValue rel,super_rel ;
{
    set_superrels (rel, append1 (get_superrels (rel), super_rel));
}

/*
 *      nonoverlap-rels
 *
 *      test if two join relations overlap, i.e., includes the same
 *      relation.
 *
 *      'rel1' and 'rel2' are two join relations
 *
 *      Returns non-nil if rel1 and rel2 do not overlap.
 *
 */

/*  .. add-new-joininfos
 */
bool
nonoverlap_rels (rel1,rel2)
LispValue rel1,rel2 ;
{
    bool nonoverlap_sets();
    return (nonoverlap_sets (get_relids (rel1),get_relids (rel2)));
}

bool
nonoverlap_sets (s1,s2)
LispValue s1,s2 ;
{
    LispValue x = LispNil;

    foreach (x,s1)  {
       LispValue e = CAR(x);
       if (member (e,s2))
	  return (false);
    }
    return (true);
}

/*
 *	cleanup-joininfos
 *
 *      after a round of find-join-paths, clear the JoinInfos that
 *      have already been considered.
 *
 *      'outer-rels' is a list of rel nodes
 *
 *      Modifies JoinInfo fields.
 *
 */

/* .. add-new-joininfos
*/
void
cleanup_joininfos (outer_rels)
LispValue outer_rels;
{
    LispValue xrel = LispNil;

    foreach (xrel,outer_rels)  {
	Rel rel = (Rel)CAR(xrel);
	set_joininfo (rel,LispNil);
    }
}

void
deactivate_joininfo (joininfo)
JInfo joininfo;
{
    set_inactive (joininfo, true);
}

bool
joininfo_inactive (joininfo)
JInfo joininfo;
{
    bool get_inactive();
    return (get_inactive (joininfo));
}
#endif /* _xprs_ */
