/*     
**      FILE
**     	xfunc
**     
**      DESCRIPTION
**     	Routines to handle expensive function optimization
**     
*/


/*     
**      EXPORTS
**              xfunc_clause_compare
**              xfunc_disjunct_sort
**              xfunc_trypullup
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
#include "planner/relnode.h"
#include "parser/parse.h"
#include "planner/keys.h"
#include "planner/tlist.h"
#include "lib/lispsort.h"
#include "lib/copyfuncs.h"
#include "access/heapam.h"

#define ever ; 1 ;

/*
** Comparison function for lispsort() on a list of CInfo's.
** arg1 and arg2 should really be of type (CInfo *)  
*/
int xfunc_cinfo_compare(arg1, arg2)
    void *arg1;
    void *arg2;
{
    CInfo info1 = *(CInfo *) arg1;
    CInfo info2 = *(CInfo *) arg2;

    LispValue clause1 = (LispValue) get_clause(info1),
              clause2 = (LispValue) get_clause(info2);
    
    return(xfunc_clause_compare((void *) &clause1, (void *) &clause2));
}

/*
** xfunc_clause_compare: comparison function for lispsort() that compares two 
** clauses based on expense/(1 - selectivity)
** arg1 and arg2 are really pointers to clauses.
*/
int xfunc_clause_compare(arg1, arg2)
    void *arg1;
    void *arg2;
{
    LispValue clause1 = *(LispValue *) arg1;
    LispValue clause2 = *(LispValue *) arg2;
    double measure1,  /* total cost of clause1 */ 
           measure2;  /* total cost of clause2 */
    int infty1 = 0, infty2 = 0;

    measure1 = xfunc_measure(clause1);
    if (measure1 == -1.0) infty1 = 1;
    measure2 = xfunc_measure(clause2);
    if (measure2 == -1.0) infty2 = 1;

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
** calculate expense/(1-selectivity).  Returns -1 if selectivity = 1 
** (note that expense >= 0, and selectivity <=1 by definition, so this will 
** never calculate a negative number except -1.)
*/
double xfunc_measure(clause)
     LispValue clause;
{
    double selec, denom;

    selec = compute_clause_selec(clause, LispNil);
    denom = 1.0 - selec;
    if (denom)
      return(xfunc_expense(clause) / denom);
    else return(-1.0);
}

/*
** Recursively find expense of a clause.  See comment below for calculating
** the expense of a function.
*/
double xfunc_expense(clause)
    LispValue clause;
{
    HeapTuple tupl; /* the pg_proc tuple for each function */
    Form_pg_proc proc; /* a data structure to hold the pg_proc tuple */
    int width = 0;  /* byte width of the field referenced by each clause */
    double cost = 0;
    LispValue tmpclause;

    if (IsA(clause,Const) || IsA(clause,Var)) /* base case */
      return(0);
    else if (fast_is_clause(clause)) /* cost is sum of operands' costs */
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

	 /* find width of operands */
	 for (tmpclause = CDR(clause); tmpclause != LispNil;
	      tmpclause = CDR(tmpclause))
	   width += xfunc_width(CAR(tmpclause));

	 return(cost +  
		proc->propercall_cpu + 
		proc->properbyte_cpu * proc->probyte_pct/100.00 * width
/*
**   The following terms removed until we can get better statistics
**
**		+ disk_fraction * proc->prodisk_pct/100.00 * width +
**		arch_fraction * proc->proarch_pct/100.00 * width
*/
		);
     }

    else if (fast_not_clause(clause)) /* cost is cost of operand */
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
** xfunc_width --
**    recursively find the width of a clause
*/

int xfunc_width(clause)
     LispValue clause;
{
    Relation relptr;   /* a structure to hold the open pg_attribute table */
    ObjectId reloid;   /* oid of pg_attribute */
    HeapTuple tupl;    /* structure to hold a cached tuple */
    Form_pg_proc proc; /* structure to hold the pg_proc tuple */
    int retval = 0;
    LispValue tmpclause;

    if (IsA(clause,Const))
     {
	 /* base case: width is the width of this constant */
	 retval = ((Const) clause)->constlen;
	 goto exit;
     }
    else if (IsA(clause,Var))
     {
	 /* base case: width is width of this attribute, as stored in
	    the pg_attribute table */
	 relptr = amopenr ((Name) "pg_attribute");
	 reloid = RelationGetRelationId ( relptr );
	 
	 tupl = SearchSysCacheTuple(ATTNUM, reloid, ((Var) clause)->varattno, 
				    NULL, NULL );
	 if (!HeapTupleIsValid(tupl)) {
	     elog(WARN, "getattnvals: no attribute tuple %d %d",
		  reloid, ((Var) clause)->varattno);
	     return(-1);
	 }
	 retval = (int)((AttributeTupleForm) GETSTRUCT(tupl))->attlen;
	 goto exit;
     }
    else if (fast_is_funcclause(clause))
     {
	 tupl = SearchSysCacheTuple(PROOID, 
				    get_funcid((Func)get_function(clause)),
				    NULL, NULL, NULL);
	 proc = (Form_pg_proc) GETSTRUCT(tupl);
	 /* find width of the function's arguments */
	 for (tmpclause = CDR(clause); tmpclause != LispNil; 
	      tmpclause = CDR(tmpclause))
	   retval += xfunc_width(CAR(tmpclause));
	 /* multiply by outin_ratio */
	 retval = proc->prooutin_ratio/100.0 * retval;
	 goto exit;
     }
    else
     {
	 elog(WARN, "Clause node of undetermined type");
	 return(-1);
     }
  exit:
    return(retval);
}


/*
** xfunc_trypullup --
**    check each path within a relation, and do all the pullups needed for it.
*/

void xfunc_trypullup(rel)
     Rel rel;
{
    LispValue y;            /* list ptr */
    CInfo maxcinfo;         /* The CInfo to pull up, as calculated by
			       xfunc_shouldpull() */
    JoinPath curpath;       /* current path in list */
    Rel outerrel, innerrel;
    LispValue form_relid();
    int clausetype;

    foreach(y, get_pathlist(rel))
     {
	 curpath = (JoinPath)CAR(y);
	 
	 /* find Rels for inner and outer operands of curpath */
	 innerrel = get_parent((Path) get_innerjoinpath(curpath));
	 outerrel = get_parent((Path) get_outerjoinpath(curpath));
	 
	 /*
	 ** for each operand, attempt to pullup predicates until first 
	 ** failure.
	 */
	 for(ever)
	  {
	      /* No, the following should NOT be '=='  !! */
	      if (clausetype = xfunc_shouldpull(get_innerjoinpath(curpath),
				   curpath, &maxcinfo))
		xfunc_pullup(get_innerjoinpath(curpath),
			     curpath, maxcinfo, INNER, clausetype);
	      else break;
	  }
	 for(ever)
	  {
	      /* No, the following should NOT be '=='  !! */
	      if (clausetype = xfunc_shouldpull(get_outerjoinpath(curpath), 
				   curpath, &maxcinfo))
		xfunc_pullup(get_outerjoinpath(curpath),
			     curpath, maxcinfo, OUTER, clausetype);
	      else break;
	  }
     }
}

/*
** xfunc_pullup --
**    move clause from child to parent. 
*/
void xfunc_pullup(childpath, parentpath, cinfo, whichchild, clausetype)
     Path childpath;
     JoinPath parentpath;
     CInfo cinfo;         /* clause to pull up */
     int whichchild;      /* whether child is INNER or OUTER of join */
     int clausetype;      /* whether clause to pull is join or local */
{
    /* remove clause from childpath */
    if (clausetype == XFUNC_LOCPRD)
     {
	 set_locclauseinfo(childpath, 
			   LispRemove(cinfo, get_locclauseinfo(childpath)));
     }
    else
     {
	 set_pathclauseinfo
	   ((JoinPath)childpath,
	    LispRemove(cinfo, get_pathclauseinfo((JoinPath)childpath)));
     }

    /* Fix all vars in the clause 
       to point to the right varno and varattno in parentpath */
    xfunc_fixvars(get_clause(cinfo), get_parent(childpath), whichchild);

    /* add clause to parentpath */
    set_locclauseinfo(parentpath, 
		      lispCons(cinfo, get_locclauseinfo(parentpath)));
}

/*
** xfunc_fixvars --
** After pulling up a clause, we must walk its expression tree, fixing Var 
** nodes to point to the right correct varno (either INNER or OUTER, depending
** on which child the clause was pulled from), and the right varattno in the 
** target list of the child's former relation.  If the target list of the
** child Rel does not contain the attribute we need, we add it.
*/
void xfunc_fixvars(clause, rel, varno)
     LispValue clause;  /* clause being pulled up */
     Rel rel;           /* rel it's being pulled from */
     int varno;         /* whether rel is INNER or OUTER of join */
{
    LispValue tmpclause;  /* temporary variable */
    TL member;            /* tlist member corresponding to var */

    if (IsA(clause,Const)) return;
    else if (IsA(clause,Var))
     {
	 /* here's the meat */
	 member = tlistentry_member((Var)clause, get_targetlist(rel));
	 if (member == LispNil)
	  {
	      /* 
	       ** The attribute we need is not in the target list,
	       ** so we have to add it.
	       **
	       ** Note: the last argument to add_tl_element() may be wrong,
	       ** but it doesn't seem like anybody uses this field
	       ** anyway.  It's definitely supposed to be a list of relids,
	       ** so I just figured I'd use the ones in the clause.
	       */
	      add_tl_element(rel, (Var)clause, clause_relids_vars(clause));
	      member = tlistentry_member((Var)clause, get_targetlist(rel));
	  }
	 ((Var)clause)->varno = varno;
	 ((Var)clause)->varattno = get_resno(get_resdom(get_entry(member)));
     }
    else if (fast_is_clause(clause))
     {
	 xfunc_fixvars(CAR(CDR(clause)), rel, varno);
	 xfunc_fixvars(CAR(CDR(CDR(clause))), rel, varno);
     }
    else if (fast_is_funcclause(clause))
      for (tmpclause = CDR(clause); tmpclause != LispNil; 
	   tmpclause = CDR(tmpclause))
	xfunc_fixvars(CAR(tmpclause), rel, varno);
    else if (fast_not_clause(clause))
      xfunc_fixvars(CDR(clause), rel, varno);
    else if (fast_or_clause(clause))
      for (tmpclause = CDR(clause); tmpclause != LispNil; 
	   tmpclause = CDR(tmpclause))
	xfunc_fixvars(CAR(tmpclause), rel, varno);
    else
     {
	 elog(WARN, "Clause node of undetermined type");
     }
}

/*
** xfunc_shouldpull --
**    find clause with most expensive measure, and decide whether to pull it up
** from child to parent.
**
** Returns:  0 if nothing left to pullup
**           XFUNC_LOCPRD if a local predicate is to be pulled up
**           XFUNC_JOINPRD if a secondary join predicate is to be pulled up
*/
int xfunc_shouldpull(childpath, parentpath, maxcinfopt)
     Path childpath;
     JoinPath parentpath;
     CInfo *maxcinfopt;  /* Out: pointer to clause to pullup */
{
    LispValue clauselist, tmplist;      /* lists of clauses */
    CInfo maxcinfo;                     /* clause to pullup */
    double tmpmeasure, maxmeasure = 0;  /* measures of clauses */
    double joinselec = 0;               /* selectivity of the join predicates */
    int retval = XFUNC_LOCPRD;

    clauselist = get_locclauseinfo(childpath);

    if (clauselist != LispNil)
     {
	 /* find clause in clause list with maximum measure */
	 for (tmplist = clauselist,
	      maxcinfo = (CInfo) CAR(tmplist),
	      maxmeasure = xfunc_measure(get_clause(maxcinfo));
	      tmplist != LispNil;
	      tmplist = CDR(tmplist))
	   if ((tmpmeasure = xfunc_measure(get_clause((CInfo) CAR(tmplist)))) 
	       > maxmeasure)
	    {
		maxcinfo = (CInfo) CAR(tmplist);
		maxmeasure = tmpmeasure;
	    }
     }

	 
    /* If child is a join path, and there are multiple join clauses,
       see if any join clause has even higher measure */
    if (length((get_parent(childpath))->relids) > 1
	&& length(get_pathclauseinfo((JoinPath)childpath)) > 1)
      for (tmplist = get_pathclauseinfo((JoinPath)childpath);
	   tmplist != LispNil;
	   tmplist = CDR(tmplist))
	if ((tmpmeasure = xfunc_measure(get_clause((CInfo) CAR(tmplist)))) 
	    > maxmeasure)
	 {
	     maxcinfo = (CInfo) CAR(tmplist);
	     maxmeasure = tmpmeasure;
	     retval = XFUNC_JOINPRD;
	 }

    if (maxmeasure == 0)  /* no expensive clauses */
      return(0);

    /*
    ** Pullup over join if clause is higher measure than join.
    ** Note that the cost of a secondary join clause is only what's
    ** calculated by xfunc_expense(), since the actual joining is paid for
    ** by the primary join clause
    */
    joinselec = compute_clause_selec(get_clause
				     ((CInfo) CAR
				      (get_pathclauseinfo(parentpath))), 
				     LispNil);
    if (joinselec != 1.0 &&
	xfunc_measure(get_clause(maxcinfo)) > 
	(get_path_cost(parentpath)/(1.0 - joinselec)))
     {
	 *maxcinfopt = maxcinfo;
	 return(retval);
     }

    else return(FALSE);

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
** disjuncts based on expense.
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

    measure1 = xfunc_expense(disjunct1);
    measure2 = xfunc_expense(disjunct2);
    
    if ( measure1 < measure2) 
      return(-1);
    else if (measure1 == measure2)
      return(0);
    else return(1);
}
