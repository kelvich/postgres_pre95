
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

    for (temp = list; temp != LispNil; temp = CDR(temp)) {
	if (pred(temp))
	  retval = nappend1(retval,temp);
    }
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
    return(seqtree);
}


base_log(foo)
     double foo;
{
    return(0);
}

max(foo,bar)
     int foo,bar;
{
    if (foo > bar)
      return(foo);
    else
      return(bar);
}

