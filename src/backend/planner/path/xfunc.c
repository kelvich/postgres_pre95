/* $Header$ */

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
**              xfunc_trypullup
**              xfunc_get_path_cost
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
#include "utils/palloc.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/syscache.h"
#include "catalog/pg_language.h"
#include "planner/xfunc.h"
#include "planner/clausesel.h"
#include "planner/relnode.h"
#include "planner/internal.h"
#include "parser/parse.h"
#include "planner/keys.h"
#include "planner/tlist.h"
#include "lib/lispsort.h"
#include "lib/copyfuncs.h"
#include "access/heapam.h"
#include "tcop/dest.h"

#define ever ; 1 ;

/*
** xfunc_trypullup --
**    Main entry point to expensive function optimization.
** Given a relation, check each of its paths and see if it should 
** pullup clauses from its inner and outer.
*/

void xfunc_trypullup(rel)
     Rel rel;
{
    LispValue y;            /* list ptr */
    CInfo maxcinfo;         /* The CInfo to pull up, as calculated by
			       xfunc_shouldpull() */
    JoinPath curpath;       /* current path in list */
    LispValue form_relid();
    int clausetype;

    foreach(y, get_pathlist(rel))
     {
	 curpath = (JoinPath)CAR(y);
	 
	 /*
	 ** for each operand, attempt to pullup predicates until first 
	 ** failure.
	 */
	 for(ever)
	  {
	      /* No, the following should NOT be '=='  !! */
	      if (clausetype = 
		  xfunc_shouldpull((Path)get_innerjoinpath(curpath),
				   curpath, INNER, &maxcinfo))
		xfunc_pullup((Path)get_innerjoinpath(curpath),
			     curpath, maxcinfo, INNER, clausetype);
	      else break;
	  }
	 for(ever)
	  {
	      /* No, the following should NOT be '=='  !! */
	      if (clausetype = 
		  xfunc_shouldpull((Path)get_outerjoinpath(curpath), 
				   curpath, OUTER, &maxcinfo))
		xfunc_pullup((Path)get_outerjoinpath(curpath),
			     curpath, maxcinfo, OUTER, clausetype);
	      else break;
	  }
     }
}

/*
** xfunc_shouldpull --
**    find clause with most expensive measure, and decide whether to pull it up
** from child to parent.  Currently we only pullup secondary join clauses
** that are in the pathclauseinfo.  Secondary hash and sort clauses are
** left where they are.
**
** Returns:  0 if nothing left to pullup
**           XFUNC_LOCPRD if a local predicate is to be pulled up
**           XFUNC_JOINPRD if a secondary join predicate is to be pulled up
*/
int xfunc_shouldpull(childpath, parentpath, whichchild, maxcinfopt)
     Path childpath;
     JoinPath parentpath;
     int whichchild;     /* whether child is INNER or OUTER of join */
     CInfo *maxcinfopt;  /* Out: pointer to clause to pullup */
{
    LispValue clauselist, tmplist; /* lists of clauses */
    CInfo maxcinfo;		/* clause to pullup */
    LispValue primjoinclause	/* primary join clause */
      = xfunc_primary_join(parentpath);
    double tmpmeasure, maxmeasure = 0; /* measures of clauses */
    double joinselec = 0;	/* selectivity of the join predicate */
    double joincost = 0;	/* join cost + primjoinclause cost */
    int retval = XFUNC_LOCPRD;

    clauselist = get_locclauseinfo(childpath);

    if (clauselist != LispNil)
     {
	 /* find local predicate with maximum measure */
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

	 
    /* 
    ** If child is a join path, and there are multiple join clauses,
    ** see if any join clause has even higher measure than the highest
    ** local predicate 
    */
    if (length((get_parent(childpath))->relids) > 1
	&& xfunc_num_join_clauses((JoinPath)childpath) > 1)
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

    if (maxmeasure == 0)	/* no expensive clauses */
      return(0);

    /*
    ** Pullup over join if clause is higher measure than join.
    ** Note that the cost of a secondary join clause is only what's
    ** calculated by xfunc_expense(), since the actual joining 
    ** (i.e. the usual path_cost) is paid for by the primary join clause.
    ** The cost of the join clause is the cost of the primary join clause
    ** plus the cost_per_tuple of whichchild for the join method.
    */
    if (primjoinclause != LispNil)
     {
	 joinselec = compute_clause_selec(primjoinclause, LispNil);
	 joincost = xfunc_cost_per_tuple(parentpath, whichchild) 
	   + xfunc_expense(primjoinclause);
	 if (joinselec != 1.0 &&
	     xfunc_measure(get_clause(maxcinfo)) > 
	     (joincost / (1.0 - joinselec)))
	  {
	      *maxcinfopt = maxcinfo;
	      return(retval);
	  }
	 else			/* drop through */;
     }

    return(0);

}


/*
** xfunc_pullup --
**    move clause from child pathnode to parent pathnode.   This operation 
** makes the child pathnode produce a larger relation than it used to.
** This means that we must construct a new Rel just for the childpath,
** although this Rel will not be added to the list of Rels to be joined up
** in the query; it's merely a parent for the new childpath.
**    We also have to fix up the path costs of the child and parent.
*/
void xfunc_pullup(childpath, parentpath, cinfo, whichchild, clausetype)
     Path childpath;
     JoinPath parentpath;
     CInfo cinfo;         /* clause to pull up */
     int whichchild;      /* whether child is INNER or OUTER of join */
     int clausetype;      /* whether clause to pull is join or local */
{
    Path newkid;
    Rel newrel;
    double pulled_selec;

    /* remove clause from childpath */
    if (clausetype == XFUNC_LOCPRD)
     {
	 newkid = (Path)CopyObject(childpath);
	 set_locclauseinfo(newkid, 
			   xfunc_LispRemove((LispValue)cinfo, 
					    (List)get_locclauseinfo(newkid)));
     }
    else
     {
	 newkid = (Path)CopyObject(childpath);
	 set_pathclauseinfo
	   ((JoinPath)newkid,
	    xfunc_LispRemove((LispValue)cinfo, 
			     (List)get_pathclauseinfo((JoinPath)newkid)));
     }

    /*
    ** give the new child path its own Rel node that reflects the
    ** lack of the pulled-up predicate
    */
    pulled_selec = compute_clause_selec(get_clause(cinfo), LispNil);
    xfunc_copyrel(get_parent(newkid), &newrel);
    set_parent(newkid, newrel);
    set_pathlist(newrel, lispCons((LispValue)newkid, LispNil));
    set_unorderedpath(newrel, (PathPtr)newkid);
    set_cheapestpath(newrel, (PathPtr)newkid);
    set_tuples(newrel, get_tuples(get_parent(childpath)) / pulled_selec);
    set_pages(newrel, get_pages(get_parent(childpath)) / pulled_selec);
    
    /* 
    ** fix up path cost of newkid.  To do this we subtract away all the
    ** xfunc_costs of childpath, then recompute the xfunc_costs of newkid
    */
    set_path_cost(newkid, get_path_cost(newkid) 
		  - xfunc_get_path_cost(childpath));
    set_path_cost(newkid, get_path_cost(newkid)
		  + xfunc_get_path_cost(newkid));

    /* 
    ** Fix all vars in the clause 
    ** to point to the right varno and varattno in parentpath 
    */
    xfunc_fixvars(get_clause(cinfo), newrel, whichchild);

    /*  add clause to parentpath, and fix up its cost. */
    set_locclauseinfo(parentpath, 
		      lispCons((LispValue)cinfo, 
			       (LispValue)get_locclauseinfo(parentpath)));
    /* put new childpath into the path tree */
    if (whichchild == INNER)
     {
	 set_innerjoinpath(parentpath, (pathPtr)newkid);
     }
    else
     {
	 set_outerjoinpath(parentpath, (pathPtr)newkid);
     }

    /* 
    ** recompute parentpath cost from scratch -- the cost
    ** of the join method has changed
    */
    set_path_cost(parentpath, xfunc_total_path_cost(parentpath));
}

/*
** calculate expense/(1-selectivity).  Returns -1 if selectivity = 1 
** (note that expense >= 0, and selectivity <=1 by definition, so this will 
** never calculate a negative number except -1.)
*/
double xfunc_measure(clause)
     LispValue clause;
{
    double selec;  /* selectivity of clause */
    double denom;  /* denominator of expression (i.e. 1 - selec) */

    selec = compute_clause_selec(clause, LispNil);
    denom = 1.0 - selec;
    if (denom)  /* be careful not to divide by zero! */
      return(xfunc_expense(clause) / denom);
    else return(-1.0);
}

/*
** Recursively find the per-tuple expense of a clause.  See
** xfunc_func_expense for more discussion.
*/
double xfunc_expense(clause)
    LispValue clause;
{
    double cost = 0;   /* running expense */
    LispValue tmpclause;

    /* First handle the base case */
    if (IsA(clause,Const) || IsA(clause,Var) || IsA(clause,Param)) 
      return(0);
    /* now other stuff */
    else if (IsA(clause,Iter))
      /* Too low. Should multiply by the expected number of iterations. */
      return(xfunc_expense(get_iterexpr((Iter)clause)));
    else if (IsA(clause,ArrayRef))
      return(xfunc_expense(get_refexpr((ArrayRef)clause)));
    else if (fast_is_clause(clause)) 
      return(xfunc_func_expense((LispValue)get_op(clause), 
				(LispValue)get_opargs(clause)));
    else if (fast_is_funcclause(clause))
      return(xfunc_func_expense((LispValue)get_function(clause),
				(LispValue)get_funcargs(clause)));
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
** xfunc_func_expense --
**    given a Func or Oper and its args, find its expense.
** Note: in Stonebraker's SIGMOD '91 paper, he uses a more complicated metric
** than the one here.  We can ignore the expected number of tuples for
** our calculations; we just need the per-tuple expense.  But he also
** proposes components to take into account the costs of accessing disk and
** archive.  We didn't adopt that scheme here; eventually the vacuum
** cleaner should be able to tell us what percentage of bytes to find on
** which storage level, and that should be multiplied in appropriately
** in the cost function below.  Right now we don't model the cost of
** accessing secondary or tertiary storage, since we don't have sufficient
** stats to do it right.
*/
double xfunc_func_expense(node, args)
LispValue node;
LispValue args;
{
    HeapTuple tupl;    /* the pg_proc tuple for each function */
    Form_pg_proc proc; /* a data structure to hold the pg_proc tuple */
    int width = 0;     /* byte width of the field referenced by each clause */
    regproc funcid;    /* ID of function associate with node */
    double cost = 0;   /* running expense */
    LispValue tmpclause;

    if (IsA(node,Oper))
     {
	 /* don't trust the opid in the Oper node.  Use the opno. */
	 if (!(funcid = get_opcode(get_opno((Oper)node))))
	   elog(WARN, "Oper's function is undefined");
     }
    else funcid = get_funcid((Func)node);

    /* look up tuple in cache */
    tupl = SearchSysCacheTuple(PROOID, funcid, NULL, NULL, NULL);
    if (!HeapTupleIsValid(tupl))
      elog(WARN, "Cache lookup failed for procedure %d", funcid);
    proc = (Form_pg_proc) GETSTRUCT(tupl);

    /* 
    ** if it's a Postquel function, its cost is stored in the
    ** associated plan.
    */
    if (proc->prolang == POSTQUELlanguageId)
     {
	 LispValue tmpplan;
	 List planlist;
	 
	 if (IsA(node,Oper) || get_func_planlist((Func)node) == LispNil)
	  {
	      ObjectId *argOidVect; /* vector of argtypes */
	      char *pq_src;	/* text of PQ function */
	      int nargs;	/* num args to PQ function */
	      LispValue parseTree_list;	/* dummy variable */
	      ObjectId *funcname_get_funcargtypes();

	      /* 
	      ** plan the function, storing it in the Func node for later 
	      ** use by the executor.  
	      */
	      pq_src = (char *) textout(&(proc->prosrc));
	      nargs = proc->pronargs;
	      if (nargs > 0)
		argOidVect = funcname_get_funcargtypes(&proc->proname.data[0]);
	      planlist = (List)pg_plan(pq_src, argOidVect, nargs, 
				       &parseTree_list, None);
	      if (IsA(node,Func))
		set_func_planlist((Func)node, planlist);
	  }
	 else /* plan has been cached inside the Func node already */
	   planlist = get_func_planlist((Func)node);

	 /*
	 ** Return the sum of the costs of the plans (the PQ function
	 ** may have many queries in its body).
	 */
	 foreach(tmpplan, planlist)
	   cost += get_cost((Plan)CAR(tmpplan));
	 return(cost);
     }
    else			/* it's a C function */
     {
	 /* find cost of evaluating the function's arguments */
	 for (tmpclause = args; tmpclause != LispNil; 
	      tmpclause = CDR(tmpclause))
	   cost += xfunc_expense(CAR(tmpclause));

	 /* find width of operands */
	 for (tmpclause = args; tmpclause != LispNil;
	      tmpclause = CDR(tmpclause))
	   width += xfunc_width(CAR(tmpclause));

	 /* 
	 ** when stats become available, add in cost of accessing secondary
	 ** and tertiary storage here.
	 */
	 return(cost +  
		proc->propercall_cpu + 
		proc->properbyte_cpu * proc->probyte_pct/100.00 * width
	     /* 
                   * Pct_of_obj_in_mem
		DISK_COST * proc->probyte_pct/100.00 * width
		   * Pct_of_obj_on_disk +
		ARCH_COST * proc->probyte_pct/100.00 * width
		   * Pct_of_obj_on_arch 
	     */
		);
     }
}

/* 
** xfunc_width --
**    recursively find the width of a expression
*/

int xfunc_width(clause)
     LispValue clause;
{
    Relation relptr;     /* a structure to hold the open pg_attribute table */
    ObjectId reloid;     /* oid of pg_attribute */
    int vnum;            /* range table entry for rel in a var */
    Name relname;        /* name of a rel */
    Relation rd;         /* Relation Descriptor */
    HeapTuple tupl;      /* structure to hold a cached tuple */
    int retval = 0;

    if (IsA(clause,Const))
     {
	 /* base case: width is the width of this constant */
	 retval = get_constlen((Const) clause);
	 goto exit;
     }
    else if (IsA(clause,ArrayRef))
     {
	 /* base case: width is width of the refelem within the array */
	 retval = get_refelemlength((ArrayRef)clause);
	 goto exit;
     }
    else if (IsA(clause,Var))  /* base case: width is width of this attribute */
     {
	 if (get_varattno((Var)clause) == 0)
	  {
	      /* clause is a tuple.  Get its width */
	      vnum = CInteger(CAR(get_varid((Var)clause)));
	      relname = (Name)planner_VarnoGetRelname(vnum);
	      rd = heap_openr(relname);
	      retval = xfunc_tuple_width(rd);
	      heap_close(rd);
	  }
	 else
	  {
	      /* attribute is a base type */
	      relptr = amopenr ((Name) "pg_attribute");
	      reloid = RelationGetRelationId ( relptr );
	 
	      tupl = SearchSysCacheTuple(ATTNUM, reloid, 
					 get_varattno((Var)clause),
					 NULL, NULL );
	      if (!HeapTupleIsValid(tupl)) {
		  elog(WARN, "xfunc_width: class %d has no attribute %d",
		       reloid, get_varattno((Var)clause));
		  return(-1);
	      }
	      retval = (int)((AttributeTupleForm) GETSTRUCT(tupl))->attlen;
	      amclose(relptr);
	  }
	 goto exit;
     }
    else if (IsA(clause,Param))
     {
	 if (typeid_get_relid(get_paramtype((Param)clause)))
	  {
	      /* Param node returns a tuple.  Find its width */
	      rd = heap_open(typeid_get_relid(get_paramtype((Param)clause)));
	      retval = xfunc_tuple_width(rd);
	      heap_close(rd);
	  }
	 else if (get_param_tlist((Param)clause) != LispNil)
	  {
	      /* Param node projects a complex type */
	      Assert(length(get_param_tlist((Param)clause)) == 1); /* sanity */
	      retval = 
		xfunc_width((LispValue)
			    get_expr((TLE)CAR(get_param_tlist((Param)clause))));
	  }
	 else
	  {
	      /* Param node returns a base type */
	      retval = tlen(get_id_type(get_paramtype((Param)clause)));
	  }
	 goto exit;	 
     }
    else if (IsA(clause,Iter))
     {
	 /* 
         ** An Iter returns a setof things, so return the width of a single
	 ** thing.
	 ** Note:  THIS MAY NOT WORK RIGHT WHEN AGGS GET FIXED,
	 ** SINCE AGG FUNCTIONS CHEW ON THE WHOLE SETOF THINGS!!!!
	 */
	 retval = xfunc_width(get_iterexpr((Iter)clause));
	 goto exit;
     }
    else if (fast_is_clause(clause))
     {
	 /*
	 ** get function associated with this Oper, and treat this as 
	 ** a Func 
	 */
	 tupl = SearchSysCacheTuple(OPROID, get_opno((Oper)get_op(clause)),
				    NULL, NULL, NULL);
	 if (!HeapTupleIsValid(tupl))
	   elog(WARN, "Cache lookup failed for procedure %d", 
		get_opno((Oper)get_op(clause)));
	 return(xfunc_func_width
		((regproc)(((Form_pg_operator)(GETSTRUCT(tupl)))->oprcode), 
		 (LispValue)get_opargs(clause)));
     }
    else if (fast_is_funcclause(clause))
     {
	 Func func = (Func)get_function(clause);
	 if (get_func_tlist(func) != LispNil)
	  {
	      /* this function has a projection on it.  Get the length
		 of the projected attribute */
	      Assert(length(get_func_tlist(func)) == 1);   /* sanity */
	      retval = 
		xfunc_width((LispValue)
			    get_expr((TLE)CAR(get_func_tlist(func))));
	      goto exit;
	  }
	 else
	  {
	      return(xfunc_func_width((regproc)get_funcid(func),
				      (LispValue)get_funcargs(clause)));
	  }
     }
    else
     {
	 elog(WARN, "Clause node of undetermined type");
	 return(-1);
     }

  exit:
    if (retval == -1)
      retval = VARLEN_DEFAULT;
    return(retval);
}


/*
** xfunc_primary_join:
**   Find the primary join clause: for Hash and Merge Joins, this is the
** min measure Hash or Merge clause, while for Nested Loop it's the
** min measure pathclause
*/
LispValue xfunc_primary_join(pathnode)
     JoinPath pathnode;
{
    LispValue joinclauselist = get_pathclauseinfo(pathnode);
    CInfo mincinfo;
    LispValue tmplist;
    LispValue minclause;
    double minmeasure, tmpmeasure;

    if (IsA(pathnode,MergePath)) 
     {
	 for(tmplist = get_path_mergeclauses((MergePath)pathnode),
	     minclause = CAR(tmplist),
	     minmeasure = xfunc_measure(minclause);
	     tmplist != LispNil;
	     tmplist = CDR(tmplist))
	   if ((tmpmeasure = xfunc_measure(CAR(tmplist)))
	       < minmeasure)
	    {
		minmeasure = tmpmeasure;
		minclause = CAR(tmplist);
	    }
	 return(minclause);
     }
    else if (IsA(pathnode,HashPath)) 
     {
	 for(tmplist = get_path_hashclauses((HashPath)pathnode),
	     minclause = CAR(tmplist),
	     minmeasure = xfunc_measure(minclause);
	     tmplist != LispNil;
	     tmplist = CDR(tmplist))
	   if ((tmpmeasure = xfunc_measure(CAR(tmplist)))
	       < minmeasure)
	    {
		minmeasure = tmpmeasure;
		minclause = CAR(tmplist);
	    }
	 return(minclause);
     }

    /* if we drop through, it's nested loop join */
    if (joinclauselist == LispNil)
      return(LispNil);

    for(tmplist = joinclauselist, mincinfo = (CInfo) CAR(joinclauselist),
	minmeasure = xfunc_measure(get_clause((CInfo) CAR(tmplist)));
	tmplist != LispNil;
	tmplist = CDR(tmplist))
      if ((tmpmeasure = xfunc_measure(get_clause((CInfo) CAR(tmplist))))
	  < minmeasure)
       {
	   minmeasure = tmpmeasure;
	   mincinfo = (CInfo) CAR(tmplist);
       }
    return((LispValue)get_clause(mincinfo));
}

/*
** xfunc_get_path_cost
**   get the expensive function costs of the path
*/
int xfunc_get_path_cost(pathnode)
Path pathnode;
{
    int cost = 0;
    LispValue tmplist;
    double selec = 1.0;
    
    /* 
    ** first add in the expensive local function costs.
    ** We ensure that the clauses are sorted by measure, so that we
    ** know (via selectivities) the number of tuples that will be checked
    ** by each function.
    */
    set_locclauseinfo(pathnode, lisp_qsort(get_locclauseinfo(pathnode),
					   xfunc_cinfo_compare));
    for(tmplist = get_locclauseinfo(pathnode), selec = 1.0;
	tmplist != LispNil;
	tmplist = CDR(tmplist))
     {
	 cost += xfunc_expense(get_clause((CInfo)CAR(tmplist)))
	         * get_tuples(get_parent(pathnode)) * selec;
	 selec *= compute_clause_selec(get_clause((CInfo)CAR(tmplist)), 
				       LispNil);
     }

    /* 
    ** Now add in any node-specific expensive function costs.
    ** Again, we must ensure that the clauses are sorted by measure.
    */
    if (IsA(pathnode,JoinPath))
     {
	 set_pathclauseinfo((JoinPath)pathnode, 
			    lisp_qsort(get_pathclauseinfo((JoinPath)pathnode),
				       xfunc_cinfo_compare));
	 for(tmplist = get_pathclauseinfo((JoinPath)pathnode), selec = 1.0;
	     tmplist != LispNil;
	     tmplist = CDR(tmplist))
	  {
	      cost += xfunc_expense(get_clause((CInfo)CAR(tmplist)))
		      * get_tuples(get_parent(pathnode)) * selec;
	      selec *= compute_clause_selec(get_clause((CInfo)CAR(tmplist)),
					    LispNil);
	  }
     }
    if (IsA(pathnode,HashPath))
     {
	 set_path_hashclauses
	   ((HashPath)pathnode, 
	    lisp_qsort(get_path_hashclauses((HashPath)pathnode),
		       xfunc_clause_compare));
	 for(tmplist = get_path_hashclauses((HashPath)pathnode), selec = 1.0;
	     tmplist != LispNil;
	     tmplist = CDR(tmplist))
	  {
	      cost += xfunc_expense(CAR(tmplist))
		      * get_tuples(get_parent(pathnode)) * selec;
	      selec *= compute_clause_selec(CAR(tmplist), LispNil);
	  }
     }
    if (IsA(pathnode,MergePath))
     {
	 set_path_mergeclauses
	   ((MergePath)pathnode, 
	    lisp_qsort(get_path_mergeclauses((MergePath)pathnode),
		       xfunc_clause_compare));
	 for(tmplist = get_path_mergeclauses((MergePath)pathnode), selec = 1.0;
	     tmplist != LispNil;
	     tmplist = CDR(tmplist))
	  {
	      cost += xfunc_expense(CAR(tmplist))
		      * get_tuples(get_parent(pathnode)) * selec;
	      selec *= compute_clause_selec(CAR(tmplist), LispNil);
	  }
     }
    return(cost);
}

/*
** Recalculate the cost of a path node.  This includes the basic cost of the 
** node, as well as the cost of its expensive functions.
** We need to do this to the parent after pulling a clause from a child into a
** parent.  Thus we should only be calling this function on JoinPaths.
*/
int xfunc_total_path_cost(pathnode)
JoinPath pathnode;
{
    int cost = xfunc_get_path_cost((Path)pathnode);

    Assert(IsA(pathnode,JoinPath));
    if (IsA(pathnode,MergePath))
     {
	 MergePath mrgnode = (MergePath)pathnode;
	 return(cost + 
		cost_mergesort(get_path_cost((Path)get_outerjoinpath(mrgnode)),
			       get_path_cost((Path)get_innerjoinpath(mrgnode)),
			       get_outersortkeys(mrgnode),
			       get_innersortkeys(mrgnode),
			       get_tuples(get_parent((Path)get_outerjoinpath
						     (mrgnode))),
			       get_tuples(get_parent((Path)get_innerjoinpath
						     (mrgnode))),
			       get_width(get_parent((Path)get_outerjoinpath
						    (mrgnode))),
			       get_width(get_parent((Path)get_innerjoinpath
						    (mrgnode)))));
     }
    else if (IsA(pathnode,HashPath))
     {
	 HashPath hashnode = (HashPath)pathnode;
	 return(cost +
		cost_hashjoin(get_path_cost((Path)get_outerjoinpath(hashnode)),
			      get_path_cost((Path)get_innerjoinpath(hashnode)),
			      get_outerhashkeys(hashnode),
			      get_innerhashkeys(hashnode),
			      get_tuples(get_parent((Path)get_outerjoinpath
						    (hashnode))),
			      get_tuples(get_parent((Path)get_innerjoinpath
						    (hashnode))),
			      get_width(get_parent((Path)get_outerjoinpath
						   (hashnode))),
			      get_width(get_parent((Path)get_innerjoinpath
						   (hashnode)))));
     }
    else /* Nested Loop Join */
      return(cost +
	     cost_nestloop(get_path_cost((Path)get_outerjoinpath(pathnode)),
			   get_path_cost((Path)get_innerjoinpath(pathnode)),
			   get_tuples(get_parent((Path)get_outerjoinpath
						 (pathnode))),
			   get_tuples(get_parent((Path)get_innerjoinpath
						 (pathnode))),
			   get_pages(get_parent((Path)get_outerjoinpath
						(pathnode))),
			   IsA(get_innerjoinpath(pathnode),IndexPath)));
}


/*
** xfunc_cost_per_tuple --
**    return the expense of the join *per-tuple* of the input relation.
** This will be different for tuples of INNER and OUTER
*/
double xfunc_cost_per_tuple(joinnode, whichrel)
JoinPath joinnode;
int whichrel;       /* INNER or OUTER of joinnode */
{
    int outersize = get_tuples(get_parent((Path)get_outerjoinpath(joinnode)));
    int innersize = get_tuples(get_parent((Path)get_innerjoinpath(joinnode)));

    /* for merge join, all you do to each tuple is the minimal CPU processing */
    if (IsA(joinnode,MergePath))
     {
	 return(_CPU_PAGE_WEIGHT_);
     }
    /* 
    ** For hash join, figure out the number of chunks of the outer we process
    ** at a time.  The cost for a tuple of the outer is the CPU cost
    ** times the size of the inner relation, and for a tuple of the inner
    ** it's the CPU cost times the number of chunks of the outer 
    */
    else if (IsA(joinnode,HashPath))
     {
	 int outerpages = 
	   get_pages(get_parent((Path)get_outerjoinpath(joinnode)));

	 int nrun = ceil((double)outerpages/(double)NBuffers);
	 if (whichrel == INNER)
	   return(_CPU_PAGE_WEIGHT_ * nrun);
	 else
	   return(_CPU_PAGE_WEIGHT_ * innersize);
     }
    /*
    ** For nested loop, the cost for tuples is the size of the outer relation
    ** times the cost of the inner relation divided by the number of tuples
    ** in whichrel.
    */
    else /* nested loop join */
      if (whichrel == INNER)
	return(outersize * get_path_cost((Path)get_innerjoinpath(joinnode)) /
	       innersize);
      else
	return(get_path_cost((Path)get_innerjoinpath(joinnode)));
}

/*
** xfunc_fixvars --
** After pulling up a clause, we must walk its expression tree, fixing Var 
** nodes to point to the correct varno (either INNER or OUTER, depending
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
    TLE tle;              /* tlist member corresponding to var */

    if (IsA(clause,Const) || IsA(clause,Param)) return;
    else if (IsA(clause,Var))
     {
	 /* here's the meat */
	 tle = tlistentry_member((Var)clause, get_targetlist(rel));
	 if (tle == LispNil)
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
	      tle = tlistentry_member((Var)clause, get_targetlist(rel));
	  }
	 ((Var)clause)->varno = varno;
	 ((Var)clause)->varattno = get_resno(get_resdom(get_entry(tle)));
     }
    else if (IsA(clause,Iter))
      xfunc_fixvars(get_iterexpr((Iter)clause), rel, varno);
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
** Comparison function for lisp_qsort() on a list of CInfo's.
** arg1 and arg2 should really be of type (CInfo *).  
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
** xfunc_clause_compare: comparison function for lisp_qsort() that compares two 
** clauses based on expense/(1 - selectivity)
** arg1 and arg2 are really pointers to clauses.
*/
int xfunc_clause_compare(arg1, arg2)
    void *arg1;
    void *arg2;
{
    LispValue clause1 = *(LispValue *) arg1;
    LispValue clause2 = *(LispValue *) arg2;
    double measure1,             /* total xfunc measure of clause1 */ 
           measure2;             /* total xfunc measure of clause2 */
    int infty1 = 0, infty2 = 0;  /* divide by zero is like infinity */

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

/* ------------------------ UTILITY FUNCTIONS ------------------------------- */
/*
** xfunc_func_width --
**    Given a function OID and operands, find the width of the return value.
*/
int xfunc_func_width(funcid, args)
regproc funcid;
LispValue args;
{
    Relation rd;         /* Relation Descriptor */
    HeapTuple tupl;      /* structure to hold a cached tuple */
    Form_pg_proc proc;   /* structure to hold the pg_proc tuple */
    Form_pg_type type;   /* structure to hold the pg_type tuple */
    LispValue tmpclause; 
    int retval;

    /* lookup function and find its return type */
    Assert(RegProcedureIsValid(funcid));
    tupl = SearchSysCacheTuple(PROOID, funcid, NULL, NULL, NULL);
    if (!HeapTupleIsValid(tupl))
      elog(WARN, "Cache lookup failed for procedure %d", funcid);
    proc = (Form_pg_proc) GETSTRUCT(tupl);
    
    /* if function returns a tuple, get the width of that */
    if (typeid_get_relid(proc->prorettype))
     {
	 rd = heap_open(typeid_get_relid(proc->prorettype));
	 retval = xfunc_tuple_width(rd);
	 heap_close(rd);
	 goto exit;
     }
    else /* function returns a base type */
     {
	 tupl = SearchSysCacheTuple(TYPOID,
				    proc->prorettype,
				    NULL, NULL, NULL);
	 if (!HeapTupleIsValid(tupl))
	   elog(WARN, "Cache lookup failed for type %d", proc->prorettype);
	 type = (Form_pg_type) GETSTRUCT(tupl);
	 /* if the type length is known, return that */
	 if (type->typlen != -1)
	  {
	      retval = type->typlen;
	      goto exit;
	  }
	 else /* estimate the return size */
	  {
	      /* find width of the function's arguments */
	      for (tmpclause = args; tmpclause != LispNil; 
		   tmpclause = CDR(tmpclause))
		retval += xfunc_width(CAR(tmpclause));
	      /* multiply by outin_ratio */
	      retval = proc->prooutin_ratio/100.0 * retval;
	      goto exit;
	  }
     }
  exit:
    return(retval);
}

/*
** xfunc_tuple_width --
**     Return the sum of the lengths of all the attributes of a given relation
*/
int xfunc_tuple_width(rd)
     Relation rd;
{
    int i;
    int retval = 0;
    TupleDescriptor tdesc = RelationGetTupleDescriptor(rd);
    Assert(TupleDescIsValid(tdesc));

    for (i = 0; i < RelationGetNumberOfAttributes(rd); i++)
     {
	 if (tdesc->data[i]->attlen != -1)
	   retval += tdesc->data[i]->attlen;
	 else retval += VARLEN_DEFAULT;
     }

    return(retval);
}

/*
** xfunc_num_join_clauses --
**   Find the number of join clauses associated with this join path
*/
int xfunc_num_join_clauses(path)
JoinPath path;
{
    int num = length(get_pathclauseinfo(path));
    if (IsA(path,MergePath))
      return(num + length(get_path_mergeclauses((MergePath)path)));
    else if (IsA(path,HashPath))
      return(num + length(get_path_hashclauses((HashPath)path)));
    else return(num);
}

/*
** xfunc_LispRemove --
**   Just like LispRemove, but it whines if the item to be removed ain't there
*/
LispValue xfunc_LispRemove(foo, bar)
     LispValue foo;
     List bar;
{
    LispValue temp = LispNil;
    LispValue result = LispNil;
    int sanity = false;

    for (temp = bar; !null(temp); temp = CDR(temp))
      if (! equal((Node)(foo),(Node)(CAR(temp))) ) 
       {
	   result = append1(result,CAR(temp));
       }
      else sanity = true; /* found a matching item to remove! */

    if (!sanity)
      elog(WARN, "xfunc_LispRemove: didn't find a match!");

    return(result);
}

#define Node_Copy(a, b, c, d) \
    if (NodeCopy(((a)->d), &((b)->d), c) != true) { \
       return false; \
    } 

/*
** xfunc_copyrel --
**   Just like _copyRel, but doesn't copy the paths
*/
bool
xfunc_copyrel(from, to)
    Rel	from;
    Rel	*to;
{
    Rel	newnode;
    Pointer (*alloc)() = palloc;

    /*  COPY_CHECKARGS() */
    if (to == NULL) 
     { 
	 return false;
     } 
				      
    /* COPY_CHECKNULL() */
    if (from == NULL) 
     {
	 (*to) = NULL;
	 return true;
     } 

    /* COPY_NEW(c) */
    newnode =  (Rel)(*alloc)(classSize(Rel));
    if (newnode == NULL) 
     { 
	 return false;
     } 
    
    /* ----------------
     *	copy node superclass fields
     * ----------------
     */
    CopyNodeFields(from, newnode, alloc);

    /* ----------------
     *	copy remainder of node
     * ----------------
     */
    Node_Copy(from, newnode, alloc, relids);
    
    newnode->indexed = from->indexed;
    newnode->pages =   from->pages;
    newnode->tuples =  from->tuples;
    newnode->size =    from->size;
    newnode->width =   from->width;

    Node_Copy(from, newnode, alloc, targetlist);
/* No!!!!    Node_Copy(from, newnode, alloc, pathlist);  
    Node_Copy(from, newnode, alloc, unorderedpath);
    Node_Copy(from, newnode, alloc, cheapestpath);  */
    Node_Copy(from, newnode, alloc, classlist);
    Node_Copy(from, newnode, alloc, indexkeys);
    Node_Copy(from, newnode, alloc, ordering);
    Node_Copy(from, newnode, alloc, clauseinfo);
    Node_Copy(from, newnode, alloc, joininfo);
    Node_Copy(from, newnode, alloc, innerjoin);
    Node_Copy(from, newnode, alloc, superrels);
    
    (*to) = newnode;
    return true;
}
