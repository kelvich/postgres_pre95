/*     
 *      FILE
 *     	initsplan
 *     
 *      DESCRIPTION
 *     	Target list, qualification, joininfo initialization routines
 *     
 */

/* RcsId("$Header$");  */

/*     
 *      EXPORTS
 *     		initialize-targetlist
 *     		initialize-qualification
 *     		initialize-join-clause-info
 *     
 *      NOTES
 *     	These routines are called by(subplanner) and modify global state of
 *     	the planner(adding entries to *query-relation-list* or modifying an
 *     	existing entry).
 */

#include "tmp/c.h"

#include "nodes/pg_lisp.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "utils/lsyscache.h"

#include "planner/internal.h"
#include "planner/clause.h"
#include "planner/clauses.h"
#include "planner/clausesel.h"
#include "planner/initsplan.h"
#include "planner/joininfo.h"
#include "planner/relnode.h"
#include "planner/tlist.h"

extern bool _enable_mergesort_;
extern bool _enable_hashjoin_;
extern int Quiet;

/*     	============
 *     	TARGET LISTS
 *     	============
 */

/*    
 *    	initialize-targetlist
 *    
 *    	Creates rel nodes for every relation mentioned in the target list
 *    	'tlist' (if a node hasn't already been created) and adds them to
 *    	*query-relation-list*.  Creates targetlist entries for each member of
 *    	'tlist' and adds them to the tlist field of the appropriate rel node.
 *    
 *    	Returns nothing.
 *    
 */

/*  .. subplanner      */

void 
initialize_targetlist(tlist)
     List tlist ;
{
    List tlist_vars = LispNil;
    List xtl = LispNil;
    List tvar = LispNil;
    
#ifdef NO_PLEASE
    if(!Quiet) {
    lispDisplay(tlist,0);
    printf("\n");
    fflush(stdout);
    }
#endif NO_PLEASE

    foreach (xtl,tlist) {
	TLE entry;
	(LispValue)entry = CAR(xtl);

#ifdef NO_PLEASE
        if(!Quiet) {
	lispDisplay(entry,0);
	fflush(stdout);
	}
#endif NO_PLEASE
	tlist_vars = nconc(tlist_vars,pull_var_clause(get_expr(entry)));
    }

    foreach (tvar, tlist_vars) {
	Var 	var;
	Index   varno;
	Rel	result;
	
	var = (Var)CAR(tvar);
	varno = get_varno(var);
	result = get_rel(lispInteger(varno));

	add_tl_element(result,var,LispNil);
    }
    
}  /* function end   */

/*     	==============
 *     	QUALIFICATIONS
 *     	==============
 */

/*    
 *    	initialize-qualification
 *    
 *    	Initializes ClauseInfo and JoinInfo fields of relation entries for all
 *    	relations appearing within clauses.  Creates new relation entries if
 *    	necessary, adding them to *query-relation-list*.
 *    
 *    	Returns nothing of interest.
 *    
 */

/*  .. initialize-qualification, subplanner      */

void
initialize_qualification(clauses)
     LispValue clauses ;
{
     LispValue clause = LispNil;

     foreach (clause, clauses) {
	 if(consp(CAR(clause)) && IsA(CAR(clause),Func))
	    initialize_qualification(get_funcargs(CAR(clause)));
	  else 
	    if(consp(CAR(clause)) && IsA(CAR(clause),Oper))
	      initialize_qualification(get_opargs(CAR(clause)));
	  add_clause_to_rels(CAR(clause));
     }
}  /* function end   */

/*    
 *    	add-clause-to-rels
 *    
 *    	Add clause information to either the 'ClauseInfo' or 'JoinInfo' field
 *    	of a relation entry(depending on whether or not the clause is a join)
 *    	by creating a new ClauseInfo node and setting appropriate fields
 *    	within the nodes.
 *    
 *    	Returns nothing of interest.
 *    
 */

/*  .. initialize-qualification		 */

void
add_clause_to_rels(clause)
     LispValue clause ;
{
     
    /* Retrieve all relids and vars contained within the clause in the form */
    /* ((relid relid ...) (var var ...)) */
    
    LispValue relids_vars = clause_relids_vars(clause);
    LispValue relids = nth(0,relids_vars);
    LispValue vars = nth(1,relids_vars);
    
    if(nested_clause_p(clause) || 
       relation_level_clause_p(clause)) 
      /* Ignore quals containing dot fields or relation level clauses, */
      /* but place the vars in the target list so their values can be  */
      /* referenced later. */
      
      add_vars_to_rels(vars,LispNil);
    else {
	CInfo clauseinfo = RMakeCInfo();
	set_clause(clauseinfo,(Expr)clause);
	set_notclause(clauseinfo,contains_not(clause));
	set_selectivity(clauseinfo,0);
	set_indexids(clauseinfo,LispNil);
	set_mergesortorder(clauseinfo,(MergeOrder)NULL);
	set_hashjoinoperator(clauseinfo,(ObjectId)0);
	
	if(1 == length(relids)) {
	    
	    /* There is only one relation participating in 'clause', */
	    /* so 'clause' must be a restriction clause. */
	    /* XXX - let form, maybe incorrect */
	    Rel rel = get_rel(CAR(relids));
	    set_clauseinfo(rel,cons(clauseinfo,get_clauseinfo(rel)));
	} 
	else {
	    /* 'clause' is a join clause, since there is more than one */
	    /*  atom in the relid list. */
	    
	    set_selectivity(clauseinfo,compute_clause_selec(clause,
							      LispNil));
	    add_join_clause_info_to_rels(clauseinfo,relids);
	    add_vars_to_rels(vars,relids);
	} 
    }
}  /* function end   */

/*    
 *    	add-join-clause-info-to-rels
 *    
 *    	For every relation participating in a join clause, add 'clauseinfo' to
 *    	the appropriate joininfo node(creating a new one and adding it to the
 *    	appropriate rel node if necessary).
 *    
 *    	'clauseinfo' describes the join clause
 *    	'join-relids' is the list of relations participating in the join clause
 *    
 *    	Returns nothing.
 *    
 */

/*  .. add-clause-to-rels		 */

void
add_join_clause_info_to_rels(clauseinfo,join_relids)
     CInfo clauseinfo;
     List join_relids ;
{
    LispValue join_relid = LispNil;
    foreach (join_relid, join_relids) {
	JInfo joininfo = 
	  find_joininfo_node(get_rel(CAR(join_relid)),
			      LispRemove(CAR(join_relid),join_relids));
	set_jinfoclauseinfo(joininfo,
			     lispCons((LispValue)clauseinfo,
				   get_jinfoclauseinfo(joininfo)));

     }
} /* function end  */

/*    
 *    	add-vars-to-rels
 *    
 *    	For each variable appearing in a clause,
 *    	(1) If a targetlist entry for the variable is not already present in
 *    	    the appropriate relation's target list, add one.
 *    	(2) If a targetlist entry is already present, but the var is part of a
 *    	    join clause, add the relids of the join relations to the JoinList
 *    	    entry of the targetlist entry.
 *    
 *    	'vars' is the list of var nodes
 *    	'join-relids' is the list of relids appearing in the join clause
 *    		(if this is a join clause)
 *    
 *    	Returns nothing.
 *    
 */

/*  .. add-clause-to-rels     */
 
void
add_vars_to_rels(vars,join_relids)
     List vars,join_relids ;
{
    Var var;
    LispValue temp = LispNil;
    Rel rel = (Rel)NULL;
    LispValue tlistentry = LispNil;
    LispValue other_join_relids = LispNil;
    
    foreach (temp, vars) {
	var = (Var)CAR(temp);
	rel = get_rel(lispInteger(get_varno(var)));
	tlistentry = tlistentry_member(var,get_targetlist(rel));
	other_join_relids = LispRemove(lispInteger(get_varno(var)),
				    join_relids);
	if(null(tlistentry))
	  /*   add a new entry */
	  add_tl_element(rel,var,other_join_relids);
	else 
	  if(get_joinlist(tlistentry)) {
	      set_joinlist(tlistentry,
			    /*   modify an existing entry */
			    LispUnion(get_joinlist(tlistentry),
				       other_join_relids));
	  } 
    }
} /* function end   */

/*     	========
 *     	JOININFO
 *     	========
 */

/*    
 *    	initialize-join-clause-info
 *    
 *    	Set the MergeSortable or HashJoinable field for every joininfo node
 *    	(within a rel node) and the MergeSortOrder or HashJoinOp field for
 *    	each clauseinfo node(within a joininfo node) for all relations in a
 *    	query.
 *    
 *    	Returns nothing.
 *    
 */

/*  .. subplanner    */
 
void
initialize_join_clause_info(rel_list)
     LispValue rel_list ;
{
    LispValue x;
    Rel rel;
    LispValue y;
    JInfo joininfo;
    LispValue z;
    CInfo clauseinfo;
    LispValue clause;
    foreach (x, rel_list) {
	rel = (Rel)CAR(x);
	foreach (y, get_joininfo(rel)) {
	    joininfo = (JInfo)CAR(y);
	    foreach (z, get_jinfoclauseinfo(joininfo)) {
		clauseinfo = (CInfo)CAR(z);
		clause = (LispValue)get_clause(clauseinfo);
		if( join_clause_p(clause) ) {
		    MergeOrder sortop;
		    ObjectId hashop;

		    if( _enable_mergesort_ ) 
		      sortop = mergesortop(clause);
		    if( _enable_hashjoin_ ) 
		      hashop = hashjoinop(clause);

		    if( sortop) {
			set_mergesortorder(clauseinfo, sortop);
			set_mergesortable(joininfo,true);
		    }
		    if( hashop) {
			set_hashjoinoperator(clauseinfo,hashop);
			set_hashjoinable(joininfo,true);
		    }
		}
	    }
	}
    }
}  /* function end   */

/*    
 *    	mergesortop			=== MERGE
 *    
 *    	Returns the mergesort operator of an operator iff 'clause' is
 *    	mergesortable, i.e., both operands are single vars and the operator is
 *    	a mergesortable operator.
 *    
 */
MergeOrder
mergesortop(clause)
     LispValue clause;
{
     LispValue sortops = op_mergesortable(get_opno(get_op(clause)),
					   get_vartype(get_leftop(clause)),
					   get_vartype(get_rightop(clause)));
     if( consp(sortops) ) {
	  return(MakeMergeOrder(get_opno(get_op(clause)),
				(ObjectId)nth(0,sortops),
				(ObjectId)nth(1,sortops),
				get_vartype(get_leftop(clause)),
				get_vartype(get_rightop(clause))));
     }
     else 
       return(NULL);
}  /* function end  */

/*    
 *    	hashjoinop			===  HASH
 *    
 *    	Returns the hashjoin operator of an operator iff 'clause' is
 *    	hashjoinable, i.e., both operands are single vars and the operator is
 *    	a hashjoinable operator.
 *    
 */

/*  .. initialize-join-clause-info   */
 
ObjectId
hashjoinop(clause)
     LispValue clause ;
{
     return(op_hashjoinable(get_opno(get_op(clause)),
			      get_vartype(get_leftop(clause)),
			      get_vartype(get_rightop(clause))));
}
