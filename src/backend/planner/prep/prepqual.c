
/*     
 *      FILE
 *     	prepqual
 *     
 *      DESCRIPTION
 *     	Routines for preprocessing the parse tree qualification
 *     
 */
/* RcsId ("$Header$");  */

/*     
 *      EXPORTS
 *     		preprocess-qualification
 */

#include "clauses.h"
#include "internal.h"
#include "pg_lisp.h"
#include "clause.h"
#include "prepqual.h"

extern LispValue get_negator(); /* defined in lsyscache.c */

/*
extern LispValue cnfify();
extern LispValue pull_args();
extern LispValue pull_ors();
extern LispValue pull_ands();
extern LispValue find_nots();
extern LispValue push_nots();
extern LispValue or_normalize();
extern LispValue cleanup();
extern LispValue distribute_args();
extern LispValue remove_ands();
extern LispValue update_relations();
extern LispValue normalize();
extern LispValue update_clauses();
*/
/*    
 *    	preprocess-qualification
 *    
 *    	Driver routine for modifying the parse tree qualification.
 *    
 *    	Returns the new base qualification and the existential qualification
 *    	in a list.
 *    
 */

/*  .. init-query-planner    */

LispValue
preprocess_qualification (qual,tlist)
     LispValue qual,tlist ;
{
  LispValue cnf_qual = cnfify (qual);
  LispValue existential_qual = 
    update_clauses (cons (_query_result_relation_,
			  update_relations (tlist)),
		    cnf_qual,_query_command_type_);
  if ( existential_qual ) 
    return (list (set_difference (cnf_qual,existential_qual),
		  existential_qual));
  else 
    return (list (cnf_qual,existential_qual));

}  /* function end   */

/*     	=======================
 *     	CNF CONVERSION ROUTINES
 *     	=======================
 */

/*     
 *      NOTES:
 *     	The basic algorithms for normalizing the qualification are taken
 *     	from ingres/source/qrymod/norml.c
 *     
 *     	Remember that the initial qualification may consist of ARBITRARY
 *     	combinations of clauses.  In addition, before this routine is called,
 *     	the qualification will contain explicit "AND"s.
 *     
 */

/*    
 *    	cnfify
 *    
 *    	Convert a qualification to conjunctive normal form by applying
 *    	successive normalizations.
 *    
 *    	Returns the modified qualification.
 *    
 */

/*  .. preprocess-qualification, print_parse    */

LispValue
cnfify (qual)
     LispValue qual ;
{
  if ( consp (qual) ) {
    /* XXX - let form, maybe incorrect */
    LispValue newqual = LispNil;
    newqual = find_nots (pull_args (qual));
    newqual = normalize (pull_args (newqual));
    newqual = cleanup (pull_args (newqual));
    newqual = pull_args (newqual);;

    if(and_clause (newqual)) 
      return (remove_ands (newqual));
    else 
      return (remove_ands (make_andclause (list (newqual))));
  }
} /*  function end   */

/*    
 *    	pull-args
 *    
 *    	Given a qualification, eliminate nested 'and' and 'or' clauses.
 *    
 *    	Returns the modified qualification.
 *    
 */

/*  .. cnfify, pull-args    */

LispValue
pull_args (qual)
     LispValue qual ;
{
  if(null (qual)) 
    return (LispNil);
  else 
    if (is_clause (qual)) 
      make_clause (get_op (qual),
		   pull_args (get_leftop (qual)),
		   pull_args (get_rightop (qual)));

    else 
      if (and_clause (qual)) {
	LispValue temp = LispNil;
	LispValue t_list = LispNil;

	foreach (temp,get_andclauseargs(qual)) 
	  t_list = nappend1 (t_list, pull_args(temp));
	return (make_andclause (pull_ands (t_list)));
      }
      else 
	if (or_clause (qual)) {
	  LispValue temp = LispNil;
	  LispValue t_list = LispNil;
	  
	  foreach (temp,get_orclauseargs(qual)) 
	    t_list = nappend1 (t_list, pull_args(temp));
	  return (make_orclause (pull_ors (t_list)));
	}
	else 
	  if (not_clause (qual)) 
	    return (make_notclause (pull_args (get_notclausearg (qual))));
	  else 
	    return (qual);
} /* function end  */

/*    
 *    	pull-ors
 *    
 *    	Pull the arguments of an 'or' clause nested within another 'or'
 *    	clause up into the argument list of the parent.
 *    
 *    	Returns the modified list.
 */

/*  .. distribute-args, pull-args, pull-ors   */

LispValue
pull_ors (orlist)
     LispValue orlist ;
{
  if(null (orlist)) 
    return (LispNil);
  else 
    if (or_clause (CAR (orlist))) 
      return (pull_ors (append (CDR (orlist),
				get_orclauseargs (CAR (orlist)))));
    else 
      return (cons (CAR (orlist),pull_ors (CDR (orlist))));
}  /* function end   */

/*    
 *    	pull-ands
 *    
 *    	Pull the arguments of an 'and' clause nested within another 'and'
 *    	clause up into the argument list of the parent.
 *    
 *    	Returns the modified list.
 */

/*  .. pull-ands, pull-args    */

LispValue
pull_ands (andlist)
     LispValue andlist ;
{
  if(null (andlist)) 
    return (LispNil);
  else 
    if (and_clause (CAR (andlist))) 
      return (pull_ands (append (CDR (andlist),
				 get_andclauseargs (CAR (andlist)))));
    else 
      return (cons (CAR (andlist),pull_ands (CDR (andlist))));
}  /* function end   */

/*    
 *    	find-nots
 *    
 *    	Traverse the qualification, looking for 'not's to take care of.
 *    	For 'not' clauses, remove the 'not' and push it down to the clauses'
 *    	descendants.
 *    	For all other clause types, simply recurse.
 *    
 *    	Returns the modified qualification.
 *    
 */

/*  .. cnfify, find-nots, push-nots    */

LispValue
find_nots (qual)
     LispValue qual ;
{
  if(null (qual)) 
    return (LispNil);
  else 
    if (is_clause (qual)) 
      return (make_clause (get_op (qual),
			   find_nots (get_leftop (qual)),
			   find_nots (get_rightop (qual))));
    else 
      if (and_clause (qual)) {
	LispValue temp = LispNil;
	LispValue t_list = LispNil;

	foreach (temp,(get_andclauseargs(qual))) {
	  t_list = nappend1(t_list,find_nots(temp));
	}

	return (make_andclause (t_list));
      } else
	if (or_clause (qual)) {
	  LispValue temp = LispNil;
	  LispValue t_list = LispNil;

	  foreach (temp,get_orclauseargs(qual)) {
	    t_list = nappend1(t_list,find_nots(temp));
	  }
	  return (make_orclause (t_list));
	} else
	  if (not_clause (qual)) 
	    return (push_nots (get_notclausearg (qual)));
	  else 
	    return (qual);
} /* function end   */

/*    
 *    	push-nots
 *    
 *    	Negate the descendants of a 'not' clause.
 *    
 *    	Returns the modified qualification.
 *    
 */

/*  .. find-nots, push-nots    */

LispValue
push_nots (qual)
     LispValue qual ;
{
  if(null (qual)) 
    return (LispNil);
  else 
    /*    Negate an operator clause if possible: */
    /*   	("NOT" (< A B)) => (> A B) */
    /*    Otherwise, retain the clause as it is (the 'not' can't be pushed */
    /*    down any farther). */

    if(is_clause (qual)) {
      LispValue oper = get_op (qual);
      LispValue negator = get_negator (get_opno (oper));
      if(negator &&  !zerop (negator)) 
	return (make_clause (make_oper (negator,
					get_oprelationlevel (oper),
					get_opresulttype (oper)),
			     get_leftop (qual),get_rightop (qual)));
      else 
	return (make_notclause (qual));
    }
    else 
      /*    Apply DeMorgan's Laws: */
      /*   	("NOT" ("AND" A B)) => ("OR" ("NOT" A) ("NOT" B)) */
      /*   	("NOT" ("OR" A B)) => ("AND" ("NOT" A) ("NOT" B)) */
      /*    i.e., continue negating down through the clause's descendants. */
      if (and_clause (qual)) {
	LispValue temp = LispNil;
	LispValue t_list = LispNil;

	foreach(temp,get_andclauseargs(qual)) {
	  t_list = nappend1(t_list,push_nots(temp));
	}
	return (make_orclause (t_list));
      }
      else 
	if (or_clause (qual)) {
	  LispValue temp = LispNil;
	  LispValue t_list = LispNil;

	  foreach(temp,get_orclauseargs(qual)) {
	    t_list = nappend1(t_list,push_nots(temp));
	}
	return (make_andclause (t_list));
	}
	else 
	  /*    Another 'not' cancels this 'not', so eliminate the 'not' and */
	  /*    stop negating this branch. */
	  if (not_clause (qual)) 
	    return (find_nots (get_notclausearg (qual)));
	  else  
	    /*    We don't know how to negate anything else, */
	    /*	  place a 'not' at this */
	    /*    level. */
	    return (make_notclause (qual));

}  /* function end  */

/*    
 *    	normalize
 *    
 *    	Given a qualification tree with the 'not's pushed down, convert it
 *    	to a tree in CNF by repeatedly applying the rule:
 *    		("OR" A ("AND" B C))  => ("AND" ("OR" A B) ("OR" A C))
 *    	bottom-up.
 *    	Note that 'or' clauses will always be turned into 'and' clauses.
 *    
 *    	Returns the modified qualification.
 *    
 */

/*  .. cnfify, normalize
 */
LispValue
normalize (qual)
     LispValue qual ;
{
  if(null (qual)) 
    return (LispNil);
  else 
    if (is_clause (qual)) 
      return (make_clause (get_op (qual),
			   normalize (get_leftop (qual)),
			   normalize (get_rightop (qual))));
    else 
      if (and_clause (qual)) {
	LispValue temp = LispNil;
	LispValue t_list = LispNil;

	foreach (temp,get_andclauseargs(qual)) {
	  t_list = nappend1(t_list,normalize(temp));
	}
	return (make_andclause (t_list));
      } else
	if (or_clause (qual)) {
	  /* XXX - let form, maybe incorrect */
	  LispValue orlist = LispNil;
	  LispValue temp = LispNil;
	  LispValue has_andclause = LispNil;

	  foreach(temp,get_orclauseargs(qual)) {
	    orlist = nappend1(orlist,normalize(temp));
	  }
	  temp = LispNil;
	  /*  XXX was some  */
	  foreach (temp,orlist) {
	    if (and_clause (temp)) 
	      has_andclause = LispTrue;
	    if (has_andclause == LispTrue)
	      break;
	  }
	  if(has_andclause == LispTrue) 
	    return (make_andclause (or_normalize (orlist)));
	  else 
	    return (make_orclause (orlist));

	} else
	  if (not_clause (qual)) 
	    return (make_notclause (normalize (get_notclausearg (qual))));
	  else 
	    return (qual);

}  /* function end   */

/*    
 *    	or-normalize
 *    
 *    	Given a list of exprs which are 'or'ed together, distribute any
 *    	'and' clauses.
 *    
 *    	Returns the modified list.
 *    
 */

/*  .. distribute-args, normalize, or-normalize    */

LispValue
or_normalize (orlist)
     LispValue orlist ;
{
     if ( consp (orlist) ) {
	  LispValue distributable = LispNil;
	  LispValue new_orlist = LispNil;
	  LispValue temp = LispNil;
	  
	  foreach(temp,orlist) {
	       if (and_clause(temp)) 
		 distributable = LispTrue;
	  }
	  if (distributable == LispTrue) 
	    new_orlist = remove(distributable,orlist);
	  
	  if(new_orlist) 
	    return (or_normalize (cons (distribute_args 
					(CAR (new_orlist),
					 get_andclauseargs (distributable)),
					CDR (new_orlist))));
	  
	  else
	    return (orlist);
	  
     }
}  /* function end   */

/*    
 *    	distribute-args
 *    
 *    	Create new 'or' clauses by or'ing 'item' with each element of 'args'.
 *    	E.g.: (distribute-args A ("AND" B C)) => ("AND" ("OR" A B) ("OR" A C))
 *    
 *    	Returns an 'and' clause.
 *    
 */

/*  .. or-normalize     */

LispValue
distribute_args (item,args)
     LispValue item,args ;
{
  if(null (args))
    return (item);
  else {
    LispValue temp = LispNil;
    LispValue t_list = LispNil;

    foreach (temp,args) {
      t_list = nappend1(t_list,
		     make_orclause(or_normalize(pull_ors(list(item,temp)))));
    }
    return (make_andclause (t_list));
  }
}  /* function end  */

/*    
 *    	cleanup
 *    
 *    	Fix up a qualification by removing duplicate entries (left over from
 *    	normalization), and by removing 'and' and 'or' clauses which have only
 *    	one valid expr (e.g., ("AND" A) => A).
 *    
 *    	Returns the modified qualfication.
 *    
 */

/*  .. cleanup, cnfify    */

LispValue
cleanup (qual)
     LispValue qual ;
{
     if(null (qual)) 
       return (LispNil);
     else 
       if (is_clause (qual)) 
	 return (make_clause (get_op (qual),
			      cleanup (get_leftop (qual)),
			      cleanup (get_rightop (qual))));
       else 
	 if (and_clause (qual)) {
	      /* XXX - let form, maybe incorrect */
	      LispValue temp = LispNil;
	      LispValue t_list = LispNil;
	      LispValue new_and_args = LispNil;

	      foreach(temp,get_andclauseargs(qual)) 
		   t_list = nappend1(t_list,cleanup(temp));
	      new_and_args = remove_duplicates (t_list);

	      if(length (new_and_args) > 1) 
		return (make_andclause (new_and_args));
	      else 
		   return (CAR (new_and_args));
	 }
	 else 
	   if (or_clause (qual)) {
		/* XXX - let form, maybe incorrect */
		LispValue temp = LispNil;
		LispValue t_list = LispNil;
		LispValue new_or_args = LispNil;

		foreach (temp,get_orclauseargs(qual)) 
		  t_list = nappend1(t_list,cleanup(temp));
		new_or_args = remove_duplicates (t_list);

		if(length (new_or_args) > 1) 
		  return (make_orclause (new_or_args));
		else 
		  return (CAR (new_or_args));
	   } else 
	      if (not_clause (qual)) 
		   return (make_notclause (cleanup (get_notclausearg (qual))));
	      else 
		return (qual);
}  /* function end   */

/*    
 *    	remove-ands
 *    
 *    	Remove the explicit "AND"s from the qualification:
 *    		("AND" A B) => (A B)
 *    
 *    	Returns the modified qualification.
 *    
 */

/*  .. cnfify, remove-ands     */

LispValue
remove_ands (qual)
     LispValue qual ;
{
     LispValue t_list = LispNil;

     if(null (qual)) 
       return (LispNil);
     else 
       if (is_clause (qual)) 
	 return (make_clause (get_op (qual),
			      remove_ands (get_leftop (qual)),
			      remove_ands (get_rightop (qual))));
       else 
	 if (and_clause (qual)) {
	      LispValue temp = LispNil;
	      foreach (temp,get_andclauseargs(temp))
		t_list = nappend1(t_list,remove_ands(temp));
	      return(t_list);
	 } else 
	   if (or_clause (qual)) {
		LispValue temp = LispNil;
		foreach (temp,get_orclauseargs(temp))
		  t_list = nappend1(t_list,remove_ands(temp));
		return (make_orclause (t_list));
	   } else 
	     if (not_clause (qual)) 
		  return (make_notclause (remove_ands 
					  (get_notclausearg (qual))));
	     else 
	       return (qual);
}  /* function end   */

/*     	==========================
 *     	EXISTENTIAL QUALIFICATIONS
 *     	==========================
 */

/*    
 *    	update-relations
 *    
 *    	Returns the range table indices (i.e., varnos) for all relations which
 *    	are referenced in the target list.
 *    
 */

/*  .. preprocess-qualification     */

LispValue
update_relations (tlist)
     LispValue tlist ;
{
     LispValue xtl = LispNil;
     LispValue var = LispNil;
     LispValue t_list1 = LispNil;
     LispValue t_list2 = LispNil;

     /* was mapCAR nested with mapcan  */
     foreach(xtl,tlist) 
       t_list1 = nappend1(t_list1,pull_var_clause(tl_expr(xtl)));
     foreach(var,t_list1) 
	  t_list2 = nappend1(t_list2,get_varno(var));
     remove_duplicates (t_list2);

} /* function end  */

/*    
 *    	update-clauses
 *    
 *    	Returns those qualifier clauses which reference ONLY non-update
 *    	relations, i.e., those that are not referenced in the targetlist
 *    
 *    	A var node is existential iff its varno
 *    	(1) does not reference a relation which is referenced
 *    		in the target list, and
 *    	(2) is a number (non-numbers are references to internal 
 *    		results, etc., which are non-existential).
 *    
 *    	Note that clauses without any vars are considered to
 *    	be existential.
 *    
 */
LispValue
update_clauses (update_relids,qual,command)
     LispValue update_relids,qual,command ;
{
     return (LispNil);
}

/*   XXX Close but no cigar.  Turn it off for now.
 *  .. preprocess-qualification
 *  (defun update-clauses (update-relids qual command)
 *    #+opus43 (declare (special update-relids))
 *    (case command
 *  	(delete
 *  	 (collect #'(lambda (clause)
 *  		      #+opus43 (declare (special update-relids))
 *  		      (every #'(lambda (var)
 *  				 #+opus43 (declare (special update-relids))
 *  				 (and (var-p var)
 *  				      (integerp (get_varno var))
 *  				      (not (member (get_varno var)
 *  						   update-relids))))
 *  			     (pull_var_clause clause)))
 *  		  qual))))
 */
