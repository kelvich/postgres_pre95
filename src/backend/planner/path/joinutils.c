
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

#include "internal.h"
#include "pg_lisp.h"

extern LispValue new_join_pathkey();
extern LispValue new_matching_subkeys();
extern LispValue match_pathkey_joinkeys();

extern Boolean var_equal();

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
	/* XXX - let form, maybe incorrect */
     LispValue matched_joinkeys = LispNil;
     LispValue matched_joinclauses = LispNil;
     LispValue pathkey = LispNil;
     LispValue t_list = LispNil;

     foreach (pathkey, pathkeys) {
	  /* XXX - let form, maybe incorrect */
	  LispValue matched_joinkey_index = 
	    match_pathkey_joinkeys (pathkey,joinkeys,which_subkey);
	  
	  if(integerp (matched_joinkey_index) ) {
	       /* XXX - let form, maybe incorrect */
	       LispValue xjoinkey = nth (matched_joinkey_index,joinkeys);
	       LispValue joinclause = nth (matched_joinkey_index,joinclauses);
	       push (xjoinkey,matched_joinkeys);
	       push (joinclause,matched_joinclauses);
	       joinkeys = remove (xjoinkey,joinkeys);
		
	  } 
	  else {
	       return (LispNil);
	  } 
		
     }
     if ( length (matched_joinkeys) == length (pathkeys) ) {
	  t_list = list (nreverse (matched_joinkeys),
			 nreverse (matched_joinclauses));
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

LispValue
match_pathkey_joinkeys (pathkey,joinkeys,which_subkey)
     LispValue pathkey,joinkeys,which_subkey ;
{
     LispValue path_subkey = LispNil;
     LispValue temp = LispNil;

     foreach(path_subkey,pathkey) {
	  if (temp = position(path_subkey,joinkeys,test(var_equal),
			      extract_subkey(joinkeys, which_subkey))) {
	       /*  XXX fix me !  */
	       return(temp);
	  }
     }
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

/*  .. match-unsorted-outer	 */

LispValue
match_paths_joinkeys (joinkeys,ordering,paths,which_subkey)
     LispValue joinkeys,ordering,paths,which_subkey ;
{
#ifdef 
		opus43;

#endif
		;
		declare (special (joinkeys,ordering,which_subkey));
		find_if (/* XXX - hash-quote */ LispValue

		/* XXX - Move me */ 
		LAMBDA_UNKNOWN_FUNCTION (path)
		    LispValue path ;
		{
#ifdef 
			opus43;

#endif
			;
			declare (special (joinkeys,ordering,which_subkey));
			and (equal_path_path_ordering (ordering,get_ordering (path)),length (joinkeys) == length (get_keys (path)),every (/* XXX - hash-quote */ LispValue

			/* XXX - Move me */ 
			LAMBDA_UNKNOWN_FUNCTION (joinkey,pathkey)
			    LispValue joinkey,pathkey ;
			{
#ifdef 
				opus43;

#endif
				;
				declare (special (which_subkey));
				find (extract_subkey (joinkey,which_subkey),pathkey,test/* XXX - hash-quote */ ,var_equal);
			}
			,joinkeys,get_keys (path)));
		}
		,paths);
	}

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
     LispValue joinkeys,tlist,which_subkey ;
{
     LispValue joinkey = LispNil;
     LispValue t_list = LispNil;
     LispValue temp_node = LispNil;

     foreach(joinkey,joinkeys) {
	  temp_node =
	    list (matching_tlvar (extract_subkey (joinkey,
						  which_subkey),tlist));
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

     foreach(outer_pathkey,outer_pathkeys) {
	  
	  temp_node = 
	    list (new_join_pathkey (outer_pathkey,
				    outer_pathkey,join_rel_tlist,joinclauses));
	  t_list = nconc(t_list,temp_node);
     }
     
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
     LispValue temp_node = LispNil;
     LispValue subkey = LispNil;

     foreach(subkey,subkeys) {

	  LispValue matched_subkeys = 
	    new_matching_subkeys (subkey,considered_subkeys,
				  join_rel_tlist,joinclauses);
	  LispValue tlist_key = matching_tlvar (subkey,join_rel_tlist);
	  LispValue newly_considered_subkeys = LispNil;

	  if ( tlist_key ) {
	       newly_considered_subkeys = adjoin (tlist_key,matched_subkeys);
	  } 
	  else {
	       newly_considered_subkeys = matched_subkeys;
	  } 
	  
	  considered_subkeys = 
	    append (considered_subkeys,newly_considered_subkeys);
	  return(newly_considered_subkeys);
     }

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
     LispValue subkey,considered_subkeys,join_rel_tlist,joinclauses ;
{
     LispValue joinclause = LispNil;
     LispValue t_list = LispNil;
     LispValue temp_node = LispNil;

     foreach(joinclause,joinclauses) {
	  /* XXX - let form, maybe incorrect */
	  LispValue tlist_other_var = 
	    matching_tlvar (other_join_clause_var (subkey,joinclause),
			    join_rel_tlist);

	  if(tlist_other_var && 
	     !(member (tlist_other_var,considered_subkeys))) {
	       push (tlist_other_var,considered_subkeys);
	       list (tlist_other_var);

	  } 
     }
}  /* function end  */
