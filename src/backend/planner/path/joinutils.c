
/*     
 *      FILE
 *     	joinutils
 *     
 *      DESCRIPTION
 *     	Utilities for matching and building join and path keys
 *     
 */

/*  RcsId("$Header$");   */

/*     
 *      EXPORTS
 *     		match-pathkeys-joinkeys
 *     		match-paths-joinkeys
 *     		extract-path-keys
 *     		new-join-pathkeys
 */

#include "pg_lisp.h"
#include "planner/internal.h"
#include "relation.h"
#include "relation.a.h"
#include "plannodes.h"
#include "plannodes.a.h"
#include "planner/joinutils.h"
#include "planner/var.h"
#include "planner/keys.h"
#include "planner/tlist.h"


/*     	===============
 *     	KEY COMPARISONS
 *     	===============
 */

/*    
 *    	match-pathkeys-joinkeys
 *    
 *    	Attempts to match the keys of a path against the keys of join clauses.
 *    	This is done by looking for a matching join key in 'joinkeys' for
 *    	every path key in the list 'pathkeys'. If there is a matching join key
 *    	(not necessarily unique) for every path key, then the list of 
 *    	corresponding join keys and join clauses are returned in the order in 
 *    	which the keys matched the path keys.
 *    
 *    	'pathkeys' is a list of path keys:
 *    		( ( (var) (var) ... ) ( (var) ... ) )
 *    	'joinkeys' is a list of join keys:
 *    		( (outer inner) (outer inner) ... )
 *    	'joinclauses' is a list of clauses corresponding to the join keys in
 *    		'joinkeys'
 *    	'which-subkey' is a flag that selects the desired subkey of a join key
 *    		in 'joinkeys'
 *    
 *    	Returns the join keys and corresponding join clauses in a list if all
 *    	of the path keys were matched:
 *    		( 
 *    		 ( (outerkey0 innerkey0) ... (outerkeyN innerkeyN) )
 *    		 ( clause0 ... clauseN ) 
 *    		)
 *    	and nil otherwise.
 *    
 */

/*  .. match-unsorted-inner, match-unsorted-outer    */

LispValue
match_pathkeys_joinkeys (pathkeys,joinkeys,joinclauses,which_subkey)
     LispValue pathkeys,joinkeys,joinclauses,which_subkey ;
{
     LispValue matched_joinkeys = LispNil;
     LispValue matched_joinclauses = LispNil;
     LispValue pathkey = LispNil;
     LispValue t_list = LispNil;
     LispValue i = LispNil;
     int matched_joinkey_index = -1;

     foreach (i, pathkeys) {
	 pathkey = CAR(i);
	 matched_joinkey_index = 
	   match_pathkey_joinkeys (pathkey,joinkeys,which_subkey);
	 
	 if(matched_joinkey_index != -1 ) {
	     LispValue xjoinkey = nth (matched_joinkey_index,joinkeys);
	     LispValue joinclause = nth (matched_joinkey_index,joinclauses);

	     /* XXX was "push" function */
	     
	     matched_joinkeys = nappend1(matched_joinkeys,xjoinkey);
	     matched_joinkeys = nreverse(matched_joinkeys);

	     matched_joinclauses = nappend1(matched_joinclauses,joinclause);
	     matched_joinclauses = nreverse(matched_joinclauses);

	     joinkeys = remove (xjoinkey,joinkeys);
	     
	 } 
	 else {
	     return (LispNil);
	 } 
		
     }
     if ( length (matched_joinkeys) == length (pathkeys) ) {
	 t_list = lispCons (nreverse (matched_joinkeys),
			    lispCons(nreverse (matched_joinclauses),
				     LispNil));
     } 
     return(t_list);

 }  /* function end  */

/*    
 *    	match-pathkey-joinkeys
 *    
 *    	Returns the 0-based index into 'joinkeys' of the first joinkey whose
 *    	outer or inner subkey matches any subkey of 'pathkey'.
 *    
 */

/*  .. match-pathkeys-joinkeys	 */

int
match_pathkey_joinkeys (pathkey,joinkeys,which_subkey)
     LispValue pathkey,joinkeys,which_subkey ;
{
    LispValue path_subkey = LispNil;
    int pos;
    LispValue i = LispNil;
    LispValue x = LispNil;
    JoinKey jk;

    foreach(i,pathkey) {
	path_subkey = CAR(i);
	pos = 0;
	foreach(x,joinkeys) {
	    jk = (JoinKey)CAR(x);
	    if (var_equal(path_subkey,extract_subkey(jk, which_subkey)))
		return(pos);
	    pos++;
	}
    }
    return (-1);    /* no index found   */

}  /* function end   */

/*    
 *    	match-paths-joinkeys
 *    
 *    	Attempts to find a path in 'paths' whose keys match a set of join
 *    	keys 'joinkeys'.  To match,
 *    	1. the path node ordering must equal 'ordering'.
 *    	2. each subkey of a given path must match (i.e., be (var_equal) to) the
 *    	   appropriate subkey of the corresponding join key in 'joinkeys',
 *    	   i.e., the Nth path key must match its subkeys against the subkey of
 *    	   the Nth join key in 'joinkeys'.
 *    
 *    	'joinkeys' is the list of key pairs to which the path keys must be 
 *    		matched
 *    	'ordering' is the ordering of the (outer) path to which 'joinkeys'
 *    		must correspond
 *    	'paths' is a list of (inner) paths which are to be matched against
 *    		each join key in 'joinkeys'
 *    	'which-subkey' is a flag that selects the desired subkey of a join key
 *    		in 'joinkeys'
 *    
 *    	Returns the matching path node if one exists, nil otherwise.
 *    
 */

/*  function used by match_paths_joinkeys */
bool
every_func (joinkeys, pathkey, which_subkey)
     LispValue joinkeys, pathkey, which_subkey;
{
     LispValue xjoinkey = LispNil;
     LispValue temp = LispNil;
     LispValue tempkey = LispNil;
     bool found = false;
     LispValue i = LispNil;
     LispValue j = LispNil;

     foreach (i,joinkeys) {
	 xjoinkey = CAR(i);
	 found = false;
	 foreach(j,pathkey) {
	     temp = CAR(j);
	     if (temp == LispNil) continue;
	     tempkey = extract_subkey(xjoinkey,which_subkey);
	     if (var_equal(tempkey,temp)) {
		 found = true;
		 break;
	     }
	 }
	 if (found == false)
	   return(false);
     }
     return(found);
}

/*  .. match-unsorted-outer	 */

Path
match_paths_joinkeys (joinkeys,ordering,paths,which_subkey)
     LispValue joinkeys,ordering,paths,which_subkey ;
{
    Path path = (Path)NULL ;
    bool key_match = false;
    LispValue i = LispNil;
 
    foreach(i,paths) {
	path = (Path)CAR(i);
	key_match = every_func(joinkeys,
			       get_keys (path),
			       which_subkey);

	if (equal_path_path_ordering (ordering,
				      get_p_ordering (path)) &&
	    length (joinkeys) == length (get_keys (path)) &&
	    key_match) {
	    return(path);
	}
    }
    return((Path) LispNil);
}  /* function end  */



/*    
 *    	extract-path-keys
 *    
 *    	Builds a subkey list for a path by pulling one of the subkeys from
 *    	a list of join keys 'joinkeys' and then finding the var node in the
 *    	target list 'tlist' that corresponds to that subkey.
 *    
 *    	'joinkeys' is a list of join key pairs
 *    	'tlist' is a relation target list
 *    	'which-subkey' is a flag that selects the desired subkey of a join key
 *    		in 'joinkeys'
 *    
 *    	Returns a list of pathkeys: ((tlvar1)(tlvar2)...(tlvarN)).
 *    
 */

/*  .. hash-inner-and-outer, match-unsorted-inner, match-unsorted-outer
 *  .. sort-inner-and-outer
 */
LispValue
extract_path_keys (joinkeys,tlist,which_subkey)
     LispValue joinkeys,tlist;
     int which_subkey ;
{
    JoinKey xjoinkey = (JoinKey)NULL;
    LispValue t_list = LispNil;
    LispValue temp_node = LispNil;
    LispValue i = LispNil;

    foreach(i,joinkeys) {
	xjoinkey = (JoinKey)CAR(i);
	temp_node =
	  lispCons (matching_tlvar (extract_subkey (xjoinkey,
						    which_subkey),tlist),
		    LispNil);
	t_list = nappend1(t_list,temp_node);
    }
    return(t_list);
} /* function end  */

/*     	=====================
 *     	NEW PATHKEY FORMATION
 *     	=====================
 */

/*    
 *    	new-join-pathkeys
 *    
 *    	Find the path keys for a join relation by finding all vars in the list
 *    	of join clauses 'joinclauses' such that:
 *    		(1) the var corresponding to the outer join relation is a
 *    		    key on the outer path
 *    		(2) the var appears in the target list of the join relation
 *    	In other words, add to each outer path key the inner path keys that
 *    	are required for qualification.
 *    
 *    	'outer-pathkeys' is the list of the outer path's path keys
 *    	'join-rel-tlist' is the target list of the join relation
 *    	'joinclauses' is the list of restricting join clauses
 *    
 *    	Returns the list of new path keys. 
 *    
 */

/*  .. hash-inner-and-outer, match-unsorted-inner, match-unsorted-outer
 *  .. sort-inner-and-outer
 */
LispValue
new_join_pathkeys (outer_pathkeys,join_rel_tlist,joinclauses)
     LispValue outer_pathkeys,join_rel_tlist,joinclauses ;
{
    LispValue outer_pathkey = LispNil;
    LispValue t_list = LispNil;
    LispValue temp_node = LispNil;
    LispValue i = LispNil;

    foreach(i,outer_pathkeys) {
	outer_pathkey = CAR(i);
	temp_node = 
	  lispCons (new_join_pathkey (outer_pathkey,
				      outer_pathkey,
				      join_rel_tlist,joinclauses),
		    LispNil);
	t_list = nconc(t_list,temp_node);
    }
    return(t_list);
     
}

/*    
 *    	new-join-pathkey
 *    
 *    	Finds new vars that become subkeys due to qualification clauses that
 *    	contain any previously considered subkeys.  These new subkeys plus the
 *    	subkeys from 'subkeys' form a new pathkey for the join relation.
 *    
 *    	Note that each returned subkey is the var node found in
 *    	'join-rel-tlist' rather than the joinclause var node.
 *    
 *    	'subkeys' is a list of subkeys for which matching subkeys are to be
 *    		found
 *    	'considered-subkeys' is the current list of all subkeys corresponding
 *    		to a given pathkey
 *    
 *    	Returns a new pathkey (list of subkeys).
 *    
 */

/*  .. new-join-pathkeys  	 */

LispValue
new_join_pathkey (subkeys,considered_subkeys,join_rel_tlist,joinclauses)
     LispValue subkeys,considered_subkeys,join_rel_tlist,joinclauses ;
{
    LispValue t_list = LispNil;
    LispValue subkey = LispNil;
    LispValue i = LispNil;
    LispValue matched_subkeys = LispNil;
    Expr tlist_key = (Expr)NULL;
    LispValue newly_considered_subkeys = LispNil;

    foreach(i,subkeys) {
	subkey = CAR(i);
	if (null(subkey))
	  break;    /* XXX something is wrong */
	matched_subkeys = 
	  new_matching_subkeys (subkey,considered_subkeys,
				join_rel_tlist,joinclauses);
	tlist_key = matching_tlvar (subkey,join_rel_tlist);
	newly_considered_subkeys = LispNil;

	if ( tlist_key ) {
	    /* XXX was "adjoin" function */
	    if (member(tlist_key,matched_subkeys))
	      newly_considered_subkeys = lispCons(lispCons(tlist_key,
							   matched_subkeys),
						  LispNil);
	} 
	else {
	    newly_considered_subkeys = matched_subkeys;
	} 
	
	considered_subkeys = 
	  append (considered_subkeys,newly_considered_subkeys);
	t_list = nconc(t_list,newly_considered_subkeys);
    }
    return(t_list);
    
}  /* function end  */

/*    
 *    	new-matching-subkeys
 *    
 *    	Returns a list of new subkeys:
 *    	(1) which are not listed in 'considered-subkeys'
 *    	(2) for which the "other" variable in some clause in 'joinclauses' is
 *    	    'subkey'
 *    	(3) which are mentioned in 'join-rel-tlist'
 *    
 *    	Note that each returned subkey is the var node found in
 *    	'join-rel-tlist' rather than the joinclause var node.
 *    
 *    	'subkey' is the var node for which we are trying to find matching
 *    		clauses
 *    
 *    	Returns a list of new subkeys.
 *    
 */

/*  .. new-join-pathkey */

LispValue
new_matching_subkeys (subkey,considered_subkeys,join_rel_tlist,joinclauses)
     LispValue considered_subkeys,join_rel_tlist,joinclauses ;
     Var subkey;
{
     LispValue joinclause = LispNil;
     LispValue t_list = LispNil;
     LispValue temp = LispNil;
     LispValue i = LispNil;
     Expr tlist_other_var = (Expr)NULL;

     foreach(i,joinclauses) {
	 joinclause = CAR(i);
	 tlist_other_var = 
	   matching_tlvar (other_join_clause_var (subkey,joinclause),
			   join_rel_tlist);

	 if(tlist_other_var && 
	    !(member (tlist_other_var,considered_subkeys))) {
	     /* XXX was "push" function  */
	     considered_subkeys = nappend1(considered_subkeys,
					   tlist_other_var);
	     /* considered_subkeys = nreverse(considered_subkeys); 
		XXX -- I am not sure of this. */

	     temp = lispCons (tlist_other_var,LispNil);
	     t_list = nconc(t_list,temp);
	 } 
     }
     return(t_list);
 }  /* function end  */
