/*
 *  This file contains routines to perform semantic optimization for
 *  versions (i.e, union queries that pertain to versions).
 *  As such, heuristics used to govern the optimization are applicable
 *  only to the current implementation of versions, and should not be 
 *  used as a general purpose semantic optimizer.
 *
 *  $Header$
 */

#include <stdio.h>
#include <strings.h>
#include "tmp/c.h"

#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "parser/parse.h"
#include "parser/parsetree.h"
#include "tmp/utilities.h"
#include "tmp/datum.h"
#include "utils/log.h"
#include "utils/lsyscache.h"

#include "planner/internal.h"
#include "planner/plancat.h"
#include "planner/planner.h"
#include "planner/prepunion.h"
#include "planner/clauses.h"
#include "planner/semanopt.h"

/*
 *  SemantOpt:
 * 
 *  Given a tlist and qual pair, routine runs through the 
 *  qualification and substitute all redundent clauses.  Eg:
 *
 *   TClause : tautological clauses  (A.oid = A.oid) becomes (1=1)
 *   CClause : contradictory clauses (A.oid = B.oid where A != B)
 *             becomes where 1=2
 *   EClause : clauses that causes an unwanted existential query.
 *             Clauses that have no link back to the targetlist
 *             defaults to true (i.e., 1=1).
 *             THEREFORE, this will only work for AND clauses.
 *
 *   We cannot simply remove CClauses and EClauses from the qualification
 *   because then any boolean operations done on the qualification may
 *   result in the wrong answer.
 *   
 *   HOWEVER, I have included a very simplistic heuristic to remove
 *   queries with only "and" clauses which have a CClause in them.
 *   The routine will punt if it sees any "not" or "or" clauses.
 *   In the future, it should also handle those two clauses.
 *   I'm going to integrate this is semantop.
 *   
 *
 *  Returns the optimized qual.
 */

List
SemantOpt(varlist,rangetable, qual, is_redundent,is_first)
     List varlist,rangetable, qual;
     List *is_redundent;
     int is_first;
{
  static int punt;
  char *attname;
  List retqual = LispNil;
  Index leftvarno = 0;
  Index rightvarno = 0;
  ObjectId op = 0;
  List tmp_list = LispNil;
  List i = LispNil;

  /*
   *   NOTE:  Routine doesn't handle func clauses.
   */

  if (is_first)
    punt = 0;  /* to initialize the static variable */

  retqual = qual;

  if (null(qual))
    return(LispNil);
  else
    if (is_clause(qual)) {
      /* At this stage, union vars are existential */
      if (consp(get_leftop(qual)))
	retqual = MakeTClause();
      else {
	op = get_opno(get_op(qual));
	if (op == 627) {
	  leftvarno = get_varno(get_leftop(qual));
	  rightvarno = ConstVarno(rangetable, get_rightop(qual),&attname);
	  /*
	   *  test for existential clauses first 
	   */
	  if (!member(lispInteger(leftvarno),varlist) &&
	      !member(lispInteger(rightvarno),varlist))
	    retqual = MakeTClause();
	  else 
	    if (leftvarno == rightvarno) {
	      retqual = MakeFClause();
	      *is_redundent = LispTrue;
	    } 
	    else
	      if (rightvarno > 0 && 
		  strcmp(attname,"oid") == 0) {
		List rte1 = LispNil;
		List rte2 = LispNil;
		
		rte1 = nth(leftvarno -1, rangetable);
		rte2 = nth(rightvarno -1, rangetable);

		if (CInteger(CADR(CDR(rte1))) != CInteger(CADR(CDR(rte2))))
		  retqual = MakeTClause();
	      }
	} else 
	  if (op == 558) {  /* oid = */
	    AttributeNumber leftattno;
	    AttributeNumber rightattno;
	    List rte1 = LispNil;
	    List rte2 = LispNil;
	  
	    leftvarno = get_varno(get_leftop(qual));
	    rightvarno = get_varno(get_rightop(qual));
	    leftattno = get_varattno(get_leftop(qual));
	    rightattno = get_varattno(get_rightop(qual));
	    
	    if (!member(lispInteger(leftvarno),varlist) &&
		!member(lispInteger(rightvarno),varlist))
	      retqual = MakeTClause();	
	    else {
	      if (leftattno < 0 && rightattno < 0) {
		/* Comparing the oid field of 2 rels */
		if (leftvarno == rightvarno)
		  retqual = MakeTClause();
		else {
		  rte1 = nth (leftvarno -1, rangetable);
		  rte2 = nth (rightvarno -1, rangetable);
		  
		  if (strcmp(CString(CADR(rte1)),
			     CString(CADR(rte2))) != 0) {
		    retqual = MakeFClause();
		    *is_redundent = LispTrue;
		  }
		}
	      }
	    }
	  } else 
	    /* Just a normal op clause, check to see if it is existential */
	    if (IsA(get_leftop(qual),Var)) {
	      leftvarno = get_varno(get_leftop(qual));
	      if (IsA(get_rightop(qual),Var)) {
		rightvarno = get_varno(get_rightop(qual));
		if (!member(lispInteger(leftvarno),varlist) &&
		    !member(lispInteger(rightvarno),varlist))
		  retqual = MakeTClause();    /* this is true only for ANDS */
	      }
	      if (!member(lispInteger(leftvarno),varlist))
		retqual = MakeTClause();
	    }
      }
    } else  
      if (and_clause (qual)) {
	foreach(i,get_andclauseargs(qual)) {
	  tmp_list = nappend1(tmp_list,SemantOpt(varlist,
						 rangetable, 
						 CAR(i),
						 is_redundent,
						 0));
	}
	retqual = make_andclause(tmp_list);
      }
      else
	if (or_clause (qual)) {
	  punt = 1;
	  foreach(i,get_orclauseargs(qual)) {
	    tmp_list = nappend1(tmp_list,SemantOpt(varlist,
						   rangetable, 
						   CAR(i),
						   is_redundent,
						   0));
	  }
	  retqual = make_orclause(tmp_list);
	}
	else
	  if (not_clause (qual)) {
	    punt = 1;
	    retqual = make_notclause(SemantOpt(varlist, 
					       rangetable, 
					       get_notclausearg(qual),
					       is_redundent,
					       0));
	  }
  if (punt)
    *is_redundent = LispNil;

  return (retqual);
  
}

/*
 * SemantOpt2 optimizes redundent joins.
 *
 *  The theory behind this is that given to primary keys, eg OIDs
 *  a qual. of the form : 
 *   ... from e in emp where e.oid = emp.oid
 *   can be optimized into scan on emp, instead of a join.
 */
List 
SemantOpt2(rangetable,qual,modqual,tlist)
     List rangetable,qual,modqual,tlist;
{
  AttributeNumber leftattno = 0;
  AttributeNumber rightattno = 0;
  List rte1 = LispNil;
  List rte2 = LispNil;
  Index leftvarno = 0;
  Index rightvarno = 0;
  List i = LispNil;

  if (null(qual))
    return(LispNil);
  else
    if (is_clause(qual) && get_opno(get_op(qual)) == 558) {
      /*
       *  Now to test if they are the same relation.
       */

      leftvarno = get_varno(get_leftop(qual));
      rightvarno = get_varno(get_rightop(qual));
      leftattno = get_varattno(get_leftop(qual));
      rightattno = get_varattno(get_rightop(qual));

      if (leftattno < 0 && rightattno < 0) {
	/* Comparing the oid field of 2 rels */
	
	rte1 = nth(leftvarno - 1, rangetable);
	rte2 = nth(rightvarno - 1, rangetable);

	if (strcmp(CString(CADR(rte1)),
		   CString(CADR(rte2))) == 0 &&
	    leftvarno != rightvarno) {	
	  /*  Remove the redundent join */
	  List temp = MakeTClause();
	  CAR(qual) = CAR(temp);
	  CDR(qual) = CDR(temp);
	  replace_tlist(rightvarno,leftvarno,tlist);
	  replace_varnodes(rightvarno,leftvarno,modqual);
	}
      }
    }
    else
      if (and_clause(qual)) 
	foreach(i,get_andclauseargs(qual)) 
	  modqual = SemantOpt2(rangetable,CAR(i),modqual);
      else
	if (or_clause(qual))
	  foreach(i,get_orclauseargs(qual))
	    modqual = SemantOpt2(rangetable,CAR(i),modqual);
	else
	  if (not_clause(qual))
	    modqual = SemantOpt2(rangetable,CDR(qual), modqual);

return(modqual);
}

void
replace_tlist(left,right,tlist)
     Index left,right;
     List tlist;
{
  List i = LispNil;
  List tle = LispNil;
  List leftop = LispNil;
  List rightop = LispNil;

  foreach(i,tlist) {
    tle = tl_expr(CAR(i));

    if (IsA(tle,Var)) {
      if (get_varno(tle) == right) {
	set_varno(tle,left);
	set_varid(tle, lispCons(lispInteger(left),
				lispCons(CADR(get_varid(tle)),
					 LispNil)) );
      }
    } else
      if (is_clause(tle))
	replace_tlist(left,right,CDR(tlist) );

  }
  
}

void
replace_varnodes(left,right,qual)
     Index left,right;
     List qual;
{
  List leftop = LispNil;
  List rightop = LispNil;
  List i = LispNil;

  if (null(qual));
  if (is_clause(qual)) {
    leftop = (List)get_leftop(qual);
    rightop = (List)get_rightop(qual);

    if (IsA(leftop,Var))
      if (get_varno(leftop) == right) {
	set_varno(leftop,left);
	set_varid(leftop, lispCons(lispInteger(left),
				   lispCons(CADR(get_varid(leftop)),
					    LispNil)) );
      } else
	if (IsA(rightop,Var))
	  if (get_varno(rightop) == right) {
	    set_varno(rightop,left);
	    set_varid(rightop, lispCons(lispInteger(left),
				       lispCons(CADR(get_varid(rightop)),
						LispNil)) );	    
	  }
  }
  else
    if (and_clause(qual))
      foreach(i,get_andclauseargs(qual)) 
	replace_varnodes(left,right,CAR(i));
    else 
      if (or_clause(qual))
	foreach(i,get_orclauseargs(qual))
	  replace_varnodes(left,right,CAR(i));
      else
	if (not_clause(qual))
	  replace_varnodes(left,right,CDR(qual));	  
	  
}
/*
 *  Routine that runs through the tlist and qual pair
 *  and collect all varnos that are not existential.
 *  returns a list of those varnos.
 */

List
find_allvars (root,rangetable,tlist,qual)
     List root,rangetable,tlist,qual;
{
  List varlist = LispNil;
  List i = LispNil;
  List tle = LispNil;
  List expr = LispNil;
  int current_nvars;
  int prev_nvars = 0;

  if (CAtom(root_command_type_atom(root)) != RETRIEVE) 
    varlist = nappend1(varlist, root_result_relation(root));
  
  foreach(i, tlist) {
    tle = CAR(i);
    expr = tl_expr(tle);
    /*
     *  Assume that each (non const) expr is a varnode by this time.
     */
    if (IsA(expr,Var)) {
      if (!member(lispInteger(get_varno(expr)),varlist))
	varlist = nappend1(varlist, lispInteger(get_varno(expr)));
    }
  }
  current_nvars = length(varlist);

  while(current_nvars > prev_nvars) {
    prev_nvars = current_nvars;
    varlist = update_vars(rangetable,varlist,qual);
    current_nvars = length(varlist);
  }
  return(varlist);
}

/*
 *  Simply goes through the qual once and
 *  add any new varnos (rels) that participate in a 
 *  join. 
 *  A rel. participates in the query if there is a link back to 
 *  the target relations.
 */

List
update_vars(rangetable,varlist,qual)
     List rangetable,varlist,qual;
{
  List leftop;
  List rightop;
  char *attname;
  ObjectId op = 0;
  Index leftvarno = 0;
  Index rightvarno = 0;
  List tmp = LispNil;

  if (null(qual))
    return(varlist);
  else 
    if (is_clause(qual)) {
      op = get_opno(get_op(qual));
      if (!consp(get_leftop(qual))) { /* ignore union vars at this point */
	if (op == 627) {  /* Greg's horrendous not-in op */
	  leftvarno = get_varno(get_leftop(qual));
	  rightvarno = ConstVarno(rangetable,get_rightop(qual),&attname);

	  if (member(lispInteger(leftvarno),varlist)) {
	    if (rightvarno != 0 && !member(lispInteger(rightvarno),varlist))
	      varlist = nappend1(varlist,lispInteger(rightvarno));
	  }
	  else
	    if (member(lispInteger(rightvarno),varlist))
	      if (!member(lispInteger(leftvarno),varlist))
		varlist = nappend1(varlist,lispInteger(leftvarno));	
	} else {
	  leftop = (List)get_leftop(qual);
	  rightop = (List)get_rightop(qual);
	  if (IsA(leftop,Var)) {
	    if (IsA(rightop,Var)) {
	      if (member(lispInteger(get_varno(leftop)),varlist)) {
		if (!member(lispInteger(get_varno(rightop)),varlist))
		  varlist = nappend1(varlist,lispInteger(get_varno(rightop)));
	      }
	      else
		if (member(lispInteger(get_varno(rightop)),varlist))
		  if (!member(lispInteger(get_varno(leftop)),varlist))
		    varlist = nappend1(varlist,lispInteger(get_varno(leftop)));
	    }
	  } /* leftop var */
	} /*else */
      }
    } else  /* is_clause */
      if (and_clause(qual)) {
	foreach(tmp, get_andclauseargs(qual))
	  varlist =  update_vars(rangetable,
				 varlist,
				 CAR(tmp));
      }
      else
	if (or_clause(qual)) {
	  foreach (tmp, get_orclauseargs(qual))
	    varlist = update_vars(rangetable,
				  varlist,
				  CAR(tmp));
	}
	else
	  if (not_clause(qual)) 
	    varlist = update_vars(rangetable,
				  varlist,
				  get_notclausearg(qual));

  return(varlist);
}

/*
 *  This routine is necessary thanks to Greg's not-in operator.
 *  Given a const node, routine returns the varno of the relation
 *  referenced or 0 if relation is not in the rangetable.
 */

Index
ConstVarno(rangetable,constnode,attname)  
     List rangetable;
     Const constnode;
     char **attname;
{
  static char tmpstring[32];
  char *ptr;
  char *relname;
  List i = LispNil;
  Index position = 0;  /* should be quite safe since varnos start from 1. */

  if (!IsA(constnode,Const))
    elog(WARN, "Arg. is not a const node.");
  else {
    ptr = DatumGetPointer(get_constvalue(constnode));
    strcpy (tmpstring, ptr);
    relname = (char *)strtok(tmpstring, ".");
    *attname = (char *)strtok(NULL,"."); 
    
    foreach (i, rangetable) {
      position += 1;
      if (strcmp(relname, CString(CADR(CAR(i)))) == 0) 
	return (position);
    }
  }
return(0);
  
}

/*
 *  Routine returns a clause of the form 1=1
 */
List
MakeTClause()
{
  Oper newop;
  Const leftconst;
  Const rightconst;
  ObjectId objid = 96;
  ObjectId rettype = 16;
  int opsize = 0;
  
  newop = MakeOper(objid, 0,rettype,opsize,NULL);
  leftconst = MakeConst(23, 4, Int32GetDatum(1), 0, 1);
  rightconst = MakeConst(23, 4, Int32GetDatum(1), 0, 1);

  return(make_opclause(newop,leftconst,rightconst));
  
}

/*
 *  Routine returns a clause of the form 1=2
 */
List
MakeFClause()
{
  Oper newop;
  Const leftconst;
  Const rightconst;
  ObjectId objid = 96;
  ObjectId rettype = 16;
  int opsize = 0;
  
  newop = MakeOper(objid, 0,rettype,opsize,NULL);
  leftconst = MakeConst(23, 4, Int32GetDatum(1), 0);
  rightconst = MakeConst(23, 4,Int32GetDatum(2), 0);

  return(make_opclause(newop,leftconst,rightconst));
  
}
