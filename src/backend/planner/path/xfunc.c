/*     
 *      FILE
 *     	xfunc
 *     
 *      DESCRIPTION
 *     	Routines to handle expensive function optimization
 *     
 */


/*     
 *      EXPORTS
 *              xfunc_rellist_sortprds
**              xfunc_cinfo_compare
**              xfunc_clause_compare
**              xfunc_disjunct_sort
 */

#include <strings.h>

#include "nodes/pg_lisp.h"
#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/relation.h"
#include "utils/log.h"
#include "catalog/pg_proc.h"
#include "catalog/syscache.h"
#include "planner/xfunc.h"
#include "planner/clausesel.h"
#include "parser/parse.h"
#include "lib/lispsort.h"

/*
** xfunc_rellist_sortprds
**    Given a list of Rel nodes, sort the clauseinfo for each
** according to cost/(1-selectivity)
**
**  modifies: clauselist for each Rel
*/
void xfunc_rellist_sortprds(rels)
    LispValue rels;
{
    LispValue temp;
    int i;

    for(temp = rels, i = 0; temp != LispNil; temp = CDR(temp), i++)
      set_clauseinfo(CAR(temp), lisp_qsort((LispValue) get_clauseinfo(CAR(temp)), 
					   xfunc_cinfo_compare));

    return;
}

/* arg1 and arg2 should really be of type (CInfo *)  */
int xfunc_cinfo_compare(arg1, arg2)
    void *arg1;
    void *arg2;
{
    CInfo info1 = *(CInfo *) arg1;
    CInfo info2 = *(CInfo *) arg2;
    int retval;
    LispValue clause1 = (LispValue) get_clause(info1),
              clause2 = (LispValue) get_clause(info2);
    
    return(xfunc_clause_compare((void *) &clause1, (void *) &clause2));
}

/*
** xfunc_clause_compare: comparison function for qsort() that compares two 
** clauses based on expense/(1 - selectivity)
** arg1 and arg2 are really pointers to clauses.
** !!! WILL THIS NEED TO DISTINGUISH BETWEEN LOCAL AND JOIN PRD's??
*/
int xfunc_clause_compare(arg1, arg2)
    void *arg1;
    void *arg2;
{
    LispValue clause1 = *(LispValue *) arg1;
    LispValue clause2 = *(LispValue *) arg2;
    double measure1,  /* total cost of clause1 */ 
           measure2;  /* total cost of clause2 */
    int i;
    double retval;
    double denom;
    int infty1 = 0, infty2 = 0;

    retval = compute_clause_selec(clause1, LispNil);
    denom = 1.0 - retval;
    if (denom)
      measure1 = xfunc_expense(clause1) / denom;
    else infty1 = 1;
    retval = compute_clause_selec(clause2, LispNil);
    denom = 1.0 - retval;
    if (denom)
      measure2 = xfunc_expense(clause2) / denom;
    else infty2 = 1;

    if (infty1 || infty2)
     {
	 if (!infty1) return(-1);
	 else if (infty1 && infty2) return(0);
	 else return(1);
     }
    
    if ( measure1 < measure2) 
      return(-1);
    else if (measure1 == measure2)
      return(0);
    else return(1);
}


/*
** Recursively find expense of a clause
*/
double xfunc_expense(clause)
    LispValue clause;
{
    HeapTuple tupl; /* the pg_proc tuple for each function */
    Form_pg_proc proc; /* a data structure to hold the pg_proc tuple */
    int width;  /* byte width of the field referenced by each clause */
    /* !!! THE FOLLOWING LINE SHOULD BE FIXED!! */
    double disk_fraction = DISK_FRACTION, 
           arch_fraction = ARCH_FRACTION; /* fraction of objects on disk
					    and in archive respectively*/
    double cost = 0;
    LispValue tmpclause;

    if (IsA(clause,Const) || IsA(clause,Var)) /* base case */
      return(0);
    else if (fast_is_clause(clause))
      return(xfunc_expense(CAR(CDR(clause))) + 
	     xfunc_expense(CAR(CDR(CDR(clause)))));
    else if (fast_is_funcclause(clause))
     {
	 /* find cost of evaluating the function's arguments */
	 for (tmpclause = CDR(clause); tmpclause != LispNil; 
	      tmpclause = CDR(tmpclause))
	   cost += xfunc_expense(CAR(tmpclause));

	 /*
	 ** expenses for uncomposed functions are calculated by the 
	 ** following formula:
	 **
	 ** const = num_instances * field_len;  we reduce this to relative
	 **          width
	 ** cost  = percall_cpu + 
	 **     perbyte_cpu * byte_pct(f)/100 * const (i.e. CPU time) +
	 **     disk_pct * disk_pct(f)/100 * const (i.e. expected disk I/Os) +
	 **     arch_pct * arch_pct(f)/100 * const
	 **     (i.e. expected archive I/Os);
	 **
	 ** see mike's paper, ~mike/postgres/papers/sigmod91
	 */

	 tupl = SearchSysCacheTuple(PROOID, 
				    get_funcid((Func)get_function(clause)),
				    NULL, NULL, NULL);
	 proc = (Form_pg_proc) GETSTRUCT(tupl);

	 /* !!! FIX THIS -- find width of tuple */
	 width = DEFAULT_WIDTH;

	 return(cost +  
		proc->propercall_cpu + 
		proc->properbyte_cpu * proc->probyte_pct/100.00 * width +
		disk_fraction * proc->prodisk_pct/100.00 * width +
		arch_fraction * proc->proarch_pct/100.00 * width);
     }

    else if (fast_not_clause(clause))
      return(xfunc_expense(CDR(clause)));

    else if (fast_or_clause(clause))
     {
	 /* find cost of evaluating each disjunct */
	 for (tmpclause = CDR(clause); tmpclause != LispNil; 
	      tmpclause = CDR(tmpclause))
	   cost += xfunc_expense(CAR(tmpclause));
	 return(cost);
     }
    else
     {
	 elog(WARN, "Clause node of undetermined type");
	 return(-1);
     }
}


/*
** xfunc_dopullup --
**    move a particular clause from child to parent
*/
int xfunc_dopullup(child, parent, clause)
    LispValue child;
    LispValue parent;
    LispValue clause;
{

}

/*
** xfunc_shouldpull --
**    decide whether to pull up a clause from parent to child.
*/
int xfunc_shouldpull(child, parent, clause)
    LispValue child;
    LispValue parent;
    LispValue clause;
{


}

/*
** xfunc_disjunct_sort --
**   given a list of clauses, for each clause sort the disjuncts by cost
**   (this assumes the predicates have been converted to Conjunctive NF)
**   Modifies the clause list!
*/
void xfunc_disjunct_sort(clause_list)
    LispValue clause_list;
{
    LispValue temp;

    foreach(temp, clause_list)
      if(or_clause(CAR(temp)))
	CDR(CAR(temp)) = lisp_qsort(CDR(CAR(temp)), xfunc_disjunct_compare);
}


/*
** xfunc_disjunct_compare: comparison function for qsort() that compares two 
** disjuncts based on expense
** arg1 and arg2 are really pointers to disjuncts
*/
int xfunc_disjunct_compare(arg1, arg2)
    void *arg1;
    void *arg2;
{
    LispValue disjunct1 = *(LispValue *) arg1;
    LispValue disjunct2 = *(LispValue *) arg2;
    double measure1,  /* total cost of disjunct1 */ 
           measure2;  /* total cost of disjunct2 */
    int i;

    measure1 = xfunc_expense(disjunct1);
    measure2 = xfunc_expense(disjunct2);
    
    if ( measure1 < measure2) 
      return(-1);
    else if (measure1 == measure2)
      return(0);
    else return(1);
}
