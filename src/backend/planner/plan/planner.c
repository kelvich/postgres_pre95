 
/*     
 *      FILE
 *     	planner
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
#include "internal.h"
#include "planner.h"
#include "pg_lisp.h"
#include "relation.h"
#include "relation.a.h"
#include "parse.h"

/*    
 *    	*** Module loading ***
 *    
 */

/* #include "cfi.h"    /* XXX should it be a .h file?  */
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
#include "prepqual.h"

/*   normal qualification preprocessing */
#include "preprule.h"

/*   for rule queries */
#include "preptlist.h"

/*   normal targetlist preprocessing */
#include "prepunion.h"

/*   for union (archive, inheritance, ...) queries */

/*    
 *    	PLAN CREATION
 *    
 */

/*     ...Planner driver routines
 */
#include "planmain.h"

/*     ...Subplanner initialization
 */
#include "initsplan.h"

/*     ...Query plan generation
 */
#include "createplan.h"

/*   routines to create optimal subplan */
#include "setrefs.h"

/*   routines to set vars to reference other nodes */
/* #include "sortresult.h" */

/*   routines to manipulate sort keys */

/*    
 *    	SUBPLAN GENERATION
 *    
 */
#include "allpaths.h"

/*   main routines to generate subplans */
#include "indxpath.h"

/*   main routines to generate index paths */
#include "orindxpath.h"
#include "prune.h" 

/*   routines to prune the path space */
#include "joinpath.h"

/*   main routines to create join paths */
#include "joinrels.h"

/*   routines to determine which relations to join */
#include "joinutils.h"

/*   generic join method key/clause routines */
#include "mergeutils.h"

/*   routines to deal with merge keys and clauses */
#include "hashutils.h"

/*   routines to deal with hash keys and clauses */

/*    
 *    	SUBPLAN SELECTION - utilities for planner, create-plan, join routines
 *    
 */
#include "clausesel.h"

/*   routines to compute clause selectivities */
#include "costsize.h"

/*   routines to compute costs and sizes */

/*    
 *    	DATA STRUCTURE CREATION/MANIPULATION ROUTINES
 *    
 */
#include "relation.h"
#include "clauseinfo.h"
#include "indexnode.h"
#include "joininfo.h"
#include "keys.h"
#include "ordering.h"
#include "pathnode.h"
#include "clause.h"
#include "relnode.h"
#include "tlist.h"
#include "var.h"

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
    Plan special_plan;
    LispValue flag;

    if ( root_ruleinfo (root) )
      special_plan = process_rules (parse);
     else 
       foreach (flag, list(INHERITS,UNION,ARCHIVE)) {
	   /* XXX - add VERSION too, sometime */
	   int rt_index = first_matching_rt_entry (rangetable,flag);
	   if ( rt_index )
	     special_plan = (Plan)plan_union_queries (rt_index,
						      flag,
						      root,
						      tlist,
						      qual,rangetable);
       }
    if (special_plan)
      return(special_plan);
    else 
      return(init_query_planner (root,tlist,qual));
    
} /* function end  */

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
/*     LispValue tlist;
     LispValue qual;
*/
     LispValue primary_qual;
     LispValue existential_qual;

     _query_max_level_ = root_levels (root);
     _query_command_type_ = (int) root_command_type (root);
     _query_result_relation_ = root_result_relation (root);
     _query_range_table_ = root_rangetable (root);
     
     tlist = preprocess_targetlist (tlist,
				    _query_command_type_,
				    _query_result_relation_,
				    _query_range_table_);
     qual = preprocess_qualification (qual,tlist);
     primary_qual = CAR(qual);
     existential_qual = CADR(qual);

     if(null (existential_qual)) {
	 return(query_planner(_query_command_type_,
			      tlist,
			      primary_qual,
			      1,_query_max_level_));

     } else {

	 int temp = _query_command_type_;
	 Plan existential_plan;

	 /* with saved globals */
	 _query_command_type_ = RETRIEVE;
	 existential_plan = query_planner(temp,LispNil,existential_qual,1,
					  _query_max_level_);
     
	 return(make_existential (existential_plan,
				  query_planner (_query_command_type_,
						 tlist,primary_qual,
						 1,_query_max_level_)));
     }
     
 }  /* function end  */

/* 
 * 	make_existential
 *	( NB: no corresponding lisp code )
 *
 *	Instantiates an existential plan node and fills in 
 *	the left and right subtree slots.
 */

Plan
make_existential(left,right)
     Plan left,right;
{
    Existential node = CreateNode(Existential);
    
    set_lefttree(node,left);
    set_righttree(node,right);
    return(node);
}
