/*     
 *      FILE
 *     	tlist
 *     
 *      DESCRIPTION
 *     	Target list manipulation routines
 *     
 */

static char *rcsid = "$Header$";

/*     
 *      EXPORTS
 *     		tlistentry-member
 *     		matching_tlvar
 *     		add_tl_element
 *     		create_tl_element
 *     		get-actual-tlist
 *     		tlist-member
 *     		match_varid
 *     		new-unsorted-tlist
 *     		targetlist-resdom-numbers
 *     		copy-vars
 *     		flatten-tlist
 *     		flatten-tlist-vars
 */

#include "c.h"
#include "relation.h"
#include "relation.a.h"
#include "internal.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "pg_lisp.h"
#include "var.h"
#include "tlist.h"
#include "clauses.h"

/* XXX - find these references */
extern LispValue copy_seq_tree();


static TLE flatten_tlistentry();

/*    	---------- RELATION node target list routines ----------
 */

/*    
 *    	tlistentry-member
 *    
 * NOTES:    normally called with test = (*var_equal)()
 *
 * RETURNS:  the leftmost member of sequence "targetlist" that satisfies
 *           the predicate "test"
 * MODIFIES: nothing
 * REQUIRES: test = function which can operate on a lispval union
 *           var = valid var-node
 *	     targetlist = valid sequence
 */

LispValue
tlistentry_member (var,targetlist,var_equal)
     Var var;
     List targetlist;
     bool (*var_equal)();
{
    extern LispValue lambda1();
	if ( var) 
	  return ( find(var,targetlist,var_equal,lambda1));
}

/* called by tlistentry-member */
static
LispValue lambda1 (x)
     LispValue x ;
{
	get_expr (get_entry (x));
}


/*    
 * matching_tlvar
 *    
 * NOTES:    "test" should normally be var_equal()
 * RETURNS:  var node in a target list which is var_equal to 'var',
 *    	     if one exists.
 * REQUIRES: "test" operates on lispval unions,
 * 
 */

/*  .. add_tl_element, collect-index-pathkeys, extract-path-keys
 *  .. new-join-pathkey, new-matching-subkeys
 */

Expr
matching_tlvar (var,targetlist,test)
     Var var;
     List targetlist;
     bool (*test)();
{
	LispValue tlentry;

	tlentry = tlistentry_member (var,targetlist,test);
	if ( tlentry ) 
		return((Expr)get_expr (get_entry (tlentry)) );
}

/*    
 *    	add_tl_element
 *    
 *    	Creates a targetlist entry corresponding to the supplied var node
 *    	'var' and adds the new targetlist entry to the targetlist field of
 *    	'rel' with the joinlist field set to 'joinlist'.
 *    
 * RETURNS: nothing
 * MODIFIES: vartype and varid fields of leftmost varnode that matches
 *           argument "var" (sometimes).
 * CREATES:  new var-node iff no matching var-node exists in targetlist
 */

/*  .. add-vars-to-rels, initialize-targetlist
 */
LispValue
add_tl_element (rel,var,joinlist)
     LispValue rel,var,joinlist ;
{
    Expr oldvar;
    
    oldvar = matching_tlvar (var,get_targetlist (rel));
    
    /* If 'var' is not already in 'rel's target list, add a new node. 
     * Otherwise, put the var with fewer nestings into the target list. 
     */
    
    if ( null( oldvar ) ) {
	Var newvar = MakeVar (get_varno (var),
			       get_varattno (var),
			       get_vartype (var),LispNil,
			       (consider_vararrayindex(var) ?
					      varid_array_index(var) :
				0 ),
			       get_varid (var));

	set_targetlist (rel,nappend1 (get_targetlist (rel),
				create_tl_element (newvar,
						   get_size(rel) -1,
						   joinlist)));

    } else if (length (get_varid (oldvar)) > length (get_varid (var))) {

	set_vartype (oldvar,get_vartype (var));
	set_varid (oldvar,get_varid (var));

    } 
}

/*    
 *    	create_tl_element
 *    
 *    	Creates a target list entry node and its associated (resdom var) pair
 *    	with its resdom number equal to 'resdomno' and the joinlist field set
 *    	to 'joinlist'.
 *    
 * RETURNS:  newly created tlist-entry
 * CREATES:  new targetlist entry (always).
 */

/*  .. add_tl_element, new-join-tlist
 */

TL
create_tl_element (var,resdomno,joinlist)
     Var var;
     Resdom resdomno;
     List joinlist;
{
	TL tlelement;
	tlelement = lispList();
/*
	set_entry (tlelement,
		   MakeTLE (MakeResdom (resdomno, get_vartype (var),
					get_typlen (get_vartype (var)),
					  LispNil,0,LispNil),
			    var));
	set_joinlist (tlelement,joinlist);
*/
	return(tlelement);
}

/*    
 *    	get-actual-tlist
 *    
 *    	Returns the targetlist elements from a relation tlist.
 *    
 */

/*  .. compute-rel-width, create_plan
 */
LispValue
get_actual_tlist (tlist)
     LispValue tlist ;
{
	LispValue element;
	LispValue result;

	for (element = tlist; element != LispNil; element = CDR(element) )
	  result = nappend1 (result, get_entry (element));

	return(result);
}

/*    	---------- GENERAL target list routines ----------
 */

/*    
 *    	tlist-member
 *    
 *    	Determines whether a var node is already contained within a
 *    	target list.
 *    
 *    	'var' is the var node
 *    	'tlist' is the target list
 *    	'dots' is t if we must match dotfields to determine uniqueness
 *    
 *    	Returns the resdom entry of the matching var node.
 *    
 */

/*  .. flatten-tlist, replace-joinvar-refs, replace-nestvar-refs
 *  .. set-temp-tlist-operators
 */

Resdom
tlist_member (var,tlist,dots,key,test)
     Var var;
     List tlist,dots,key;
     bool (*test)();
{
    /* declare (special (dots)); */
/*    
    if ( var) {
	TLE tl_elt = (TLE)find (var,tlist,
				key , get_expr ,
				test, var_equal );
	if ( consp (tl_elt) ) 
	  return(get_resdom (tl_elt));
	else 
	  return((Resdom)NULL);
    }
*/
}

/*    
 *    	match_varid
 *    
 *    	Searches a target list for an entry with some desired varid.
 *    
 *    	'varid' is the desired id
 *    	'tlist' is the target list that is searched
 *    
 *    	Returns the target list entry (resdom var) of the matching var.
 *    
 */

/*  .. flatten-tlistentry, replace-resultvar-refs
 */
static 
LispValue lambda2(x)
     LispValue x ;
{
	get_varid (get_expr (x));
}

LispValue
match_varid (varid,tlist,key)
     LispValue varid,tlist,key ;
{
	return( find (varid,tlist,key, lambda2));
}


/*    
 *    	new-unsorted-tlist
 *    
 *    	Creates a copy of a target list by creating new resdom nodes
 *    	without sort information.
 *    
 *    	'targetlist' is the target list to be copied.
 *    
 *    	Returns the resulting target list.
 *    
 */

/*  .. make_temp, sort-level-result
 */
List
new_unsorted_tlist (targetlist)
     List targetlist ;
{
	List new_targetlist = copy_seq_tree (targetlist);
	List x = NULL;

	foreach (x, new_targetlist) {
		set_reskey (get_resdom (x),0);
		set_reskeyop (get_resdom (x),LispNil);
	}
}

/*    
 *    	targetlist-resdom-numbers
 *    
 *    	Returns a list containing all resdom numbers in 'targetlist'.
 *    
 */

/*  .. expand-targetlist, write-decorate
 */
LispValue
targetlist_resdom_numbers (targetlist)
     LispValue targetlist ;
{
	LispValue element;
	LispValue result;
	
	for(element = targetlist; element != LispNil; element = CDR(element)) 
	  result = nappend1 (result, 
			     lispInteger(get_resno (get_resdom (element))));

	return (result);
}

/*    
 *    	copy-vars
 *    
 *    	Replaces the var nodes in the first target list with those from
 *    	the second target list.  The two target lists are assumed to be
 *    	identical except their actual resdoms and vars are different.
 *    
 *    	'target' is the target list to be replaced
 *    	'source' is the target list to be copied
 *    
 *    	Returns a new target list.
 *    
 */

/*  .. set-temp-tlist-references
 */
LispValue
copy_vars (target,source)
     LispValue target,source ;
{
    LispValue result, src, dest;
    
    for ( src = source, dest = target; src != LispNil &&
	 dest != LispNil; src = CDR(src), dest = CDR(dest)) {
	LispValue temp = MakeTLE(get_resdom(dest),
				 get_expr(src));
	result = nappend1(result,temp);
    }
    return(result);
}

/*    
 *    	flatten-tlist
 *    
 *    	Create a target list that only contains unique variables.
 *    
 *    	XXX Resdom nodes are numbered based on the length of the target list -
 *    	    this hasn't caused problems yet, but....
 *    
 *    	'tlist' is the current target list
 *    
 *    	Returns the new target list.
 *    
 */

/*  .. query_planner
 */
LispValue
flatten_tlist (tlist)
     LispValue tlist ;
{
    int last_resdomno = 0;
    List new_tlist = LispNil;
    LispValue tlist_vars = LispNil;
    LispValue temp;
    LispValue var;

    return(tlist);
/*
    foreach(temp,tlist) 
      tlist_vars = nconc(tlist_vars,
			 pull_var_clause (get_expr(CAR(temp))));
    
    foreach (var,tlist_vars ) {
	if ( !(tlist_member (CAR(var),new_tlist,1 ))) {
	    push (MakeTLE ( MakeResdom(last_resdomno++ ,
				       get_vartype (var),
				       get_typlen (get_vartype (var)),
				       LispNil,0,LispNil),var),
		  new_tlist);
	}
    }
    return(nreverse(new_tlist));
*/

}
    
/*    
 *    	flatten-tlist-vars
 *    
 *    	Redoes the target list of a query with no nested attributes by
 *    	replacing vars within computational expressions with vars from
 *    	the 'flattened' target list of the query.
 *    
 *    	'full-tlist' is the actual target list
 *    	'flat-tlist' is the flattened (var-only) target list
 *    
 *    	Returns the modified actual target list.
 *    
 */

/*  .. query_planner
 */
LispValue
flatten_tlist_vars (full_tlist,flat_tlist)
     LispValue full_tlist,flat_tlist ;
{
	LispValue x = LispNil;
	LispValue result = LispNil;

	foreach(x,full_tlist)
		result = nappend1(result,
				  MakeTLE (get_resdom (x),
					  flatten_tlistentry (get_expr (x),
							      flat_tlist)));
}

/*    
 *    	flatten-tlistentry
 *    
 *    	Replaces vars within a target list entry with vars from a flattened
 *    	target list.
 *    
 *    	'tlistentry' is the target list entry to be modified
 *    	'flat-tlist' is the flattened target list
 *    
 *    	Returns the (modified) target_list entry from the target list.
 *    
 */

/*  .. flatten-tlist-vars, flatten-tlistentry
 */

static TLE
flatten_tlistentry (tlistentry,flat_tlist)
     TLE tlistentry,flat_tlist ;
{
	if(null (tlistentry)) {
		return((TLE)NULL);
	} 
	else if (IsA (tlistentry,Var)) {
		return((TLE)get_expr (match_varid (get_varid (tlistentry),
					     flat_tlist)));
	} 
	else if (single_node (tlistentry)) {
		return(tlistentry);
	} 
	else if (is_funcclause (tlistentry)) {
		LispValue temp_result = LispNil;
		LispValue elt = LispNil;

		foreach(elt,get_funcargs(tlistentry)) 
		  temp_result = nappend1(temp_result,
				    flatten_tlistentry(elt,flat_tlist));

		return((TLE)make_funcclause (get_function (tlistentry),
					temp_result));
	} else {
		return((TLE)make_clause (get_op (tlistentry),
			     flatten_tlistentry (get_leftop (tlistentry),
						 flat_tlist),
			     flatten_tlistentry (get_rightop (tlistentry),
						 flat_tlist)));
	}
}
