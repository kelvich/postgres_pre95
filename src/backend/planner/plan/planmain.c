
/*     
 *      FILE
 *     	planmain
 *     
 *      DESCRIPTION
 *     	Routines to plan a single query
 *     
 */
/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		Plan query_planner();
 */


#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "parser/parse.h"

#include "planner/internal.h"
#include "planner/allpaths.h"
#include "planner/clause.h"
#include "planner/createplan.h"
#include "planner/keys.h"
#include "planner/planmain.h"
#include "planner/setrefs.h"
#include "planner/sortresult.h"
#include "planner/sortresult.h"
#include "planner/tlist.h"
#include "tcop/dest.h"
#include "utils/log.h"
#include "nodes/mnodes.h"
#include "utils/mcxt.h"

extern int testFlag;

/*    
 *    	query_planner
 *    
 *    	Routine to create a query plan.  It does so by first creating a
 *    	subplan for the topmost level of attributes in the query.  Then,
 *    	it modifies all target list and qualifications to consider the next
 *    	level of nesting and creates a plan for this modified query by
 *    	recursively calling itself.  The two pieces are then merged together
 *    	by creating a result node that indicates which attributes should
 *    	be placed where and any relation level qualifications to be
 *    	satisfied.
 *    
 *    	'command-type' is the query command, e.g., retrieve, delete, etc.
 *    	'tlist' is the target list of the query
 *    	'qual' is the qualification of the query
 *    	'currentlevel' is the current nesting level
 *    	'maxlevel' is the maximum nesting level in this query
 *    
 *    	Returns a query plan.
 *    
 */

/*  .. init-query-planner, query_planner    */

Plan
query_planner (command_type,tlist,qual,currentlevel,maxlevel)
     int command_type;
     List tlist,qual;
     int currentlevel;
     int maxlevel ;
{
     LispValue constant_qual = LispNil;
     LispValue sortkeys = LispNil;
     LispValue flattened_tlist = LispNil;
     List 	level_tlist = LispNil;
     List	result_of_flatten_tlist = LispNil;
     List	agg_tlist = LispNil;
     List	all_but_agg_tlist = LispNil;
     int	aggidnum = -17; /* okay, a hack on uniquness */
     Plan	thePlan = (Plan)NULL;
     Plan	subplan = (Plan)NULL;
     List	subtlist = LispNil;
     Plan 	restplan = (Plan)NULL;
     List	resttlist = LispNil;
     List	relation_level_clauses = LispNil;
     Plan 	plan = (Plan)NULL;

     /*    For the topmost nesting level, */
     /* 1. Pull out any non-variable qualifications so these can be put in */
     /*    the topmost result node.  The opids for the remaining */
     /*    qualifications will be changed to regprocs later. */
     /*    ------
      *    NOTE: we used to have one field called 'opno' in the Oper
      *    nodes which used to be also be called 'opid' in some comments.
      *    This field was either the pg_operator oid, or the
      *    regproc oid. However now we have two separate
      *    fields, the 'opno' (pg_operator oid) and the 'opid' (pg_proc
      *    oid) so things are a little bit more clear now...   [sp.]
      *    ------
      */
     /* 2. Determine the keys on which the result is to be sorted. */
     /* 3. Create a target list that consists solely of (resdom var) target */
     /*    list entries, i.e., contains no arbitrary expressions. */
     
     if ( currentlevel == 1) {
	 /* A command without a target list or qualification is an error, */
	 /* except for "delete foo". */
	 
	 if (null (tlist) && null (qual)) {
	     if ( command_type == DELETE ||
	     /* Total hack here. I don't know how to handle
		statements like notify in action bodies.
		Notify doesn't return anything but
		scans a system table. */
		 command_type == NOTIFY) {
		 return ((Plan)make_seqscan(LispNil, LispNil,
				       (Index)CInteger(_query_result_relation_),
				       (Plan)NULL));
	     } else
	       return((Plan)NULL);
	 }
	 constant_qual = pull_constant_clauses (qual);
	 qual = nset_difference (qual,constant_qual);
	 fix_opids (constant_qual);
	 sortkeys = relation_sortkeys (tlist);
	 /* flatten_tlist will now return (var, aggs, rest) */

	 result_of_flatten_tlist = flatten_tlist(tlist);

	 flattened_tlist = CAR(result_of_flatten_tlist);
	 agg_tlist = CADR(result_of_flatten_tlist);

	 result_of_flatten_tlist = CDR(result_of_flatten_tlist);
	 all_but_agg_tlist = CADR(result_of_flatten_tlist);
	 if (flattened_tlist)
	   level_tlist = flattened_tlist;
	 else if (tlist)
	   level_tlist = all_but_agg_tlist;
	   /* orig_tlist minus the aggregates */
	 else
	   level_tlist = (List)NULL;
     }

  if(agg_tlist) {
     if(all_but_agg_tlist) {
	thePlan = query_planner(command_type,
		all_but_agg_tlist, qual, currentlevel, maxlevel);
     }
     else {
	 /* all we have are aggregates */
	 thePlan = (Plan)make_agg(CAR(agg_tlist), --aggidnum);
	 /* also, there should never be a case by now where we neither 
	  * have aggs nor all_but_aggs
	  */
	  agg_tlist = CDR(agg_tlist);
     }
     if(agg_tlist != NULL) {  /* are there any aggs left */
	   thePlan = make_aggplan(thePlan, agg_tlist, aggidnum);
      }
      return(thePlan);
   }

     /*    A query may have a non-variable target list and a non-variable */
     /*    qualification only under certain conditions: */
     /*    - the query creates all-new tuples, or */
     /*   - the query is a replace (a scan must still be done in this case). */

     if ((0 == maxlevel || 
	  (1 == currentlevel && null (flattened_tlist))) &&
	 null (qual)) {
	  switch (command_type) {
	     case RETRIEVE : 
	     case APPEND :
	       return ((Plan)make_result (tlist,
				    LispNil,
				    constant_qual,
				    (Plan)NULL,
				    (Plan)NULL));
	       break;

	     case DELETE :
	     case REPLACE : 
	       {
		    /* XXX - let form, maybe incorrect */
		   SeqScan scan = make_seqscan (tlist,
					       LispNil,
					       (Index)CInteger(
						   _query_result_relation_),
					       (Plan)NULL);
		   if ( consp (constant_qual) ) {
		       return ((Plan)make_result (tlist,
						 LispNil,
						 constant_qual,
						 (Plan)scan,
						 (Plan)NULL));
		   } 
		   else {
		       return ((Plan)scan);
		   } 
	       }
	       break;
	       
	     default: /* return nil */
	       return((Plan)NULL);
	  }
     }
/*    Find the subplan (access path) for attributes at this nesting level */
/*    and destructively modify the target list of the newly created */
/*    subplan to contain the appropriate join references. */
	  
     subplan = subplanner (level_tlist,
				tlist,
				qual,
				currentlevel,
				sortkeys);
     
     subtlist = get_qptargetlist (subplan);
     set_tlist_references (subplan);

/*    If there are further nesting levels, call the planner again to */
/*    create subplans for lower levels of nesting.  Modify the target */
/*    list and qualifications to reflect the new nesting level. */

       if ( !(currentlevel == maxlevel ) ) {
	    restplan = query_planner (command_type,
				      new_level_tlist (level_tlist,
						       subtlist,
						       currentlevel),
				      new_level_qual (qual,
						      subtlist,
						      currentlevel),
				      currentlevel + 1,
				      maxlevel);
       }
/*    Build a result node linking the plan for deeper nesting levels and */
/*    the subplan for this level.  Note that a plan that has no right */
/*    subtree may still have relation level and constant quals, so we */
/*    still have to build an appropriate result node. */

     relation_level_clauses = pull_relation_level_clauses (qual);
     
     if ( restplan ||
	 relation_level_clauses ||
	 constant_qual) {
	  List resttlist = LispNil;
	  List subtlist = LispNil;
	  Plan plan;
	  
	  if ( restplan ) 
	    resttlist = get_qptargetlist (restplan);
	  subtlist = get_qptargetlist (subplan);
	  
	  plan = (Plan)make_result (new_result_tlist (tlist,
						      subtlist,
						      resttlist,
						      currentlevel,
						      valid_sortkeys(sortkeys)),
				   new_result_qual(relation_level_clauses,
						   subtlist,
						   resttlist,
						   currentlevel),
				    constant_qual,
				   subplan,
				   restplan);
	  /*
	   * Change all varno's of the Result's node target list.
	   */
	  set_result_tlist_references((Result)plan);

	  if ( valid_numkeys (sortkeys) ) 
	    return (sort_level_result (plan, CInteger(sortkeys)));
	  else 
	    return (plan);
     }
	/*    Remodify the target list of the subplan so it no longer is */
	/*    'flattened', unless this has already been done in create_plan */
	/*    for a path that had to be explicitly sorted. */

     if ( 1 == maxlevel &&
	 !(IsA(subplan,Temp) || 
	   (IsA(subplan,SeqScan) && 
	    get_lefttree(subplan) && 
	    IsA (get_lefttree (subplan),Temp)))) {
	  set_qptargetlist (subplan,
			    flatten_tlist_vars (tlist,
						get_qptargetlist (subplan)));
	}
	/*    If a sort is required, explicitly sort this subplan since: */
	/*    there is only one level of attributes in this query, and */
	/*the sort spans across expressions and/or multiple relations and so */
	/*    	indices were not considered in planning the sort. */

     if ( valid_numkeys (sortkeys) ) 
       return (sort_level_result (subplan,CInteger(sortkeys)));
     else 
       return (subplan);

}  /* function end  */

/*    
 *    	subplanner
 *    
 *    	Subplanner creates an entire plan consisting of joins and scans
 *    	for processing a single level of attributes.  Nested attributes are 
 *    	transparent (i.e., essentially ignored) from here on.
 *    
 *    	'flat-tlist' is the flattened target list
 *	--which now is of the form (vars, aggs)--, jc
 *    	'original-tlist' is the unflattened target list
 *    	'qual' is the qualification to be satisfied
 *    	'level' is the current nesting level
 *    
 *    	Returns a subplan.
 *    
 */

/*  .. query_planner    */

Plan
subplanner (flat_tlist,original_tlist,qual,level,sortkeys)
     LispValue flat_tlist,original_tlist,qual,sortkeys ;
     int level;
{
    Rel final_relation;
    List final_relation_list;

     /* Initialize the targetlist and qualification, adding entries to */
     /* *query-relation-list* as relation references are found (e.g., in the */
     /*  qualification, the targetlist, etc.) */

    _base_relation_list_ = LispNil;
    _join_relation_list_ = LispNil;
    initialize_targetlist (flat_tlist);
    initialize_qualification (qual);

    
/* Find all possible scan and join paths. */
/* Mark all the clauses and relations that can be processed using special */
/* join methods, then do the exhaustive path search. */

    initialize_join_clause_info (_base_relation_list_);
    final_relation_list = find_paths (_base_relation_list_,
					level,
					sortkeys);
    if (final_relation_list)
      final_relation = (Rel)CAR (final_relation_list);
    else
      final_relation = (Rel)LispNil;
     
/*    If we want a sorted result and indices could have been used to do so, */
/*    then explicitly sort those paths that weren't sorted by indices so we */
/*  can determine whether using the implicit sort (index) is better than an */
/*  explicit one. */

     if ( valid_sortkeys (sortkeys)) {
	 sort_relation_paths (get_pathlist (final_relation),
			       sortkeys,
			      final_relation,
			      compute_targetlist_width(original_tlist));
     }
/*    Determine the cheapest path and create a subplan corresponding to it. */
    
    if (final_relation) {
      if (testFlag) {
	  List planlist = LispNil;
	  List pathlist = LispNil;
	  LispValue x;
	  Path path;
	  Plan plan;
	  Choose chooseplan;
	  pathlist = get_pathlist(final_relation);
	  foreach (x, pathlist) {
	      path = (Path)CAR(x);
	      plan = create_plan(path, original_tlist);
	      planlist = nappend1(planlist, (LispValue)plan);
	    }
	  chooseplan = RMakeChoose();
	  set_chooseplanlist(chooseplan, planlist);
	  set_qptargetlist((Plan)chooseplan, get_qptargetlist(plan));
	  return (Plan)chooseplan;
	}
      return (create_plan ((Path)get_cheapestpath (final_relation),
			   original_tlist));
     }
    else {
	printf(" Warn: final relation is nil \n");
	return(create_plan ((Path)NULL, original_tlist));
    }
    
}  /* function end  */

Result
make_result( tlist,resrellevelqual,resconstantqual,left,right)
     List tlist;
     List resrellevelqual,resconstantqual;
     Plan left,right;
{
    Result node = RMakeResult();
    
    set_cost((Plan) node, 0.0);
    set_fragment((Plan) node, 0);
    set_state((Plan) node, NULL);
    set_qptargetlist((Plan)node, tlist);
    set_qpqual((Plan) node, LispNil);
    set_lefttree((Plan)node, (PlanPtr)left);
    set_righttree((Plan)node, (PlanPtr)right);

    set_resrellevelqual(node, resrellevelqual); 
    set_resconstantqual(node, resconstantqual); 
    set_resstate(node, NULL);
    
    return(node);
} 

/* for modifying in case of aggregates.
 * we know that there are aggregates in the tlist*/
Plan
make_aggplan(subplan, agg_tlist, aggidnum)
    Plan subplan;
    List agg_tlist;
    int aggidnum;
{
     List agg_tl_entry = LispNil;
     List add_to_tl = LispNil;
     NestLoop joinnode = (NestLoop)NULL;
     LispValue entry = LispNil;
     List level_tlist = LispNil;
     Agg aggnode;
/*    extern search_quals(); */

	/* we're assuming that subplan is not null from the caller*/
	agg_tl_entry = CAR(agg_tlist);
	aggnode = make_agg(agg_tl_entry, --aggidnum);

	level_tlist = nconc(get_qptargetlist(subplan),
					 get_qptargetlist((Plan)aggnode));
	joinnode = make_nestloop(level_tlist, LispNil, (Plan)aggnode,
							  subplan);
	/* inner tree is the aggregate, outer tree is the rest of
	 * the plan.  quals are nil here since we don't have aggregate
	 * functions yet.
	 */

	if(CDR(agg_tlist)) {  /* is this the last agg_tlist entry? */
	   /* if not */
	   return make_aggplan((Plan)joinnode, CDR(agg_tlist), aggidnum);
            /* XXX jc.  type problem with joinnode and Plan subplan? */
         }
	 else { /* if so */
	      return((Plan) joinnode);
	 }
}


	

bool
plan_isomorphic(p1, p2)
Plan p1, p2;
{
    if (p1 == NULL) return (p2 == NULL);
    if (p2 == NULL) return (p1 == NULL);
    if (IsA(p1,Join) && IsA(p2,Join)) {
	return (plan_isomorphic(get_lefttree(p1), get_lefttree(p2)) &&
	        plan_isomorphic(get_righttree(p1), get_righttree(p2)));
      }
    if (IsA(p1,Scan) && IsA(p2,Scan)) {
	if (get_scanrelid((Scan)p1) == get_scanrelid((Scan)p2))
	    if (get_scanrelid((Scan)p1) == _TEMP_RELATION_ID_)
		return plan_isomorphic(get_lefttree(p1), get_lefttree(p2));
	    else
		return true;
	else if (get_scanrelid((Scan)p1) == _TEMP_RELATION_ID_)
	    return plan_isomorphic(get_lefttree(p1), p2);
	else if (get_scanrelid((Scan)p2) == _TEMP_RELATION_ID_)
	    return plan_isomorphic(p1, get_lefttree(p2));
	return false;
      }
    if (IsA(p1,Temp) || IsA(p1,Hash) ||
	(IsA(p1,Scan) && get_scanrelid((Scan)p1) == _TEMP_RELATION_ID_))
	return plan_isomorphic(get_lefttree(p1), p2);
    if (IsA(p2,Temp) || IsA(p2,Hash) ||
	(IsA(p2,Scan) && get_scanrelid((Scan)p2) == _TEMP_RELATION_ID_))
	return plan_isomorphic(p1, get_lefttree(p2));
    return false;
}

List
group_planlist(planlist)
List	planlist;
{
    List plangroups = LispNil;
    Plan p1, p2;
    List plist;
    List g, x;

    plist = planlist;
    while (!lispNullp(plist)) {
	p1 = (Plan)CAR(plist);
	g = lispCons((LispValue)p1, LispNil);
	foreach (x, CDR(plist)) {
	    p2 = (Plan)CAR(x);
	    if (plan_isomorphic(p1, p2)) {
		g = nappend1(g, (LispValue)p2);
	      }
	  }
	plist = nset_difference(plist, g);
	plangroups = nappend1(plangroups, g);
      }
    return plangroups;
}

bool
allNestLoop(plan)
Plan	plan;
{
    if (plan == NULL) return true;
    if (IsA(plan,Temp) ||
	(IsA(plan,Scan) && get_scanrelid((Scan)plan) == _TEMP_RELATION_ID_))
	return allNestLoop(get_lefttree(plan));
    if (IsA(plan,NestLoop)) {
	return allNestLoop(get_lefttree(plan)) &&
	       allNestLoop(get_righttree(plan));
      }
    if (IsA(plan,Join))
	return false;
    return true;
}

Plan
pickNestLoopPlan(plangroup)
List	plangroup;
{
    LispValue x;
    Plan p;

    foreach (x, plangroup) {
	p = (Plan)CAR(x);
	if (allNestLoop(p))
	    return p;
      }
    return NULL;
}

void
setPlanStats(p1, p2)
Plan p1, p2;
{
    if (p1 == NULL || p2 == NULL)
	return;
    if (IsA(p1,Join) && IsA(p2,Join)) {
	set_plan_size(p2, get_plan_size(p1));
	setPlanStats(get_lefttree(p1), get_lefttree(p2));
	/*
	setPlanStats(get_righttree(p1), get_righttree(p2));
	*/
	return;
      }
    if (IsA(p1,Temp) || IsA(p1,Hash) ||
	(IsA(p1,Scan) && get_scanrelid((Scan)p1) == _TEMP_RELATION_ID_)) {
	setPlanStats(get_lefttree(p1), p2);
	return;
      }
    if (IsA(p2,Temp) || IsA(p2,Hash) ||
	(IsA(p2,Scan) && get_scanrelid((Scan)p2) == _TEMP_RELATION_ID_)) {
	set_plan_size(p2, get_plan_size(p1));
	setPlanStats(p1, get_lefttree(p2));
	return;
      }
    if (IsA(p1,Scan) && IsA(p2,Scan)) {
	set_plan_size(p2, get_plan_size(p1));
	return;
      }
    return;
}

void
resetPlanStats(p)
Plan p;
{
    if (p == NULL) return;
    set_plan_size(p, 0);
    if (IsA(p,Join)) {
	resetPlanStats(get_lefttree(p));
	resetPlanStats(get_righttree(p));
	return;
      }
    if (IsA(p,Scan)) {
	if (get_scanrelid((Scan)p) == _TEMP_RELATION_ID_)
	   resetPlanStats(get_lefttree(p));
	return;
      }
    resetPlanStats(get_lefttree(p));
    return;
}

void
setPlanGroupStats(plan, planlist)
Plan	plan;
List	planlist;
{
    List x;
    Plan p;

    foreach (x, planlist) {
	p = (Plan)CAR(x);
	setPlanStats(plan, p);
     }
}

bool _exec_collect_stats_ = false;

List
setRealPlanStats(parsetree, planlist)
List	parsetree;
List	planlist;
{
    List plangroups;
    List plangroup;
    LispValue x;
    List ordered_planlist;
    Plan nlplan;

    _exec_collect_stats_ = true;
    plangroups = group_planlist(planlist);
    ordered_planlist = LispNil;
    foreach (x, plangroups) {
	plangroup = CAR(x);
	nlplan = pickNestLoopPlan(plangroup);
	if (nlplan == NULL) {
	    elog(NOTICE, "no nestloop plan in plan group!");
	    break;
	  }
	if (IsA(nlplan,Join)) {
	    resetPlanStats(nlplan);
	    p_plan(nlplan);
	    ResetUsage();
	    ProcessQuery(parsetree, nlplan, None);
	    ShowUsage();
	    plangroup = nLispRemove(plangroup, (LispValue)nlplan);
	    setPlanGroupStats(nlplan, plangroup);
	  }
	else {
	    ordered_planlist = plangroup;
	    break;
	  }
	ordered_planlist = nconc(ordered_planlist, plangroup);
      }
    _exec_collect_stats_ = false;
    return ordered_planlist;
}

bool
plan_contain_redundant_hashjoin(plan)
Plan plan;
{
    Plan outerplan, innerplan;
    int outerpages, innerpages;
    if (plan == NULL)
	return false;
    if (IsA(plan,HashJoin)) {
	outerplan = (Plan) get_lefttree(plan);
	innerplan = (Plan) get_lefttree((Plan)get_righttree(plan));
	outerpages = page_size(get_plan_size(outerplan), 
			       get_plan_width(outerplan));
	innerpages = page_size(get_plan_size(innerplan),
			       get_plan_width(innerplan));
	if (!IsA(outerplan,Join) && outerpages < innerpages)
	    return true;
      }
    if (IsA(plan,Join))
	return plan_contain_redundant_hashjoin(get_lefttree(plan)) ||
	       plan_contain_redundant_hashjoin(get_righttree(plan));
    if (IsA(plan,Temp) || IsA(plan,Hash) ||
	(IsA(plan,Scan) && get_scanrelid((Scan)plan) == _TEMP_RELATION_ID_))
	return plan_contain_redundant_hashjoin(get_lefttree(plan));
    return false;
}

List
pruneHashJoinPlans(planlist)
List planlist;
{
    LispValue x;
    Plan plan;
    List retlist;

    retlist = LispNil;
    foreach (x, planlist) {
	plan = (Plan)CAR(x);
	if (!plan_contain_redundant_hashjoin(plan))
	    retlist = nappend1(retlist, (LispValue)plan);
      }
    return retlist;
}
