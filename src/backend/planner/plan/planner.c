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

#include "tmp/c.h"

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "parser/parse.h"

#include "planner/internal.h"
#include "planner/planner.h"

#include "catalog/pg_type.h"	/* for pg_checkretval() */

#include "utils/log.h"

/* ----------------
 *	Existential creator declaration
 * ----------------
 */
extern Existential RMakeExistential();

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
#include "nodes/relation.h"
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

extern bool DebugPrintPlan;

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
    LispValue uniqueflag = root_uniqueflag(root);
    LispValue sortclause = root_sortclause(root);
    LispValue sortkeys = LispNil;
    LispValue sortops = LispNil;
    Plan special_plans = (Plan)NULL;
    Plan regular_plans = (Plan)NULL;
    LispValue flag = LispNil;
    List plan_list = LispNil;

    plan_list = lispCons(lispAtom("inherits"),
		   lispCons(lispAtom("union"),
			    lispCons(lispAtom("archive"),LispNil)));
    foreach (flag,plan_list) {
	Index rt_index = first_matching_rt_entry (rangetable,CAR(flag));
	if ( rt_index != -1 )
	  special_plans = (Plan)plan_union_queries (rt_index,
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
     *  sortkeys:    a list of resdom nodes corresponding to the attributes in
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
 *      
 *      sortkeys: ( resdom1 resdom2 resdom3 ...)
 *      sortops:  (sortop1 sortop2 sortop3 ...)
 */

Plan
make_sortplan(tlist,sortkeys,sortops,plannode)
     List tlist;
     List sortkeys;
     List sortops;
     Plan plannode;

{
  Plan sortplan = (Plan)NULL;
  List temp_tlist = LispNil;
  LispValue i = LispNil;
  Resdom resnode = (Resdom)NULL;
  Resdom resdom = (Resdom)NULL;
  int keyno =1;

  /*  First make a copy of the tlist so that we don't corrupt the 
   *  the original .
   */
  
  temp_tlist = new_unsorted_tlist(tlist);
  
  foreach (i, sortkeys) {
      
      resnode = (Resdom)CAR(i);
      resdom = tlist_resdom(temp_tlist,resnode);

      /*    Order the resdom keys and replace the operator OID for each 
       *    key with the regproc OID. 
       */
      
      set_reskey (resdom,keyno);
      set_reskeyop (resdom,(OperatorTupleForm)get_opcode (CInteger(CAR(sortops))));      

      keyno += 1;
      sortops = CDR(sortops);
  }

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

     if ( DebugPrintPlan ) {
	 printf("after preprocessing, qual is :\n");
	 lispDisplay(qual);
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
    Existential node = RMakeExistential();

    set_lefttree(node,(PlanPtr)left);
    set_righttree(node,(PlanPtr)right);
    return(node);
}

/*
 *  pg_checkretval() -- check return value of a list of postquel parse
 *			trees.
 *
 *	The return value of a postquel function is the value returned by
 *	the final query in the function.  We do some ad-hoc define-time
 *	type checking here to be sure that the user is returning the
 *	type he claims.
 */

void
pg_checkretval(rettype, parselist)
    ObjectId rettype;
    List parselist;
{
    LispValue parse;
    LispValue tlist;
    LispValue tle;
    LispValue root;
    LispValue rt;
    LispValue rte;
    int cmd;
    char *typ;
    Resdom resnode;
    LispValue thenode;
    Relation reln;
    ObjectId relid;
    ObjectId tletype;
    int relnatts;
    int i;

    /* find the final query */
    while (CDR(parselist) != LispNil)
	parselist = CDR(parselist);

    parse = CAR(parselist);

    /*
     *  test 1:  if the last query is a utility invocation, then there
     *  had better not be a return value declared.
     */

    if (atom(CAR(parse))) {
	if (rettype == InvalidObjectId)
	    return;
	else
	    elog(WARN, "return type mismatch in function decl: final query is a catalog utility");
    }

    /* okay, it's an ordinary query */
    root = parse_root(parse);
    tlist = parse_targetlist(parse);
    rt = root_rangetable(root);
    cmd = (int) root_command_type(root);

    /*
     *  test 2:  if the function is declared to return no value, then the
     *  final query had better not be a retrieve.
     */

    if (rettype == InvalidObjectId) {
	if (cmd == RETRIEVE)
	    elog(WARN, "function declared with no return type, but final query is a retrieve");
	else
	    return;
    }

    /* by here, the function is declared to return some type */
    if ((typ = (char *) get_id_type(rettype)) == (char *) NULL)
	elog(WARN, "can't find return type %d for function\n", rettype);

    /*
     *  test 3:  if the function is declared to return a value, then the
     *  final query had better be a retrieve.
     */

    if (cmd != RETRIEVE)
	elog(WARN, "function declared to return type %s, but final query is not a retrieve", tname(typ));

    /*
     *  test 4:  for base type returns, the target list should have exactly
     *  one entry, and its type should agree with what the user declared.
     */

    if (get_typrelid(typ) == InvalidObjectId) {
	if (ExecTargetListLength(tlist) > 1)
	    elog(WARN, "function declared to return %s returns multiple values in final retrieve", tname(typ));

	resnode = (Resdom) CAR(CAR(tlist));
	if (get_restype(resnode) != rettype)
	    elog(WARN, "return type mismatch in function: declared to return %s, returns %s", tname(typ), tname(get_id_type(get_restype(resnode))));

	/* by here, base return types match */
	return;
    }

    /*
     *  If the target list is of length 1, and the type of the varnode
     *  in the target list is the same as the declared return type, this
     *  is okay.  This can happen, for example, where the body of the
     *  function is 'retrieve (x = func2())', where func2 has the same
     *  return type as the function that's calling it.
     */

    if (ExecTargetListLength(tlist) == 1) {
	resnode = (Resdom) CAR(CAR(tlist));
	if (get_restype(resnode) == rettype)
	    return;
    }

    /*
     *  By here, the procedure returns a (set of) tuples.  This part of
     *  the typechecking is a hack.  We look up the relation that is
     *  the declared return type, and be sure that attributes 1 .. n
     *  in the target list match the declared types.
     */

    reln = heap_open(get_typrelid(typ));

    if (!RelationIsValid(reln))
	elog(WARN, "cannot open relation relid %d", get_typrelid(typ));

    relid = reln->rd_id;
    relnatts = reln->rd_rel->relnatts;

    if (ExecTargetListLength(tlist) != relnatts)
	elog(WARN, "function declared to return type %s does not retrieve (%s.all)", tname(typ), tname(typ));

    /* expect attributes 1 .. n in order */
    for (i = 1; i <= relnatts; i++) {
	tle = CAR(tlist);
	tlist = CDR(tlist);
	thenode = CADR(tle);

	/* this is tedious */
	if (IsA(thenode,Var))
	    tletype = (ObjectId) get_vartype((Var)thenode);
	else if (IsA(thenode,Const))
	    tletype = (ObjectId) get_consttype((Const)thenode);
	else if (IsA(thenode,Param))
	    tletype = (ObjectId) get_paramtype((Param)thenode);
	else
	    elog(WARN, "function declared to return type %s does not retrieve (%s.all)", tname(typ), tname(typ));

	/* reach right in there, why don't you? */
	if (tletype != reln->rd_att.data[i-1]->atttypid)
	    elog(WARN, "function declared to return type %s does not retrieve (%s.all)", tname(typ), tname(typ));
    }

    heap_close(reln);

    /* success */
    return;
}
