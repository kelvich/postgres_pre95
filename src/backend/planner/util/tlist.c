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
#include "var.h"
#include "tlist.h"
#include "clauses.h"

/* XXX - find these references */
extern LispValue copy_seq_tree();
extern LispValue tlist_varsnreverse();
extern Var make_var();

static LispValue flatten_tlistentry();

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
tlistentry_member (var,targetlist,test)
     LispValue var,targetlist;
     bool (*test)();
{
	extern LispValue lambda1();
	if ( var != LispNil )
		return ( find (var,targetlist,lambda1,test) );
}

/* called by tlistentry-member */
static
LispValue lambda1 (x)
     LispValue x ;
{
	tl_expr (get_tlelement (x));
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
LispValue
matching_tlvar (var,targetlist,test)
     LispValue var,targetlist;
     bool (*test)();
{
	LispValue tlentry;

	tlentry = tlistentry_member (var,targetlist,test);
	if ( tlentry ) 
		return(tl_expr (get_tlelement (tlentry)) );
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
    LispValue oldvar;
    
    oldvar = matching_tlvar (var,get_tlist (rel));
    
    /* If 'var' is not already in 'rel's target list, add a new node. 
     * Otherwise, put the var with fewer nestings into the target list. 
     */
    
    if ( null( oldvar ) ) {
	Var newvar = make_var (get_varno (var),
			       get_varattno (var),
			       get_vartype (var),LispNil,
			       (consider_vararrayindex(var) ?
					      varid_array_index(var) :
				0 ),
			       get_varid (var));

	set_tlist (rel,append1 (get_tlist (rel),
				create_tl_element (newvar,
						   tlist_size(rel) -1,
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
	tlelement = CreateNode(TL);

	set_entry (tlelement,
		       list (make_resdom (resdomno, get_vartype (var),
					  get_typlen (get_vartype (var)),
					  LispNil,0,LispNil),var));
	set_joinlist (tlelement,joinlist);

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
	  result = nappend1 (result, get_tlelement (element));

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

LispValue
tlist_member (var,tlist,dots,key,test)
     LispValue var,tlist,dots,key;
     bool (*test)();
{
	/* declare (special (dots)); */

	if ( var != LispNil ) {
		LispValue tl_elt = find (var,tlist,
				     key , tl_expr /* func */,
				     test, var_equal /* func */);
		if ( consp (tl_elt) ) 
		  return(tl_resdom (tl_elt));
		else 
		  return(LispNil);
	}
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
	get_varid (tl_expr (x));
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
		set_reskey (tl_resdom (x),0);
		set_reskeyop (tl_resdom (x),LispNil);
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
	  result = nappend1 (result, get_resno (tl_Resdom (element)));

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
	     dest != LispNil; src = CDR(src), dest = CDR(dest)) 
	  result = nappend1(result,list(tl_resdom(dest),tl_expr(src)));

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
    
    foreach(temp,tlist)
      pull_var_clause (tl_expr(temp));
    
    foreach (var,tlist_varsnreverse (new_tlist)) {
	if ( !(tlist_member (var,new_tlist,1 /* XXX - true */))) {
	    push (new_tl (make_resdom(last_resdomno++ ,
				      get_vartype (var),
				      get_typlen (get_vartype (var)),
				      LispNil,0,LispNil),var),new_tlist);
	}
    }
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
				  new_tl (tl_resdom (x),
					  flatten_tlistentry (tl_expr (x),
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
 *    	Returns the (modified) node from the target list.
 *    
 */

/*  .. flatten-tlist-vars, flatten-tlistentry
 */
static LispValue
flatten_tlistentry (tlistentry,flat_tlist)
     LispValue tlistentry,flat_tlist ;
{
	if(null (tlistentry)) {
		return(LispNil);
	} 
	else if (var_p (tlistentry)) {
		return(tl_expr (match_varid (get_varid (tlistentry),
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

		return(make_funcclause (get_function (tlistentry),
					temp_result));
	} else {
		return(make_clause (get_op (tlistentry),
			     flatten_tlistentry (get_leftop (tlistentry),
						 flat_tlist),
			     flatten_tlistentry (get_rightop (tlistentry),
						 flat_tlist)));
	}
}
