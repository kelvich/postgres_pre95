/*     
 *      FILE
 *     	createplan
 *     
 *      DESCRIPTION
 *     	Routines to create the desired plan for processing a query
 *     
 */

/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		create_plan
 */

#include "internal.h"
#include "c.h"
#include "log.h"
#include "tlist.h"
#include "createplan.h"
#include "clauses.h"
#include "clause.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "clauseinfo.h"
#include "relation.a.h"
#include "relation.h"
#include "setrefs.h"
#include "lsyscache.h"

/* "Print out the total cost at each query processing operator"  */
#define _watch_costs_  LispNil

#define INDEXSCAN  20
#define SEQSCAN    21
#define NESTLOOP   22
#define HASHJOIN   23
#define MERGESORT  24
/* #define SORT       25    */
#define HASH       26

/*
extern LispValue create_scan_node();
extern LispValue create_indexscan_node();
extern LispValue create_seqscan_node();
extern LispValue fix_indxqual_references();
extern LispValue create_nestloop_node();
extern LispValue create_hashjoin_node();
extern LispValue create_join_node();
extern LispValue create_mergejoin_node();
extern LispValue switch_outer();
extern LispValue set_temp_tlist_operators();
extern LispValue make_temp();
*/

/*    	================
 *    	GENERIC ROUTINES
 *    	================
 */

/*    
 *    	create_plan
 *    
 *    	Creates the access plan for a query by tracing backwards through the
 *    	desired chain of pathnodes, starting at the node 'best-path'.  For
 *    	every pathnode found:
 *    	(1) Create a corresponding plan node containing appropriate id,
 *    	    target list, and qualification information.
 *    	(2) Modify ALL clauses so that attributes are referenced using
 *    	    relative values.
 *    	(3) Target lists are not modified, but will be in another routine.
 *    
 *    	'best-path' is the best access path
 *    	'origtlist' is the original (unflattened) targetlist for the current
 *    		nesting level
 *    
 *    	Returns the optimal (?) access plan.
 *    
 */

/*  .. create_join_node, subplanner    */

Plan
create_plan (best_path,origtlist)
     Path best_path;
     List origtlist ;
{
     List tlist = get_actual_tlist (get_targetlist (get_parent (best_path)));
     Plan plan_node;
     Plan sorted_plan_node;

     switch (get_pathtype (best_path)) {
	case INDEXSCAN : 
	case SEQSCAN :
	  plan_node = (Plan)create_scan_node (best_path,tlist);
	  break;
	case HASHJOIN :
	case MERGESORT : 
	case NESTLOOP :
	  plan_node = (Plan)create_join_node (best_path,origtlist,tlist);
	  break;
	default: /* do nothing */
	  break;
     } 
     /*    Attach a SORT node to the path if a sort path is specified. */

     if(get_sortpath (best_path) &&
	(_query_max_level_ == 1)) {
	  set_qptargetlist (plan_node,
			    fix_targetlist(origtlist,
					   get_qptargetlist (plan_node)));
	  sorted_plan_node = (Plan)sort_level_result (plan_node,
						length(get_varkeys
						       (get_sortpath(best_path)
							)));

     }
     else 
       if (get_sortpath (best_path)) {
	    sorted_plan_node = (Plan)make_temp(tlist,
					 get_varkeys(get_sortpath(best_path)),
					 get_ordering(get_sortpath(best_path)),
					 plan_node,SORT);

       } 

     if (sorted_plan_node) {
	  set_state (sorted_plan_node,get_cost (get_sortpath (best_path)));
	  if ( _watch_costs_) {
	       printf("SORTED PLAN COST =");
	       lispDisplay(get_state (sorted_plan_node),0);
	       printf("\n");
	  }
	  plan_node = sorted_plan_node;
	  
     }
     return (plan_node);

} /* function end */

/*    
 *    	create_scan_node
 *    
 *    	Create a scan path for the parent relation of 'best-path'.
 *    
 *    	'tlist' is the targetlist for the base relation scanned by 'best-path'
 *    
 *    	Returns the scan node.
 *    
 */

/*  .. create_plan    */

Scan
create_scan_node (best_path,tlist)
     Path best_path;
     List tlist ;
{
     /*Extract the relevant clauses from the parent relation and replace the */
     /* operator OIDs with the corresponding regproc ids. */

     LispValue scan_clauses = fix_opids(get_actual_clauses
					(get_clauseinfo 
					 (get_parent (best_path))));
     switch (get_pathtype (best_path)) {

	case SEQSCAN : 
	  return((Scan)create_seqscan_node (best_path,tlist,scan_clauses));
	  break;

	case INDEXSCAN : 
	  return((Scan)create_indexscan_node (best_path,tlist,scan_clauses));
	  break;
	  
	  default :
	    elog (WARN,"create_scan_node: unknown node type",
		     get_pathtype (best_path));
	  break;
     }

}  /* function end  */

/*    
 *    	create_join_node
 *    
 *    	Create a join path for 'best-path' and (recursively) paths for its
 *    	inner and outer paths.
 *    
 *    	'tlist' is the targetlist for the join relation corresponding to
 *    		'best-path'
 *    
 *    	Returns the join node.
 *    
 */

/*  .. create_plan    */

Join
create_join_node (best_path,origtlist,tlist)
     Path best_path;
     List origtlist,tlist ;
{
     Plan 	outer_node;
     LispValue 	outer_tlist;
     Plan 	inner_node;
     List 	inner_tlist;
     LispValue 	clauses;
     Join 	retval;

     outer_node = create_plan (get_outerjoinpath (best_path),origtlist);
     outer_tlist  = get_qptargetlist (outer_node);

     inner_node = create_plan (get_innerjoinpath (best_path),origtlist);
     inner_tlist = get_qptargetlist (inner_node);

     clauses = get_actual_clauses (get_clauseinfo (best_path));
     
     switch (get_pathtype (best_path)) {
	 
       case MERGESORT : 
	 retval = (Join)create_mergejoin_node (best_path,
					       tlist,clauses,
					       outer_node,outer_tlist,
					       inner_node,inner_tlist);
	 break;
       case HASHJOIN : 
	 retval = (Join)create_hashjoin_node (best_path,tlist,
					      clauses,outer_node,outer_tlist,
					      inner_node,inner_tlist);
	 break;
       case NESTLOOP : 
	 retval = (Join)create_nestloop_node (best_path,tlist,
					      clauses,outer_node,outer_tlist,
					      inner_node,inner_tlist);
	 break;
       default: /* do nothing */
	  elog (WARN,"create_join_node: unknown node type",
		get_pathtype (best_path));
     } 
     return(retval);

}  /* function end  */

/*    	==========================
 *    	BASE-RELATION SCAN METHODS
 *    	==========================
 */

/*    
 *    	create_seqscan_node
 *    
 *    	Returns a seqscan node for the base relation scanned by 'best-path'
 *    	with restriction clauses 'scan-clauses' and targetlist 'tlist'.
 *    
 */

/*  .. create_scan_node    */

SeqScan
create_seqscan_node (best_path,tlist,scan_clauses)
     LispValue best_path,tlist,scan_clauses ;
{
     /* XXX - let form, maybe incorrect */
    SeqScan scan_node = MakeSeqScan (tlist,
				     scan_clauses,
				     get_relid (get_parent (best_path)),
				     LispNil);
    
    set_state (scan_node,get_cost (best_path));
    
    if ( _watch_costs_) {
	printf("BASE SEQSCAN COST = ");
	lispDisplay (get_state (scan_node));
	printf("\n");
    }
    return (scan_node);

} /* function end  */

/*    
 *    	create_indexscan_node
 *    
 *    	Returns a indexscan node for the base relation scanned by 'best-path'
 *    	with restriction clauses 'scan-clauses' and targetlist 'tlist'.
 *    
 */

/*  .. create_scan_node    */

IndexScan
create_indexscan_node (best_path,tlist,scan_clauses)
     Path best_path;
     List tlist;
     List scan_clauses ;
{
       /* Extract the (first if conjunct, only if disjunct) clause from the */
       /* clauseinfo list. */
     Expr 	index_clause = get_clause (CAR (get_indexqual (best_path)));
     List 	indxqual = LispNil;
     List 	qpqual = LispNil;
     List 	fixed_indxqual = LispNil;
     IndexScan 	scan_node;

     /*    If an 'or' clause is to be used with this index, the indxqual */
     /*    field will contain a list of the 'or' clause arguments, e.g., the */
     /*    clause (OR a b c) will generate: ((a) (b) (c)).  Otherwise, the */
     /*   indxqual will simply contain one conjunctive qualification: ((a)). */
	
     if(or_clause_p (index_clause)) {
	  LispValue temp = LispNil;
	
	  foreach (temp,get_orclauseargs(index_clause)) 
	    indxqual = nappend1(indxqual,lispCons (temp,LispNil));
     } 
     else {
	  lispCons(get_actual_clauses (get_indexqual (best_path)),LispNil);
     } 
     /*    The qpqual field contains all restrictions except the indxqual. */

     if (or_clause(index_clause))
       qpqual = set_difference (scan_clauses,
				lispCons (index_clause,LispNil));
     else 
       qpqual = set_difference(scan_clauses, CAR (indxqual));
     
     fixed_indxqual = fix_indxqual_references (indxqual,best_path);
     scan_node = MakeIndexScan (tlist,
				qpqual,
				get_relid (get_parent (best_path)),
				get_indexid (best_path),
				fixed_indxqual);
     
     set_state (scan_node,get_cost (best_path));

     if ( _watch_costs_) {
	  printf ("BASE INDEXSCAN COST = ");
	  lispDisplay (get_state (scan_node));
	  printf("\n");
     }
     return (scan_node);

}  /* function end  */

/*  .. create_indexscan_node, fix-indxqual-references    */

LispValue
fix_indxqual_references (clause,index_path)
     LispValue clause,index_path ;
{
     if(IsA (clause,Var) && 
	equal(get_relid(get_parent(index_path)),
	      get_varno (clause))) {
	  set_varattno (clause,
			position (get_varattno (clause),
				  get_indexkeys (index_path)) + 1);
	  return (clause);

     } 
     else 
       if (is_opclause (clause) && 
	   is_funcclause (get_leftop (clause)) && 
	   get_funcisindex (get_function (get_leftop (clause)))) {

	    /* 	 (set_opno (get_op clause) 
	     *   (get_funcisindex (get_function (get_leftop clause))))
	     * 	 (make_opclause (replace_opid (get_op clause))
	     */
	    return (make_opclause (get_op (clause),
				   make_var (get_relid 
					     (get_parent (index_path)),
					     1, /* func indices have one key */
					     get_functype 
					     (get_function (clause)),
					     LispNil,
					     LispNil,
					     LispNil),
				   get_rightop (clause)));

       } 
       else {
	    LispValue type = clause_type (clause);
	    LispValue new_subclauses = LispNil; 
	    LispValue subclause = LispNil;

	    foreach(subclause,clause_subclauses(type,clause))
	      new_subclauses = nappend1(new_subclauses,
					fix_indxqual_references(subclause,
								index_path));
	    if ( consp (new_subclauses) ) {
		 apply ( make_clause (type,new_subclauses));  /* XXX fix me  */
		} 
	    else {
		 return (clause);
	    } 
       }
}  /* function end  */


/*    	============
 *    	JOIN METHODS
 *    	============
 */

/*    
 *    	create_nestloop_node
 *    
 *    	Returns a new nestloop node.
 *    
 */

/*  .. create_join_node     */

NestLoop
create_nestloop_node (best_path,tlist,clauses,
		      outer_node,outer_tlist,inner_node,inner_tlist)
     Path best_path;
     List tlist,clauses;
     Node outer_node;
     List outer_tlist;
     Node inner_node;
     List inner_tlist ;
{
    NestLoop join_node;

    if ( IsA(inner_node,IndexScan)) {

	/*  An index is being used to reduce the number of tuples scanned in 
	 *    the inner relation.
	 * There will never be more than one index used in the inner 
	 * scan path, so we need only consider the first set of 
	 *    qualifications in indxqual. 
	 */

	List inner_indxqual = CAR (get_indxqual (inner_node));
	List inner_qual = CAR (inner_indxqual);

	/* If we have in fact found a join index qualification, remove these
	 * index clauses from the nestloop's join clauses and reset the 
	 * inner (index) scan's qualification so that the var nodes refer to
	 * the proper outer join relation attributes.
	 */

	if  (!(qual_clause_p (inner_qual))) {
	    LispValue new_inner_qual = LispNil;
	    
	    clauses = set_difference (clauses,inner_indxqual);
	    new_inner_qual = index_outerjoin_references (inner_indxqual,
							 get_qptargetlist 
							 (outer_node),
							 get_scanrelid 
							 (inner_node));
	    set_indxqual (inner_node,lispCons (new_inner_qual,LispNil));
	  }
      }
    join_node = MakeNestLoop (tlist,
			       join_references (clauses,
						outer_tlist,
						inner_tlist),
			       outer_node,
			       inner_node);

    set_state (join_node,get_cost (best_path));

    if ( _watch_costs_) {
	printf ("NESTLOOP COST = ");
	lispDisplay (get_state (join_node));
	printf("\n");
    }

    return(join_node);

}  /* function end  */


/*    
 *    	create_mergejoin_node
 *    
 *    	Returns a new mergejoin node.
 *    
 */

/*  .. create_join_node     */


MergeSort
create_mergejoin_node (best_path,tlist,clauses,
		       outer_node,outer_tlist,
		       inner_node,inner_tlist)
     Path best_path;
     List tlist,clauses;
     Node outer_node;
     List outer_tlist;
     Node inner_node;
     List inner_tlist ;
{
     /*    Separate the mergeclauses from the other join qualification 
      *    clauses and set those clauses to contain references to lower 
      *    attributes. 
      */

     LispValue qpqual = 
       join_references (set_difference (clauses,
					get_mergeclauses (best_path)),
			outer_tlist,
			inner_tlist);

     /*    Now set the references in the mergeclauses and rearrange them so 
      *    that the outer variable is always on the left. 
      */
     
     LispValue mergeclauses = 
       switch_outer (join_references (get_mergeclauses (best_path),
				      outer_tlist,
				      inner_tlist));
     RegProcedure opcode = 
       get_opcode (get_join_operator (get_ordering (best_path)));
     
     LispValue outer_order = 
       lispCons (get_left_operator (get_ordering (best_path)), LispNil);
     
     LispValue inner_order = 
       lispCons (get_right_operator (get_ordering (best_path)), LispNil);
     
     MergeSort join_node;
     
     /*    Create explicit sort paths for the outer and inner join paths if 
      *    necessary.  The sort cost was already accounted for in the path. 
      */
     if ( get_outersortkeys (best_path)) {
	 Temp sorted_outer_node = make_temp (outer_tlist,
					     get_outersortkeys 
					     (best_path),
					     outer_order,
					     outer_node,
					     SORT);
	 set_state (sorted_outer_node,get_state (outer_node));
	 outer_node = (Node)sorted_outer_node;
     }

     if ( get_innersortkeys (best_path)) {
	  Temp sorted_inner_node = make_temp (inner_tlist,
						   get_innersortkeys 
						   (best_path),
						   inner_order,
						   inner_node,SORT);
	  set_state (sorted_inner_node,get_state (inner_node));
	  inner_node = (Node)sorted_inner_node;
     }

     join_node = MakeMergeSort (tlist,
				qpqual,
				mergeclauses,
				opcode,
				outer_node,
				inner_node);
     set_state (join_node,get_cost (best_path));
     if ( _watch_costs_) {
	  printf ("MERGEJOIN COST = ");
	  lispDisplay (get_state (join_node));
	  printf("\n");
     }
     return(join_node);

} /* function end  */

/*    
 *    	switch_outer
 *    
 *    	Given a list of merge clauses, rearranges the elements within the
 *    	clauses so the outer join variable is on the left and the inner is on
 *    	the right.
 *    
 *      Returns the rearranged list ?
 *    	XXX Shouldn't the operator be commuted?!
 *    
 */

/*  .. create_hashjoin_node, create_mergejoin_node    */

LispValue
switch_outer (clauses)
     LispValue clauses ;
{
     LispValue t_list = LispNil;
     LispValue clause = LispNil;
     LispValue temp = LispNil;
     foreach(clause,clauses) {
	  if(var_is_outer (get_rightop (clause))) {
	       temp = make_clause (get_op (clause),
				   get_rightop (clause),
				   get_leftop (clause));
	       t_list = nappend1(t_list,temp);
	  } 
	  else 
	    t_list = nappend1(t_list,clause);

     }
     return(t_list);
}  /* function end   */

/*    
 *    	create_hashjoin_node			XXX HASH
 *    
 *    	Returns a new hashjoin node.
 *    
 *    	XXX hash join ops are totally bogus -- how the hell do we choose
 *    		these??  at runtime?  what about a hash index?
 *    
 */

/*  .. create_join_node     */

HashJoin
create_hashjoin_node (best_path,tlist,clauses,outer_node,outer_tlist,
		      inner_node,inner_tlist)
     Path best_path;
     List tlist,clauses;
     Node outer_node;
     List outer_tlist;
     Node inner_node;
     List inner_tlist ;
{
     LispValue qpqual = 
       /*    Separate the hashclauses from the other join qualification clauses
	*    and set those clauses to contain references to lower attributes. 
	*/
       join_references (set_difference (clauses,
					get_hashclauses (best_path)),
			outer_tlist,
			inner_tlist);
     LispValue hashclauses = 
       /*    Now set the references in the hashclauses and rearrange them so 
	*    that the outer variable is always on the left. 
	*/
       switch_outer (join_references (get_hashclauses (best_path),
				      outer_tlist,
				      inner_tlist));
     int opcode = 666;	/*   XXX BOGUS HASH FUNCTION */
     LispValue outer_hashop = lispCons (666,LispNil); 	/*   XXX BOGUS HASH OP */
     LispValue inner_hashop = lispCons (666,LispNil);	/* XXX BOGUS HASH OP */
     HashJoin join_node;

     if ( get_outerhashkeys (best_path)) {
	 Temp hashed_outer_node = make_temp (outer_tlist,
					     get_outerhashkeys 
					     (best_path),
					     outer_hashop,
					     outer_node,
					     HASH);
	 set_state (hashed_outer_node,get_state (outer_node));
	  outer_node = (Node)hashed_outer_node;
     }
     if ( get_innerhashkeys (best_path)) {
	  Temp hashed_inner_node = make_temp (inner_tlist,
						   get_innerhashkeys 
						   (best_path),
						   inner_hashop,
						   inner_node,
						   HASH);
	  set_state (hashed_inner_node,get_state (inner_node));
	  inner_node = (Node)hashed_inner_node;
     }
     join_node = MakeHashJoin (tlist,
				qpqual,
				hashclauses,
				opcode,
				outer_node,
				inner_node);
     set_state (join_node,get_cost (best_path));
     if ( _watch_costs_) {
	  printf ("HASHJOIN COST = ");
	  lispDisplay (get_state (join_node));
	  printf("\n");
     }
     return(join_node);

} /* function end  */

/*    	===================
 *    	UTILITIES FOR TEMPS
 *    	===================
 */

/*    
 *    	make_temp
 *    
 *    	Create plan nodes to sort or hash relations into temporaries.  The
 *    	result returned for a sort will look like (SEQSCAN (SORT (plan-node)))
 *    
 *    	'tlist' is the target list of the scan to be sorted or hashed
 *    	'keys' is the list of keys which the sort or hash will be done on
 *    	'operators' is the operators with which the sort or hash is to be done
 *    		(a list of operator OIDs)
 *    	'plan-node' is the node which yields tuples for the sort
 *    	'temptype' indicates which operation (sort or hash) to perform
 *    
 */

/*  .. create_hashjoin_node, create_mergejoin_node, create_plan    */

Temp
make_temp (tlist,keys,operators,plan_node,temptype)
     List tlist;
     List keys;
     List operators;
     Plan plan_node;
     int temptype ;
{
     /*    Create a new target list for the temporary, with keys set. */
     List temp_tlist = set_temp_tlist_operators(new_unsorted_tlist(tlist),
						     keys,
						     operators);
     Temp retval;

     switch (temptype) {
       case SORT : 
	 retval = (Temp)MakeSeqScan(tlist,
				     LispNil,
				     _TEMP_RELATION_ID_,
				     MakeSort (temp_tlist,
						_TEMP_RELATION_ID_,
						plan_node,
						length (keys)));
	 break;
	 
       case HASH : 
	 retval = (Temp)MakeSeqScan(tlist,
				 LispNil,
				     _TEMP_RELATION_ID_,
				     MakeHash (temp_tlist,
						_TEMP_RELATION_ID_,
						plan_node,
						length (keys)));
	 break;
	 
       default: 
	 elog (WARN,"make_temp: unknown temp type",temptype);
	 
     }
     return(retval);

}  /* function end  */
	      

/*    
 *    	set-temp-tlist-operators
 *    
 *    	Sets the key and keyop fields of resdom nodes in a target list.
 *    
 *    	'tlist' is the target list
 *    	'pathkeys' is a list of N keys in the form ((key1) (key2)...(keyn)),
 *    		corresponding to vars in the target list that are to
 *    		be sorted or hashed
 *    	'operators' is the corresponding list of N sort or hash operators
 *    	'keyno' is the first key number 
 *	XXX - keyno ? doesn't exist - jeff
 *    
 *    	Returns the modified target list.
 *    
 */

/*  .. make_temp    */

List
set_temp_tlist_operators (tlist,pathkeys,operators)
     List tlist;
     List pathkeys;
     List operators ;
{
     LispValue 	keys = LispNil;
     List 	ops = operators;
     int 	keyno = 1;
     Resdom 	resdom ;

     foreach(keys,pathkeys) {
	  resdom = tlist_member (CAAR (keys),
				 tlist,
				 LispNil);
	  if ( resdom) {

	       /* Order the resdom keys and replace the operator OID for each 
		*    key with the regproc OID. 
		*/
	       set_reskey (resdom,keyno);
	       set_reskeyop (resdom,get_opcode (CAR (ops)));
	  }
	  keyno += 1;
	  ops = CDR (ops);
     }
     return(tlist);
} /* function end  */
