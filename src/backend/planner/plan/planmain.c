
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
 *     		query_planner
 */

#include "internal.h"


extern LispValue subplanner();

#define DELETE  10
#define APPEND  11
#define RETRIEVE 12
#define REPLACE 13

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

LispValue
query_planner (command_type,tlist,qual,currentlevel,maxlevel)
     LispValue command_type,tlist,qual,currentlevel,maxlevel ;
{
     /* XXX - prog form, maybe incorrect */
     LispValue constant_qual = LispNil;
     LispValue sortkeys = LispNil;
     LispValue flattened_tlist = LispNil;
     LispValue level_tlist = LispNil;
     LispValue subplan = LispNil;
     LispValue subtlist = LispNil;
     LispValue restplan = LispNil;
     LispValue resttlist = LispNil;
     LispValue relation_level_clauses = LispNil;
     LispValue plan = LispNil;

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
	       if ( equal (command_type,DELETE) ) {
		    return (make_seqscan (LispNil,
					  LispNil,
					  _query_result_relation_,
					  LispNil));
	       } 
	       else
		 return(LispNil);
	  }
	  constant_qual = pull_constant_clauses (qual);
	  qual = set_difference (qual,constant_qual);
	  fix_opids (constant_qual);
	  sortkeys = relation_sortkeys (tlist);
	  flattened_tlist = flatten_tlist (tlist);
	  level_tlist = or (flattened_tlist,tlist);
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
	       return (make_result (tlist,
				    LispNil,
				    constant_qual,
				    LispNil,
				    LispNil));
	       break;

	     case REPLACE : 
	       {
		    /* XXX - let form, maybe incorrect */
		    LispValue scan = make_seqscan (tlist,
						   LispNil,
						   _query_result_relation_,
						   LispNil);
		    if ( consp (constant_qual) ) {
			 return (make_result (tlist,
					      LispNil,
					      constant_qual,
					      scan,
					      LispNil));
		    } 
		    else {
			 return (scan);
		    } 
	       }
	       break;

	     default: /* return nil */
	       return(LispNil);
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

       if ( !(equal (currentlevel,maxlevel))) {
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
	  LispValue resttlist = LispNil;
	  LispValue subtlist = LispNil;
	  LispValue plan = LispNil;
	  
	  if ( restplan ) 
	    resttlist = get_qptargetlist (restplan);
	  subtlist = get_qptargetlist (subplan);
	  plan = make_result (new_result_tlist (tlist,
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
	 !(temp_p(subplan) || 
	   (seqscan_p(subplan) && 
	    get_lefttree(subplan) && 
	    temp_p (get_lefttree (subplan))))) {
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

LispValue
subplanner (flat_tlist,original_tlist,qual,level,sortkeys)
     LispValue flat_tlist,original_tlist,qual,level,sortkeys ;
{
     LispValue final_relation = LispNil;

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
	final_relation = CAR (_query_relation_list_);
     
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

     return (create_plan (get_cheapest_path (final_relation),
			  original_tlist));

}  /* function end  */
