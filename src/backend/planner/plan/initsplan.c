
/*     
 *      FILE
 *     	initsplan
 *     
 *      DESCRIPTION
 *     	Target list, qualification, joininfo initialization routines
 *     
 */

/* RcsId ("$Header$");  */

/*     
 *      EXPORTS
 *     		initialize-targetlist
 *     		initialize-qualification
 *     		initialize-join-clause-info
 *     
 *      NOTES
 *     	These routines are called by (subplanner) and modify global state of
 *     	the planner (adding entries to *query-relation-list* or modifying an
 *     	existing entry).
 */

#include "internal.h"
#include "c.h"
#include "clause.h"
#include "relation.h"
#include "relation.a.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "relnode.h"
#include "joininfo.h
#include "initsplan.h"
#include "pg_lisp.h"
#include "lsyscache.h"
#include "tlist.h"

/*
extern void *add_clause_to_rels();
extern void *add_join_clause_info_to_rels();
extern void *add_vars_to_rels();
extern LispValue hashjoinop();
extern LispValue mergesortop();
*/
extern LispValue create_mergeorder(); /* XXX - where is this from ? */

extern Boolean _enable_mergesort_;
extern Boolean _enable_hashjoin_;

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
*initialize_targetlist (tlist)
LispValue tlist ;
{
     LispValue tlist_vars = LispNil;
     LispValue xtl = LispNil;
     LispValue var = LispNil;

     foreach(xtl,tlist) 
       tlist_vars = nappend1(tlist_vars,pull_var_clause(tl_expr(xtl)));

     foreach (var, tlist_vars) 
       add_tl_element (get_rel (get_varno (var)),var,LispNil);

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
*initialize_qualification (clauses)
     LispValue clauses ;
{
     LispValue clause = LispNil;

     foreach (clause, clauses) {
	  if(func_p (clause))
	    initialize_qualification (get_funcargs (clause));
	  else 
	    if (oper_p (clause))
	      initialize_qualification (get_opargs (clause));
	  add_clause_to_rels (clause);
     }
}  /* function end   */

/*    
 *    	add-clause-to-rels
 *    
 *    	Add clause information to either the 'ClauseInfo' or 'JoinInfo' field
 *    	of a relation entry (depending on whether or not the clause is a join)
 *    	by creating a new ClauseInfo node and setting appropriate fields
 *    	within the nodes.
 *    
 *    	Returns nothing of interest.
 *    
 */

/*  .. initialize-qualification		 */

void
*add_clause_to_rels (clause)
     LispValue clause ;
{
     
     /* Retrieve all relids and vars contained within the clause in the form */
     /* ((relid relid ...) (var var ...)) */

     LispValue relids_vars = clause_relids_vars (clause);
     LispValue relids = nth (0,relids_vars);
     LispValue vars = nth (1,relids_vars);

     if(nested_clause_p (clause) || 
	relation_level_clause_p (clause)) 
	  /* Ignore quals containing dot fields or relation level clauses, */
	  /* but place the vars in the target list so their values can be  */
	  /* referenced later. */

	  add_vars_to_rels (vars,LispNil);
     else {
	  CInfo clauseinfo = CreateNode(CInfo);
	  set_clause (clauseinfo,clause);
	  set_notclause (clauseinfo,contains_not (clause));
	  if(1 == length (relids)) {

	       /* There is only one relation participating in 'clause', */
	       /* so 'clause' must be a restriction clause. */
	       /* XXX - let form, maybe incorrect */
	       Rel rel = get_rel (CAR (relids));
	       set_clause_info (rel,cons (clauseinfo,get_clause_info (rel)));
	  } 
	  else {
	       /* 'clause' is a join clause, since there is more than one */
	       /*  atom in the relid list. */

	       set_selectivity (clauseinfo,compute_clause_selec (clause,
								 LispNil));
	       add_join_clause_info_to_rels (clauseinfo,relids);
	       add_vars_to_rels (vars,relids);
	  } 
     }
}  /* function end   */

/*    
 *    	add-join-clause-info-to-rels
 *    
 *    	For every relation participating in a join clause, add 'clauseinfo' to
 *    	the appropriate joininfo node (creating a new one and adding it to the
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
*add_join_clause_info_to_rels (clauseinfo,join_relids)
     LispValue clauseinfo,join_relids ;
{
     LispValue join_relid = LispNil;
     foreach (join_relid, join_relids) {
	  /* XXX - let form, maybe incorrect */
	 JInfo joininfo = 
	   find_joininfo_node (get_rel (join_relid),
			       remove (join_relid,join_relids));
	  set_clause_info (joininfo,
			   cons (clauseinfo,
				 get_clause_info (joininfo)));

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
*add_vars_to_rels (vars,join_relids)
     List vars,join_relids ;
{
     LispValue var = LispNil;
     foreach (var, vars) {
	  Rel rel = get_rel (get_varno (var));
	  LispValue  tlistentry = tlistentry_member (var,get_tlist (rel));
	  LispValue other_join_relids = remove (get_varno (var),
						join_relids);
	  if(null (tlistentry))
	    /*   add a new entry */
	    add_tl_element (rel,var,other_join_relids);
	  else 
	    if (get_join_list (tlistentry)) {
		 set_join_list (tlistentry,
				/*   modify an existing entry */
				LispUnion (get_join_list(tlistentry),
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
 *    	each clauseinfo node (within a joininfo node) for all relations in a
 *    	query.
 *    
 *    	Returns nothing.
 *    
 */

/*  .. subplanner    */
 
void
*initialize_join_clause_info (rel_list)
     LispValue rel_list ;
{
     LispValue rel = LispNil;
     LispValue joininfo = LispNil;
     LispValue clauseinfo = LispNil;
     foreach (rel, rel_list) {
	  foreach (joininfo, get_joininfo (rel)) {
	       foreach (clauseinfo, get_clauseinfo (joininfo)) {
		    Expr clause = get_clause (clauseinfo);
		    if ( join_clause_p (clause) ) {
			 LispValue sortop = LispNil;
			 ObjectId hashop;

			 if ( _enable_mergesort_ ) 
			   sortop = mergesortop (clause);
			 if ( _enable_hashjoin_ ) 
			   hashop = hashjoinop (clause);

			 if ( sortop) {
			      set_mergesortorder (clauseinfo,sortop);
			      set_mergesortable (joininfo,true);
			 }
			 if ( hashop) {
			      set_hashjoinoperator (clauseinfo,hashop);
			      set_hashjoinable (joininfo,true);
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
LispValue
mergesortop (clause)
     LispValue clause ;
{
     LispValue sortops = op_mergesortable (get_opno (get_op (clause)),
					   get_vartype(get_leftop(clause)),
					   get_vartype (get_rightop (clause)));
     if ( consp (sortops) ) {
	  return (create_mergeorder (get_opno (get_op (clause)),
				     nth (0,sortops),
				     nth (1,sortops),
				     get_vartype (get_leftop (clause)),
				     get_vartype (get_rightop (clause))));
     }
     else 
       return(LispNil);
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
hashjoinop (clause)
     LispValue clause ;
{
     return (op_hashjoinable (get_opno (get_op (clause)),
			      get_vartype (get_leftop (clause)),
			      get_vartype (get_rightop (clause))));
}
