/*
 *  Routine takes the 'union' parsetree, messes around with it, 
 *  strips the union relations, and returns
 *  a list of orthogonal plans.
 *
 *  NOTE: Current implementation of versions disallows schema changes 
 *        between versions.  Thus we may assume that the attributes
 *        remain the same between versions and their deltas.
 *
 * $Header$
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

/* #define LispRemove remove  */

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
  int num_plans = 0;

  /*
   * SplitTlistQual returns a list of the form:
   * ((tlist1 qual1) (tlist2 qual2) ...) 
   */

  /*
   *  First remove the union flag from the rangetable entries
   *  since we are processing them now.
   */

  foreach(i,rangetable) {
    rt_entry = CAR(i);
    if (member(lispAtom("union"),rt_flags(rt_entry))) {
      rt_flags(rt_entry) = LispRemove(lispAtom("union"),
				      rt_flags(rt_entry));
    }
  }

  tlist_qual = SplitTlistQual (root,rangetable,tlist,qual);

#ifdef VERBOSE
  num_plans = length(tlist_qual);
  printf("####################\n");
  printf("number of plans generated: %d\n", num_plans);
  printf("####################\n");
#endif
  
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
  List varlist = LispNil;
  List is_redundent = LispNil;

  unionlist = collect_union_sets(tlist,qual);
#ifdef VERBOSE
  printf("##########################\n");
  printf("Union sets are: \n");
  lispDisplay(unionlist,0);
  printf("\n");
  fflush(stdout);
  printf("##########################\n");
#endif
  /*
   *  If query is a delete, the tlist is null.
   */
  if (CAtom(root_command_type_atom(root)) == DELETE) 
    foreach(i,unionlist) 
      foreach(temp,CAR(i))
	tl_list = nappend1(tl_list, LispNil);
  else {
    tl_list = lispCons(tlist, LispNil);
    foreach(i, unionlist)   /*  This loop effectively handles union joins */
      tl_list = SplitTlist(CAR(i),tl_list);
  }
  qual_list = lispCons(qual,LispNil);
  foreach(i,unionlist) {
    qual_list = SplitQual(CAR(i),qual_list); 
  }

/*
 *  The assumption here is that both the tl_list and the qual_list, 
 *  must be of the same length, if not, something funky has happened.
 *
 *  XXX I think this is not true in the case of 'DELETEs' ! sp.
 *  XXX In this case, 'tl_list' is nil.
 */
  
  if (CAtom(root_command_type_atom(root)) != DELETE)
      if (length (tl_list) != length(qual_list))
	elog (WARN, "UNION:  tlist with missing qual, or vice versa");

    if(null(tl_list)) {
      /*
       * this is a special case, where tl_list is null (i.e. this
       * is a DELETE command.
       * DISCLAIMER: I don't know what this code is doing, I don't
       * know what it is supposed to do, all I know is that before it
       * didn't work in queries like:
       * "delete foobar from f in (foo | bar) where foobar.a = 1"
       * and now it works (provided that you made a couple of prayers
       * and/or sacrifices to the gods....).
       * sp._
       */
      temp = NULL;
      varlist = find_allvars(root,rangetable, temp, CAR(qual_list));
      is_redundent = LispNil;
      mod_qual = SemantOpt(varlist,rangetable,CAR(qual_list),&is_redundent,1);
      if (is_redundent != LispTrue) {
        mod_qual = SemantOpt2(rangetable,mod_qual,mod_qual,temp);
        tqlist = nappend1(tqlist,
      		    lispCons(temp, 
      			     lispCons(mod_qual,
      				      LispNil)));
      }
      qual_list = CDR(qual_list);
    } else {
      foreach(i, tl_list) {
	temp = CAR(i);

	/*
	 *  Varlist contains the list of varnos of relations that participate
	 *  in the current query.  It is used to detect existential clauses.
	 *  Initially, varlist contains the list of target relations (varnos).
	 *  This should only be done once for each query.
	 */

	varlist = find_allvars(root,rangetable, temp, CAR(qual_list));
	is_redundent = LispNil;
	mod_qual = SemantOpt(varlist,rangetable,CAR(qual_list),&is_redundent,1);
	if (is_redundent != LispTrue) {
	  mod_qual = SemantOpt2(rangetable,mod_qual,mod_qual,temp);
	  tqlist = nappend1(tqlist,
			    lispCons(temp, 
				     lispCons(mod_qual,
					      LispNil)));
	}
      qual_list = CDR(qual_list);
      }
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
  List flatten_list = LispNil;

  foreach(t, tlists) {
    tlist = CAR(t);
    foreach (i, unionlist) {
      varnum = CAR(i);
      new_tlist = LispNil;
      new_tlist = copy_seq_tree(tlist);

      foreach (x, new_tlist) {
	tle = CAR(x);
	varlist = tl_expr(tle);
	if (IsA(varlist,Const) || IsA(varlist,Var))
	  flatten_list = LispNil;
	else
	  if (is_clause(varlist)) 
	    split_tlexpr(CDR(varlist),varnum);
	  else
	    if (CAtom(CAR(varlist)) == UNION) {
	      flatten_list = flatten_union_list(CDR(varlist));
	      foreach (j, flatten_list) {
		varnode = (Var)CAR(j);
		/*
		 * Find matching entry, and change the tlist to it.
		 */

		if (CInteger(varnum) == get_varno(varnode)) {
		  tl_expr(tle) = (List)varnode;
		  break;
		}
	      }
	    } /* for, varlist */
	    
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
 * This routine is called to split the tlist whenever
 *  there is an expression in the targetlist.
 * Routine returns nothing as it modifies the tlist in place.
 */

void
split_tlexpr(clauses,varnum)
     List clauses;
     List varnum;
{
  List x = LispNil;
  List ulist = LispNil;
  List clause = LispNil;
  List flat_list = LispNil;
  Var varnode = (Var)NULL;
  List i = LispNil;

  foreach (x,clauses) {
    clause = CAR(x);
    if (IsA(clause,Const) || 
	IsA(clause,Var) )
      flat_list = LispNil;
    else
      if (is_clause(clause))
	split_tlexpr(CDR(clause));
      else
	if (consp(clause) &&
	    CAtom(CAR(clause)) == UNION) {
	  flat_list = flatten_union_list(CDR(clause));
	  foreach(i,flat_list) {
	    varnode = (Var)CAR(i);
	    if (CInteger(varnum) == get_varno(varnode)) {
	      CAR(clause) = (List)varnode;
	      break;
	    }
	  }
	}
  }
}

/*
 *  collect_union_sets
 *  runs through the tlist and qual, and forms a list of 
 *  unions sets in the query.  
 *  If there is > 1 union set, we are dealing with a union join.
 */

List 
collect_union_sets(tlist,qual)
     List tlist;
     List qual;
{
  List i = LispNil;
  List x = LispNil;
  List j = LispNil;
  int varno = 0;
  List tle = LispNil;
  List varlist = LispNil;
  List current_union_set = LispNil;
  List union_sets = LispNil;
  List qual_union_sets = LispNil;
  List flattened_ulist = LispNil;

/*
 *  First we run through the targetlist and collect all union sets
 *  there.
 */

  foreach(i, tlist) {
    tle = CAR(i);
    varlist = tl_expr(tle);
    current_union_set = LispNil;

    if (is_clause(varlist)) { /* if it is an expression */
      flattened_ulist = collect_tlist_uset(CDR(varlist));
      foreach (x, flattened_ulist) 
	current_union_set = nappend1(current_union_set,
				     lispInteger(get_varno(CAR(x))) );
    } else
      if (consp(varlist) &&
	  CAtom(CAR(varlist)) == UNION) {
	flattened_ulist = flatten_union_list(CDR(varlist));
	foreach (x, flattened_ulist) 
	  current_union_set = nappend1(current_union_set,
				       lispInteger(get_varno(CAR(x))) );

      }
    union_sets = nappend1(union_sets,
			  current_union_set);

  }
  union_sets = remove_subsets(union_sets);

/*
 *  Now we run through the qualifications to collect 
 *  union sets.
 */
  
  qual_union_sets = find_qual_union_sets(qual);
  
  if (qual_union_sets != LispNil)
    foreach(i, qual_union_sets)
      union_sets = nappend1(union_sets, CAR(i));

  if (length(union_sets) > 1)
    union_sets = remove_subsets(union_sets);

  return(union_sets);
}

/*
 *  This routine is called whenever there is a 
 *  an expression in the targetlist.
 *  Returns a flattened ulist if found.
 */

List collect_tlist_uset(args)
     List args;
{
  List retlist = LispNil;
  List x = LispNil;
  List i = LispNil;
  List current_clause = LispNil;
  List ulist = LispNil;
  
  foreach (x, args) {
    current_clause = CAR(x);
    if (IsA(current_clause,Const))
      retlist = LispNil;
    else
      if (is_clause(current_clause))
	retlist = collect_tlist_uset(CDR(current_clause));
      else
	if (consp(current_clause) &&
	    CAtom(CAR(current_clause)) == UNION) {
	  retlist = flatten_union_list(CDR(current_clause));
	}
	else
	  retlist = LispNil;   /* If it is just a varnode. */

    foreach(i,retlist)
      if (CAR(i) != LispNil)
	ulist = nappend1(ulist,CAR(i));
  }

  return(ulist);
}
List
find_qual_union_sets(qual)
     List qual;
{
  List leftop = LispNil;
  List rightop = LispNil;
  List i = LispNil;
  List j = LispNil;
  List union_sets = LispNil;
  List current_union_set = LispNil;
  List qual_uset = LispNil;
  List flattened_ulist = LispNil;

  if (null(qual))
    return(LispNil);
  else
    if (is_clause(qual)) {
      leftop = (List) get_leftop(qual);
      rightop = (List) get_rightop(qual);

      if (consp(leftop) && CAtom(CAR(leftop)) == UNION) {
	current_union_set = LispNil;
	flattened_ulist = flatten_union_list(CDR(leftop));
	foreach(i, flattened_ulist) 
	  current_union_set = nappend1(current_union_set,
				       lispInteger(get_varno(CAR(i))) );
	union_sets = nappend1(union_sets,
			      current_union_set);
      }    
      if (consp(rightop) && CAtom(CAR(rightop)) == UNION) {
	current_union_set = LispNil;
	flattened_ulist = flatten_union_list(CDR(rightop));
	foreach(i, flattened_ulist) 
	  current_union_set = nappend1(current_union_set,
				       lispInteger(get_varno(CAR(i))) );
	union_sets = nappend1(union_sets,
			      current_union_set);
      }
    } else
      if (and_clause(qual)) {
	foreach(j,get_andclauseargs(qual)) {
	  qual_uset = find_qual_union_sets(CAR(j));
	  if (qual_uset != LispNil) 
	    foreach(i,qual_uset) 
	      union_sets = nappend1(union_sets,
				    CAR(i));
	}
      } else
	if (or_clause(qual)) {
	  foreach(j,get_orclauseargs(qual)) {
	    qual_uset = find_qual_union_sets(CAR(j));
	    if (qual_uset != LispNil)
	      foreach(i,qual_uset)
		union_sets = nappend1(union_sets,
				      CAR(i));
	  }
	} else
	  if (not_clause(qual)) {
	    qual_uset = find_qual_union_sets(CDR(qual));
	    if (qual_uset != LispNil)
	      foreach(i,qual_uset)
		union_sets = nappend1(union_sets,
				      CAR(i));
	  }

  return(union_sets);

}

List
flatten_union_list(ulist)
     List ulist;
{
  List retlist = LispNil;
  List i = LispNil;
  List tmp_var = LispNil;
  List tmplist = LispNil;

  foreach(i,ulist) {
    tmp_var = CAR(i);
    if (consp(tmp_var) && CAtom(CAR(tmp_var)) == UNION)
      tmplist = flatten_union_list(CDR(tmp_var));
    else
      retlist = nappend1(retlist, tmp_var);
  }

  if (tmplist)
    foreach(i, tmplist)
      retlist= nappend1(retlist, CAR(i));

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
    foreach(j, CDR(i)) {
      uset2 = CAR(j);
      if (uset2 == LispNil)
	break;


      if (length (uset1) >= length(uset2) &&
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
      is_subset = 1;
    } /* inner loop usets */
  } /* outer loop usets */
  
  foreach (i, usets) 
    if (length (CAR(i)) > 1)
      retlist = nappend1(retlist, CAR(i) );

  return(retlist);

}

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
    varlist = flatten_union_list(CDR(*leftarg));
/*    varlist = CDR(*leftarg); */
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
    varlist = flatten_union_list(CDR(*rightarg));
/*    varlist = CDR(*rightarg); */
    foreach(i,varlist) {
      uvarnode = (Var)CAR(i);
      if (member(lispInteger(get_varno(uvarnode)), unionlist)) {
	*rightarg = (List)uvarnode;
	break;
      }
    }
  }

}
#ifdef NOT_USED
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
#endif
