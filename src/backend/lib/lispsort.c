/* #include <stdlib.h> */
#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "nodes/relation.h"
#include "lib/copyfuncs.h"
#include "lib/lispsort.h"

/*
** lisp_qsort: Takes a lisp list as input, copies it into an array of lisp 
**             nodes which it sorts via qsort() with the comparison function
**             as passed into lisp_qsort(), and returns a new list with 
**             the nodes sorted.  The old list is *not* freed or modified (?)
*/

LispValue lisp_qsort(LispValue list,    /* the list to be sorted */
		     int (*compare)())  /* function to compare two nodes */
{
    int num, i;
    LispValue *nodearray;
    LispValue tmp, nodecopy, output;

    /* find size of list */
    for (num = 0, tmp = list; tmp != LispNil; tmp = CDR(tmp))
      num ++;
    if (num < 2) return(list);

    /* copy elements of the list into an array */
    nodearray = (LispValue *) palloc(num * sizeof(LispValue));

    for (tmp = list, i = 0; tmp != LispNil; tmp = CDR(tmp), i++)
      nodearray[i] = lispCopy(CAR(tmp));

    /* sort the array */
    qsort(nodearray, num, sizeof(LispValue), compare);
    
    /* cons together the array elements */
    output = LispNil;
    for (i = num - 1; i >= 0; i--)
      output = lispCons(nodearray[i], output);

    return(output);
}