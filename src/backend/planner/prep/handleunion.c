/*
 *  Routine takes the 'union' parsetree, messes around with it, 
 *  strips the union relations, and returns
 *  a list of orthogonal plans.
 *
 *  NOTE: Current implementation of versions disallows schema changes 
 *        between versions.  Thus we may assume that the attributes
 *        remain the same between versions and their deltas.
 *
 * $Header:
 */

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
#include "utils/log.h"
#include "utils/lsyscache.h"

#include "planner/internal.h"
#include "planner/plancat.h"
#include "planner/planner.h"
#include "planner/prepunion.h"
#include "planner/clauses.h"
#include "planner/handleunion.h"
#include "planner/semanopt.h"
/* #include "planner/cfi.h" */

List
handleunion (root,rangetable, tlist, qual)
     List root, rangetable, tlist, qual;
{

  List new_root = LispNil;
  List temp_tlist = LispNil;
  List temp_qual = LispNil;
  List i = LispNil;
  List tlist_qual = LispNil;
  List rt_entry = LispNil;
  List temp = LispNil;
  List new_parsetree = LispNil;
  List union_plans = LispNil;
  int planno = 0;
  List tmp_uplans = LispNil;
  List firstplan = LispNil;
  List secondplan = LispNil;

  /*
   * SplitTlistQual returns a list of the form:
   * ((tlist1 qual1) (tlist2 qual2) ...) 
   */

  tlist_qual = SplitTlistQual (root,rangetable,tlist,qual);

  foreach (i, tlist_qual) {
    temp = CAR(i);
    new_parsetree = lispCons(root,lispCons(CAR(temp),
					   lispCons(CADR(temp),
						    LispNil) ));
    union_plans = nappend1(union_plans,
			   planner(new_parsetree));

  }

  /*
   * testing:
   */
  
/*  union_plans = CDR(union_plans);  */

#ifdef SWAP_PLANS
  firstplan = CAR(union_plans);
  secondplan = CADR(union_plans);

  CAR(union_plans) = secondplan;
  CADR(union_plans) = firstplan;
#endif
/*
  foreach(i, union_plans) {
    planno += 1;
    tmp_uplans = nappend1(tmp_uplans, CAR(i));
  }
*/

return(union_plans);  

}


/*
 *  Given a 'union'ed tlist and qual, this routine splits them
 *  up into separate othrogonal (tlist qual) pairs.
 *  For example, 
 *  tlist:  (emp | emp1).name
 *  qual:   (emp | emp1). salary < 5000
 *  will be split into:
 *  ( ((emp.name) (emp.salary < 5000)) ((emp1.name) (emp1.salary < 5000)) )
 */


List
SplitTlistQual(root,rangetable,tlist, qual)
     List root,rangetable,tlist, qual;
{
  List i = LispNil;
  List tqlist = LispNil;
  List  temp = LispNil;
  List tl_list = LispNil;
  List qual_list = LispNil;
  List unionlist = LispNil;
  List mod_qual = LispNil;

  unionlist = find_union_sets(rangetable);

  /*
   *  If query is a delete, the tlist is null.
   */
  if (CAtom(root_command_type_atom(root)) == DELETE) 
    foreach(i,unionlist) 
      foreach(temp,CAR(i))
	tl_list = nappend1(tl_list, LispNil);
  else {
    tl_list = lispCons(tlist, LispNil);
    foreach(i, unionlist)
      tl_list = SplitTlist(CAR(i),tl_list);
  }
  qual_list = lispCons(qual,LispNil);
  foreach(i,unionlist) {
    qual_list = SplitQual(CAR(i),qual_list); 
  }

/*
 *  The assumption here is that both the tl_list and the qual_list, 
 *  must be of the same length, if not, something funky has happened.
 */
  
  if (length (tl_list) != length(qual_list))
    elog (WARN, "UNION:  tlist with missing qual, or vice versa");

  foreach(i, tl_list) {
    temp = CAR(i);
    mod_qual = SemantOpt(root,rangetable,temp,CAR(qual_list));
    tqlist = nappend1(tqlist,
		      lispCons(temp, 
			       lispCons(mod_qual,
					LispNil)));
    qual_list = CDR(qual_list);
  }

return (tqlist);

}



/*
 * SplitTlist
 * returns a list of the form (tlist1 tlist2 ...)
 */

List
SplitTlist (unionlist,tlists)
     List unionlist;
     List tlists;
{
  List i = LispNil;
  List j = LispNil;
  List x = LispNil;
  List tlist = LispNil;
  List t = LispNil;
  List varnum = LispNil;
  List new_tlist ;
  List tle = LispNil;
  List varlist = LispNil;
  Var varnode = (Var)NULL;
  List tl_list = LispNil;

  foreach(t, tlists) {
    tlist = CAR(t);
    foreach (i, unionlist) {
      varnum = CAR(i);
      new_tlist = LispNil;
      new_tlist = copy_seq_tree(tlist);

      foreach (x, new_tlist) {
	tle = CAR(x);
	varlist = tl_expr(tle);
	if (IsA(varlist,Const) || IsA(varlist,Var)) ;
	else
	  if (CAtom(CAR(varlist)) == UNION) {
	    foreach (j, CDR(varlist)) {
	      varnode = (Var)CAR(j);
	      /*
	       * Find matching entry, and change the tlist to it.
	       *
	       * NOTE: Also need to handle the case when a particular
	       *       rel. is not found in the union.  In that case, 
	       *       we should remove the tle (i.e, (resdom var) pair).
	       *       HOWEVER, this is not done just yet.
	       */
	      if (CInteger(varnum) == get_varno(varnode)) {
		tl_expr(tle) = (List)varnode;
		break;
	      }
	    } /* for, varlist */
	  }
      } /* for, new_tlist*/
    
      /*
       *  At this point, the new_tlist is an orthogonal targetlist, without
       *  any union relations.
       */
    
      if (new_tlist == NULL) 
	elog(WARN, "UNION: resulting tlist is empty");
    
      tl_list = nappend1(tl_list,new_tlist);
    }
  } /*tlists */
  return(tl_list);

}


/*
 *  find_union_sets
 *  runs through the rangetable, and forms a list of all union
 *  sets.  Routine removes the union flag from the rte after
 *  it is done processing .  
 */

List
find_union_sets(rangetable)
     List rangetable;
{
  List i = LispNil;
  List x = LispNil;
  List j = LispNil;
  int varno = 0;
  List rt_entry = LispNil;
  List rt_vars = LispNil;
  List retlist = LispNil;
  List ulist = LispNil;
  List u_set = LispNil;
  List vlist = LispNil;
  List varname = LispNil;
  int In_List;
  int position;

  foreach(i,rangetable) {
    rt_entry = CAR(i);
    varno += 1;
    if (member(lispAtom("union"),rt_flags(rt_entry))) {
      rt_flags(rt_entry) = remove(lispAtom("union"),
				  rt_flags(rt_entry));
    }
    rt_vars = CAR(rt_entry);
    if (consp(rt_vars)) {
      foreach(x,rt_vars) {
	varname = CAR(x);
	In_List = 0;
	position = 0;
	if (ulist == LispNil) {
	  ulist = nappend1(ulist, lispCons(varname,
					   LispNil));
	  vlist = nappend1(vlist, lispCons(lispInteger(varno),
					   LispNil));
	  In_List = 1;
	} else
	  foreach(j,ulist) {
	    if (member(varname, CAR(j))) {
	      u_set = nth(position,vlist);
	      if (!member(lispInteger(varno), u_set))
		u_set = nappend1(u_set,lispInteger(varno));
	      In_List = 1;
	      break;
	    }
	    position += 1;
	  }
	if (!In_List) {
	  ulist = nappend1(ulist, lispCons(varname, LispNil));
	  vlist = nappend1(vlist,lispCons(lispInteger(varno), LispNil));
	}
      } /* rt_vars */
    } else {  /*  If it is a single var. */
      varname = rt_vars;
      In_List = 0;
      position = 0;
      if (ulist == LispNil) {
	ulist = nappend1(ulist, lispCons(varname,
					 LispNil));
	vlist = nappend1(vlist, lispCons(lispInteger(varno),
					 LispNil));
	In_List = 1;
      } else
	foreach(j,ulist) {
	  if (member(varname, CAR(j))) {
	    u_set = nth(position,vlist);
	    if (!member(lispInteger(varno), u_set))
	      u_set = nappend1(u_set,lispInteger(varno));
	    In_List = 1;
	    break;
	  }
	  position += 1;
	} /* ulist */
      if (!In_List) {
	ulist = nappend1(ulist, lispCons(varname, LispNil));
	vlist = nappend1(vlist,lispCons(lispInteger(varno), LispNil));
      }
    }
  }  /* rangetable */

  foreach(i,vlist) {
    if (length(CAR(i)) > 1) {  /* i.e a union set */
      retlist = nappend1 (retlist, CAR(i));
    }
  }
  retlist = remove_subsets(retlist);
  return(retlist);
}

/*
 *  remove_subsets
 *  Routine finds and removes any subsets in the given list.
 *  Returns the modified list.
 *  Thus given ( (A B C) (B C) (D E F) (D F) ), routine returns:
 *  ( (A B C) (D E F) )
 */

List 
remove_subsets(usets)
     List usets;
{
  List retlist = LispNil;
  List i = LispNil;
  List j = LispNil;
  List k = LispNil;
  List uset1 = LispNil;
  List uset2 = LispNil;
  int is_subset = 1;

  foreach(i, usets) {
    uset1 = CAR(i);
    foreach(j, CDR(usets)) {
      uset2 = CAR(j);
      if (uset2 == LispNil)
	break;

      /*  Here, assume that there are no duplicate sets. */

      if (length (uset1) > length(uset2) &&
	  length(uset2) != 1) {
	foreach(k, uset2) 
	  if (!member(CAR(k),uset1)) {
	    is_subset = 0;
	    break;
	  }
	if (is_subset) 
	  CAR(j) = lispCons(lispInteger(1), LispNil);
      } /* uset1 > uset2 */
      else 
	if (length(uset1) < length(uset2) &&
	    length(uset1) != 1) {
	  foreach(k,uset1) 
	    if (!member(CAR(k), uset2)) {
	      is_subset = 0;
	      break;
	    }

	  if (is_subset) 
	    CAR(i)= lispCons(lispInteger(1),LispNil);
	}
    } /* inner loop usets */
  } /* outer loop usets */
  
  foreach (i, usets) 
    if (length (CAR(i)) > 1)
      retlist = nappend1(retlist, CAR(i) );

  return(retlist);

}

/*
 *  Routine is not used.
 *  find_union_vars
 *  runs through the rangetable, and forms a list of all the 
 *  relations that are unioned. It will also remove the union
 *  flag from the rangetable entries after it is done processing
 *  them.
 *  REturns a list of varnos.
 */

List
find_union_vars (rangetable)
     List rangetable;
{
  
  List i = LispNil;
  List unionlist = LispNil;
  List rt_entry = LispNil;
  Index uvarno = 0;

  /*
   * XXX what about the case when there are 2 emps in the rangetable, and
   * both are union relations?
   * Currently, both would appear in the unionlist.
   */

  foreach( i, rangetable) {
    rt_entry = CAR(i);
    uvarno += 1;
    if (member(lispAtom("union"),rt_flags(rt_entry))) {
      unionlist = nappend1(unionlist, lispInteger(uvarno));
      rt_flags(rt_entry) = LispRemove(lispAtom("union"), 
				  rt_flags(rt_entry)); 
    }     
  }
  return(unionlist);
}

/*
  List tle = LispNil;
  List expr = LispNil;
  List x = LispNil;
  List varlist = LispNil;
  Var unionvar = (Var)NULL;

  foreach (i,tlist) {
    tle = CAR(i);
    expr = tl_expr(tle);

    if (CAtom(CAR(expr)) == UNION ) {
      varlist = CDR(expr);
      foreach (x, varlist) {
	unionvar = (Var)CAR(x);
	uvarno = get_varno(unionvar);
	if (!(member(lispInteger(uvarno), unionlist)))
	  unionlist = nappend1(unionlist,
			       lispInteger(uvarno));
      }
    }
  }
*/


/*
 *  SplitQual:
 *
 *  Given a list of union rels, this routine splits the qualification
 *  up accordingly.  
 *  NOTE:  Only union rels in the qualification will be split.
 *  ( I believe that utimately, we want to split union rels by position. )
 *
 *  Input:  (uvar1 uvar2 ...) (unioned qual)
 *  Output: (qual1 qual2 ...)
 */

List 
SplitQual (ulist, uquals)
     List ulist;
     List uquals;
{
  List i = LispNil;
  List x = LispNil;
  List uqual = LispNil;
  List uvarno = LispNil;
  List qual_list = LispNil;
  List currentlist = LispNil;
  List new_qual = LispNil;

  foreach(x, uquals) {
    uqual = CAR(x);
    if (uqual == LispNil) {
      foreach(i, ulist) 
	qual_list = nappend1(qual_list, LispNil);
      return(qual_list);
    }
    foreach(i, ulist) {
      uvarno = CAR(i);
      currentlist = nappend1(currentlist, uvarno);
      new_qual = copy_seq_tree(uqual);
      new_qual = find_matching_union_qual(currentlist, new_qual);
      qual_list = nappend1(qual_list, new_qual);
      currentlist = LispNil;
    }
  }
  return(qual_list);

}

/*
 * find_matching_union_qual
 *
 * Given a union qual, and a union relation, returns 
 * the qualifications that apply to it.
 */

List 
find_matching_union_qual(ulist, qual)
     List ulist, qual;
{
  List leftop = LispNil;
  List rightop = LispNil;
  List op = LispNil;
  List matching_clauseargs = LispNil;
  List retqual = LispNil;
  List i = LispNil;
  List tmp_list = LispNil;

  if (null(qual))
    return(LispNil);
  else
    if (is_clause(qual)) {
      leftop = (List)get_leftop(qual);
      rightop = (List)get_rightop(qual);
      op = get_op(qual);
      leftop = find_matching_union_qual(ulist,leftop);
      rightop = find_matching_union_qual(ulist,rightop);
      match_union_clause(ulist, &leftop, &rightop);
      retqual = make_opclause(op,leftop,rightop);

      /*
       * if the clause does not belong to the current union relation,
       * remove it from the qualification.
       */

      /*
     if (matching_clauseargs == LispNil)
	remove(clause, clauses);  * NOTE: may have to mod. qual *
      else
	clauseargs = matching_clauseargs; 
      */
    }
  else
    if (and_clause (qual)) {
      foreach(i, get_andclauseargs(qual)) {
	tmp_list = nappend1(tmp_list, find_matching_union_qual(ulist, 
							     CAR(i)));
      }
      retqual = make_andclause(tmp_list);
    }
    else
      if (or_clause (qual)) {
	foreach(i, get_orclauseargs(qual)) {
	  tmp_list = nappend1(tmp_list, find_matching_union_qual(ulist, 
								 CAR(i)));
	}
	retqual = make_orclause(tmp_list);
      }
      else
	if (not_clause (qual)) {
	  retqual = find_matching_union_qual(ulist, 
					     get_notclausearg(qual));
	  retqual = make_notclause(retqual);
	}
	else
	  retqual = qual;
    
  return(retqual);
}
/*
 *  match_union_clauses:
 *
 *  Given a list of *2* clause args, (leftarg rightarg), 
 *  it picks out the clauses within unions that
 *  are relevant to the particular union relation being scanned.
 *
 *  returns nothing. It modifies the qual in place.
 */

void
match_union_clause (unionlist, leftarg, rightarg)
     List unionlist, *leftarg, *rightarg;
{
  List i = LispNil;
  List x = LispNil;
  List varlist = LispNil;
  Var uvarnode = (Var)NULL;
  List varlist2 = LispNil;

  /*
   *  If leftarg is a union var.
   */
  if (consp(*leftarg) && CAtom(CAR(*leftarg)) == UNION) {
    varlist = CDR(*leftarg);
    foreach(i,varlist) {
      uvarnode = (Var)CAR(i);
      if (member(lispInteger(get_varno(uvarnode)), unionlist)) {
	*leftarg = (List)uvarnode;
	break;
      }
    }
  }
  
  /*
   *  If rightarg is a union var.
   */
  if (consp(*rightarg) && CAtom(CAR(*rightarg)) == UNION) {
    varlist = CDR(*rightarg);
    foreach(i,varlist) {
      uvarnode = (Var)CAR(i);
      if (member(lispInteger(get_varno(uvarnode)), unionlist)) {
	*rightarg = (List)uvarnode;
	break;
      }
    }
  }

}
