
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

#include "planner/internal.h"
#include "pg_lisp.h"
#include "parse.h"
#include "relation.h"
#include "relation.a.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "planner/planmain.h"
#include "planner/clause.h"
#include "planner/sortresult.h"
#include "planner/tlist.h"
#include "planner/sortresult.h"
#include "planner/createplan.h"
#include "planner/allpaths.h"


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
     /* 2. Determine the keys on which the result is to be sorted. */
     /* 3. Create a target list that consists solely of (resdom var) target */
     /*    list entries, i.e., contains no arbitrary expressions. */
     
     if ( currentlevel == 1) {
	 /* A command without a target list or qualification is an error, */
	 /* except for "delete foo". */
	 
	 if (null (tlist) && null (qual)) {
	     if ( command_type == DELETE ) {
		 return ((Plan)MakeSeqScan ((List) NULL, 
					     (List) NULL,
					 (Index) _query_result_relation_,
					     (Node) NULL ));
	     } else
	       return((Plan)NULL);
	 }
	 constant_qual = pull_constant_clauses (qual);
	 qual = set_difference (qual,constant_qual);
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

	     case REPLACE : 
	       {
		    /* XXX - let form, maybe incorrect */
		   SeqScan scan = MakeSeqScan (tlist,
					       LispNil,
					       _query_result_relation_,
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
    
    if (final_relation)
      return (create_plan (get_cheapestpath (final_relation),
			   original_tlist));
    else {
	printf(" Warn: final relation is nil \n");
	return(create_plan (LispNil,original_tlist));
    }

}  /* function end  */

Result
make_result( tlist,resrellevelqual,left,right)
     List tlist;
     List resrellevelqual;
     Plan left,right;
{
    extern void PrintResult();
    
    Result node = New_Node(Result);
    
    set_resrellevelqual( node ,resrellevelqual); 
    set_resconstantqual( node ,LispNil); 
    set_qptargetlist ((Plan)node, tlist);
    set_lefttree((Plan)node,left);
    set_righttree((Plan)node,right);
    node->printFunc = PrintResult; 
    return(node);
} 
