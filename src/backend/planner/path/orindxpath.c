/* ----------------------------------------------------------------
 *   FILE
 *	orindxpath
 *     
 *   DESCRIPTION
 *	Routines to find index paths that match a set of 'or' clauses
 *
 *   INTERFACE ROUTINES
 *     		create-or-index-paths
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$"
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

#include "tmp/nodeFuncs.h"

#include "planner/internal.h"
#include "planner/clauses.h"
#include "planner/orindxpath.h"
#include "planner/costsize.h"
#include "planner/cfi.h"
#include "planner/xfunc.h"

#include "lib/copyfuncs.h"

#define INDEX_SCAN 1

/*    
 *    	create-or-index-paths
 *    
 *    	Creates index paths for indices that match 'or' clauses.
 *    
 *    	'rel' is the relation entry for which the paths are to be defined on
 *    	'clauses' is the list of available restriction clause nodes
 *    
 *    	Returns a list of these index path nodes.
 *    
 */

/*  .. create-or-index-paths, find-rel-paths */

LispValue
create_or_index_paths (rel,clauses)
     Rel rel;
     List clauses ;
{
     LispValue t_list = LispNil;

     if ( consp (clauses) ) {
	  /* XXX - let form, maybe incorrect */
	  CInfo clausenode = (CInfo) (CAR (clauses));

	  /* Check to see if this clause is an 'or' clause, and, if so, */
	  /* whether or not each of the subclauses within the 'or' clause has */
	  /* been matched by an index (the 'Index field was set in */
	  /* (match_or)  if no index matches a given subclause, one of the */
	  /* lists of index nodes returned by (get_index) will be 'nil'). */

	  if(valid_or_clause (clausenode) &&
	     get_indexids (clausenode)) {
	       LispValue temp = LispNil;
	       LispValue index_list = LispNil;
	       bool index_flag = true;

	       index_list = get_indexids(clausenode);
	       foreach(temp,index_list) {
		    if (!temp) 
		      index_flag = false;
	       }
	       if (index_flag) {   /* used to be a lisp every function */
		    IndexPath pathnode = RMakeIndexPath();
		    LispValue indexinfo = 
		      best_or_subclause_indices (rel,
						 get_orclauseargs
					         ((LispValue)get_clause(clausenode)),
						 get_indexids (clausenode),
						 LispNil,(Cost)0,
						 LispNil);

		    set_pathtype((Path)pathnode,INDEX_SCAN);
		    set_parent ((Path)pathnode,rel);
		    set_indexqual (pathnode,
				   lispCons((LispValue)clausenode,LispNil));
		    set_indexid (pathnode,nth (0,indexinfo));
		    set_path_cost ((Path)pathnode,(Cost)CDouble(nth (1,indexinfo)));

		    /* copy clauseinfo list into path for expensive 
		       function processing    -- JMH, 7/7/92 */
		    set_locclauseinfo
		      (pathnode,
		       set_difference(clauses,
				      CopyObject(get_clauseinfo(rel))));

		    /* add in cost for expensive functions!  -- JMH, 7/7/92 */
		    if (XfuncMode != XFUNC_OFF) {
			set_path_cost((Path)pathnode, 
				      get_path_cost((Path)pathnode) + 
				      xfunc_get_path_cost(pathnode));
		    }

		    set_selectivity(clausenode,(Cost)CDouble(nth(2,indexinfo)));
		    t_list = 
		      lispCons ((LispValue)pathnode,
				create_or_index_paths (rel,CDR (clauses)));
	       } 
	       else {
		    t_list = create_or_index_paths (rel,CDR (clauses));
		    
	       } 
	  }
     }
     return(t_list);
}  /* function end  */

/*    
 *    	best-or-subclause-indices
 *    
 *    	Determines the best index to be used in conjunction with each subclause
 *    	of an 'or' clause and the cost of scanning a relation using these
 *    	indices.  The cost is the sum of the individual index costs.
 *    
 *    	'rel' is the node of the relation on which the index is defined
 *    	'subclauses' are the subclauses of the 'or' clause
 *    	'indices' are those index nodes that matched subclauses of the 'or'
 *    		clause
 *    	'examined-indexids' is a list of those index ids to be used with 
 *    		subclauses that have already been examined
 *    	'subcost' is the cost of using the indices in 'examined-indexids'
 *    	'selectivities' is a list of the selectivities of subclauses that
 *    		have already been examined
 *    
 *    	Returns a list of the indexids, cost, and selectivities of each
 *    	subclause, e.g., ((i1 i2 i3) cost (s1 s2 s3)), where 'i' is an OID,
 *    	'cost' is a flonum, and 's' is a flonum.
 *    
 */

/*  .. best_or_subclause_indices, create_or_index_paths    */

LispValue
best_or_subclause_indices (rel,subclauses,indices,
			   examined_indexids,subcost,selectivities)
     Rel rel;
     List subclauses,indices,examined_indexids, selectivities ;
     Cost subcost;
{
     LispValue t_list = LispNil;
     if ( null (subclauses) ) {
	  t_list = 
	    lispCons (nreverse (examined_indexids),
		      lispCons(lispFloat(subcost),
			       lispCons(nreverse (selectivities),LispNil)));
     } 
     else {
	  LispValue indexinfo = 
	    best_or_subclause_index (rel,CAR (subclauses),CAR (indices));

	  t_list = best_or_subclause_indices (rel,
					      CDR (subclauses),
					      CDR (indices),
					      lispCons (nth (0,indexinfo),
						    examined_indexids),
					      subcost + (int) nth (1,indexinfo),
					      lispCons (nth (2,indexinfo),
						    selectivities));
     } 
     return (t_list);
}  /* function end   */

/*    
 *    	best-or-subclause-index
 *    
 *    	Determines which is the best index to be used with a subclause of
 *    	an 'or' clause by estimating the cost of using each index and selecting
 *    	the least expensive.
 *    
 *    	'rel' is the node of the relation on which the index is defined
 *    	'subclause' is the subclause
 *    	'indices' is a list of index nodes that match the subclause
 *    
 *    	Returns a list (index-id index-subcost index-selectivity)
 *    	(a fixnum, a fixnum, and a flonum respectively).
 *    
 */

/*  .. best-or-subclause-index, best-or-subclause-indices   o */

LispValue
best_or_subclause_index (rel,subclause,indices)
     Rel 	rel;
     LispValue  subclause;
     List	indices ;
{
     LispValue t_list = LispNil;

     if ( consp (indices) ) {
	  Datum 	value;
	  int 		flag = 0;
	  LispValue pagesel = LispNil;
	  Cost 		subcost;
	  LispValue 	bestrest = LispNil;
	  Rel	 	index = (Rel)CAR (indices);
	  AttributeNumber attno = get_varattno (get_leftop (subclause));
	  ObjectId 	opno = get_opno((Oper)CAR(subclause));
	  bool 		constant_on_right = non_null((Expr)get_rightop(subclause));

	  if(constant_on_right) {
	       value = get_constvalue((Const)get_rightop (subclause));
	  } 
	  else {
	       value = NameGetDatum("");
	  } 
	  if(constant_on_right) {
	       flag = (_SELEC_IS_CONSTANT_ ||_SELEC_CONSTANT_RIGHT_);
	  } 
	  else {
	       flag = _SELEC_CONSTANT_RIGHT_;
	  } 
	  pagesel = index_selectivity (CInteger(CAR(get_relids (index))) ,
				       get_classlist (index),
				       lispInteger(opno),
				       CInteger(getrelid (CInteger
						    (CAR(get_relids(rel))),
						_query_range_table_)),
				       lispCons (lispInteger(attno),LispNil),
				       lispCons (lispInteger(value),LispNil),
				       lispCons (lispInteger(flag),LispNil),
				       1);

	  subcost = cost_index((ObjectId)CInteger(nth (0,get_relids(index))),
				(Count)CDouble(CAR(pagesel)),
				(Cost)CDouble(CADR(pagesel)),
				get_pages (rel),
				get_tuples (rel),
				get_pages (index),
				get_tuples (index), false);
	  bestrest = best_or_subclause_index (rel,subclause,CDR (indices));
		
	  if(null (bestrest) || (subcost < CInteger(CAR(bestrest)) )) {
	       /* XXX - this used to be "list "(CAR(get....index),.." */
	       t_list = lispCons (get_relids (index),
				  lispCons(lispFloat(subcost),
					   lispCons(CADR(pagesel),
						    LispNil)));
	  } 
	  else {
	       t_list = bestrest;
	  } 
     } 
     return(t_list);
}
