/*     *      FILE *     	planner
 *     
 *      DESCRIPTION
 *     	The query optimizer external interface.
 *     
 */

/* RcsId ("$Header$");  */

/*     
 *      EXPORTS
 *     		planner
 */

#include "c.h"
#include "planner/internal.h"
#include "planner/planner.h"
#include "pg_lisp.h"
#include "relation.h"
#include "relation.a.h"
#include "parse.h"

/*** JJJ ***/
#include "recursion.h"
extern Plan	RecursiveQueryPlan();
/*** JJJ ***/

/*    
 *    	*** Module loading ***
 *    
 */

#include "planner/cfi.h"   
/*   C function interface routines */
/* #include "pppp.h" */
/*   POSTGRES plan pretty printer */

/* #include "storeplan.h" */

/*   plan i/o routines */

/*    
 *    	PLANNER INITIALIZATION
 *    
 */

/*     ...Query tree preprocessing
 */
#include "planner/prepqual.h"

/*   for rule queries */
#include "planner/preptlist.h"

/*   normal targetlist preprocessing */
#include "planner/prepunion.h"

/*   for union (archive, inheritance, ...) queries */

/*    
 *    	PLAN CREATION
 *    
 */

/*     ...Planner driver routines
 */
#include "planner/planmain.h"

/*     ...Subplanner initialization
 */
#include "planner/initsplan.h"

/*     ...Query plan generation
 */
#include "planner/createplan.h"

/*   routines to create optimal subplan */
#include "planner/setrefs.h"

/*   routines to set vars to reference other nodes */
/* #include "planner/sortresult.h" */

/*   routines to manipulate sort keys */

/*    
 *    	SUBPLAN GENERATION
 *    
 */
#include "planner/allpaths.h"

/*   main routines to generate subplans */
#include "planner/indxpath.h"

/*   main routines to generate index paths */
#include "planner/orindxpath.h"
#include "planner/prune.h" 

/*   routines to prune the path space */
#include "planner/joinpath.h"

/*   main routines to create join paths */
#include "planner/joinrels.h"

/*   routines to determine which relations to join */
#include "planner/joinutils.h"

/*   generic join method key/clause routines */
#include "planner/mergeutils.h"

/*   routines to deal with merge keys and clauses */
#include "planner/hashutils.h"

/*   routines to deal with hash keys and clauses */

/*    
 *    	SUBPLAN SELECTION - utilities for planner, create-plan, join routines
 *    
 */
#include "planner/clausesel.h"

/*   routines to compute clause selectivities */
#include "planner/costsize.h"

/*   routines to compute costs and sizes */

/*    
 *    	DATA STRUCTURE CREATION/MANIPULATION ROUTINES
 *    
 */
#include "relation.h"
#include "planner/clauseinfo.h"
#include "planner/indexnode.h"
#include "planner/joininfo.h"
#include "planner/keys.h"
#include "planner/ordering.h"
#include "planner/pathnode.h"
#include "planner/clause.h"
#include "planner/relnode.h"
#include "planner/tlist.h"
#include "planner/var.h"

extern int Quiet;
/*    
 *    	*** Query optimizer entry point ***
 *    
 */

/*    
 *    	planner
 *    
 *    	Main query optimizer routine.
 *    
 *    	Invokes the planner on union queries if there are any left,
 *    	recursing if necessary to get them all, then processes normal plans.
 *    
 *    	Returns a query plan.
 *    
 */

/*  .. make-rule-plans, plan-union-query, process-rules    */

Plan
planner (parse)
     LispValue parse ;
{
    LispValue root = parse_root (parse);
    LispValue tlist = parse_targetlist (parse);
    LispValue qual = parse_qualification (parse);
    LispValue rangetable = root_rangetable (root);
    LispValue commandType = root_command_type_atom (root);
    LispValue uniqueflag = nth(6,root);
    LispValue sortclause = nth(7,root);
    LispValue sortkeys = LispNil;
    LispValue sortops = LispNil;
    List special_plans = LispNil;
    Plan regular_plans = (Plan) NULL;
    LispValue flag = LispNil;
    List plan_list = LispNil;
    bool isRecursiveQuery = false;

    if ( (consp(commandType)) && (CInteger(CAR(commandType)) == '*') ) {
      printf(" *** JJJ *** Planner.c -- recursive flag \n");
      isRecursiveQuery = true;
      commandType = CDR(commandType);
      root = lispCons(CAR(root),
                      lispCons(commandType,
                               CDR(CDR(root))));
	/* XXX the following is a dangerous hack, solves tcop error */
      rplaca(parse, root);
    }
/* else, don't touch the command type list -- it is supposed to be there 
 * JJJ - the star IS removed 
 */

/*** JJJ ***/
    if ( isRecursiveQuery ) {
      printf(" *** JJJ *** Planner.c -- recursive flag seconded \n");
      special_plans = (List)RecursiveQueryPlan (parse);
    }
      /* JJJ fall through with NULL means not recursive, planned below */
/*** JJJ ***/
    
    plan_list = lispCons(lispAtom("inherits"),
		   lispCons(lispAtom("union"),
			    lispCons(lispAtom("archive"),LispNil)));
    foreach (flag,plan_list) {
	int rt_index = first_matching_rt_entry (rangetable,CAR(flag));
	if ( rt_index != -1 )
	  special_plans = (List)plan_union_queries (rt_index,
						    CAtom(CAR(flag)),
						    root,
						    tlist,
						    qual,rangetable);
    }

    /*
     *  For now, before we hand back the plan, check to see if there
     *  is a user-specified sort that needs to be done.  Eventually, this 
     *  will be moved into the guts of the planner s.t. user specified 
     *  sorts will be considered as part of the planning process. 
     *  Since we can only make use of user-specified sorts in
     *  special cases, we can do the optimization step later.
     *   
     *  sortkeys:    a list of var nodes corresponding to the var. nodes in
     *               the tlist that are to be sorted.
     *  sortops:     a corresponding list of sort operators.               
     */

    if (uniqueflag || sortclause) {
	sortkeys = CADR(sortclause);
	sortops = CADDR(sortclause);
    }

    if (special_plans) {
	if (uniqueflag) {
	    Plan sortplan = make_sortplan(tlist,sortkeys,
					  sortops,special_plans);
	    return((Plan)make_unique(tlist,sortplan));
	}
	else 
	  if (sortclause) 
	      return(make_sortplan(tlist,sortkeys,sortops,special_plans));
	  else 
	    return((Plan)special_plans);
    }
    else {  
	regular_plans = init_query_planner(root,tlist,qual);
	
	if (uniqueflag) {
	    Plan sortplan = make_sortplan(tlist,sortkeys,
					  sortops,regular_plans);
	    return((Plan)make_unique(tlist,sortplan));
	}
	else
	  if (sortclause)
	    return(make_sortplan(tlist,sortkeys,sortops,regular_plans)); 
	  else
	    return(regular_plans);
    }
    
} /* function end  */

/*
 *      make_sortplan
 *   
 *      Returns a sortplan which is basically a SORT node attached to the
 *      top of the plan returned from the planner.  It also adds the 
 *      cost of sorting into the plan.
 */

Plan
make_sortplan(tlist,sortkeys,sortops,plannode)
     List tlist;
     List sortkeys;
     List sortops;
     List plannode;

{
  Plan sortplan = (Plan)NULL;
  List temp_tlist = LispNil;

  temp_tlist = set_temp_tlist_operators(new_unsorted_tlist(tlist),
					     sortkeys,
					     sortops);
  sortplan = (Plan) make_sort(temp_tlist,
			      _TEMP_RELATION_ID_,
			      (Plan)plannode,
			      length(sortkeys));

  /*
   *  XXX Assuming that an internal sort has no. cost. 
   *      This is wrong, but given that at this point, we don't
   *      know the no. of tuples returned, etc, we can't do
   *      better than to add a constant cost.
   *      This will be fixed once we move the sort further into the planner,
   *      but for now ... functionality....
   */

  set_cost(sortplan, get_cost(plannode));
  
  return(sortplan);
}


/*    
 *    	init-query-planner
 *    
 *    	Deals with all non-union preprocessing, including existential
 *    	qualifications and CNFifying the qualifications.
 *    
 *    	Returns a query plan.
 *	MODIFIES: tlist,qual
 *    
 */

/*  .. plan-union-queries, planner    */

Plan
init_query_planner (root,tlist,qual)
     LispValue root,tlist,qual ;
{
     LispValue primary_qual;
     LispValue existential_qual;
     Existential exist_plan;

     _query_max_level_ = root_numlevels (root);
     _query_command_type_ = (int) root_command_type (root);
     _query_result_relation_ = root_result_relation (root);
     _query_range_table_ = root_rangetable (root);
     
     tlist = preprocess_targetlist (tlist,
				    _query_command_type_,
				    _query_result_relation_,
				    _query_range_table_);

     qual = preprocess_qualification (qual,tlist);

     if (!Quiet) {
     printf("after preprocessing, qual is :\n");
     lispDisplay(qual,0);
     printf("\n");
     fflush(stdout);
     }

     if (qual) {
	 primary_qual = CAR(qual);
	 existential_qual = CADR(qual);
     } else {
	 primary_qual = LispNil;
	 existential_qual = LispNil;
     }

     if(lispNullp (existential_qual)) {
	 return(query_planner(_query_command_type_,
			      tlist,
			      primary_qual,
			      1,_query_max_level_));

     } else {

	 int temp = _query_command_type_;
	 Plan existential_plan;

	 /* with saved globals */
	 save_globals();

	 _query_command_type_ = RETRIEVE;
	 existential_plan = query_planner(temp,LispNil,existential_qual,1,
					  _query_max_level_);
     
	 exist_plan = make_existential (existential_plan,
					query_planner (_query_command_type_,
						       tlist,primary_qual,
						       1,
						       _query_max_level_));

	 restore_globals();
	 return((Plan)exist_plan);
}

 }  /* function end  */

/* 
 * 	make_existential
 *	( NB: no corresponding lisp code )
 *
 *	Instantiates an existential plan node and fills in 
 *	the left and right subtree slots.
 */

Existential
make_existential(left,right)
     Plan left,right;
{
    extern void PrintExistential();
    extern bool EqualExistential();

    Existential node = CreateNode(Existential);

    node->equalFunc = EqualExistential;
    node->printFunc = PrintExistential;
    set_lefttree(node,left);
    set_righttree(node,right);
    return(node);
}
