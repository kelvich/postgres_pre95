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
#include "tmp/c.h"

#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "parser/parse.h"

#include "utils/log.h"
#include "tmp/nodeFuncs.h"

#include "planner/internal.h"
#include "planner/clause.h"
#include "planner/clauseinfo.h"
#include "planner/clauses.h"
#include "planner/keys.h"
#include "planner/setrefs.h"
#include "planner/tlist.h"
#include "planner/var.h"

/*     
 *     	NESTING LEVEL REFERENCES
 *     
 */

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
     LispValue tlist,ltlist,rtlist;
     int levelnum;
     bool sorted;
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
		      set_reskey ((Resdom)tl_resdom(entry),0);
		      set_reskeyop ((Resdom)tl_resdom(entry),0);
		  }
		  temp = lispCons(tl_resdom (entry),
				  lispCons 
				  ((LispValue)replace_clause_resultvar_refs 
				   ((Expr)get_expr(entry),
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
     LispValue clauses,ltlist,rtlist;
     int levelnum ;
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
    if(IsA (clause,Var) && (get_varattno ((Var)clause) == 0))
      return(clause);
    else if (IsA (clause,Var)) 
      return ((Expr)replace_resultvar_refs((Var)clause,
					ltlist,
					rtlist,
					levelnum));
    else if (single_node ((Node)clause)) 
      return (clause);
    else if (or_clause ((LispValue)clause)) 
      return ((Expr)make_orclause (replace_subclause_resultvar_refs 
				   (get_orclauseargs ((LispValue)clause),
				    ltlist,
				    rtlist,
				    levelnum)));
    else if (IsA(clause,ArrayRef)) {
      Expr temp;
      temp = replace_clause_resultvar_refs(get_refindexpr((ArrayRef)clause),
					      ltlist,
					      rtlist,
					      levelnum);
      set_refindexpr((ArrayRef)clause,(LispValue)temp);
      temp = replace_clause_resultvar_refs(get_refexpr((ArrayRef)clause),
					      ltlist,
					      rtlist,
					      levelnum);
      set_refexpr((ArrayRef)clause,(LispValue)temp);
      return(clause);
    }
    else if (is_funcclause ((LispValue)clause)) 
      return ((Expr)make_funcclause (get_function ((LispValue)clause),
					replace_subclause_resultvar_refs 
					(get_funcargs ((LispValue)clause),
					 ltlist,
					 rtlist,
					 levelnum)));
    else if (not_clause ((LispValue)clause))
      return((Expr)make_notclause((List)replace_clause_resultvar_refs
				   ((Expr)get_notclausearg ((LispValue)clause),
					  ltlist,
					  rtlist,
					  levelnum)));
     else /*   operator clause */
      return((Expr)make_opclause(replace_opid((Oper)get_op((LispValue)clause)),
					(Var)replace_clause_resultvar_refs 
					((Expr)get_leftop((LispValue)clause),
					 ltlist,
					 rtlist,
					 levelnum),
					(Var)replace_clause_resultvar_refs 
					((Expr)get_rightop((LispValue)clause),
					 ltlist,
					 rtlist,
					 levelnum)));
}  /* function end  */

/*  .. new-result-qual, replace-clause-resultvar-refs	 */

LispValue
replace_subclause_resultvar_refs (clauses,ltlist,rtlist,levelnum)
     LispValue clauses,ltlist,rtlist;
     int levelnum ;
{
     LispValue t_list = LispNil;
     LispValue clause = LispNil;
     LispValue temp = LispNil;

     foreach(clause,clauses) {
	 temp = (LispValue)replace_clause_resultvar_refs ((Expr)CAR(clause),
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
	  varno = lispCons (lispInteger(levelnum - 1),
			    lispCons((LispValue)get_resno ((Resdom)tl_resdom 
						  (match_varid (var, ltlist))),
				     LispNil));
     else
       varno = lispCons (lispInteger(levelnum),
			 lispCons ((LispValue)get_resno ((Resdom)tl_resdom 
					  (match_varid (var, rtlist))),
				   LispNil));
     
     return (MakeVar ((Index)varno,
		      0,
		      get_vartype (var),
		      get_varid (var), (Pointer)NULL));
     
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
     Plan plan ;
{
    if(null (plan)) 
      LispNil;
    else 
      if (IsA (plan,Join)) 
	set_join_tlist_references ((Join)plan);
      else 
	if (IsA(plan,SeqScan) &&
	    get_lefttree (plan) && 
	    IsA (get_lefttree(plan),Temp))
	  set_tempscan_tlist_references((SeqScan)plan);
	else 
	  if (IsA (plan,Sort))
	    set_temp_tlist_references ((Temp)plan);
	  else if (IsA(plan,Result))
		set_result_tlist_references((Result)plan);
	    else if (IsA(plan,Hash))
		set_tlist_references((Plan)get_lefttree(plan));
	    else if (IsA(plan,Choose)) {
		LispValue x;
		foreach (x, get_chooseplanlist((Choose)plan)) {
		    set_tlist_references((Plan)CAR(x));
		  }
	      }

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
    Plan 	outer = (Plan)get_lefttree ((Plan)join);
    Plan	inner = (Plan)get_righttree ((Plan)join);
    List 	new_join_targetlist = LispNil;
    TLE		temp = (TLE)NULL;
    LispValue  	entry = LispNil;
    List	inner_tlist = NULL;
    List	outer_tlist = NULL;
    TLE	xtl = 	(TLE)NULL;
    List	qptlist = get_qptargetlist((Plan)join);

     foreach(entry,qptlist) {
	 xtl = (TLE)CAR(entry);
	 inner_tlist = ( null(inner) ? LispNil : get_qptargetlist(inner));
	 outer_tlist = ( null(outer) ? LispNil : get_qptargetlist(outer));

	 temp = MakeTLE(tl_resdom (xtl),
			replace_clause_joinvar_refs((LispValue)get_expr(xtl),
						     outer_tlist,
						     inner_tlist)
			);
	 new_join_targetlist = nappend1(new_join_targetlist,temp);
     }
	
    set_qptargetlist ((Plan)join,new_join_targetlist);
    if ( ! null (outer))
      set_tlist_references (outer);
    if ( ! null (inner))
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

     Temp temp = (Temp)get_lefttree((Plan)tempscan);
     set_qptargetlist ((Plan)tempscan,
		       tlist_temp_references(get_tempid(temp),
					     get_qptargetlist((Plan)tempscan)));
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
     Plan source = (Plan)get_lefttree ((Plan)temp);

     if (!null(source)) {
	 set_tlist_references (source);
	 set_qptargetlist ((Plan)temp,
			   copy_vars (get_qptargetlist ((Plan)temp),
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
     LispValue inner_indxqual,outer_tlist;
     Index inner_relid ;
{
    LispValue t_list = LispNil;
    LispValue temp = LispNil;
    LispValue t_clause = LispNil;
    LispValue clause = LispNil;

    foreach (t_clause,inner_indxqual) {
	clause = CAR(t_clause);
	/*
	 * if inner scan on the right.
	 */
	if(OperandIsInner((LispValue)get_rightop(clause), inner_relid)) {
	    temp = make_opclause(replace_opid((Oper)get_op (clause)),
				 (Var)replace_clause_joinvar_refs 
				  ((LispValue)get_leftop (clause),
				   outer_tlist,
				   LispNil),
				  get_rightop (clause));
	    t_list = nappend1(t_list,temp);
	} 
	else {   /*   inner scan on left */
	    temp = make_opclause(replace_opid((Oper)get_op(clause)),
				get_leftop (clause),
				(Var)replace_clause_joinvar_refs(
						(LispValue)get_rightop(clause),
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
    if(IsA (clause,Var)) {
	if(temp = (LispValue)
	   replace_joinvar_refs((Var)clause,outer_tlist,inner_tlist))
	   return(temp);
	else
	  if (clause != LispNil)
	    return(clause);
	  else
	    return(LispNil);
    }
    else if (single_node ((Node)clause))
	return (clause);
    else if (or_clause (clause)) 
	return (make_orclause (replace_subclause_joinvar_refs 
				 (get_orclauseargs (clause),
				  outer_tlist,
				  inner_tlist)));
    else if (IsA(clause,ArrayRef)) {
	LispValue temp;
	temp = replace_clause_joinvar_refs(get_refindexpr((ArrayRef)clause),
					      outer_tlist,
					      inner_tlist);
	set_refindexpr((ArrayRef)clause,temp);
	temp = replace_clause_joinvar_refs(get_refexpr((ArrayRef)clause),
					      outer_tlist,
					      inner_tlist);
	set_refexpr((ArrayRef)clause,temp);
	return(clause);
      }
    else if (is_funcclause (clause)) {
	return (make_funcclause (get_function (clause),
				       replace_subclause_joinvar_refs 
						(get_funcargs (clause),
						outer_tlist,
						inner_tlist)));
       }
    else if (not_clause (clause)) 
	 return (make_notclause (replace_clause_joinvar_refs 
				      (get_notclausearg (clause),
					outer_tlist,
				       inner_tlist)));
    else if (is_clause (clause)) 
	 return (make_opclause(replace_opid((Oper)get_op(clause)),
				     (Var)replace_clause_joinvar_refs 
				     ((LispValue)get_leftop (clause),
				       outer_tlist,
				      inner_tlist),
				     (Var)replace_clause_joinvar_refs 
				     ((LispValue)get_rightop (clause),
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

    outer_resdom= tlist_member(var,outer_tlist);

    if ( !null(outer_resdom) && IsA (outer_resdom,Resdom) ) {
	 return (MakeVar (OUTER,
			  get_resno (outer_resdom),
			  get_vartype (var),
			  get_varid (var),
			  0));
     } 
    else {
	Resdom inner_resdom;
	inner_resdom = tlist_member(var,inner_tlist);
	if ( !null(inner_resdom) && IsA (inner_resdom,Resdom) ) {
	    return (MakeVar (INNER,
			     get_resno (inner_resdom),
			     get_vartype (var),
			     get_varid (var),
				 0));
	 } 
    } 
    return (Var) NULL;

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
				      get_resno((Resdom)tl_resdom (xtl)),
				  get_restype ((Resdom)tl_resdom (xtl)),
				      var1,
					  0));
	 
	 t_list = nappend1(t_list,temp);
     }
     return(t_list);
 }  /* function end  */

/*---------------------------------------------------------
 *
 * set_result_tlist_references
 *
 * Change the target list of a Result node, so that it correctly
 * addresses the tuples returned by its left tree subplan.
 *
 * NOTE:
 *  1) we ignore the right tree! (in the current implementation
 *     it is always nil
 *  2) this routine will probably *NOT* work with nested dot
 *     fields....
 */
void
set_result_tlist_references(resultNode)
Result resultNode;
{
    Plan subplan;
    List resultTargetList;
    List subplanTargetList;
    LispValue t;
    TLE entry;
    Expr expr;

    resultTargetList= get_qptargetlist((Plan)resultNode);

    /*
     * NOTE: we only consider the left tree subplan.
     * This is usually a seq scan.
     */
    subplan = (Plan)get_lefttree((Plan)resultNode);
    if (subplan != NULL) {
	subplanTargetList = get_qptargetlist(subplan);
    } else {
	subplanTargetList = LispNil;
    }

    /*
     * now for traverse all the entris of the target list.
     * These should be of the form (Resdom_Node Expression).
     * For every expression clause, call "replace_result_clause()"
     * to appropriatelly change all the Var nodes.
     */
    foreach (t, resultTargetList) {
	entry = (TLE) CAR(t);
	expr = (Expr) get_expr(entry);
	replace_result_clause((LispValue)expr, subplanTargetList);
    }

}

/*---------------------------------------------------------
 *
 * replace_result_clause
 *
 * This routine is called from set_result_tlist_references().
 * and modifies the expressions of the target list of a Result
 * node so that all Var nodes reference the target list of its subplan.
 * 
 */

void
replace_result_clause(clause, subplanTargetList)
LispValue clause;
List subplanTargetList;		/* target list of the subplan */
{
    
    Expr subExpr;
    LispValue t;
    List subClause;
    LispValue subplanVar;

    if (IsA(clause,Var)) {
	/*
	 * Ha! A Var node!
	 */
	subplanVar = match_varid((Var)clause, subplanTargetList);
	/*
	 * Change the varno & varattno fields of the
	 * var node.
	 *
	 */
	set_varno((Var)clause,(Index) OUTER);
	set_varattno((Var)clause, get_resno((Resdom)tl_resdom(subplanVar)));
    } else if (is_funcclause(clause)) {
	/*
	 * This is a function. Recursively call this routine
	 * for its arguments...
	 */
	subClause = get_funcargs(clause);
	foreach (t, subClause) {
	    replace_result_clause(CAR(t),subplanTargetList);
	   }
    } else if (IsA(clause,ArrayRef)) {
	/*
	 * This is an arrayref. Recursively call this routine
	 * for its expression and its index expression...
	 */
	replace_result_clause(get_refindexpr((ArrayRef)clause),
			      subplanTargetList);
	replace_result_clause(get_refexpr((ArrayRef)clause),
			      subplanTargetList);
    } else if (is_clause(clause)) {
	/*
	 * This is an operator. Recursively call this routine
	 * for both its left and right operands
	 */
	subClause = (List)get_leftop(clause);
	replace_result_clause(subClause,subplanTargetList);
	subClause = (List)get_rightop(clause);
	replace_result_clause(subClause,subplanTargetList);
    } else if (IsA(clause,Param) || IsA(clause,Const)) {
	/* do nothing! */
    } else {
	/*
	 * Ooops! we can not handle that!
	 */
	elog(WARN,"replace_result_clause: Can not handle this tlist!\n");
    }
}

bool
OperandIsInner(opnd, inner_relid)
    LispValue opnd;
    int inner_relid;
{

    /*
     * Can be the inner scan if its a varnode or a function and the
     * inner_relid is equal to the varnode's var number or in the 
     * case of a function the first argument's var number (all args
     * in a functional index are from the same relation).
     */
    if ( IsA (opnd,Var) && 
	 (inner_relid == get_varno((Var)opnd)) )
    {
	return true;
    }
    if ( consp(opnd) && IsA(CAR(opnd),Func) )
    {
	LispValue firstArg = CAR(get_funcargs(opnd));

	if ( IsA (firstArg,Var) &&
	     (inner_relid == get_varno((Var)firstArg)) )
	{
		return true;
	}
    }
    return false;
}
