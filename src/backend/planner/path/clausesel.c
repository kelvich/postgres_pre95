/* ----------------------------------------------------------------
 *   FILE
 *	clausesel 
 *
 *   DESCRIPTION 
 *	Routines to compute and set clause selectivities 
 *
 *   INTERFACE ROUTINES
 *	set_clause_selectivities
 *	product_selec
 *	set_rest_relselec
 *	compute_clause_selec
 * ----------------------------------------------------------------
 */ 
 
#include "tmp/c.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

#include "planner/internal.h"
#include "planner/clause.h"
#include "planner/clauses.h"
#include "planner/clausesel.h"
#include "planner/plancat.h"
#include "parser/parsetree.h"

#include "catalog/pg_proc.h"
#include "catalog/pg_operator.h"

#include "utils/log.h"

/*     		----  ROUTINES TO SET CLAUSE SELECTIVITIES  ----   */

/*    
 *    	set_clause_selectivities
 *    
 *    	Sets the selectivity field for each of clause in 'clauseinfo-list'
 *    	to 'new-selectivity'.  If the selectivity has already been set, reset 
 *    	it only if the new one is better.
 *    
 *    	Returns nothing of interest.
 *
 */

/*  .. create_index_path
 */
void
set_clause_selectivities (clauseinfo_list,new_selectivity)
     LispValue clauseinfo_list; 
     Cost new_selectivity ;
{
    LispValue temp;
    CInfo clausenode;
    Cost cost_clause;

    foreach (temp,clauseinfo_list) {
	clausenode = (CInfo)CAR(temp);
	cost_clause = get_selectivity(clausenode);
	if ( FLOAT_IS_ZERO(cost_clause) || new_selectivity < cost_clause) {
	    set_selectivity (clausenode,new_selectivity);
	}
    }
} /* end set_clause_selectivities */

/*    
 *    	product_selec
 *    
 *    	Multiplies the selectivities of each clause in 'clauseinfo-list'.
 *    
 *    	Returns a flonum corresponding to the selectivity of 'clauseinfo-list'.
 *    
 */

/*  .. compute-joinrel-size, compute-rel-size     */

Cost
product_selec (clauseinfo_list)
     LispValue clauseinfo_list ;
{
     Cost result = 1.0;
     if ( consp (clauseinfo_list) ) {
	  LispValue xclausenode = LispNil;
	  Cost temp;

	  foreach(xclausenode,clauseinfo_list) {
	       temp = get_selectivity((CInfo)CAR(xclausenode));
	       result = result * temp;
	  }
     }
     return(result);
} /* end product_selec */

/*    
 *    	set_rest_relselec
 *    
 *    	Scans through clauses on each relation and assigns a selectivity to
 *    	those clauses that haven't been assigned a selectivity by an index.
 *    
 *    	Returns nothing of interest.
 *   	MODIFIES: selectivities of the various rel's clauseinfo
 *		  slots. 
 */

/*  .. find-paths    */

void
set_rest_relselec (rel_list)
     LispValue rel_list ;
{
     Rel rel;
     LispValue x;
     foreach (x,rel_list) {
	 rel = (Rel)CAR(x);
	 set_rest_selec(get_clauseinfo(rel));
     }
}

/*    
 *    	set_rest_selec
 *    
 *    	Sets the selectivity fields for those clauses within a single
 *    	relation's 'clauseinfo-list' that haven't already been set.
 *    
 *    	Returns nothing of interest.
 *    
 */

/*  .. set_rest_relselec    */

void
set_rest_selec (clauseinfo_list)
LispValue clauseinfo_list ;
{
    LispValue temp = LispNil;
    CInfo clausenode = (CInfo)NULL;
    Cost cost_clause;
    
    foreach (temp,clauseinfo_list) {
	clausenode = (CInfo)CAR(temp);
	cost_clause = get_selectivity(clausenode);

	/*    Check to see if the selectivity of this clause or any 'or' */
	/*    subclauses (if any) haven't been set yet. */

	if (valid_or_clause(clausenode) || FLOAT_IS_ZERO(cost_clause)) {
	    set_selectivity (clausenode,
			     compute_clause_selec((List)get_clause(clausenode),
					      lispCons(lispFloat(cost_clause), 
			/* XXX this bogus */		    LispNil)));
	}
    }
} /* end set_rest_selec */

/*    		----  ROUTINES TO COMPUTE SELECTIVITIES  ----
 */

/*    
 *    	compute_clause_selec
 *    
 *    	Given a clause, this routine will compute the selectivity of the
 *    	clause by calling 'compute_selec' with the appropriate parameters
 *    	and possibly use that return value to compute the real selectivity
 *    	of a clause.
 *    
 *    	'or-selectivities' are selectivities that have already been assigned
 *    		to subclauses of an 'or' clause.
 *    
 *    	Returns a flonum corresponding to the clause selectivity.
 *    
 */

/*  .. add-clause-to-rels, set_rest_selec    */

Cost
compute_clause_selec (clause,or_selectivities)
     LispValue clause,or_selectivities ;
{
/*    if (!is_clause (clause)) {  NO!!! THIS IS A BUG.  -- JMH 3/9/92 */
      if (is_clause (clause)) {
	/* Boolean variables get a selectivity of 1/2. */
	return(0.5);
    } 
    else if (not_clause (clause)) {
	/* 'not' gets "1.0 - selectivity-of-inner-clause". */
	return (1.000000 - compute_selec (lispCons(get_notclausearg (clause),
						   LispNil),
					  or_selectivities));
    }
    else if (or_clause (clause)) {
	/* Both 'or' and 'and' clauses are evaluated as described in 
	 *    (compute_selec). 
	 */
	return (compute_selec (get_orclauseargs (clause),or_selectivities));
    } 
    else {
	return(compute_selec (lispCons(clause,LispNil),or_selectivities));
    } 
}

/*    
 *    	compute_selec
 *    
 *    	Computes the selectivity of a clause.
 *    
 *    	If there is more than one clause in the argument 'clauses', then the
 *    	desired selectivity is that of an 'or' clause.  Selectivities for an
 *    	'or' clause such as (OR a b) are computed by finding the selectivity
 *    	of a (s1) and b (s2) and computing s1+s2 - s1*s2.
 *    
 *    	In addition, if the clause is an 'or' clause, individual selectivities
 *    	may have already been assigned by indices to subclauses.  These values
 *    	are contained in the list 'or-selectivities'.
 *    
 *    	Returns the clause selectivity as a flonum.
 *    
 */

/*  .. compute_clause_selec, compute_selec   */

Cost
compute_selec(clauses, or_selectivities)
    LispValue clauses, or_selectivities;
{
    Cost s1 = 0;
    LispValue clause = CAR(clauses);

    if (null(clauses)) {
	s1 = 1.0;
    } else if (IsA(clause,Const)) {
	s1 = ((bool) get_constvalue((Const) clause)) ? 1.0 : 0.0;
    } else if (IsA(clause,Var)) {
	LispValue relid = translate_relid(CAR(get_varid((Var) clause)));

	/*
	 * we have a bool Var.  This is exactly equivalent to the clause:
	 *	reln.attribute = 't'
	 * so we compute the selectivity as if that is what we have. The
	 * magic #define constants are a hack.  I didn't want to have to
	 * do system cache look ups to find out all of that info.
	 */
	s1 = restriction_selectivity(EqualSelectivityProcedure,
				     BooleanEqualOperator,
				     CInteger(relid),
				     CInteger(CADR(get_varid((Var) clause))),
				     (Datum) 't',
				     _SELEC_CONSTANT_RIGHT_);
    } else if (or_selectivities) {
	/* If s1 has already been assigned by an index, use that value. */ 
	LispValue this_sel = CAR(or_selectivities);

	Assert(floatp(this_sel));
	s1 = CDouble(this_sel);
    } else if (IsA(CAR(clause),Func)) {
	/* this isn't an Oper, it's a Func!! */
	/*
	 ** This is not an operator, so we guess at the selectivity.  
	 ** THIS IS A HACK TO GET V4 OUT THE DOOR.  FUNCS SHOULD BE
	 ** ABLE TO HAVE SELECTIVITIES THEMSELVES.
	 **     -- JMH 7/9/92
	 */
	s1 = 0.1;
    } else if (NumRelids((Expr) clause) == 1) {
	/* ...otherwise, calculate s1 from 'clauses'. 
	 *    The clause is not a join clause, since there is 
	 *    only one relid in the clause.  The clause 
	 *    selectivity will be based on the operator 
	 *    selectivity and operand values. 
	 */
	LispValue relattvals = get_relattval(clause);
	ObjectId opno = get_opno((Oper) get_op(clause));
	RegProcedure oprrest = get_oprrest(opno);
	LispValue relid = translate_relid(CAR(relattvals));

	/* if the oprrest procedure is missing for whatever reason,
	   use a selectivity of 0.5*/
	if (!oprrest)
	  s1 = (Cost) (0.5);
	else
	  s1 = (Cost) restriction_selectivity(oprrest,
					      opno,
					      CInteger(relid),
					      CInteger(CADR(relattvals)),
					      (char *)
					      CInteger((CADDR(relattvals))),
					      CInteger(CADDR 
						       (CDR(relattvals))));
    } else {
	/*    The clause must be a join clause.  The clause 
	 *    selectivity will be based on the relations to be 
	 *    scanned and the attributes they are to be joined 
	 *    on. 
	 */
	LispValue relsatts = get_relsatts (clause);
	ObjectId opno = get_opno((Oper)get_op (clause));
	RegProcedure oprjoin = get_oprjoin (opno);
	LispValue relid1 = translate_relid(CAR(relsatts));
	LispValue relid2 = translate_relid(CADDR(relsatts));
	
	/* if the oprjoin procedure is missing for whatever reason,
	   use a selectivity of 0.5*/
	if (!oprjoin)
	  s1 = (Cost) (0.5);
	else
	  s1 = (Cost) join_selectivity(oprjoin,
				       opno,
				       CInteger(relid1),
				       CInteger(CADR(relsatts)),
				       CInteger(relid2),
				       CInteger(CADDR(CDR(relsatts))));
    }
    
    /*    A null clause list eliminates no tuples, so return a selectivity 
     *    of 1.0.  If there is only one clause, the selectivity is not 
     *    that of an 'or' clause, but rather that of the single clause.
     */
    
    if (length (clauses) < 2) {
	return(s1);
    } else {
	/* Compute selectivity of the 'or'ed subclauses. */
	/* Added check for taking CDR(LispNil).  -- JMH 3/9/92 */
	Cost s2;

	if (or_selectivities != LispNil)
	    s2 = compute_selec(CDR(clauses), CDR(or_selectivities));
	else
	    s2 = compute_selec(CDR(clauses), LispNil);
	return(s1 + s2 - s1 * s2);
    }
}

/*
 *	translate_relid
 *
 *	Translates a relid (range table index) into a pg_class oid
 *	using the information already stored in the range table.
 *
 *	RETURNS: a lispInteger storing the oid.
 */

/*  .. compute_selec */

LispValue
translate_relid(relid)
    LispValue relid;
{
    if (integerp(relid))
	return(getrelid(CInteger(relid), _query_range_table_));
    /*
     * XXX Since this is used as an oid search key, this should
     * probably be InvalidObjectId instead of _SELEC_VALUE_UNKNOWN_
     * (an internal planner code).
     */
    elog(NOTICE, "translate_relid: non-integer relid");
    return(lispInteger(_SELEC_VALUE_UNKNOWN_));
}
