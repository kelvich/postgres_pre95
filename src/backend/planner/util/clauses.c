
/*    
 *    	clauses
 *    
 *    	Clause access and manipulation routines
 *    $Header$
 */

/*    
 *    		clause-type
 *    		make-clause
 *    		is_opclause
 *    		make_opclause
 *    		get_opargs
 *    		get_op
 *    		get_leftop
 *    		get_rightop
 *    		is_funcclause
 *    		make_funcclause
 *    		get_funcargs
 *    		get_function
 *    		or_clause
 *    		make_orclause
 *    		get_orclauseargs
 *    		not_clause
 *    		make_notclause
 *    		get_notclausearg
 *    		and_clause
 *    		make_andclauseargs
 *    		get_andclauseargs
 */



#include "tmp/c.h"

#include "parser/parse.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"

#include "planner/clauses.h"

/*    
 *    	clause_head
 *    
 *    	Returns the first element of a clause, if it is a clause
 *    
 */

/*  .. and_clause, clause-type, is_funcclause, is_opclause, not_clause
 *  .. or_clause
 */
LispValue
clause_head (clause)
     LispValue clause ;
{
    if ( consp (clause) ) {
	return(CAR (clause));
    } else {
	return(LispNil);
    } 
}

/*  .. clause-args, fix-indxqual-references
 */
LispValue
clause_type (clause)
     LispValue clause ;
{
    LispValue clauseand = lispInteger(AND);
    LispValue clauseor = lispInteger(OR);
    LispValue clausenot = lispInteger(NOT);
    
    if ( consp (clause) ) {
	LispValue type = clause_head (clause);
	if (type == LispNil) return(LispNil);
	if ( member (type,lispCons(clauseand,
				   lispCons(clauseor,
					    lispCons(clausenot,
						     LispNil)))) ||
	    IsA (type,Func) || IsA (type,Oper)) 
	  return(type);
    } else
      return(LispNil);

}
/*  .. clause-subclauses
 */
LispValue
clause_args (clause)
     LispValue clause ;
{
    LispValue clauseand = lispInteger(AND);
    LispValue clauseor = lispInteger(OR);
    LispValue clausenot = lispInteger(NOT);

    if ( consp (clause) ) {
	LispValue type = clause_type (clause);
	if ( member (type,lispCons(clauseand,
				   lispCons(clauseor,
					    lispCons(clausenot,
						     LispNil)))) ||
	    IsA (type,Func) || IsA(type,Oper) ) 
	  return(CDR (clause));
    } else 
      return(LispNil);
}


LispValue
make_clause (type,args)
     LispValue type,args ;
{
    LispValue clauseand = lispInteger(AND);
    LispValue clauseor = lispInteger(OR);
    LispValue clausenot = lispInteger(NOT);

    if (null(type))
      return(args);
    else if (IsA(type,LispInt) ) {
	int actual = CInteger(type);
	if (actual == AND || actual == OR || actual == NOT)
	  return(lispCons(type,args));
    } else if (IsA(type,Func) || IsA(type,Oper)) {
	return(lispCons(type,args));


    } else
      return (args);
} 


/*  .. fix-indxqual-references
 */

LispValue
clause_subclauses (type,clause)
     LispValue type,clause ;
{
     if(type) {
	  return(clause_args (clause));
     } else if (consp (clause)) {
	  return(clause);
     } else
       return(LispNil);
}

/*    	-------- OPERATOR clause macros
 */

/*    
 *    	is_opclause
 *    
 *    	Returns t iff the clause is an operator clause:
 *    		(op expr expr) or (op expr).
 *    
 */

/*  .. fix-indxqual-references
 */
bool
is_opclause (clause)
     LispValue clause ;
{
    if (clause_head(clause))
      return((bool)IsA (clause_head (clause),Oper));
}

/*    
 *    	make_opclause
 *    
 *    	Creates a clause given its operator left operand and right
 *    	operand (if it is non-null).
 *    
 */

/*  .. fix-indxqual-references
 */
LispValue
make_opclause (op,leftop,rightop)
     LispValue op,leftop,rightop ;
{
     if(rightop) {
	  return(lispCons (op,
			   lispCons(leftop,
				    lispCons(rightop,
					     LispNil))));
     } else {
	  return(lispCons (op,
			   lispCons(leftop,LispNil)));
     }
}

/*    
 *    	get_opargs
 *    
 *    	Returns the argument exprs of an op clause.
 *    
 */

LispValue
get_opargs (clause)
     LispValue clause ;
{
     return(CDR (clause));
}

/*    
 *    	get_op
 *    
 *    	Returns the operator in a clause of the form (op expr expr) or
 *    	(op expr)
 *    
 */

LispValue
get_op (clause)
     LispValue clause ;
{
     return(nth (0,clause));
}

/*    
 *    	get_leftop
 *    
 *    	Returns the left operand of a clause of the form (op expr expr)
 *    	or (op expr)
 *      NB: it is assumed (for now) that all expr must be Var nodes    
 */

Var
get_leftop (clause)
     LispValue clause ;
{
     return((Var)nth (1,clause));
}

/*    
 *    	get_rightop
 *    
 *    	Returns the right operand in a clause of the form (op expr expr).
 *    
 */

Var
get_rightop (clause)
     LispValue clause ;
{
     return((Var)nth (2,clause));
}

/*    	-------- FUNC clause macros
 */

/*    
 *    	is_funcclause
 *    
 *    	Returns t iff the clause is a function clause: (func { expr }).
 *    
 */

/*  .. ExecEvalExpr, fix-indxqual-references, fix-opid, flatten-tlistentry
 *  .. nested-clause-p, print_clause, pull_var_clause
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs
 */

bool
is_funcclause (clause)
     LispValue clause ;
{
    if (clause_head(clause))
      return((bool)IsA (clause_head (clause),Func));
}

/*    
 *    	make_funcclause
 *    
 *    	Creates a function clause given the FUNC node and the functional
 *    	arguments.
 *    
 */

/*  .. flatten-tlistentry, replace-clause-joinvar-refs
 *  .. replace-clause-nestvar-refs, replace-clause-resultvar-refs
 */
LispValue
make_funcclause (func,funcargs)
     LispValue func,funcargs ;
{
     return(lispCons (func,funcargs));
}

/*    
 *    	get_function
 *    
 *    	Returns the FUNC node from a function clause.
 *    
 */

/*  .. ExecEvalFunc, fix-indxqual-references, flatten-tlistentry
 *  .. print_clause, replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs
 */

LispValue
get_function (func)
     LispValue func ;
{
     return(CAR (func));
}

/*    
 *    	get_funcargs
 *    
 *    	Returns the functional arguments from a function clause.
 *    
 */

/*  .. fix-opid, flatten-tlistentry, initialize-qualification
 *  .. nested-clause-p, print_clause, pull_var_clause
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs
 */
LispValue
get_funcargs (func)
     LispValue func ;
{
     return(CDR (func));
}

/*    	-------- OR clause macros
 */

/*    
 *    	or_clause
 *    
 *    	Returns t iff the clause is an 'or' clause: (OR { expr }).
 *    
 */

/*  .. ExecEvalExpr, cleanup, compute_clause_selec, contains-not
 *  .. create_indexscan_node, find-nots, fix-opid, nested-clause-p
 *  .. normalize, print_clause, pull-args, pull-ors, pull_var_clause
 *  .. push-nots, relation-level-clause-p, remove-ands
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs, valid-or-clause
 * XXX - should be called or_clause_p instead
 */

bool
or_clause (clause)
     LispValue clause ;
{
    if (consp (clause))
      return(bool) ( equal(lispInteger(OR),clause_head(clause)));
   /*  return(equal ("OR",clause_head (clause))); */
}

/*    
 *    	make_orclause
 *    
 *    	Creates an 'or' clause given a list of its subclauses.
 *    
 */

/*  .. cleanup, distribute-args, find-nots, normalize, pull-args
 *  .. push-nots, remove-ands, replace-clause-joinvar-refs
 *  .. replace-clause-nestvar-refs, replace-clause-resultvar-refs
 */
LispValue
make_orclause (orclauses)
     LispValue orclauses ;
{
    return(lispCons (lispInteger(OR),orclauses));
}

/*    
 *    	get_orclauseargs
 *    
 *    	Retrieves the subclauses of an 'or' clause.
 *    
 */

/*  .. ExecEvalExpr, cleanup, compute_clause_selec, contains-not
 *  .. create-or-index-paths, create_indexscan_node, find-nots, fix-opid
 *  .. match-index-orclauses, nested-clause-p, normalize, print_clause
 *  .. pull-args, pull-ors, pull_var_clause, push-nots
 *  .. relation-level-clause-p, remove-ands, replace-clause-joinvar-refs
 *  .. replace-clause-nestvar-refs, replace-clause-resultvar-refs
 */
LispValue
get_orclauseargs (orclause)
     LispValue orclause ;
{
    if ( consp (orclause))
      return(CDR (orclause));
}

/*    	-------- NOT clause macros
 */

/*    
 *    	not_clause
 *    
 *    	Returns t iff this is a 'not' clause: (NOT expr).
 *    
 */

/*  .. ExecEvalExpr, cleanup, compute_clause_selec, contains-not
 *  .. find-nots, fix-opid, nested-clause-p, normalize, print_clause
 *  .. pull-args, pull_var_clause, push-nots, relation-level-clause-p
 *  .. remove-ands, replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs
 */
bool
not_clause (clause)
     LispValue clause ;
{
    if (consp (clause))
      return(bool) ( equal(lispInteger(NOT),clause_head(clause)));
}

/*    
 *    	make_notclause
 *    
 *    	Create a 'not' clause given the expression to be negated.
 *    
 */

/*  .. cleanup, normalize, pull-args, push-nots, remove-ands
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs
 */
LispValue
make_notclause (notclause)
LispValue notclause ;
{
    return(lispCons (lispInteger(NOT),
		     lispCons(notclause,LispNil))); 
}

/*    
 *    	get_notclausearg
 *    
 *    	Retrieve the clause within a 'not' clause
 *    
 */

/*  .. ExecEvalNot, cleanup, compute_clause_selec, find-nots, fix-opid
 *  .. nested-clause-p, normalize, print_clause, pull-args
 *  .. pull_var_clause, push-nots, relation-level-clause-p, remove-ands
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs
 */
LispValue
get_notclausearg (notclause)
     LispValue notclause ;
{
     return( CAR(CDR( (notclause)))) ;
}

/*    	-------- AND clause macros
 */

/*    
 *    	and_clause
 *    
 *    	Returns t iff its argument is an 'and' clause: (AND { expr }).
 *    
 */

/*  .. cleanup, cnfify, find-nots, normalize, pull-ands, pull-args
 *  .. push-nots, remove-ands
 */
bool
and_clause (clause)
     LispValue clause ;
{
    if (consp (clause))
      return(bool) ( equal(lispInteger(AND),clause_head(clause)));
}

/*    		 
 *    	make_andclause
 *    
 *    	Create an 'and' clause given its arguments in a list.
 *    
 */

/*  .. cleanup, cnfify, distribute-args, find-nots, normalize
 *  .. pull-args, push-nots
 */
LispValue
make_andclause (andclauses)
     LispValue andclauses ;
{
     return(lispCons (lispInteger(AND),andclauses));
}

/*    
 *    	get_andclauseargs
 *    
 *    	Retrieve the arguments of an 'and' clause.
 *    
 */

/*  .. cleanup, find-nots, normalize, or-normalize, pull-ands
 *  .. pull-args, push-nots, remove-ands
 */
LispValue
get_andclauseargs (andclause)
     LispValue andclause ;
{
     return(CDR (andclause));
}















