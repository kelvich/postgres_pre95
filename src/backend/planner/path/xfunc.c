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
#include "utils/palloc.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "catalog/syscache.h"
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

    /* First handle the base case */
    if (IsA(clause,Const) || IsA(clause,Var) || IsA(clause,Param)) 
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
    Form_pg_type type;
    int retval = 0;
    LispValue tmpclause;
    int vnum;
    Name relname;
    Relation rd;

    if (IsA(clause,Const))
     {
	 /* base case: width is the width of this constant */
	 retval = ((Const) clause)->constlen;
	 goto exit;
     }
    else if (IsA(clause,Var))
     {
	 /* base case: width is width of this attribute */
	 if (get_varattno((Var)clause) == 0)
	  {
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
		  elog(WARN, "getattnvals: no attribute tuple %d %d",
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
	      rd = heap_open(typeid_get_relid(get_paramtype((Param)clause)));
	      retval = xfunc_tuple_width(rd);
	      heap_close(rd);
	  }
	 else 
	   retval = 
	     xfunc_width(get_expr((TLE)CAR(get_param_tlist((Param)clause))));
	 goto exit;	 
     }
    else if (fast_is_funcclause(clause))
     {
	 if (get_func_tlist((Func)clause) != LispNil)
	  {
	      /* this function has a projection on it.  Get the length
		 of the projected attribute */
	      retval = 
		xfunc_width(get_expr((TLE)CAR(get_func_tlist((Func)clause))));
	      goto exit;
	  }
	 else
	  {
	      /* lookup function and find its return type */
	      tupl = SearchSysCacheTuple(PROOID, 
					 get_funcid((Func)get_function(clause)),
					 NULL, NULL, NULL);
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
		   type = (Form_pg_type) GETSTRUCT(tupl);
		   if (type->typlen != -1)
		    {
			retval = type->typlen;
			goto exit;
		    }
		   else
		    {
			/* find width of the function's arguments */
			for (tmpclause = CDR(clause); tmpclause != LispNil; 
			     tmpclause = CDR(tmpclause))
			  retval += xfunc_width(CAR(tmpclause));
			/* multiply by outin_ratio */
			retval = proc->prooutin_ratio/100.0 * retval;
			goto exit;
		    }
	       }
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
    Path newkid;
    Rel newrel;
    double pulled_selec;

    /* remove clause from childpath */
    if (clausetype == XFUNC_LOCPRD)
     {
	 set_locclauseinfo(((Path)newkid = (Path)CopyObject(childpath)), 
			   LispRemove(cinfo, get_locclauseinfo(newkid)));
     }
    else
     {
	 set_pathclauseinfo
	   (((JoinPath)(newkid = (Path)CopyObject(childpath))),
	    LispRemove(cinfo, get_pathclauseinfo((JoinPath)newkid)));
     }

    /*
    ** give the new child path its own Rel node that reflects the
    ** lack of the pulled-up predicate
    */
    pulled_selec = compute_clause_selec(get_clause(cinfo), LispNil);
    xfunc_copyrel(get_parent(newkid), &newrel);
    set_parent(newkid, newrel);
    set_pathlist(newrel, lispCons(newkid, LispNil));
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

    /* Fix all vars in the clause 
       to point to the right varno and varattno in parentpath */
    xfunc_fixvars(get_clause(cinfo), newrel, whichchild);

    /* add clause to parentpath, and fix up its xfunc costs */
    set_path_cost(parentpath, get_path_cost(parentpath) 
		  - xfunc_get_path_cost(parentpath));
    set_locclauseinfo(parentpath, 
		      lispCons(cinfo, get_locclauseinfo(parentpath)));
    set_path_cost(newkid, get_path_cost(parentpath)
		  + xfunc_get_path_cost(parentpath));
    if (whichchild == INNER)
     {
	 set_innerjoinpath(parentpath, (pathPtr)newkid);
     }
    else
     {
	 set_outerjoinpath(parentpath, (pathPtr)newkid);
     }
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

    if (IsA(clause,Const) || IsA(clause,Param)) return;
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
    CInfo primjoinclause            /* primary join clause */
      = xfunc_primary_join(get_pathclauseinfo(parentpath));
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
    /* sanity check: we better not be pulling up the primary join clause */
    if (retval == XFUNC_JOINPRD)
      Assert(primjoinclause != maxcinfo);

    if (maxmeasure == 0)  /* no expensive clauses */
      return(0);
    /*
    ** Pullup over join if clause is higher measure than join.
    ** Note that the cost of a secondary join clause is only what's
    ** calculated by xfunc_expense(), since the actual joining 
    ** (i.e. the path_cost) is paid for by the primary join clause
    */
    joinselec = compute_clause_selec(get_clause(primjoinclause), LispNil);
    if (joinselec != 1.0 &&
	xfunc_measure(get_clause(maxcinfo)) > 
	((xfunc_expense(get_clause(primjoinclause)) 
	  + get_path_cost(parentpath))
	 / (1.0 - joinselec)))
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

/*
** xfunc_primary_join:
**   Find the join clause of minimum measure.  This clause cannot be pulled
** up.
*/
CInfo xfunc_primary_join(joinclauselist)
     LispValue joinclauselist;
{
    CInfo mincinfo;
    LispValue tmplist;
    double minmeasure, tmpmeasure;

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
    return(mincinfo);
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
    
    /* first add in the expensive local function costs */
    foreach(tmplist, get_locclauseinfo(pathnode))
      cost += xfunc_expense(get_clause((CInfo)CAR(tmplist)))
	         * get_tuples(get_parent(pathnode));

    /* Now add in any node-specific expensive function costs */
    if (IsA(pathnode,JoinPath))
      foreach(tmplist, get_pathclauseinfo((JoinPath) pathnode))
	cost += xfunc_expense(get_clause((CInfo)CAR(tmplist)))
	           * get_tuples(get_parent(pathnode));
    if (IsA(pathnode,HashPath))
      foreach(tmplist, get_path_hashclauses((HashPath) pathnode))
	cost += xfunc_expense(CAR(tmplist))
	           * get_tuples(get_parent(pathnode));
    if (IsA(pathnode,MergePath))
      foreach(tmplist, get_path_mergeclauses((MergePath) pathnode))
	cost += xfunc_expense(CAR(tmplist))
	           * get_tuples(get_parent(pathnode));
    return(cost);
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
