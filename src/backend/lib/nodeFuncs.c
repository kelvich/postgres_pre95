
/*    
 *    	nodeFuncs
 *    
 *    	All node routines more complicated than simple access/modification
 *    	$Header$
 */

#include "c.h"
#include "primnodes.h"
#include "nodes.h"
#include "pg_lisp.h"
#include "nodeFuncs.h"
#include "keys.h"

/* XXX - find what this really means */
extern LispValue last_element();


/*    
 *    	single_node
 *    
 *    	Returns t if node corresponds to a single-noded expression
 *    
 */

/*  .. contains-not, fix-opid, flatten-tlistentry, nested-clause-p
 *  .. pull_var_clause, relation-level-clause-p, replace-clause-joinvar-refs
 *  .. replace-clause-nestvar-refs, replace-clause-resultvar-refs
 *  .. valid-or-clause
 */
bool
single_node (node)
     Node node ;
{
    if(atom (node) || constant_p (node) || var_p (node))
      return(true);
    else
      return(false);
}

/*    	=========
 *    	VAR nodes
 *    	=========
 */

/*    
 *    	var_is_outer
 *    	var_is_inner
 *    	var_is_mat
 *    	var_is_rel
 *    
 *    	Returns t iff the var node corresponds to (respectively):
 *    	the outer relation in a join
 *    	the inner relation of a join
 *    	a materialized relation
 *    	a base relation (i.e., not an attribute reference, a variable from
 *    		some lower join level, or a sort result)
 *    
 */

/*  .. ExecEvalVar, switch_outer, var_is_rel
 */

bool
var_is_outer (var)
     Var var ;
{
    return((bool)(get_varno (var) == OUTER));
}

/*  .. ExecEvalVar, var_is_rel
 */
bool
var_is_inner (var)
     Var var ;
{
    return ( (bool) (get_varno (var) == INNER));
}

/*  .. get_relattval, get_relsatts
 */

bool
var_is_mat (var)
     Var var ;
{
    return((bool)listp (get_varno (var)));
}

/*  .. ExecEvalVar
 */
bool
var_is_rel (var)
     Var var ;
{
    return ((bool) (atom (get_varno (var)) && 
		    ! (var_is_inner (var) || 
		       var_is_outer (var))));
}

/*  .. consider_vararrayindex, nested-clause-p, new-level-tlist
 *  .. relation-sortkeys, replace-nestvar-refs, var_equal
 */
bool
var_is_nested (var)
     Var var ;
{
    if ( get_vardotfields (var) != NULL)
      return(true);
    else
      return(false);
}

/*  .. consider_vararrayindex, numlevels
 */
bool
varid_indexes_into_array (var)
     Var var ;
{
    return((bool)listp (last_element (get_varid (var))));
}

/*  .. add_tl_element, replace-nestvar-refs
 */
bool
varid_array_index (var)
LispValue var ;
{
    if ( CAR (last_element (get_varid (var))))
      return(true);
    else
      return(false);
}

/*  .. add_tl_element, replace-nestvar-refs
 */
bool
consider_vararrayindex (var)
     Var var ;
{
    if (!var_is_nested (var) && varid_indexes_into_array (var))
      return(true);
    else
      return(false);
}

/*    	==========
 *    	OPER nodes
 *    	==========
 */

/*    
 *    	replace_opid
 *    
 *    	Given a oper node, resets the opno (operator OID) field with the
 *    	procedure OID (regproc id).
 *    
 *    	Returns the modified oper node.
 *    
 */

/*  .. fix-opid, index-outerjoin-references, replace-clause-joinvar-refs
 *  .. replace-clause-resultvar-refs
 */

Oper
replace_opid (oper)
     Oper oper ;
{
    set_opno (oper,get_opcode (get_opno (oper)));
    return(oper);
}

/*    	===========
 *    	PARAM nodes
 *    	===========
 */

/*    
 *    	param_is_attr
 *    
 *    	Returns t iff the argument is a parameter of the $.name type.
 *    
 */

bool
param_is_attr (param)
     Param param ;
{
    if ( stringp (get_paramid (param)))
      return(true);
    else
      return(false);
}

/*    	=============================
 *    	constant (CONST, PARAM) nodes
 *    	=============================
 */

/*    
 *    	constant-p
 *    
 *    	Returns t iff the argument is a constant or a parameter.
 *    
 */

/*  .. get_relattval, match-index-orclause, qual-clause-p, single_node
 */

bool
constant_p (node)
     Expr node ;
{
    if ( const_p (node) || param_p (node) )
      return(true);
    else
      return(false);
}

/*    
 *    	non_null
 *    
 *    	Returns t if the node is a non-null constant, e.g., if the node has a
 *    	valid `constvalue' field.
 *    
 */

/*  .. MakeAttlist, best-or-subclause-index, get_relattval, is_null
 */

bool
non_null (const)
     Expr const ;
{
    if (const_p (const) && !get_constisnull (const) )
      return(true);
    else
      return(false);
}

/*    
 *    	is_null
 *    
 *    	Returns t if the node is anything but a non-null constant (has no valid
 *    	`constvalue' field).
 *    
 */

/*  .. ExecEvalNot, ExecEvalOr, ExecIsNull, ExecQual1, print_const
 */

bool
is_null (const)
     Expr const ;
{
    if (!non_null (const))
      return(true);
    else
      return(false);

}

/*    	==========
 *    	PLAN nodes
 *    	==========
 */

/*  .. print_subplan, set-tlist-references
 */
bool
join_p (node)
     Node node ;
{
    if(nestloop_p (node) || hashjoin_p (node) || mergesort_p (node))
      return(true);
    else
      return(false);
}

/*  .. print_subplan
 */

bool
scan_p (node)
     Node node ;
{
    if(seqscan_p (node) || indexscan_p (node))
      return(true);
    else
      return(false);
}

/*  .. print_subplan, query_planner, set-tlist-references
 */
bool
temp_p (node)
     Node node ;
{
    if (sort_p (node) || hash_p (node))
      return(true);
    else
      return(false);
}

bool
plan_p (node)
     Node node ;
{
    if (result_p (node) || existential_p (node))
      return(true);
    else
      return(false);
}
