
/*     
 *      FILE
 *     	orindxpath
 *     
 *      DESCRIPTION
 *     	Routines to find index paths that match a set of 'or' clauses
 *     
 */

/*  RcsId("$Header$"); */

/*     
 *      EXPORTS
 *     		create-or-index-paths
 */

#include "internal.h"
#include "pg_lisp.h"

extern LispValue best_or_subclause_indices();
extern LispValue best_or_subclause_index();

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
     LispValue rel,clauses ;
{
     LispValue t_list = LispNil;
     LispValue temp = LispNil;

     if ( consp (clauses) ) {
	  /* XXX - let form, maybe incorrect */
	  LispValue clausenode = CAR (clauses);

	  /* Check to see if this clause is an 'or' clause, and, if so, */
	  /* whether or not each of the subclauses within the 'or' clause has */
	  /* been matched by an index (the 'Index field was set in */
	  /* (match_or)  if no index matches a given subclause, one of the */
	  /* lists of index nodes returned by (get_index) will be 'nil'). */

	  if(valid_or_clause (clausenode) &&
	     get_index (clausenode)) {
	       LispValue temp = LispNil;
	       LispValue index_list = LispNil;
	       LispValue index_flag = LispTrue;
	       index_list = get_index(clausenode);
	       foreach(temp,index_list) {
		    if (identity(temp) == LispNil) 
		      index_flag = LispNil;
	       }
	       if (index_flag) {   /* used to be a lisp every function */
		    /* XXX - let form, maybe incorrect */
		    LispValue pathnode = create_node ("Path");
		    LispValue indexinfo = 
		      best_or_subclause_indices (rel,
						 get_orclauseargs
					          (get_clause (clausenode)),
						 get_index (clausenode),
						 LispNil,0,
						 LispNil);

		    set_pathtype (pathnode,INDEX_SCAN);
		    set_parent (pathnode,rel);
		    set_indexqual (pathnode,list (clausenode));
		    set_indexid (pathnode,nth (0,indexinfo));
		    set_cost (pathnode,nth (1,indexinfo));
		    set_selectivity (clausenode,nth (2,indexinfo));
		    t_list = 
		      lispCons (pathnode,
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

/*  .. best-or-subclause-indices, create-or-index-paths    */

LispValue
best_or_subclause_indices (rel,subclauses,indices,
			   examined_indexids,subcost,selectivities)
     LispValue rel,subclauses,indices,examined_indexids,
     selectivities ;
     double subcost;
{
     LispValue t_list = LispNil;
     if ( null (subclauses) ) {
	  t_list = 
	    list (nreverse (examined_indexids),subcost,
		  nreverse (selectivities));
     } 
     else {
	  /* XXX - let form, maybe incorrect */
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
     LispValue rel,subclause,indices ;
{
     LispValue t_list = LispNil;

     if ( consp (indices) ) {
	  LispValue value = LispNil;
	  LispValue flag = LispNil;
	  LispValue pagesel = LispNil;
	  LispValue subcost = LispNil;
	  LispValue bestrest = LispNil;
	  LispValue index = CAR (indices);
	  LispValue attno = get_varattno (get_leftop (subclause));
	  LispValue opno = get_opno (subclause);
	  LispValue constant_on_right = non_null (get_rightop (subclause));
	  if(constant_on_right) {
	       value = get_constvalue (get_rightop (subclause));
	  } 
	  else {
	       value = "";
	  } 
	  if(constant_on_right) {
	       flag = logior (_SELEC_IS_CONSTANT_,_SELEC_CONSTANT_RIGHT_);
	  } 
	  else {
	       flag = _SELEC_CONSTANT_RIGHT_;
	  } 
	  pagesel = index_selectivity (nth (0,get_indexid (rel)),
				       get_class (index),
				       list (opno),
				       getrelid (get_relid (rel),
						 _query_range_table_),
				       list (attno),
				       list (value),
				       list (flag),1);
	  subcost = cost_index (nth (0,get_indexid (index)),
				floor (nth (0,pagesel)),
				nth (1,pagesel),
				get_pages (rel),
				get_tuples (rel),
				get_pages (index),
				get_tuples (index));
	  bestrest = best_or_subclause_index (rel,subclause,CDR (indices));
		
	  if(null (bestrest) || 
	     subcost < nth (1,bestrest)) {
	       t_list = list (nth (0,get_indexid (index)),
			      subcost,nth (1,pagesel));
	  } 
	  else {
	       t_list = bestrest;
	  } 
     } 
     return(t_list);
}
