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
#include "clausesel.h"
#include "relation.h"
#include "relation.a.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "parsetree.h"

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
     LispValue clausenode;
     foreach (clausenode,clauseinfo_list) {
	  if ( null (get_selectivity (clausenode)) ||
	      new_selectivity < get_selectivity (clausenode)) {
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
set_rest_relselec (rel_list)
     LispValue rel_list ;
{
     LispValue rel;
     foreach (rel,rel_list) {
	  set_rest_selec (get_clauseinfo (rel));
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
     LispValue clausenode;
     foreach (clausenode,clauseinfo_list) {
	  /*    Check to see if the selectivity of this clause or any 'or' */
	  /*    subclauses (if any) haven't been set yet. */
	  if ( valid_or_clause (clausenode) ||
	      null (get_selectivity (clausenode))) {
	       set_selectivity (clausenode,
				compute_clause_selec (get_clause (clausenode),
						      get_selectivity 
						      (clausenode)));
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
     if (!is_clause (clause)) {
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
     double s1;
     LispValue clause = CAR (clauses);
     if(null (clauses)) {
	  s1 = 1.0;
     } 
     else if 
       /* If s1 has already been assigned by an index, use that value. */
       (or_selectivities) {
	    s1 = (int) CAR (or_selectivities);
       } 
     else if (1 == NumRelids (clause)) {

    /* ...otherwise, calculate s1 from 'clauses'. 
     *    The clause is not a join clause, since there is 
     *    only one relid in the clause.  The clause 
     *    selectivity will be based on the operator 
     *    selectivity and operand values. 
     */

	  LispValue relattvals = get_relattval (clause);
	  ObjectId opno = get_opno (get_op (clause));
	  ObjectId relid ;

	  if (translate_relid (CAR(relattvals))) 
	    relid = translate_relid(CAR(relattvals));
	  else
	    relid = _SELEC_VALUE_UNKNOWN_;
       
	  s1 = restriction_selectivity (get_oprrest (opno),opno,relid,
					CADR (relattvals),
					CADDR (relattvals),
					CADDR (CDR(relattvals)));
     }
     else {
       
    /*    The clause must be a join clause.  The clause 
     *    selectivity will be based on the relations to be 
     *    scanned and the attributes they are to be joined 
     *    on. 
     */
	  LispValue relsatts = get_relsatts (clause);
	  ObjectId opno = get_opno (get_op (clause));
	  ObjectId relid1 ;
	  ObjectId relid2 ;

	  if(translate_relid (CAR(relsatts)))
	    relid1 = translate_relid(CAR(relsatts));
	  else
	    relid1 =  _SELEC_VALUE_UNKNOWN_;
	  if (translate_relid (CADDR (relsatts)))
	    relid2 = translate_relid(CADDR(relsatts));
	  else
	    relid2 =  _SELEC_VALUE_UNKNOWN_;
	  
	  s1 = join_selectivity (get_oprjoin (opno),opno,relid1,
				 CADR (relsatts),
				 relid2,
				 CADDR (CDR (relsatts)));
     }

  /*    A null clause list eliminates no tuples, so return a selectivity 
   *    of 1.0.  If there is only one clause, the selectivity is not 
   *    that of an 'or' clause, but rather that of the single clause.
   */

     if(length (clauses) < 2) {
	  return(s1);
     } 
     else {
	  /* Compute selectivity of the 'or'ed subclauses. */

	  double s2 = compute_selec (CDR (clauses),CDR (or_selectivities));
	  return (double)(s1 + s2 - s1*s2);
     }
} /* end compute_selec */

/*  .. compute_selec
 */
ObjectId
translate_relid (relid)
     ObjectId relid ;
{
     if ( integerp (relid) && relid > 0 ) {
	  return ((ObjectId)getrelid (relid,_query_range_table_));
     } 
     return((ObjectId)0);
}

