
/*     
 *      FILE
 *     	setrefs
 *     
 *      DESCRIPTION
 *     	Routines to change varno/attno entries to contain references
 *     
 */

/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		new-level-tlist
 *     		new-level-qual
 *     		new-result-tlist
 *     		new-result-qual
 *     		set-tlist-references
 *     		index-outerjoin-references
 *     		join-references
 *     		tlist-temp-references
 */

#include "planner/internal.h"
#include "planner/tlist.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "relation.h"
#include "relation.a.h"
#include "pg_lisp.h"
#include "planner/clause.h"
#include "planner/clauseinfo.h"
#include "planner/clauses.h"
#include "planner/var.h"
#include "c.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "planner/setrefs.h"
#include "nodeFuncs.h"
#include "planner/keys.h"
#include "parse.h"

/*     
 *     	NESTING LEVEL REFERENCES
 *     
 */

/*    
 *    	new-level-tlist
 *    
 *    	Creates a new target list for a new nesting level by removing those
 *    	var nodes that no longer have any dot fields and modifying those
 *    	that do to reflect the new level of processing.
 *    
 *    	'tlist' is the old target list
 *    	'prevtlist' is the target list from the previous nesting level
 *    	'prevlevel' is the previous nesting level
 *    
 *    	Returns the modified target list.
 *    
 */

/*  .. query_planner    */

LispValue
new_level_tlist (tlist,prevtlist,prevlevel)
     LispValue tlist,prevtlist,prevlevel ;
{
    LispValue new_tlist = LispNil;
    int last_resdomno = 0;
    LispValue xtl = LispNil;
    TLE entry = (TLE)NULL;
    
    foreach (xtl, tlist) {
	entry = CAR(xtl);
	if ( var_is_nested (get_expr (entry))) {
	    push (MakeTLE (MakeResdom (last_resdomno += 1,
					  _UNION_TYPE_ID_,
				       0,
					  LispNil,
				       0,
				       LispNil),
			   replace_nestvar_refs (get_expr (entry),
						 prevtlist,
						 prevlevel)),
		  new_tlist);
	}
    }
    return(nreverse(new_tlist));
    
}  /* function end  */

/*    
 *    	new-level-qual
 *    
 *    	Creates new qualifications for a new nesting level by removing
 *    	those qualifications that no longer need to be considered.  A
 *    	qualification need not be considered any further if all vars within
 *    	a clause no longer have dot fields.  This indicates that the 
 *    	qualification has been considered at some previous nesting level.
 *    	Vars in qualifications that still need to be considered are modified
 *    	to reflect a new nesting level.
 *    
 *    	'quals' is the old list of qualifications
 *    	'prevtlist' is the target list from the previous nesting level
 *    	'prevlevel' is the previous nesting level
 *    
 *    	Returns the new qualification.
 *    
 */

/*  .. query_planner	 */

LispValue
new_level_qual (quals,prevtlist,prevlevel)
     LispValue quals,prevtlist,prevlevel ;
{
     LispValue temp = LispNil;
     LispValue t_list = LispNil;
     LispValue qual = LispNil;

     foreach(qual,collect(nested_clause_p(quals))) {
	  temp = replace_clause_nestvar_refs ( CAR(qual),prevtlist,prevlevel);
	  t_list = nappend1(t_list,temp);
     }
     return(t_list);

}  /* function end  */


/*    
 *    	replace-clause-nestvar-refs
 *    	replace-subclause-nestvar-refs
 *    	replace-nestvar-refs
 *    
 *    	Modifies a qualification clause by modifying every variable to reflect
 *    	a new nesting level.  This is done by setting the varno to a reference
 *    	pair that indicates which attribute from the tuple at the previous
 *    	nesting level corresponds to the varno/varattno of this var node.
 *    	This reference pair looks like:
 *    			(levelnum resno)
 *    	where 'levelnum' is the number of the previous nesting level and
 *    	'resno' is the result domain number corresponding to the desired
 *    	attribute in the target list from the previous nesting level.
 *    	varattno is set to the car of dotfields and dotfields is set to the
 *    	cdr.  If a var is already a reference var with no further dotfields to
 *    	be considered, the old var is returned.
 *    
 *    	'clause' is the clause to be modified
 *    	'prevtlist' is the target list from the previous nesting level
 *    	'prevlevel' is the previous nesting level
 *    
 *    	Returns the new clause.
 *    
 */

/*  .. new-level-qual, replace-clause-nestvar-refs
 *  .. replace-subclause-nestvar-refs
 */
LispValue
replace_clause_nestvar_refs (clause,prevtlist,prevlevel)
     LispValue clause,prevtlist,prevlevel ;
{
     if(IsA (clause,Var))
       return ((LispValue)replace_nestvar_refs (clause,prevtlist,prevlevel));
     else 
       if (single_node (clause)) 
	 return(clause);
       else 
	 if (or_clause (clause)) 
	   return (make_orclause (replace_subclause_nestvar_refs 
				  (get_orclauseargs (clause),
				   prevtlist,prevlevel)));
	 else 
	   if (is_funcclause (clause)) 
	     return (make_funcclause (get_function (clause),
				      replace_subclause_nestvar_refs 
				      (get_funcargs (clause),
				       prevtlist,prevlevel)));
	   else 
	     if (not_clause (clause)) 
	       return (make_notclause (replace_clause_nestvar_refs 
				       (get_notclausearg (clause),
					prevtlist,prevlevel)));
		else
		  return(make_opclause (get_op (clause),
					replace_clause_nestvar_refs 
					(get_leftop (clause),
					 prevtlist,
					 prevlevel),
					replace_clause_nestvar_refs 
					(get_rightop (clause),
					 prevtlist,
					 prevlevel)));
}  /* function end  */


/*  .. replace-clause-nestvar-refs	 */

LispValue
replace_subclause_nestvar_refs (clauses,prevtlist,prevlevel)
     LispValue clauses,prevtlist,prevlevel ;
{
     LispValue t_list = LispNil;
     LispValue clause = LispNil;
     LispValue temp = LispNil;

     foreach(clause,clauses) {
	  temp = replace_clause_nestvar_refs (CAR(clause),prevtlist,prevlevel);
	  t_list = nappend1(t_list,temp);
     }
     return(t_list);

}  /* function end  */


/*  .. new-level-tlist, replace-clause-nestvar-refs   	 */

Var
replace_nestvar_refs (var,prevtlist,prevlevel)
     Var var;
     List prevtlist;
     int prevlevel ; /* XXX - ??? is this right */
{
    ObjectId var1 = -1;
    List var2 = LispNil;

    if(null (get_varattno (var)) && ! (var_is_nested (var))) {
	return (var);
    } else {
	/*    Nested vars are of UNION (unknown) type. */
	if (var_is_nested (var)) 
	  var1 = _UNION_TYPE_ID_;
	else
	  var1 = get_vartype(var);
	
	if (consider_vararrayindex (var))
	  var2 = varid_array_index(var);
	else
	  var2 = get_varid(var);
	
	return ((Var)MakeVar (lispCons (lispInteger(prevlevel - 1),
					lispCons (get_resno 
					 (tlist_member (var,   /* resno */
							prevtlist,
							LispNil)),
					 lispCons(CAR (get_vardotfields (var)),
						  lispCons(var1,
							   lispCons 
							    (CDR
							     (get_vardotfields 
							      (var)),
							     lispCons(var2,
								      LispNil))
							   ))))));

      }
    
  }  /* function end  */

/*     
 *     	RESULT REFERENCES
 *     
 */

/*    
 *    	new-result-tlist
 *    
 *    	Creates the target list for a result node by copying a tlist but
 *    	assigning reference values for the varno entry.  The reference pair 
 *    	corresponds to the nesting level where the attribute can be found and 
 *    	the result domain number within the tuple at that nesting level.
 *    
 *    	'tlist' is the the target list to be copied
 *    	'ltlist' is the target list from level 'levelnum'
 *    	'rtlist' is the target list from level 'levelnum+1'
 *    	'levelnum' is the current nesting level
 *    	'sorted' is t if the desired user result has been sorted in
 *    		some internal node
 *    
 *    	Returns the new target list.
 *    
 */

/*  .. query_planner	 */

LispValue
new_result_tlist (tlist,ltlist,rtlist,levelnum,sorted)
     LispValue tlist,ltlist,rtlist,levelnum,sorted ;
{
    LispValue t_list = LispNil;
    
    if ( consp (tlist) ) {
	  if ( consp (rtlist) ) {
	      LispValue xtl = LispNil;
	      LispValue temp = LispNil;
	      TLE entry;
	      
	      foreach(xtl,tlist) {
		  entry = CAR(xtl);
		  if ( sorted) {
		      set_reskey (tl_resdom (entry),0);
		      set_reskeyop (tl_resdom (entry),0);
		  }
		  temp = lispCons(tl_resdom (entry),
				  lispCons 
				  (replace_clause_resultvar_refs 
				   (get_expr(entry),
				    ltlist,
				    rtlist,
				    levelnum),
				   LispNil));
		  t_list = nappend1(t_list,temp);
	       }
	      return(t_list);
	  }
	  else
	    return (tlist);
     }
     else
       return(LispNil);

}  /* function end  */

/*    
 *    	new-result-qual
 *    
 *    	Modifies a list of relation level clauses by assigning reference
 *    	values for varnos within vars appearing in the clauses.
 *    
 *    	'relquals' is the list of relation level clauses	
 *    	'ltlist' is the target list from level 'levelnum'
 *    	'rtlist' is the target list from level 'levelnum+1'
 *    	'levelnum' is the current nesting level
 *    
 *    	Returns a new qualification.
 *    
 */

/*  .. query_planner   	 */

LispValue
new_result_qual (clauses,ltlist,rtlist,levelnum)
     LispValue clauses,ltlist,rtlist,levelnum ;
{
     return (replace_subclause_resultvar_refs (clauses,
					       ltlist,
					       rtlist,
					       levelnum));
} /* function end  */

/*    
 *    	replace-clause-resultvar-refs
 *    	replace-subclause-resultvar-refs
 *    	replace-resultvar-refs
 *    
 *    	Creates a new expression corresponding to a target list entry or
 *    	qualification in a result node by replacing the varno value in all
 *    	vars with a reference pair.  The reference pair is assigned by finding
 *    	a matching var in a previous target list.  If the var already is a
 *    	reference to a previous nesting, the var is just returned.
 *    
 *    	'expr' is the target list entry or qualification
 *    	'ltlist' is the target list from level 'levelnum'
 *    	'rtlist' is the target list from level 'levelnum+1'
 *    	'levelnum' is the current nesting level
 *    
 *    	Returns the new target list entry.
 *    
 */

/*  .. new-result-tlist, replace-clause-resultvar-refs
 *  .. replace-subclause-resultvar-refs
 */

Expr
replace_clause_resultvar_refs (clause,ltlist,rtlist,levelnum)
     Expr clause;
     List ltlist,rtlist;
     int levelnum ;
{
    if(IsA (clause,Var) && null (get_varattno (clause)))
      return(clause);
    else 
      if (IsA (clause,Var)) 
	return ((Expr)replace_resultvar_refs (clause,
					ltlist,
					rtlist,
					levelnum));
      else 
	if (single_node (clause)) 
	  return (clause);
	else 
	  if (or_clause (clause)) 
	    return ((Expr)make_orclause (replace_subclause_resultvar_refs 
				   (get_orclauseargs (clause),
				    ltlist,
				    rtlist,
				    levelnum)));
	   else 
	     if (is_funcclause (clause)) 
	       return ((Expr)make_funcclause (get_function (clause),
					replace_subclause_resultvar_refs 
					(get_funcargs (clause),
					 ltlist,
					 rtlist,
					 levelnum)));
	     else 
	       if (not_clause (clause))
		 return ((Expr)make_notclause (replace_clause_resultvar_refs 
					 (get_notclausearg (clause),
					  ltlist,
					  rtlist,
					  levelnum)));
	       else
		    /*   operator clause */
		    return((Expr)make_clause (replace_opid (get_op (clause)),
					replace_clause_resultvar_refs 
					(get_leftop (clause),
					 ltlist,
					 rtlist,
					 levelnum),
					replace_clause_resultvar_refs 
					(get_rightop (clause),
					 ltlist,
					 rtlist,
					 levelnum)));
}  /* function end  */

/*  .. new-result-qual, replace-clause-resultvar-refs	 */

LispValue
replace_subclause_resultvar_refs (clauses,ltlist,rtlist,levelnum)
     LispValue clauses,ltlist,rtlist,levelnum ;
{
     LispValue t_list = LispNil;
     LispValue clause = LispNil;
     LispValue temp = LispNil;

     foreach(clause,clauses) {
	 temp = (LispValue)replace_clause_resultvar_refs (CAR(clause),
							  ltlist,
							  rtlist,
							  levelnum);
	 t_list = nappend1(t_list,temp);
     }
     return(t_list);
}  /* function end  */

/*  .. replace-clause-resultvar-refs  	 */

Var
replace_resultvar_refs (var,ltlist,rtlist,levelnum)
     Var var;
     List ltlist,rtlist;
     int levelnum ;
{
     LispValue varno = LispNil;
     
     if (levelnum == numlevels (var)) 
	  varno = lispCons (levelnum - 1,
			    lispCons(get_resno (tl_resdom 
						(match_varid (get_varid (var),
							      ltlist))),
				     LispNil));
     else
       varno = lispCons (levelnum,
			 lispCons (get_resno (tl_resdom 
					  (match_varid (get_varid (var),
							rtlist))),
				   LispNil));
     
     return (MakeVar (varno,
		      LispNil,
		      get_vartype (var),
		      LispNil,
		      LispNil,
		      get_varid (var)));
     
 } /* function end */

/*     
 *     	SUBPLAN REFERENCES
 *     
 */

/*    
 *    	set-tlist-references
 *    
 *    	Modifies the target list of nodes in a plan to reference target lists
 *    	at lower levels.
 *    
 *    	'plan' is the plan whose target list and children's target lists will
 *    		be modified
 *    
 *    	Returns nothing of interest, but modifies internal fields of nodes.
 *    
 */

/* query_planner, set-join-tlist-references, set-temp-tlist-references	 */

void
set_tlist_references (plan)
     LispValue plan ;
{
    if(null (plan)) 
      LispNil;
    else 
      if (IsA (plan,Join)) 
	set_join_tlist_references (plan);
      else 
	if (IsA(plan,SeqScan) &&
	    get_lefttree (plan) && 
	    IsA (get_lefttree (plan),Temp))
	  set_tempscan_tlist_references (plan);
	else 
	  if (IsA (plan,Sort))
	    set_temp_tlist_references (plan);

}   /* function end  */

/*    
 *    	set-join-tlist-references
 *    
 *    	Modifies the target list of a join node by setting the varnos and
 *    	varattnos to reference the target list of the outer and inner join
 *    	relations.
 *    
 *    	Creates a target list for a join node to contain references by setting 
 *    	varno values to OUTER or INNER and setting attno values to the 
 *    	result domain number of either the corresponding outer or inner join 
 *    	tuple.
 *    
 *    	'join' is a join plan node
 *    
 *   	Returns nothing of interest, but modifies internal fields of nodes.
 *    
 */

/*  .. set-tlist-references  	 */

void 
set_join_tlist_references (join)
     Join join ;
{
    Plan 	outer = get_lefttree (join);
    Plan	inner = get_righttree (join);
    List 	new_join_targetlist = LispNil;
    TLE		temp = (TLE)NULL;
    LispValue  	entry = LispNil;
    TLE	xtl = 	(TLE)NULL;
    
     foreach(entry,get_qptargetlist(join)) {
	 
	 xtl = (TLE)CAR(entry);
	 temp = MakeTLE(tl_resdom (xtl),
			replace_clause_joinvar_refs (get_expr (xtl),
						     get_qptargetlist(outer),
						     get_qptargetlist(inner))
			);
	 new_join_targetlist = nappend1(new_join_targetlist,temp);
     }
	
    set_qptargetlist (join,new_join_targetlist);
    set_tlist_references (outer);
    set_tlist_references (inner);
    
} /* function end  */

/*    
 *    	set-tempscan-tlist-references
 *    
 *    	Modifies the target list of a node that scans a temp relation (i.e., a
 *    	sort or hash node) so that the varnos refer to the child temporary.
 *    
 *    	'tempscan' is a seqscan node
 *    
 *    	Returns nothing of interest, but modifies internal fields of nodes.
 *    
 */

/*  .. set-tlist-references	 */

void
set_tempscan_tlist_references (tempscan)
     SeqScan tempscan ;
{

     Plan temp = get_lefttree (tempscan);
     set_qptargetlist (tempscan,
		       tlist_temp_references(get_tempid(temp),
					     get_qptargetlist (tempscan)));
     set_temp_tlist_references (temp);

}  /* function end  */

/*    
 *    	set-temp-tlist-references
 *    
 *    	The temp's vars are made consistent with (actually, identical to) the
 *    	modified version of the target list of the node from which temp node
 *    	receives its tuples.
 *    
 *    	'temp' is a temp (e.g., sort, hash) plan node
 *    
 *    	Returns nothing of interest, but modifies internal fields of nodes.
 *    
 */

/*  .. set-tempscan-tlist-references, set-tlist-references 	 */

void 
set_temp_tlist_references (temp)
     Temp temp ;
{
     Plan source = get_lefttree (temp);

     if (!null(source)) {
	 set_tlist_references (source);
	 set_qptargetlist (temp,
			   copy_vars (get_qptargetlist (temp),
				      get_qptargetlist (source)));
     }
     else
       printf (" WARN: calling set_temp_tlist_references with empty lefttree\n");
 }  /* function end  */

/*    
 *    	join-references
 *    
 *    	Creates a new set of join clauses by replacing the varno/varattno
 *    	values of variables in the clauses to reference target list values
 *    	from the outer and inner join relation target lists.
 *    
 *    	'clauses' is the list of join clauses
 *    	'outer-tlist' is the target list of the outer join relation
 *    	'inner-tlist' is the target list of the inner join relation
 *    
 *    	Returns the new join clauses.
 *    
 */

/* create_hashjoin_node, create_mergejoin_node, create_nestloop_node	 */

LispValue
join_references (clauses,outer_tlist,inner_tlist)
     LispValue clauses,outer_tlist,inner_tlist ;
{
     return (replace_subclause_joinvar_refs (clauses,
					     outer_tlist,
					     inner_tlist));
}  /* function end  */

/*    
 *    	index-outerjoin-references
 *    
 *    	Given a list of join clauses, replace the operand corresponding to the
 *    	outer relation in the join with references to the corresponding target
 *    	list element in 'outer-tlist' (the outer is rather obscurely
 *    	identified as the side that doesn't contain a var whose varno equals
 *    	'inner-relid').
 *    
 *    	As a side effect, the operator is replaced by the regproc id.
 *    
 *    	'inner-indxqual' is the list of join clauses (so-called because they
 *    	are used as qualifications for the inner (inbex) scan of a nestloop)
 *    
 *    	Returns the new list of clauses.
 *    
 */

/*  .. create_nestloop_node	 */

LispValue
index_outerjoin_references (inner_indxqual,outer_tlist,inner_relid)
     LispValue inner_indxqual,outer_tlist,inner_relid ;
{
    LispValue t_list = LispNil;
    LispValue temp = LispNil;
    LispValue t_clause = LispNil;
    LispValue clause = LispNil;

    foreach (t_clause,inner_indxqual) {
	clause = CAR(t_clause);
	if(IsA (get_rightop (clause),Var) &&    /*   inner var on right */
	   equal (inner_relid,get_varno (get_rightop (clause)))) {
	       temp = make_clause (replace_opid (get_op (clause)),
				   replace_clause_joinvar_refs 
				   (get_leftop (clause),
				    outer_tlist,
				    LispNil),
				   get_rightop (clause));
	       t_list = nappend1(t_list,temp);
	   } 
	else {   /*   inner var on left */
	    temp = make_clause (replace_opid (get_op (clause)),
				   get_leftop (clause),
				replace_clause_joinvar_refs (get_rightop 
							     (clause),
								outer_tlist,
							     LispNil));
	    t_list = nappend1(t_list,temp);
	  } 
		
     }
     return(t_list);
     
}  /* function end  */

/*    
 *    	replace-clause-joinvar-refs
 *    	replace-subclause-joinvar-refs
 *    	replace-joinvar-refs
 *    
 *    	Replaces all variables within a join clause with a new var node
 *    	whose varno/varattno fields contain a reference to a target list
 *    	element from either the outer or inner join relation.
 *    
 *    	'clause' is the join clause
 *    	'outer-tlist' is the target list of the outer join relation
 *    	'inner-tlist' is the target list of the inner join relation
 *    
 *    	Returns the new join clause.
 *    
 */

/*  .. index-outerjoin-references, replace-clause-joinvar-refs
 *  .. replace-subclause-joinvar-refs, set-join-tlist-references
 */
LispValue
replace_clause_joinvar_refs (clause,outer_tlist,inner_tlist)
     LispValue clause,outer_tlist,inner_tlist ;
{
    LispValue temp = LispNil;
    if(IsA (clause,Var) && get_varattno (clause)) {
	if(temp = (LispValue)
	   replace_joinvar_refs (clause,outer_tlist,inner_tlist))
	  return(temp);
	else
	  if (clause != LispNil)
	    return(clause);
	  else
	    return(LispNil);
    }
    else 
      if (single_node (clause))
	return (clause);
      else 
	if (or_clause (clause)) 
	  return (make_orclause (replace_subclause_joinvar_refs 
				 (get_orclauseargs (clause),
				  outer_tlist,
				  inner_tlist)));
	else 
	  if (is_funcclause (clause))
	    return (make_funcclause (get_function (clause),
				     replace_subclause_joinvar_refs 
				      (get_funcargs (clause),
				       outer_tlist,
				       inner_tlist)));
	  else 
	    if (not_clause (clause)) 
	      
	      return (make_notclause (replace_clause_joinvar_refs 
				      (get_notclausearg (clause),
					outer_tlist,
				       inner_tlist)));
	    else 
	      if (is_clause (clause)) 
		return (make_clause (replace_opid (get_op (clause)),
				     replace_clause_joinvar_refs 
				     (get_leftop (clause),
				       outer_tlist,
				      inner_tlist),
				     replace_clause_joinvar_refs 
				     (get_rightop (clause),
				      outer_tlist,
				      inner_tlist)));
}  /* function end  */

/*  .. join-references, replace-clause-joinvar-refs	 */

LispValue
replace_subclause_joinvar_refs (clauses,outer_tlist,inner_tlist)
     LispValue clauses,outer_tlist,inner_tlist ;
{
     LispValue t_list = LispNil;
     LispValue temp = LispNil;
     LispValue clause = LispNil;

     foreach (clause,clauses) {
	  temp = replace_clause_joinvar_refs (CAR(clause),
					      outer_tlist,
					      inner_tlist);
	  t_list = nappend1(t_list,temp);
     }
     return(t_list);

} /* function end  */

/*  .. replace-clause-joinvar-refs 	 */

Var
replace_joinvar_refs (var,outer_tlist,inner_tlist)
     Var var;
     List outer_tlist,inner_tlist ;
{
    Resdom outer_resdom =(Resdom)NULL;

    outer_resdom= tlist_member (var,outer_tlist,LispNil);

    if ( !null(outer_resdom) && IsA (outer_resdom,Resdom) ) {
	 return (MakeVar (OUTER,
			  get_resno (outer_resdom),
			  get_vartype (var),
			  LispNil,
			  LispNil,
			  get_varid (var)));
     } 
    else {
	Resdom inner_resdom;
	inner_resdom = tlist_member (var,inner_tlist,LispNil);
	if ( !null(inner_resdom) && IsA (inner_resdom,Resdom) ) {
	    return (MakeVar (INNER,
			     get_resno (inner_resdom),
			     get_vartype (var),
			     LispNil,
			     LispNil,
			     get_varid (var)));
	 } 
    } 
}  /* function end  */

/*    
 *    	tlist-temp-references
 *    
 *    	Creates a new target list for a node that scans a temp relation,
 *    	setting the varnos to the id of the temp relation and setting varids
 *    	if necessary (varids are only needed if this is a targetlist internal
 *    	to the tree, in which case the targetlist entry always contains a var
 *    	node, so we can just copy it from the temp).
 *    
 *    	'tempid' is the id of the temp relation
 *    	'tlist' is the target list to be modified
 *    
 *    	Returns new target list
 *    
 */

/*  .. set-tempscan-tlist-references, sort-level-result	 */

List
tlist_temp_references (tempid,tlist)
     ObjectId tempid;
     List tlist ;
{
     LispValue t_list = LispNil;
     TLE temp = (TLE)NULL;
     LispValue xtl = LispNil;
     LispValue var1 = LispNil;
     LispValue entry;
     
     foreach (entry,tlist) {
	 xtl = CAR(entry);
	 if ( IsA(get_expr (xtl),Var))
	   var1 = get_varid (get_expr (xtl));
	  else
	    var1 = LispNil;
	 
	 temp = (TLE)MakeTLE (tl_resdom (xtl),
			      MakeVar(tempid,
				      get_resno (tl_resdom (xtl)),
				  get_restype (tl_resdom (xtl)),
				      LispNil,
				      LispNil,
				      var1));
	 
	 t_list = nappend1(t_list,temp);
     }
     return(t_list);
 }  /* function end  */



