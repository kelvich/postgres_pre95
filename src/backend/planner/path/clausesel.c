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

#include "internal.h"
#include "clause.h"
#include "pg_lisp.h"

extern void *set_rest_selec();
extern double compute_selec();
extern LispValue translate_relid();
extern double compute_clause_selec();

/* declare (localf (set_rest_selec,compute_selec)); */

/*     		----  ROUTINES TO SET CLAUSE SELECTIVITIES  ----
 */

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
*set_clause_selectivities (clauseinfo_list,new_selectivity)
LispValue clauseinfo_list,new_selectivity ;
{
  LispValue clausenode;
  for (clausenode = CAR(clauseinfo_list); clauseinfo_list != LispNil; 
       clauseinfo_list = CDR(clauseinfo_list)) {
    if ( or (LispNull (get_selectivity (clausenode)),
	     new_selectivity < get_selectivity (clausenode))) {
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

/*  .. compute-joinrel-size, compute-rel-size
 */
double
product_selec (clauseinfo_list)
     LispValue clauseinfo_list ;
{
  double result = 1;
  if ( consp (clauseinfo_list) ) {
    LispValue xclausenode = LispNil;
    double temp;

    foreach(xclausenode,clauseinfo_list) {
      temp = get_selectivity(xclausenode);
      result = result * temp;
    }
  }
  else {
    result = 1.0;
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
 *    
 */

/*  .. find-paths    */

void
*set_rest_relselec (rel_list)
     LispValue rel_list ;
{
  LispValue rel;
  for (rel = CAR(rel_list); rel_list != LispNil; 
       rel_list = CDR(rel_list)) {
    set_rest_selec (get_clause_info (rel));
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
*set_rest_selec (clauseinfo_list)
     LispValue clauseinfo_list ;
{
  LispValue clausenode;
  for (clausenode = CAR(clauseinfo_list); clauseinfo_list != LispNil;
       clauseinfo_list = CDR(clauseinfo_list) ) {
    
    /*    Check to see if the selectivity of this clause or any 'or' */
    /*    subclauses (if any) haven't been set yet. */
    if ( or (valid_or_clause (clausenode),
	     LispNull (get_selectivity (clausenode)))) {
      set_selectivity (clausenode,
		       compute_clause_selec (get_clause (clausenode),
					     get_selectivity (clausenode)));
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
  if (not (is_clause (clause))) {
        /* Boolean variables get a selectivity of 1/2. */
    return(0.5);
  } 
  else if (not_clause (clause)) {
    /* 'not' gets "1.0 - selectivity-of-inner-clause". */
    return (1.000000 - compute_selec (Lisplist (get_notclausearg (clause)),
				 or_selectivities));
  }
  else if (or_clause (clause)) {
    /* Both 'or' and 'and' clauses are evaluated as described in 
     *    (compute_selec). 
     */
    return (compute_selec (get_orclauseargs (clause),or_selectivities));
  } 
  else {
    return(compute_selec (list (clause),or_selectivities));
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
  LispValue s1;
  LispValue clause = CAR (clauses);
  if(null (clauses)) {
    s1 = 1.0;
  } 
  else if 
    /* If s1 has already been assigned by an index, use that value. */
    (or_selectivities) {
      s1 = CAR (or_selectivities);
    } 
  else if (1 == NumRelids (clause)) {

    /* ...otherwise, calculate s1 from 'clauses'. 
     *    The clause is not a join clause, since there is 
     *    only one relid in the clause.  The clause 
     *    selectivity will be based on the operator 
     *    selectivity and operand values. 
     */

      LispValue relattvals = get_relattval (clause);
      LispValue opno = get_opno (get_op (clause));
      LispValue relid = or (translate_relid (nth (0,relattvals)),
			    _SELEC_VALUE_UNKNOWN_);
      s1 = restriction_selectivity (get_oprrest (opno),opno,relid,
				    nth (1,relattvals),nth (2,relattvals),
				    nth (3,relattvals));
    }
  else {
    
    /*    The clause must be a join clause.  The clause 
     *    selectivity will be based on the relations to be 
     *    scanned and the attributes they are to be joined 
     *    on. 
     */
    LispValue relsatts = get_relsatts (clause);
    LispValue opno = get_opno (get_op (clause));
    LispValue relid1 = or (translate_relid (nth (0,relsatts)),
			   _SELEC_VALUE_UNKNOWN_);
    LispValue relid2 = or (translate_relid (nth (2,relsatts)),
			   _SELEC_VALUE_UNKNOWN_);
    s1 = join_selectivity (get_oprjoin (opno),opno,relid1,
			   nth (1,relsatts),relid2,nth (3,relsatts));
  }

  /*    A null clause list eliminates no tuples, so return a selectivity 
   *    of 1.0.  If there is only one clause, the selectivity is not 
   *    that of an 'or' clause, but rather that of the single clause.
   */

  if(length (clauses) < 2) {
    s1;
  } 
  else {
    /* Compute selectivity of the 'or'ed subclauses. */
      /* XXX - let form, maybe incorrect */
      LispValue s2 = compute_selec (CDR (clauses),CDR (or_selectivities));
      sub (add (s1,s2),mul (s1,s2));
    }
} /* end compute_selec */

/*  .. compute_selec
 */
LispValue
translate_relid (relid)
     LispValue relid ;
{
  if ( and (integerp (relid),relid > 0) ) {
    getrelid (relid,_query_range_table_);
  } 
}

