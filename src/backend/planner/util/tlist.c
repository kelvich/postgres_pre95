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

#include "tmp/c.h"

#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "planner/internal.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/pg_lisp.h"
#include "planner/var.h"
#include "planner/tlist.h"
#include "planner/clauses.h"
#include "utils/log.h"

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
tlistentry_member (var,targetlist)
     Var var;
     List targetlist;
{
    extern LispValue lambda1();
    if ( var) {
	LispValue temp = LispNil;
	foreach (temp,targetlist) {
	    if ( var_equal(var, get_expr(get_entry(CAR(temp)))))
	      return(CAR(temp));
	}
    }
    return (LispNil);
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
matching_tlvar (var,targetlist)
     Var var;
     List targetlist;
{
    LispValue tlentry;
    tlentry = tlistentry_member (var,targetlist);
    if ( tlentry ) 
      return((Expr)get_expr (get_entry (tlentry)) );

    return((Expr) NULL);
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
     Rel rel;
     Var var;
     List joinlist ;
{
    Expr oldvar = (Expr)NULL;
    
    oldvar = matching_tlvar (var,get_targetlist (rel));
    
    /* If 'var' is not already in 'rel's target list, add a new node. 
     * Otherwise, put the var with fewer nestings into the target list. 
     */
    
    if ( null( oldvar ) ) {
	LispValue tlist = get_targetlist(rel);
	Var newvar = MakeVar (get_varno (var),
			      get_varattno (var),
			      get_vartype (var),LispNil,
				  get_vararraylist(var),
			      get_varid (var),0);

	set_targetlist (rel,nappend1 (tlist,
				      (LispValue)create_tl_element( newvar,
							 length(tlist) + 1,
							 joinlist)));

    } else if (length (get_varid ((Var)oldvar)) > length (get_varid (var))) {
	
	set_vartype ((Var)oldvar,get_vartype (var));
	set_varid ((Var)oldvar,get_varid (var));

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
     int resdomno;
     List joinlist;
{
    TL tlelement;
    tlelement = lispList();
    
    set_entry (tlelement,
	       MakeTLE (MakeResdom (resdomno, get_vartype (var),
				    get_typlen (get_vartype (var)),
				    (Name)LispNil,
				    (Index)0,
				    (OperatorTupleForm)NULL,0),
			var));
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
    LispValue element = LispNil;
    LispValue result = LispNil;
    
    if (null(tlist)) {
	elog(DEBUG,"calling get_actual_tlist with empty tlist");
	return(LispNil);
    }
    /* XXX - it is unclear to me what exactly get_entry 
       should be doing, as it is unclear to me the exact
       relationship between "TL" "TLE" and joinlists */

    foreach(element,tlist)
      result = nappend1 (result, get_entry (CAR(element)));
    
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
tlist_member (var,tlist)
     Var var;
     List tlist;
{
    LispValue i = LispNil;
    TLE 	temp_tle = (TLE)NULL;
    TLE		tl_elt = (TLE)NULL;

    if ( var) {
	foreach (i,tlist) {
	    temp_tle = (TLE)CAR(i);
	    if (var_equal(var,get_expr(temp_tle))) {
		tl_elt = temp_tle;
		break;
	    }
	}

	if ( consp (tl_elt) ) 
	  return(get_resdom (tl_elt));
	else 
	  return((Resdom)NULL);
    }
    return ((Resdom)NULL);
    
}

/*
 *   Routine to get the resdom out of a targetlist.
 */

Resdom
tlist_resdom(tlist,resnode)
     LispValue tlist;
     Resdom resnode;
{
    Resdom resdom = (Resdom)NULL;
    LispValue i = LispNil;
    TLE temp_tle = (TLE)NULL;

    foreach(i,tlist) {
	temp_tle = (TLE)CAR(i);
	resdom = get_resdom(temp_tle);
	/*  Since resnos are supposed to be unique */
	if (get_resno(resnode) == get_resno(resdom))  
	  return(resdom);
    }
    return((Resdom)NULL);
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
 *      Now checks to make sure array references (in addition to range
 *      table indices) are identical - retrieve (a.b[1],a.b[2]) should
 *      not be turned into retrieve (a.b[1],a.b[1]).
 *    
 */

/*  .. flatten-tlistentry, replace-resultvar-refs
 */

LispValue
match_varid (test_var,tlist)
	Var test_var;
	LispValue tlist;
{
    LispValue i;
    TLE entry = (TLE)NULL;
    oid type_var, type_entry;
    List test_varid;

    test_varid = (List) get_varid(test_var);
    type_var = (oid) get_vartype(test_var);

    foreach (i,tlist)
    {
	entry = CAR(i);
	if (equal((Node)get_varid(get_expr(entry)), (Node)test_varid))
	{
	    if ((get_vartype(get_expr(entry)) == type_var) &&
			(equal( (Node)get_vararraylist(get_expr(entry)),
				(Node)get_vararraylist(test_var))))
	    {
	    	return(entry);
	    }
	}
    }
    return (LispNil);
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
    List new_targetlist = (List)CopyObject (targetlist);
    List x = LispNil;

    foreach (x, new_targetlist) {
	set_reskey (get_resdom (CAR(x)),0);
	set_reskeyop (get_resdom (CAR(x)),(OperatorTupleForm)NULL);
    }
    return(new_targetlist);
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
    LispValue result = LispNil;
    
    foreach (element,targetlist)
      result = nappend1 (result, 
			 lispInteger(get_resno(get_resdom(CAR(element)))));
    
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
    LispValue result = LispNil;
    LispValue src = LispNil;
    LispValue dest = LispNil;
    
    for ( src = source, dest = target; src != LispNil &&
	 dest != LispNil; src = CDR(src), dest = CDR(dest)) {
	LispValue temp = MakeTLE(get_resdom(CAR(dest)),
				 get_expr(CAR(src)));
	result = nappend1(result,temp);
    }
    return(result);
}

/*    
 *    	flatten-tlist
 *    
 *    	Create a target list that only contains unique variables.
 *	-jc--also cons' the agg nodes to the var node list.
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
    List tlist_aggs = LispNil;
    LispValue temp;
    LispValue var;
    LispValue tlist_thing = LispNil;
    LispValue tlist_rest = LispNil;
    LispValue temp_entry = LispNil;
    TLE  full_agg_entry;
    Resdom agg_resdom;
    extern List MakeList();

    foreach(temp,tlist)  {
       temp_entry = CAR(temp);
       tlist_thing = pull_agg_clause(CADR(temp_entry));
       if(tlist_thing != LispNil) {
	 agg_resdom = get_resdom(temp_entry);
	 full_agg_entry = MakeTLE(agg_resdom, tlist_thing);
	 tlist_aggs = (LispValue)nappend1(tlist_aggs, full_agg_entry);
	}
	else {
	   tlist_thing = pull_var_clause ((LispValue)get_expr(temp_entry));
	   if(tlist_thing != LispNil) {
	       tlist_vars = nconc(tlist_vars, tlist_thing);
	    }
	    tlist_rest = nappend1(tlist_rest, temp_entry);
	}
     }
    /* XXX - This used to use the function push and nreverse.  */
    foreach (var,tlist_vars ) {
	if ( !(tlist_member ((Var)CAR(var),new_tlist))) {
	    new_tlist = (LispValue)append1(new_tlist,
				MakeTLE (MakeResdom(++last_resdomno,
						    get_vartype((Var)CAR(var)),
						    get_typlen 
						    (get_vartype 
						     ((Var)CAR(var))),
						    (Name)NULL,
						    (Index)0,
						    (OperatorTupleForm)NULL,0),
					 CAR(var)));
	    
	}
    }
    new_tlist = MakeList( new_tlist, tlist_aggs, tlist_rest, -1); /*jc */
    return(new_tlist);
    
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
			MakeTLE (get_resdom (CAR(x)),
				 flatten_tlistentry ( get_expr (CAR(x)),
						     flat_tlist)));

    return(result);
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
	return((TLE)get_expr (match_varid ((Var)tlistentry, flat_tlist)));
    } 
    else if (single_node (tlistentry)) {
	return(tlistentry);
    } 
    else if (is_funcclause (tlistentry)) {
	LispValue temp_result = LispNil;
	LispValue elt = LispNil;
	
	foreach(elt,get_funcargs(tlistentry)) 
	  temp_result = nappend1(temp_result,
				 flatten_tlistentry(CAR(elt),flat_tlist));
	
	return((TLE)make_funcclause (get_function (tlistentry),
					temp_result));
    } else {
	return((TLE)
	    make_opclause( (Oper)get_op (tlistentry),
			   (Var)flatten_tlistentry( get_leftop(tlistentry),
						    flat_tlist),
			   (Var)flatten_tlistentry( get_rightop(tlistentry),
						    flat_tlist)));
    }
}
