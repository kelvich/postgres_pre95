
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

#include "pg_lisp.h"
#include "internal.h"

extern LispValue match_hashop_hashinfo();

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
	/* XXX - let form, maybe incorrect */
	LispValue hashinfo_list = LispNil;
	LispValue clauseinfo;
	for (clauseinfo = car(clauseinfo_list); clauseinfo_list != LispNil;
	     clauseinfo_list = cdr(clauseinfo_list)) {
		/* XXX - let form, maybe incorrect */

		LispValue hashjoinop = get_hashjoinoperator(clauseinfo);

		/*    Create a new hashinfo node and add it to */
		/*    'hashinfo-list' if one does not yet */
		/*    exist for this hash operator.   */

		if (hashjoinop == LispTrue ) {
			LispValue hashinfo = 
			  match_hashop_hashinfo (hashjoinop,hashinfo_list);
			LispValue clause = get_clause (clauseinfo);
			LispValue leftop = get_leftop (clause);
			LispValue rightop = get_rightop (clause);
			LispValue keys = LispNil;
			if (equal(inner_relid,get_varno (leftop))) {
				keys = make_joinkey(outer(rightop),
						    inner(leftop));
			}
			else {
				keys = make_joinkey(outer(leftop),
						    inner(rightop));
			}
			
			if ( null(hashinfo)) {
				hashinfo = make_hashinfo(op(hashjoinop));
				push (hashinfo,hashinfo_list);
			}
			push (clause,joinmethod_clauses (hashinfo));
			push (keys,joinmethod_keys (hashinfo));

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

LispValue
match_hashop_hashinfo (hashop,hashinfo_list)
     LispValue hashop,hashinfo_list ;
{
	LispValue key = LispNil;
	LispValue hashinfo = LispNil;

	/* XXX -- Lisp find and lambda function --- maybe wrong */
	for( hashinfo = car(hashinfo_list); hashinfo_list != LispNil;
	    hashinfo_list = cdr (hashinfo_list)) {
		key = hashinfo_op(hashinfo);
		if (equal(hashop,key)) {  /* found */
			return(hashinfo);    /* should be a hashinfo node ! */
		}
	}
	return(LispNil);
}
