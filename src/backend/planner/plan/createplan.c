/*     
 *      FILE
 *     	createplan
 *     
 *      DESCRIPTION
 *     	Routines to create the desired plan for processing a query
 *     
 */

/* RcsId("$Header$"); */

/*     
 *      EXPORTS
 *     		create_plan
 */

#include "tmp/c.h"

#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

#include "utils/log.h"
#include "utils/lsyscache.h"

#include "planner/internal.h"
#include "planner/clause.h"
#include "planner/clauseinfo.h"
#include "planner/clauses.h"
#include "planner/createplan.h"
#include "planner/setrefs.h"
#include "planner/tlist.h"
#include "planner/planner.h"

#include "lib/lisplist.h"

#define TEMP_SORT	1
#define TEMP_MATERIAL	2
/* "Print out the total cost at each query processing operator"  */

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
 *    	'origtlist' is the original(unflattened) targetlist for the current
 *    		nesting level
 *    
 *    	Returns the optimal(?) access plan.
 *    
 */

/*  .. create_join_node, subplanner    */

Plan
create_plan(best_path,origtlist)
     Path best_path;
     List origtlist ;
{
     List tlist;
     Plan plan_node = (Plan)NULL;
     Plan sorted_plan_node = (Plan)NULL;
     Rel parent_rel;
     Count size;
     Count width;
     Count pages;
     Count tuples;

     parent_rel = get_parent(best_path);
     tlist = get_actual_tlist(get_targetlist(parent_rel));
     size = get_size(parent_rel);
     width = get_width(parent_rel);
     pages = get_pages(parent_rel);
     tuples = get_tuples(parent_rel);

     switch(get_pathtype(best_path)) {
	case T_IndexScan : 
	case T_SeqScan :
	  plan_node = (Plan)create_scan_node(best_path,tlist);
	  break;
	case T_HashJoin :
	case T_MergeJoin : 
	case T_NestLoop:
	  plan_node = (Plan)create_join_node((JoinPath)best_path,
					     origtlist,tlist);
	  break;
	default: /* do nothing */
	  break;
     } 

     set_plan_size(plan_node, size);
     set_plan_width(plan_node, width);
     if (pages == 0) pages = 1;
     set_plan_tupperpage(plan_node, tuples/pages);
     set_fragment(plan_node, 0);

     /*    Attach a SORT node to the path if a sort path is specified. */

     if (get_pathsortkey(best_path) &&
	(_query_max_level_ == 1)) {
	  set_qptargetlist(plan_node,
			    (List)fix_targetlist(origtlist,
					   get_qptargetlist(plan_node)));
	  sorted_plan_node = (Plan)sort_level_result(plan_node,
					length(get_varkeys(
						get_pathsortkey(best_path))));
     }
     else 
       if(get_pathsortkey(best_path)) {
	    sorted_plan_node = (Plan)make_temp(tlist,
				      get_varkeys(get_pathsortkey(best_path)),
				      get_sortorder(get_pathsortkey(best_path)),
				      plan_node,TEMP_SORT);

       } 

     if(sorted_plan_node) {
	  plan_node = sorted_plan_node;
	  
     }
     return(plan_node);

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
create_scan_node(best_path,tlist)
     Path best_path;
     List tlist ;
{
     /*Extract the relevant clauses from the parent relation and replace the */
     /* operator OIDs with the corresponding regproc ids. */

     Scan node;
     LispValue scan_clauses = fix_opids(get_actual_clauses
					(get_clauseinfo 
					 (get_parent(best_path))));
     switch(get_pathtype(best_path)) {

	case T_SeqScan : 
	  node = (Scan)create_seqscan_node(best_path,tlist,scan_clauses);
	  break;

	case T_IndexScan:
	  node = (Scan)create_indexscan_node((IndexPath)best_path,
					     tlist,scan_clauses);
	  break;
	  
	  default :
	    elog(WARN,"create_scan_node: unknown node type",
		     get_pathtype(best_path));
	  break;
     }
     set_parallel((Plan)node, 1);
     return node;

}  /* function end  */

/*    
 *    	create_join_node
 *    
 *    	Create a join path for 'best-path' and(recursively) paths for its
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
create_join_node(best_path,origtlist,tlist)
     JoinPath best_path;
     List origtlist,tlist ;
{
     Plan 	outer_node;
     LispValue 	outer_tlist;
     Plan 	inner_node;
     List 	inner_tlist;
     LispValue 	clauses;
     Join 	retval;

     outer_node = create_plan(get_outerjoinpath(best_path),origtlist);
     outer_tlist  = get_qptargetlist(outer_node);

     inner_node = create_plan(get_innerjoinpath(best_path),origtlist);
     inner_tlist = get_qptargetlist(inner_node);

     clauses = get_actual_clauses(get_pathclauseinfo(best_path));
     
     switch(get_pathtype((Path)best_path)) {
	 
       case T_MergeJoin : 
	 retval = (Join)create_mergejoin_node((MergePath)best_path,
					       tlist,clauses,
					       outer_node,outer_tlist,
					       inner_node,inner_tlist);
	 break;
       case T_HashJoin : 
	 retval = (Join)create_hashjoin_node((HashPath)best_path,tlist,
					      clauses,outer_node,outer_tlist,
					      inner_node,inner_tlist);
	 break;
       case T_NestLoop : 
	 retval = (Join)create_nestloop_node((JoinPath)best_path,tlist,
					      clauses,outer_node,outer_tlist,
					      inner_node,inner_tlist);
	 set_parallel(inner_node, 0);
	 break;
       default: /* do nothing */
	  elog(WARN,"create_join_node: unknown node type",
		get_pathtype((Path)best_path));
     } 
     set_parallel((Plan)retval, 1);
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
create_seqscan_node(best_path,tlist,scan_clauses)
     Path best_path;
     List tlist;
     LispValue scan_clauses ;
{
    SeqScan scan_node = (SeqScan)NULL;
    Index scan_relid = -1;
    LispValue temp;

    temp = get_relids(get_parent(best_path));
    if(null(temp))
      elog(WARN,"scanrelid is empty");
    else
      scan_relid = (Index)CInteger(CAR(temp));
    scan_node = make_seqscan(tlist,
			      scan_clauses,
			      scan_relid,
			      (Plan)NULL);
    
    set_cost((Plan)scan_node,get_path_cost(best_path));
    
    return(scan_node);

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
create_indexscan_node(best_path,tlist,scan_clauses)
     IndexPath best_path;
     List tlist;
     List scan_clauses ;
{
    /* Extract the(first if conjunct, only if disjunct) clause from the */
    /* clauseinfo list. */
     Expr 	index_clause = (Expr)NULL;
     List 	indxqual = LispNil;
     List 	qpqual = LispNil;
     List 	fixed_indxqual = LispNil;
     IndexScan 	scan_node = (IndexScan)NULL;

     /*    If an 'or' clause is to be used with this index, the indxqual */
     /*    field will contain a list of the 'or' clause arguments, e.g., the */
     /*    clause(OR a b c) will generate: ((a) (b) (c)).  Otherwise, the */
     /*   indxqual will simply contain one conjunctive qualification: ((a)). */
	
     if(!null(get_indexqual(best_path)))
	  index_clause = get_clause((CInfo)CAR(get_indexqual(best_path)));
     if(or_clause_p((LispValue)index_clause)) {
	  LispValue temp = LispNil;
	
	  foreach(temp,get_orclauseargs((LispValue)index_clause)) 
	    indxqual = nappend1(indxqual,lispCons(CAR(temp),LispNil));
      } 
     else {
	 indxqual = lispCons(get_actual_clauses(get_indexqual(best_path)),
			     LispNil);
     } 
     /*    The qpqual field contains all restrictions except the indxqual. */

     if(or_clause((LispValue)index_clause))
       qpqual = set_difference(scan_clauses,
				lispCons((LispValue)index_clause,LispNil));
     else 
       qpqual = set_difference(scan_clauses, CAR(indxqual));
     
     /*  XXX
     fixed_indxqual = fix_indxqual_references(indxqual,best_path);
     */
     fixed_indxqual = indxqual;
     scan_node = make_indexscan(tlist,
				 qpqual,
				 CInteger(CAR(get_relids 
					      (get_parent((Path)best_path)))),
				 get_indexid(best_path),
				 fixed_indxqual);
     
     set_cost((Plan)scan_node,get_path_cost((Path)best_path));

     return(scan_node);

}  /* function end  */

/*  .. create_indexscan_node, fix-indxqual-references    */

LispValue
fix_indxqual_references(clause,index_path)
     LispValue clause;
     Path index_path ;
{
    LispValue newclause;
    if(IsA(clause,Var) && 
       CInteger(CAR(get_relids(get_parent(index_path)))) ==
       get_varno((Var)clause)) {
	int pos = 0;
	LispValue x = LispNil;
	int varatt = get_varattno((Var)clause);
	foreach(x,get_indexkeys(get_parent(index_path))) {
	    int att = CInteger(CAR(x));
	    if(varatt == att) {
	       break;
	    }
	    pos++;
	}
	newclause = (LispValue)CopyObject(clause);
	set_varattno((Var)newclause, pos + 1);
	return(newclause);
    } 
    else 
      if(IsA(clause,Const))
	return(clause);
    else 
      if(is_opclause(clause) && 
	  is_funcclause((LispValue)get_leftop(clause)) && 
	  get_funcisindex((Func)get_function((LispValue)get_leftop(clause)))) {

	  /* 	 (set_opno(get_op clause) 
	   *   (get_funcisindex(get_function(get_leftop clause))))
	   * 	 (make_opclause(replace_opid(get_op clause))
	   */
	  return(make_opclause((Oper)get_op(clause),
			       MakeVar((Index)CInteger(CAR(get_relids
				       (get_parent(index_path)))),
				       1, /* func indices have one key */
				       get_functype((Func)get_function(clause)),
				       LispNil, LispNil, LispNil, (Pointer)NULL),
			       get_rightop(clause)));

      } 
      else {
	  LispValue type = clause_type(clause);
	  LispValue new_subclauses = LispNil; 
	  LispValue subclause = LispNil;
	  LispValue i = LispNil;

	  foreach(i,clause_subclauses(type,clause)) {
	      subclause = CAR(i);
	      if(subclause)
		new_subclauses = nappend1(new_subclauses,
					  fix_indxqual_references(subclause,
								  index_path));


	  }

	  /* XXX new_subclauses should be a list of the form:
	   * ( (var var) (var const) ...) ?
	   */
	  
	  if( consp(new_subclauses) ) {
	      /* XXX was apply function  */
	      LispValue i = LispNil;
	      LispValue newclause = LispNil;
	      LispValue newclauses = LispNil;

	      newclause = make_clause(type,new_subclauses);
	      return(newclause);

	  } 
	  else {
	      return(clause);
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
create_nestloop_node(best_path,tlist,clauses,
		      outer_node,outer_tlist,inner_node,inner_tlist)
     JoinPath best_path;
     List tlist,clauses;
     Plan outer_node;
     List outer_tlist;
     Plan inner_node;
     List inner_tlist ;
{
    NestLoop join_node = (NestLoop)NULL;

    if( IsA(inner_node,IndexScan)) {

	/*  An index is being used to reduce the number of tuples scanned in 
	 *    the inner relation.
	 * There will never be more than one index used in the inner 
	 * scan path, so we need only consider the first set of 
	 *    qualifications in indxqual. 
	 */

	List inner_indxqual = CAR(get_indxqual((IndexScan)inner_node));
	List inner_qual = (inner_indxqual == LispNil)? LispNil:CAR(inner_indxqual);

	/* If we have in fact found a join index qualification, remove these
	 * index clauses from the nestloop's join clauses and reset the 
	 * inner(index) scan's qualification so that the var nodes refer to
	 * the proper outer join relation attributes.
	 */

	if  (!(qual_clause_p(inner_qual))) {
	    LispValue new_inner_qual = LispNil;
	    
	    clauses = set_difference(clauses,inner_indxqual);
	    new_inner_qual = index_outerjoin_references(inner_indxqual,
							 get_qptargetlist 
							 (outer_node),
							 get_scanrelid 
							 ((Scan)inner_node));
	    set_indxqual((IndexScan)inner_node,lispCons(new_inner_qual,LispNil));
	  }
      }
    else if (IsA(inner_node,Join)) {
	inner_node = (Plan)make_temp(inner_tlist, LispNil, LispNil, 
			             inner_node, TEMP_MATERIAL);
      }
    join_node = make_nestloop(tlist,
			       join_references(clauses,
						outer_tlist,
						inner_tlist),
			       outer_node,
			       inner_node);

    set_cost((Plan)join_node,get_path_cost((Path)best_path));

    return(join_node);

}  /* function end  */


/*    
 *    	create_mergejoin_node
 *    
 *    	Returns a new mergejoin node.
 *    
 */

/*  .. create_join_node     */


MergeJoin
create_mergejoin_node(best_path,tlist,clauses,
		       outer_node,outer_tlist,
		       inner_node,inner_tlist)
     MergePath best_path;
     List tlist,clauses;
     Plan outer_node;
     List outer_tlist;
     Plan inner_node;
     List inner_tlist ;
{
     /*    Separate the mergeclauses from the other join qualification 
      *    clauses and set those clauses to contain references to lower 
      *    attributes. 
      */

     LispValue qpqual = 
       join_references(set_difference(clauses,
					get_path_mergeclauses(best_path)),
			outer_tlist,
			inner_tlist);

     /*    Now set the references in the mergeclauses and rearrange them so 
      *    that the outer variable is always on the left. 
      */
     
     LispValue mergeclauses = 
       switch_outer(join_references(get_path_mergeclauses(best_path),
				      outer_tlist,
				      inner_tlist));
     RegProcedure opcode = 
     get_opcode(get_join_operator((MergeOrder)get_p_ordering((Path)best_path)));
     
     LispValue outer_order =
	lispCons((LispValue) get_left_operator( (MergeOrder)
					get_p_ordering( (Path) best_path)),
		 LispNil);
     
     LispValue inner_order = 
       lispCons( (LispValue) get_right_operator( (MergeOrder)
					get_p_ordering( (Path) best_path)),
		LispNil);
     
     MergeJoin join_node;
     
     /*    Create explicit sort paths for the outer and inner join paths if 
      *    necessary.  The sort cost was already accounted for in the path. 
      */
     if( get_outersortkeys(best_path)) {
	 Temp sorted_outer_node = make_temp(outer_tlist,
					     get_outersortkeys 
					     (best_path),
					     outer_order,
					     outer_node,
					     TEMP_SORT);
	 set_cost((Plan)sorted_outer_node,get_cost(outer_node));
	 outer_node = (Plan)sorted_outer_node;
     }

     if( get_innersortkeys(best_path)) {
	  Temp sorted_inner_node = make_temp(inner_tlist,
						   get_innersortkeys 
						   (best_path),
						   inner_order,
						   inner_node,TEMP_SORT);
	  set_cost((Plan)sorted_inner_node,get_cost(inner_node));
	  inner_node = (Plan)sorted_inner_node;
     }

     join_node = make_mergesort(tlist,
				qpqual,
				mergeclauses,
				opcode,
				inner_order,
				outer_order, 
				inner_node,
				outer_node);
     set_cost((Plan)join_node,get_path_cost((Path)best_path));
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
switch_outer(clauses)
     LispValue clauses ;
{
    LispValue t_list = LispNil;
    LispValue clause = LispNil;
    LispValue temp = LispNil;
    LispValue i = LispNil;

    foreach(i,clauses) {
	clause = CAR(i);
	if(var_is_outer(get_rightop(clause))) {
	    temp = make_clause((LispValue)get_op(clause),
				lispCons((LispValue)get_rightop(clause),
					 lispCons((LispValue)get_leftop(clause),
						  LispNil)));
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
create_hashjoin_node(best_path,tlist,clauses,outer_node,outer_tlist,
		      inner_node,inner_tlist)
     HashPath best_path;
     List tlist,clauses;
     Plan outer_node;
     List outer_tlist;
     Plan inner_node;
     List inner_tlist ;
{
    LispValue qpqual = 
      /*    Separate the hashclauses from the other join qualification clauses
       *    and set those clauses to contain references to lower attributes. 
       */
      join_references(set_difference(clauses,
				       get_path_hashclauses(best_path)),
		       outer_tlist,
		       inner_tlist);
    LispValue hashclauses = 
      /*    Now set the references in the hashclauses and rearrange them so 
       *    that the outer variable is always on the left. 
       */
      switch_outer(join_references(get_path_hashclauses(best_path),
				     outer_tlist,
				     inner_tlist));
    HashJoin join_node;
    Hash hash_node;
    Var innerhashkey;

    innerhashkey = get_rightop(CAR(hashclauses));

    hash_node = make_hash(inner_tlist, innerhashkey, inner_node);
    join_node = make_hashjoin(tlist,
			       qpqual,
			       hashclauses,
			       outer_node,
			       (Plan)hash_node);
    set_cost((Plan)join_node,get_path_cost((Path)best_path));
    return(join_node);
    
} /* function end  */

/*    	===================
 *    	UTILITIES FOR TEMPS
 *    	===================
 */

/*    
 *    	make_temp
 *    
 *    	Create plan nodes to sort or materialize relations into temporaries. The
 *    	result returned for a sort will look like (SEQSCAN(SORT(plan-node)))
 *	or (SEQSCAN(MATERIAL(plan-node)))
 *    
 *    	'tlist' is the target list of the scan to be sorted or hashed
 *    	'keys' is the list of keys which the sort or hash will be done on
 *    	'operators' is the operators with which the sort or hash is to be done
 *    		(a list of operator OIDs)
 *    	'plan-node' is the node which yields tuples for the sort
 *    	'temptype' indicates which operation(sort or hash) to perform
 *    
 */

/*  .. create_hashjoin_node, create_mergejoin_node, create_plan    */

Temp
make_temp(tlist,keys,operators,plan_node,temptype)
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

     switch(temptype) {
       case TEMP_SORT : 
	 retval = (Temp)make_seqscan(tlist,
				     LispNil,
				     _TEMP_RELATION_ID_,
				     (Plan)make_sort(temp_tlist,
						     _TEMP_RELATION_ID_,
						     plan_node,
						     length(keys)));
	 break;
	 
       case TEMP_MATERIAL : 
	 retval = (Temp)make_seqscan(tlist,
				     LispNil,
				     _TEMP_RELATION_ID_,
				     (Plan)make_material(temp_tlist,
						         _TEMP_RELATION_ID_,
						         plan_node,
						         length(keys)));
	 break;
	 
       default: 
	 elog(WARN,"make_temp: unknown temp type",temptype);
	 
     }
     return(retval);

}  /* function end  */
	      

/*    
 *    	set-temp-tlist-operators
 *    
 *    	Sets the key and keyop fields of resdom nodes in a target list.
 *    
 *    	'tlist' is the target list
 *    	'pathkeys' is a list of N keys in the form((key1) (key2)...(keyn)),
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
set_temp_tlist_operators(tlist,pathkeys,operators)
     List tlist;
     List pathkeys;
     List operators ;
{
     LispValue 	keys = LispNil;
     List 	ops = operators;
     int 	keyno = 1;
     Resdom 	resdom = (Resdom)NULL ;
     LispValue i = LispNil;

     foreach(i,pathkeys) {
	 keys = CAR(CAR(i));
	 resdom = tlist_member((Var)keys,
				tlist);
	 if( resdom) {

	     /* Order the resdom keys and replace the operator OID for each 
	      *    key with the regproc OID. 
	      */
	     set_reskey(resdom,keyno);
	     set_reskeyop(resdom,(OperatorTupleForm)get_opcode((ObjectId)CAR(ops))); /* XXX */
	 }
	 keyno += 1;
	 /*
	 ops = CDR(ops);
	 */
     }
     return(tlist);
} /* function end  */

SeqScan
make_seqscan(qptlist,qpqual,scanrelid,lefttree)
     List qptlist;
     List qpqual;
     Index scanrelid;
     Plan lefttree;
{
    SeqScan node = RMakeSeqScan();
    
    if(!null(lefttree))
      Assert(IsA(lefttree,Plan));

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_state((Plan)node, (EState)NULL);
    set_qptargetlist((Plan)node, qptlist);
    set_qpqual((Plan)node , qpqual);
    set_lefttree((Plan)node, lefttree);
    set_righttree((Plan)node , (Plan) NULL);
    set_scanrelid((Scan)node , scanrelid);
    set_scanstate((Scan)node, (ScanState)NULL);

    return(node);
}

IndexScan
make_indexscan(qptlist, qpqual, scanrelid,indxid,indxqual)
     List qptlist,qpqual;
     Index scanrelid;
     List indxid, indxqual;

{
    IndexScan node = RMakeIndexScan();

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_state((Plan)node, (EState)NULL);
    set_qptargetlist((Plan)node, qptlist);
    set_qpqual((Plan)node , qpqual);
    set_lefttree((Plan)node,(Plan)NULL);
    set_righttree((Plan)node,(Plan)NULL);
    set_scanrelid((Scan)node,scanrelid);
    set_indxid(node,indxid);
    set_indxqual(node,indxqual);
    set_scanstate((Scan)node, (ScanState)NULL);

    return(node);
}


NestLoop
make_nestloop(qptlist,qpqual,lefttree,righttree)
     List qptlist;
     List qpqual;
     Plan lefttree;
     Plan righttree;
{
    NestLoop node = RMakeNestLoop();

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_state((Plan)node, (EState)NULL);
    set_qptargetlist((Plan)node, qptlist);
    set_qpqual((Plan)node , qpqual);
    set_lefttree((Plan)node, lefttree);
    set_righttree((Plan)node , righttree);
    set_nlstate((NestLoop)node, (NestLoopState)NULL);
    set_ruleinfo((Join)node, (JoinRuleInfo)NULL);

    return(node);
}

HashJoin
make_hashjoin(tlist,qpqual,hashclauses,lefttree,righttree)
     LispValue tlist, qpqual,hashclauses;
     Plan righttree,lefttree;
{
    HashJoin node = RMakeHashJoin();
    
    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_state((Plan)node, (EState)NULL);
    set_qpqual((Plan)node,qpqual);
    set_qptargetlist((Plan)node,tlist);
    set_lefttree((Plan)node,lefttree);
    set_righttree((Plan)node,righttree);
    set_ruleinfo((Join)node, (JoinRuleInfo) NULL);
    set_hashclauses(node,hashclauses);
    set_hashjointable(node, NULL);
    set_hashjointablekey(node, 0);
    set_hashjointablesize(node, 0);
    set_hashdone(node, false);

    return(node);
}

Hash
make_hash(tlist, hashkey, lefttree)
     LispValue tlist;
     Var hashkey;
     Plan lefttree;
{
    Hash node = RMakeHash();

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_parallel((Plan)node, 1);
    set_state((Plan)node, (EState)NULL);
    set_qpqual((Plan)node,LispNil);
    set_qptargetlist((Plan)node,tlist);
    set_lefttree((Plan)node,lefttree);
    set_righttree((Plan)node,(Plan)NULL);
    set_hashkey(node, hashkey);
    set_hashtable(node, NULL);
    set_hashtablekey(node, 0);
    set_hashtablesize(node, 0);

    return(node);
}

MergeJoin
make_mergesort(tlist, qpqual, mergeclauses, opcode, rightorder, leftorder,
	       righttree, lefttree)
     LispValue tlist, qpqual,mergeclauses;
     ObjectId opcode;
     LispValue rightorder, leftorder;
     Plan righttree,lefttree;
{
    MergeJoin node = RMakeMergeJoin();
    
    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_state((Plan)node, (EState)NULL);
    set_qpqual((Plan)node,qpqual);
    set_qptargetlist((Plan)node,tlist);
    set_lefttree((Plan)node,lefttree);
    set_righttree((Plan)node,righttree);
    set_ruleinfo((Join)node, (JoinRuleInfo) NULL);
    set_mergeclauses(node,mergeclauses);
    set_mergesortop(node,opcode);
    set_mergerightorder(node, rightorder);
    set_mergeleftorder(node, leftorder);

    return(node);

}

Sort
make_sort(tlist,tempid,lefttree, keycount)
     LispValue tlist;
     Count keycount;
     Plan lefttree;
     ObjectId tempid;
{
    Sort node = RMakeSort();

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_parallel((Plan)node, 1);
    set_state((Plan)node, (EState)NULL);
    set_qpqual((Plan)node,LispNil);
    set_qptargetlist((Plan)node,tlist);
    set_lefttree((Plan)node,lefttree);
    set_righttree((Plan)node,(Plan)NULL);
    set_tempid((Temp)node,tempid);
    set_keycount((Temp)node,keycount);

    return(node);
}

Material
make_material(tlist,tempid,lefttree, keycount)
     LispValue tlist;
     Count keycount;
     Plan lefttree;
     ObjectId tempid;
{
    Material node = RMakeMaterial(); 

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_parallel((Plan)node, 1);
    set_state((Plan)node, (EState)NULL);
    set_qpqual((Plan)node,LispNil);
    set_qptargetlist((Plan)node,tlist);
    set_lefttree((Plan)node,lefttree);
    set_righttree((Plan)node,(Plan)NULL);
    set_tempid((Temp)node,tempid);
    set_keycount((Temp)node,keycount);

    return(node);
}

Agg
make_agg(arglist, aggidnum)
     List arglist;
     ObjectId aggidnum;
{
     /* arglist is(Resnode((interesting stuff), NULL)) */
     Agg node = RMakeAgg();
     Resdom resdom = (Resdom)CAR(arglist);
     String aggname = NULL; 
     LispValue query = LispNil;
     List varnode = LispNil;
     arglist = CADR(arglist);
     aggname = CString(CAR(arglist));
     arglist = CDR(arglist);
     query = CAR(arglist);
     arglist = CDR(arglist);
     varnode = CAR(arglist);
     set_cost((Plan)node, 0.0);
     set_fragment((Plan)node, 0);
     set_parallel((Plan)node, 1);
     set_state((Plan)node, (EState)NULL);
     set_qpqual((Plan)node, LispNil);
     set_qptargetlist((Plan)node, 
		      lispCons(MakeList((LispValue)resdom, 
					varnode,
					-1),
			       LispNil));
     set_lefttree((Plan)node, planner(query));
     set_righttree((Plan)node, (Plan)NULL);
     set_tempid((Temp)node, aggidnum);
     set_aggname(node, aggname);
     return(node);
}

/*
 *  A unique node always has a SORT node in the lefttree.
 */

Unique
make_unique(tlist,lefttree)
     List tlist;
     Plan lefttree;
{
    Unique node = RMakeUnique();

    set_cost((Plan)node , 0.0 );
    set_fragment((Plan)node, 0 );
    set_parallel((Plan)node, 1);
    set_state((Plan)node, (EState)NULL);
    set_qpqual((Plan)node,LispNil);
    set_qptargetlist((Plan)node,tlist);
    set_lefttree((Plan)node,lefttree);
    set_righttree((Plan)node,(Plan)NULL);
    set_tempid((Temp)node,-1);
    set_keycount((Temp)node,0);

    return(node);
}
