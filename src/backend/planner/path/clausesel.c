/* 
 * FILE
 * 	clausesel 
 * 
 * DESCRIPTION 
 * 	Routines to compute and set clause selectivities 
 * 
 */ 
 
/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		set_clause_selectivities
 *     		product_selec
 *     		set_rest_relselec
 *     		compute_clause_selec
 */

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
    double cost_clause;

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

double
product_selec (clauseinfo_list)
     LispValue clauseinfo_list ;
{
     double result = 1.0;
     if ( consp (clauseinfo_list) ) {
	  LispValue xclausenode = LispNil;
	  double temp;

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
    double cost_clause;
    
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

double
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

double
compute_selec (clauses,or_selectivities)
     LispValue clauses,or_selectivities ;
{
    double s1 = 0;
    LispValue clause = CAR (clauses);
    if(null (clauses)) {
	s1 = 1.0;
    } 
    else if 
      /* If s1 has already been assigned by an index, use that value. */
      (or_selectivities) {
	    s1 = (int) CAR (or_selectivities);
	} 
    else if (1 == NumRelids ((Expr)clause)) {

	/* ...otherwise, calculate s1 from 'clauses'. 
	 *    The clause is not a join clause, since there is 
	 *    only one relid in the clause.  The clause 
	 *    selectivity will be based on the operator 
	 *    selectivity and operand values. 
	 */

	LispValue relattvals = get_relattval (clause);
	ObjectId opno = get_opno ((Oper)get_op (clause));
	LispValue relid = LispNil;
	
	if (translate_relid (CAR(relattvals)))
	  relid = translate_relid(CAR(relattvals));
	else
	  relid = lispInteger(_SELEC_VALUE_UNKNOWN_);
	
	s1 = (double)restriction_selectivity (get_oprrest (opno),
					      opno,
					      CInteger(relid),
					      CInteger(CADR (relattvals)),
					      (char*)CInteger((CADDR(relattvals))),
					      CInteger(CADDR 
						       (CDR(relattvals))));
    }
     else {
	 
	 /*    The clause must be a join clause.  The clause 
	  *    selectivity will be based on the relations to be 
	  *    scanned and the attributes they are to be joined 
	  *    on. 
	  */
	 LispValue relsatts = get_relsatts (clause);
	 ObjectId opno = get_opno((Oper)get_op (clause));
	 RegProcedure oprjoin = get_oprjoin (opno);
	 LispValue relid1 = LispNil;
	 LispValue relid2 = LispNil;
	 
	 if(translate_relid (CAR(relsatts)))
	   relid1 = translate_relid(CAR(relsatts));
	 else
	   relid1 =  lispInteger(_SELEC_VALUE_UNKNOWN_);
	 if (translate_relid (CADDR (relsatts)))
	    relid2 = translate_relid(CADDR(relsatts));
	 else
	   relid2 =  lispInteger(_SELEC_VALUE_UNKNOWN_);

	 /*
	 ** If there's no operator associated with this function, we guess
	 ** at the selectivity.  THIS IS A HACK TO GET V4 OUT THE DOOR.
	 **     -- JMH 7/9/92
	 */
	 if (oprjoin == (RegProcedure) NULL) 
	   s1 = 0.5;
	 else /* There is an operator associated, so get its selectivity */
	   s1 = (double)join_selectivity (oprjoin,
					  opno,
					  CInteger(relid1),
					  CInteger(CADR (relsatts)),
					  CInteger(relid2),
					  CInteger(CADDR (CDR (relsatts))));
     }
    
    /*    A null clause list eliminates no tuples, so return a selectivity 
     *    of 1.0.  If there is only one clause, the selectivity is not 
     *    that of an 'or' clause, but rather that of the single clause.
     */
    
    if (length (clauses) < 2) {
	return(s1);
    } 
    else {
	/* Compute selectivity of the 'or'ed subclauses. */
	/* Added check for taking CDR(LispNil).  -- JMH 3/9/92 */
	  double s2;
	  if (or_selectivities != LispNil)
	    s2 = compute_selec (CDR (clauses),CDR (or_selectivities));
	  else
	    s2 = compute_selec (CDR (clauses), LispNil);
	  return (s1 + s2 - s1*s2);
      }
} /* end compute_selec */

/*  .. compute_selec */

LispValue
translate_relid(relid)
    LispValue relid;
{
    if (integerp(relid))
	return
	    getrelid(CInteger(relid),_query_range_table_);
    
    return
	lispInteger(0);
}

