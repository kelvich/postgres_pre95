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
 *  Returns the optimized qual.
 */

List
SemantOpt(root,rangetable,tlist, qual)
     List root,rangetable,tlist, qual;
{
  List retqual = LispNil;
  List varlist = LispNil;
  Index leftvarno = 0;
  Index rightvarno = 0;
  ObjectId op = 0;
  List tmp_list = LispNil;
  List i = LispNil;

  /*
   *   NOTE:  Routine doesn't handle func clauses.
   */

  /*
   *  Varlist contains the list of varnos of relations that participate
   *  in the current query.  It is used to detect existential clauses.
   *  Initially, varlist contains the list of target relations (varnos).
   */
  varlist = find_allvars(root,rangetable,tlist,qual);
  retqual = qual;

  if (null(qual))
    return(LispNil);
  else
    if (is_clause(qual)) {
      op = get_opno(get_op(qual));
      if (op == 627) {
	leftvarno = get_varno(get_leftop(qual));
	rightvarno = ConstVarno(rangetable, get_rightop(qual));
	/*
	 *  test for existential clauses first 
	 */
	if (!member(lispInteger(leftvarno),varlist) &&
	    !member(lispInteger(rightvarno),varlist))
	  retqual = MakeTClause();
	else 
	  if (leftvarno == rightvarno)
	    retqual = MakeFClause();
      } else 
	if (op == 558) {
	  leftvarno = get_varno(get_leftop(qual));
	  rightvarno = get_varno(get_rightop(qual));

	  if (!member(lispInteger(leftvarno),varlist) &&
	      !member(lispInteger(rightvarno),varlist))
	    retqual = MakeTClause();	
	  else 
	    if (leftvarno != rightvarno)
	      retqual = MakeFClause();
	    else
	      retqual = MakeTClause();
	} else 
	  /* Just a normal op clause, check to see if it is existential */
	  if (IsA(get_leftop(qual),Var)) {
	    leftvarno = get_varno(get_leftop(qual));
	    if (IsA(get_rightop(qual),Var)) {
	      rightvarno = get_varno(get_rightop(qual));
	      if (!member(lispInteger(leftvarno),varlist) &&
		  !member(lispInteger(rightvarno),varlist))
		retqual = MakeTClause();       /* this is true only for ANDS */
	    }
	    if (!member(lispInteger(leftvarno),varlist))
	      retqual = MakeTClause();
	  }
	  else {    /* At this stage, union vars are existential */
	    if (consp(get_leftop(qual))) 
	      retqual = MakeTClause();
	  }
    } else  
      if (and_clause (qual)) {
	foreach(i,get_andclauseargs(qual)) {
	  tmp_list = nappend1(tmp_list,SemantOpt(root,
						     rangetable, 
						     tlist, 
						     CAR(i)));
	}
	retqual = make_andclause(tmp_list);
      }
      else
	if (or_clause (qual)) {
	  foreach(i,get_orclauseargs(qual)) {
	    tmp_list = nappend1(tmp_list,SemantOpt(root,
						   rangetable, 
						   tlist, 
						   CAR(i)));
	  }
	  retqual = make_orclause(tmp_list);
	}
	else
	  if (not_clause (qual))
	    retqual = make_notclause(SemantOpt(root,
					       rangetable, 
					       tlist, 
					       get_notclausearg(qual)));
  
  return (retqual);
  
}

/*
 * routine is no longer needed.
 */
List
add_varlist(leftvarno, rightvarno, varlist,qual)
     List varlist,qual;
     Index leftvarno,rightvarno;
{
  if (!member(lispInteger(leftvarno), varlist))
    varlist = nappend1(varlist, lispInteger(leftvarno));  
  if ((rightvarno != 0) && 
      !member(lispInteger(rightvarno),varlist)) 
    varlist = nappend1(varlist, lispInteger(rightvarno));

  return(varlist);
}
	  


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
  ObjectId op = 0;
  Index leftvarno = 0;
  Index rightvarno = 0;

  if (null(qual))
    return(varlist);
  else 
    if (is_clause(qual)) {
      op = get_opno(get_op(qual));
      if (op == 627) {  /* Greg's horrendous not-in op */
	leftvarno = get_varno(get_leftop(qual));
	rightvarno = ConstVarno(rangetable,get_rightop(qual));

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
    } else  /* is_clause */
      if (and_clause(qual))
	varlist = update_vars(rangetable,
			      varlist,
			      get_andclauseargs(qual));
      else
	if (or_clause(qual))
	  varlist = update_vars(rangetable,
				varlist,
				get_orclauseargs(qual));
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
ConstVarno(rangetable,constnode)  
     List rangetable;
     Const constnode;
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
    relname = strtok(tmpstring, ".");
    
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
  
  newop = MakeOper(objid, 0,rettype,opsize);
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
  
  newop = MakeOper(objid, 0,rettype,opsize);
  leftconst = MakeConst(23, 4, Int32GetDatum(1), 0);
  rightconst = MakeConst(23, 4,Int32GetDatum(2), 0);

  return(make_opclause(newop,leftconst,rightconst));
  
}
