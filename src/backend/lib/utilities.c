
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

#include <math.h>
#include "nodes/pg_lisp.h"
#include "tmp/c.h"
#include "utils/log.h"
#include "nodes/nodes.h"

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
	  retval = nappend1(retval,CAR(temp));
    }
    return(retval);
}

/*    
 *    	same
 *    
 *    	Returns t if two lists contain the same elements.
 *       now defined in lispdep.c
 *    
 */

/*  .. best-innerjoin, prune-joinrel
 */



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
    LispValue new_seqtree;
    LispValue elem, new_elem;
    LispValue i;

    /*
     * handle the trivial case first.
     */
    if (null(seqtree))
	return(LispNil);

    switch(seqtree->type) {
	case PGLISP_ATOM:
	    new_seqtree = lispInteger(CAtom(seqtree));
	    NodeSetTag(new_seqtree,classTag(LispSymbol));
	    break;
	case PGLISP_VECI:
	    elog(WARN,"copying vectors unsupported");
	    break;
	case PGLISP_FLOAT:
	    elog(WARN,"copying floats unsupported");
	    break;
	case PGLISP_INT:
	    new_seqtree = lispInteger(CInteger(seqtree));
	    break;
	case PGLISP_STR:
	    new_seqtree = lispString(CString(seqtree));
	    break;
	case PGLISP_DTPR:
	    /*
	     * recursivelly handle all the list elements...
	     */
	    new_seqtree = LispNil;
	    foreach (i,seqtree) {
		elem = CAR(i);
		new_elem = copy_seq_tree(elem);
		new_seqtree = nappend1(new_seqtree,new_elem);
	    }
	    break;
	default:
	    new_seqtree = (LispValue)NewNode(
				NodeTagGetSize(NodeType(seqtree)),
						NodeType(seqtree));
	    /*
	     * XXX!
	     * WARNING! we'd better use the copy functions here!
	     */
	    bcopy(seqtree,new_seqtree, NodeTagGetSize(NodeType(seqtree)));
	    break;
    } /*switch*/

    return(new_seqtree);
}


double
base_log(x,b)
     double x,b;
{
    return(log(x)/log(b));
}

max(foo,bar)
     int foo,bar;
{
    if (foo > bar)
      return(foo);
    else
      return(bar);
}

