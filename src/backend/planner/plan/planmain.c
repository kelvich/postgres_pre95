
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
#include "planner/planmain.h"
#include "planner/sortresult.h"
#include "planner/sortresult.h"
#include "planner/tlist.h"
#include "tcop/dest.h"

extern int testFlag;
/* ----------------
 *	Result creator declaration
 * ----------------
 */
extern Result RMakeResult();
extern Choose RMakeChoose();

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
     Plan	subplan = (Plan)NULL;
     List	subtlist = LispNil;
     Plan 	restplan = (Plan)NULL;
     List	resttlist = LispNil;
     List	relation_level_clauses = LispNil;
     Plan 	plan = (Plan)NULL;

     extern Plan subplanner();
     
     
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
	     if ( command_type == DELETE ) {
		 return ((Plan)make_seqscan((List) NULL, 
					     (List) NULL,
					     (Index) CInteger(
						_query_result_relation_),
					     (Node) NULL ));
	     } else
	       return((Plan)NULL);
	 }
	 constant_qual = pull_constant_clauses (qual);
	 qual = nset_difference (qual,constant_qual);
	 fix_opids (constant_qual);
	 sortkeys = relation_sortkeys (tlist);
	 flattened_tlist = flatten_tlist (tlist);
	 if (flattened_tlist)
	   level_tlist = flattened_tlist;
	 else if (tlist)
	   level_tlist = tlist;
	 else
	   level_tlist = (List)NULL;
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
				    LispNil,
				    LispNil));
	       break;

	     case DELETE :
	     case REPLACE : 
	       {
		    /* XXX - let form, maybe incorrect */
		   SeqScan scan = make_seqscan (tlist,
					       LispNil,
					       (Index) CInteger(
						   _query_result_relation_),
					       LispNil);
		   if ( consp (constant_qual) ) {
		       return ((Plan)make_result (tlist,
						 LispNil,
						 constant_qual,
						 scan,
						 LispNil));
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
	  set_result_tlist_references(plan);

	  if ( valid_numkeys (sortkeys) ) 
	    return (sort_level_result (plan,sortkeys));
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
       return (sort_level_result (subplan,sortkeys));
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
     LispValue flat_tlist,original_tlist,qual,level,sortkeys ;
{
    Rel final_relation;

     /* Initialize the targetlist and qualification, adding entries to */
     /* *query-relation-list* as relation references are found (e.g., in the */
     /*  qualification, the targetlist, etc.) */

    _query_relation_list_ = LispNil;
    initialize_targetlist (flat_tlist);
    initialize_qualification (qual);

    
/* Find all possible scan and join paths. */
/* Mark all the clauses and relations that can be processed using special */
/* join methods, then do the exhaustive path search. */

    initialize_join_clause_info (_query_relation_list_);
    _query_relation_list_ = find_paths (_query_relation_list_,
					level,
					sortkeys);
    if (_query_relation_list_)
      final_relation = (Rel)CAR (_query_relation_list_);
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
	  List prunePathListForTest();
	  LispValue x;
	  Path path;
	  Plan plan;
	  Choose chooseplan;
	  pathlist = get_pathlist(final_relation);
	  pathlist = prunePathListForTest(pathlist);
	  foreach (x, pathlist) {
	      path = (Path)CAR(x);
	      plan = create_plan(path, original_tlist);
	      planlist = nappend1(planlist, plan);
	    }
	  chooseplan = RMakeChoose();
	  set_chooseplanlist(chooseplan, planlist);
	  set_qptargetlist(chooseplan, get_qptargetlist(plan));
	  return (Plan)chooseplan;
	}
      return (create_plan (get_cheapestpath (final_relation),
			   original_tlist));
     }
    else {
	printf(" Warn: final relation is nil \n");
	return(create_plan (LispNil,original_tlist));
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
    set_lefttree((Plan)node, left);
    set_righttree((Plan)node, right);

    set_resrellevelqual(node, resrellevelqual); 
    set_resconstantqual(node, resconstantqual); 
    set_resstate(node, NULL);
    
    return(node);
} 

bool
pathShouldPruned(path, order_expected)
Path path;
bool order_expected;
{
    if (IsA(path,MergePath)) {
	return pathShouldPruned(get_outerjoinpath(path), 
				!get_outersortkeys(path)) ||
	       pathShouldPruned(get_innerjoinpath(path), 
				!get_innersortkeys(path));
      }
    if (IsA(path,HashPath)) {
	return pathShouldPruned(get_outerjoinpath(path), false) ||
	       pathShouldPruned(get_innerjoinpath(path), false);
      }
    if (IsA(path,JoinPath)) {
	if (!IsA(get_innerjoinpath(path),IndexPath) &&
	    !IsA(get_innerjoinpath(path),JoinPath) &&
	    length(get_relids(get_parent(get_innerjoinpath(path)))) == 1)
	   return true;
	return pathShouldPruned(get_outerjoinpath(path), order_expected) ||
	       pathShouldPruned(get_innerjoinpath(path), false);
      }
    if (IsA(path,IndexPath)) {
	return lispNullp(get_indexqual(path)) && !order_expected;
      }
    return false;
}

List
prunePathListForTest(pathlist)
List pathlist;
{
    LispValue x, y;
    Path path, path1;
    List path_prune_list = LispNil;
    List new_pathlist;

    foreach (x, pathlist) {
	path = (Path)CAR(x);
	if (pathShouldPruned(path, false))
	    path_prune_list = nappend1(path_prune_list, path);
      }
    new_pathlist = nset_difference(pathlist, path_prune_list);
    path_prune_list = LispNil;
    foreach (x, new_pathlist) {
	path = (Path)CAR(x);
	if (IsA(path,MergePath)) {
	    foreach (y, CDR(x)) {
		path1 = (Path)CAR(y);
		if (IsA(path1,MergePath) && 
		    equal(get_outerjoinpath(path), get_innerjoinpath(path1)) &&
		    equal(get_innerjoinpath(path), get_outerjoinpath(path1)))
		    path_prune_list = nappend1(path_prune_list, path1);
	      }
	  }
       }
    new_pathlist = nset_difference(new_pathlist, path_prune_list);
	    
    return new_pathlist;
}

bool
plan_isomorphic(p1, p2)
Plan p1, p2;
{
    if (p1 == NULL) return (p2 == NULL);
    if (p2 == NULL) return (p1 == NULL);
    if (IsA(p1,Join) && IsA(p2,Join)) {
	return (plan_isomorphic(get_lefttree(p1), get_lefttree(p2)) &&
	        plan_isomorphic(get_righttree(p1), get_righttree(p2))) ||
	       (plan_isomorphic(get_lefttree(p1), get_righttree(p2)) &&
		plan_isomorphic(get_righttree(p1), get_lefttree(p2)));
      }
    if (IsA(p1,Scan) && IsA(p2,Scan)) {
	if (get_scanrelid(p1) == get_scanrelid(p2))
	    if (get_scanrelid(p1) == _TEMP_RELATION_ID_)
		return plan_isomorphic(get_lefttree(p1), get_lefttree(p2));
	    else
		return true;
	else if (get_scanrelid(p1) == _TEMP_RELATION_ID_)
	    return plan_isomorphic(get_lefttree(p1), p2);
	else if (get_scanrelid(p2) == _TEMP_RELATION_ID_)
	    return plan_isomorphic(p1, get_lefttree(p2));
	return false;
      }
    if (IsA(p1,Temp) || IsA(p1,Hash))
	return plan_isomorphic(get_lefttree(p1), p2);
    if (IsA(p2,Temp) || IsA(p2,Hash))
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
	g = lispCons(p1, LispNil);
	foreach (x, CDR(plist)) {
	    p2 = (Plan)CAR(x);
	    if (plan_isomorphic(p1, p2)) {
		g = nappend1(g, p2);
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
	if (plan_isomorphic(get_lefttree(p1), get_lefttree(p2))) {
	    setPlanStats(get_lefttree(p1), get_lefttree(p2));
	    setPlanStats(get_righttree(p1), get_righttree(p2));
	  }
	else {
	    setPlanStats(get_lefttree(p1), get_righttree(p2));
	    setPlanStats(get_righttree(p1), get_lefttree(p2));
	  }
	return;
      }
    if (IsA(p1,Scan) && IsA(p2,Scan)) {
	set_plan_size(p2, get_plan_size(p1));
	if (get_scanrelid(p1) == _TEMP_RELATION_ID_)
	    setPlanStats(get_lefttree(p1), get_lefttree(p2));
	return;
      }
    if (IsA(p1,Temp)) {
	setPlanStats(get_lefttree(p1), p2);
	return;
      }
    if (IsA(p2,Temp)) {
	set_plan_size(p2, get_plan_size(p1));
	setPlanStats(p1, get_lefttree(p2));
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
	if (get_scanrelid(p) == _TEMP_RELATION_ID_)
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
    foreach (x, plangroups) {
	plangroup = CAR(x);
	nlplan = pickNestLoopPlan(plangroup);
	if (IsA(nlplan,Join)) {
	    resetPlanStats(nlplan);
	    ProcessQuery(parsetree, nlplan, None);
	    setPlanGroupStats(nlplan, nLispRemove(plangroup, nlplan));
	  }
	else
	    break;
      }
    _exec_collect_stats_ = false;
    ordered_planlist = LispNil;
    foreach (x, plangroups) {
	ordered_planlist = nconc(ordered_planlist, CAR(x));
      }
    return ordered_planlist;
}
