
/*     
 *      FILE
 *     	utilities
 *     
 *      DESCRIPTION
 *     	Utility routines for strings, vectoris, etc.
 *	$Header$
 *     
 *      EXPORTS
 *     		vectori-long-to-list
 *     		collect
 *     		same
 *     		last-element
 *     		number-list
 *     		mesg
 *     		copy-seq-tree
 *     		base-log
 *     		key-sort
 *     		current-process-identifier
 *     		current-user-identifier
 *     		add
 *     		sub
 *     		mul
 *     		div
 */

#include "pg_lisp.h"
#include "c.h"
#include "log.h"

/*     	=================
 *     	GENERAL UTILITIES
 *     	=================
 *     	
 *      - LISP-independent routines
 *     	
 */

/*    
 *    	vectori-long-to-list
 *    
 *    	Put the first 'n' non-zero longword values in 'vectori', starting at
 *    	vectori location 'start', into a list. 
 *    
 *    	XXX No error-checking is done, so if 'n' <= 0 or either argument is
 *    	    out-of-bounds for the vectori, you lose.
 *    
 *    	Returns the list of extracted values.
 *    
 */

/*  .. index-info
 */

LispValue
vectori_long_to_list (vectori,start,n)
     int *vectori;
     int start,n ;
{
    LispValue retval = LispNil;
    int i,j;

    for (i = start, j = 1; j < n ; j++, i++) {
	retval = nappend1(retval,lispInteger(vectori[i]));
    }
    return(retval);
}

/*    
 *    	collect
 *    
 *    	Collects all members of 'list' that satisfy 'predicate'.
 *    
 */

/*  .. make-parameterized-plans, new-level-qual, pull-constant-clauses
 *  .. pull-relation-level-clauses, update-clauses
 */

LispValue
collect (pred,list)
     bool (*pred)();
     LispValue list ;
{
    LispValue retval  = LispNil;
    LispValue temp = LispNil;

    foreach (temp,list) {
	if ((* pred)(CAR(temp)))
	  retval = nappend1(retval,temp);
    }
    return(retval);
}

/*    
 *    	same
 *    
 *    	Returns t if two lists contain the same elements.
 *    
 */

/*  .. best-innerjoin, prune-joinrel
 */

LispValue
same (list1,list2)
     LispValue list1,list2 ;
{
    elog(WARN,"'same' is an unsuppported function");
/*
    if ((length(list1) == length(list2)) &&
	(length(list1) == subset(list1,list2)))
      return(true);
    else
      return(false);
*/
}

/*    
 *    	last-element
 *    
 *    	Returns the last list member (not the last cons cell, like last).
 *    
 */

LispValue
last_element (list)
     LispValue list ;
{
    return (CAR ((LispValue)last (list)));
}

/*    
 *    	copy-seq-tree
 *    
 *    	Returns a copy of a tree of sequences.
 *    
 */

/*  .. fix-rangetable, make-rule-plans, new-rangetable-entry
 *  .. new-unsorted-tlist, plan-union-query, print_plan, process-rules
 *  .. subst-rangetable
 */

LispValue 
copy_seq_tree (seqtree)
     LispValue seqtree;
{
    LispValue new_seq = LispNil;
    LispValue new_elem = LispNil;
    LispValue elem = LispNil;
    LispValue i = LispNil;

    if (IsA(seqtree,LispList))
      foreach (i,seqtree) {
	  LispValue elem = CAR(i);
	  switch(elem->type) {
	    case PGLISP_ATOM:
	      new_elem = lispAtom(CAtom(elem));
	      break;
	    case PGLISP_DTPR:
	      elog(NOTICE,"sequence is more than one deep !");
	      new_elem = copy_seq_tree(elem);
	      break;
	    case PGLISP_VECI:
	      elog(WARN,"copying vectors unsupported");
	      break;
	    case PGLISP_FLOAT:
	      elog(WARN,"copying floats unsupported");
	      break;
	    case PGLISP_INT:
	      new_elem = lispInteger(CInteger(elem));
	      break;
	    case PGLISP_STR:
	      new_elem = lispString(CString(elem));
	      break;
	    default:
	      elog(NOTICE,"copying non-lisp type");
	      new_elem = (LispValue)palloc(sizeof(*elem));
	      bcopy(new_elem,elem,sizeof(*elem));
	      break;
	  }
	  new_seq = nappend1(new_seq,new_elem);
      }

    return(new_seq);
}


base_log(foo)
     double foo;
{
    elog(NOTICE,"base_log, unsupported function returns 0");
    return(0.0);
}

max(foo,bar)
     int foo,bar;
{
    if (foo > bar)
      return(foo);
    else
      return(bar);
}

