
/*    
 *    	nodeFuncs
 *    
 *    	All node routines more complicated than simple access/modification
 *    	$Header$
 */

#include "tmp/c.h"

RcsId("$Header$");

#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "tmp/nodeFuncs.h"

#include "planner/keys.h"


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
    if(atom (node) || IsA(node,Const) || IsA(node,Var) || IsA(node,Param)) 
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
 *      var node is an array reference
 *    
 */

/*  .. ExecEvalVar, switch_outer, var_is_rel   */

bool
var_is_outer (var)
     Var var ;
{
    return((bool)(get_varno (var) == OUTER));
}

/*  .. ExecEvalVar, var_is_rel   */

bool
var_is_inner (var)
     Var var ;
{
    return ( (bool) (get_varno (var) == INNER));
}

/*  .. get_relattval, get_relsatts  */
 
/* XXX - completely bogus, Var needs an extra flags that is 
   "materialized ?" */

bool
var_is_mat (var)
     Var var ;
{
    return (false);
    /* return((bool)listp (get_varno(var))); */
}

/*  .. ExecEvalVar  */

bool
var_is_rel (var)
     Var var ;
{
    return (bool)
	! (var_is_inner (var) ||  var_is_outer (var));
}

/*    	==========
 *    	OPER nodes
 *    	==========
 */

/*    
 *    	replace_opid
 *    
 *    	Given a oper node, resets the opfid field with the
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
    set_opid (oper,get_opcode (get_opno (oper)));
    set_op_fcache(oper, NULL); 
    return(oper);
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
    if ( IsA(node,Const) || IsA(node,Param) )
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
non_null (c)
     Expr c;
{

    if ( IsA(c,Const) && !get_constisnull ((Const)c) )
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
is_null (constant)
     Expr constant ;
{
    if (!non_null (constant))
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
    if( IsA(node,NestLoop) || IsA(node,HashJoin) || IsA(node,MergeJoin))
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
    if( IsA(node,SeqScan) || IsA(node,IndexScan))
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
    if ( IsA(node,Sort) || IsA(node,Hash))
      return(true);
    else
      return(false);
}

bool
plan_p (node)
     Node node ;
{
    if ( IsA(node,Result) || IsA(node,Existential))
      return(true);
    else
      return(false);
}
