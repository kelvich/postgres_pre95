
/*     
 *      FILE
 *     	hashutils
 *     
 *      DESCRIPTION
 *     	Utilities for finding applicable merge clauses and pathkeys
 *     
 */
/* RcsId ("$Header$"); */

/*     
 *      EXPORTS
 *     		group-clauses-by-hashop
 *     		match-hashop-hashinfo
 */

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "planner/internal.h"
#include "planner/hashutils.h"
#include "planner/clauses.h"

/* ----------------
 *	HInfo creator declaration
 * ----------------
 */
extern HInfo RMakeHInfo();

/*    
 *    	group-clauses-by-hashop
 *    
 *    	If a join clause node in 'clauseinfo-list' is hashjoinable, store
 *    	it within a hashinfo node containing other clause nodes with the same
 *    	hash operator.
 *    
 *    	'clauseinfo-list' is the list of clauseinfo nodes
 *    	'inner-relid' is the relid of the inner join relation
 *    
 *    	Returns the new list of hashinfo nodes.
 *    
 */

/*  .. find-all-join-paths */

LispValue
group_clauses_by_hashop (clauseinfo_list,inner_relid)
     LispValue clauseinfo_list,inner_relid ;
{
    LispValue hashinfo_list = LispNil;
    CInfo clauseinfo = (CInfo)NULL;
    LispValue i = LispNil;
    LispValue temp = LispNil;
    LispValue temp2 = LispNil;
    ObjectId hashjoinop = 0;
    
    foreach (i,clauseinfo_list) {
	clauseinfo = (CInfo)CAR(i);
	hashjoinop = get_hashjoinoperator(clauseinfo);
	
	/*    Create a new hashinfo node and add it to */
	/*    'hashinfo-list' if one does not yet */
	/*    exist for this hash operator.   */
	
	if (hashjoinop ) {
	    HInfo xhashinfo = (HInfo)NULL;
	    Expr clause = get_clause (clauseinfo);
	    Var leftop = get_leftop (clause);
	    Var rightop = get_rightop (clause);
	    JoinKey keys = (JoinKey)NULL;
	    
	    xhashinfo = 
	      match_hashop_hashinfo (hashjoinop,hashinfo_list);
	    
	    if (CInteger(inner_relid) == get_varno (leftop)){
		keys = MakeJoinKey(rightop,
				   leftop);
	    }
	    else {
		keys = MakeJoinKey(leftop,
				   rightop);
	    }
	    
	    if ( null(xhashinfo)) {
		xhashinfo = RMakeHInfo();
		set_hashop(xhashinfo,
			   hashjoinop);

		set_jmkeys(xhashinfo,LispNil);
		set_clauses(xhashinfo,LispNil);

/*		set_clause(xhashinfo,(Expr)NULL);
		set_selectivity(xhashinfo,0);
		set_notclause(xhashinfo,false);
		set_indexids(xhashinfo,LispNil);
		set_mergesortorder(xhashinfo,(MergeOrder)NULL);
 */

		/* XXX was push  */
		hashinfo_list = nappend1(hashinfo_list,xhashinfo);
		hashinfo_list = nreverse(hashinfo_list);
	    }

	    /* XXX was "push" function  */
	    /* push (clause,joinmethod_clauses (xhashinfo));
	     *  push (keys,joinmethod_keys (xhashinfo));
	     */
	    
	    if (null(get_clauses(xhashinfo)))
	      set_clauses(xhashinfo,lispCons(clause,LispNil));
	    else {
		temp = lispList();
		CAR(temp) = CAR(get_clauses(xhashinfo));
		CDR(temp) = CDR(get_clauses(xhashinfo));
		CDR(get_clauses(xhashinfo)) = temp;
		CAR(get_clauses(xhashinfo)) = (LispValue)clause;
	    }
	    if (null(get_jmkeys(xhashinfo)))
	      set_jmkeys(xhashinfo,lispCons(keys,LispNil));
	    else {
		temp2 = lispList();
	    
		CAR(temp2) = CAR(get_jmkeys(xhashinfo));
		CDR(temp2) = CDR(get_jmkeys(xhashinfo));
		CDR(get_jmkeys(xhashinfo)) = temp2;
		CAR(get_jmkeys(xhashinfo)) = (LispValue)keys;
	    }

/*	    temp2 = get_jmkeys(xhashinfo);
	    temp2 = nappend1(temp2,keys);
	    temp2 = nreverse(temp2);
 */	    
	}
    }
    return(hashinfo_list);
} /* function end */


/*    
 *    	match-hashop-hashinfo
 *    
 *    	Searches the list 'hashinfo-list' for a hashinfo node whose hash op
 *    	field equals 'hashop'.
 *    
 *    	Returns the node if it exists.
 *    
 */

/*  .. group-clauses-by-hashop */

HInfo
match_hashop_hashinfo (hashop,hashinfo_list)
     LispValue hashinfo_list ;
     ObjectId hashop;
{
    ObjectId key = 0;
    HInfo xhashinfo = (HInfo)NULL;
    LispValue i = LispNil;
    
    /* XXX -- Lisp find and lambda function --- maybe wrong */
    foreach( i, hashinfo_list) {
	xhashinfo = (HInfo)CAR(i);
	key = get_hashop(xhashinfo);
	if (equal(hashop,key)) {  /* found */
	    return(xhashinfo);    /* should be a hashinfo node ! */
	}
    }
    return((HInfo)LispNil);
}
