
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
#include "relation.h"
#include "relation.a.h"
#include "hashutils.h"
#include "clauses.h"

/* extern LispValue match_hashop_hashinfo(); */

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
     LispValue clauseinfo = LispNil;
     foreach (clauseinfo,clauseinfo_list) {
	  
	  ObjectId hashjoinop = get_hashjoinoperator(clauseinfo);
	  
	  /*    Create a new hashinfo node and add it to */
	  /*    'hashinfo-list' if one does not yet */
	  /*    exist for this hash operator.   */

	  if (hashjoinop ) {
	       CInfo xhashinfo;
	       Expr clause = get_clause (clauseinfo);
	       Var leftop = get_leftop (clause);
	       Var rightop = get_rightop (clause);
	       JoinKey keys;
	       xhashinfo = 
		 match_hashop_hashinfo (hashjoinop,hashinfo_list);
	       
	       if (equal(inner_relid,get_varno (leftop))) {
		    keys = MakeJoinKey(rightop,
					leftop);
	       }
	       else {
		    keys = MakeJoinKey(leftop,
					rightop);
	       }
		     
	       if ( null(xhashinfo)) {
		    xhashinfo = CreateNode(CInfo);
		    set_hashjoinoperator(xhashinfo,
					 MakeCInfo(hashjoinop));
		    push (xhashinfo,hashinfo_list);
	       }
	       push (clause,joinmethod_clauses (xhashinfo));
	       push (keys,joinmethod_keys (xhashinfo));
	       
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

CInfo
match_hashop_hashinfo (hashop,hashinfo_list)
     LispValue hashop,hashinfo_list ;
{
     ObjectId key ;
     LispValue xhashinfo;

	/* XXX -- Lisp find and lambda function --- maybe wrong */
     foreach( xhashinfo, hashinfo_list) {
	  key = get_hashjoinoperator((CInfo)xhashinfo);
	  if (equal(hashop,key)) {  /* found */
	       return((CInfo)xhashinfo);    /* should be a hashinfo node ! */
		}
	}
	return((CInfo)LispNil);
}
