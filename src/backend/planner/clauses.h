#include "c.h"
#include "pg_lisp.h"
extern LispValue clause_head ARGS((LispValue clause));
extern LispValue clause_type ARGS((LispValue clause));
extern LispValue clause_args ARGS((LispValue clause));

/* superseded by make_opclause
extern LispValue make_clause ARGS((LispValue type, LispValue rest, LispValue args));
*/

extern LispValue clause_subclauses ARGS((LispValue type, LispValue clause));
extern LispValue is_opclause ARGS((LispValue clause));

extern LispValue make_opclause ARGS((LispValue op,leftop,rightop));

extern LispValue get_opargs ARGS((LispValue clause));
/*  .. ExecEvalOper, initialize-qualification
 */

extern LispValue get_op ARGS((LispValue clause));
/*  .. ExecEvalOper, MakeSkeys, cleanup, compute_selec, find-nots
 *  .. fix-indxqual-references, fix-opid, flatten-tlistentry, get_opnos
 *  .. hashjoinop, in-line-lambda%598040608, index-outerjoin-references
 *  .. match-index-orclause, mergesortop, normalize, print_clause
 *  .. pull-args, push-nots, relation-level-clause-p, remove-ands
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs, switch_outer
 */

extern LispValue get_leftop ARGS((LispValue clause));
/*  .. MakeSkeys, best-or-subclause-index, cleanup, find-nots
 *  .. fix-indxqual-references, fix-opid, flatten-tlistentry, get_joinvar
 *  .. get_relattval, get_relsatts, group-clauses-by-hashop
 *  .. group-clauses-by-order, hashjoinop, in-line-lambda%598040608
 *  .. index-outerjoin-references, join-clause-p, match-index-orclause
 *  .. mergesortop, nested-clause-p, normalize, other-join-clause-var
 *  .. print_clause, pull-args, pull_var_clause, push-nots, qual-clause-p
 *  .. remove-ands, replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs, switch_outer
 */

extern LispValue get_rightop ARGS((LispValue clause));
/*  .. MakeSkeys, best-or-subclause-index, cleanup, find-nots
 *  .. fix-indxqual-references, fix-opid, flatten-tlistentry, get_joinvar
 *  .. get_relattval, get_relsatts, group-clauses-by-hashop
 *  .. group-clauses-by-order, hashjoinop, in-line-lambda%598040608
 *  .. index-outerjoin-references, join-clause-p, match-index-orclause
 *  .. mergesortop, nested-clause-p, normalize, other-join-clause-var
 *  .. print_clause, pull-args, pull_var_clause, push-nots, qual-clause-p
 *  .. remove-ands, replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs, switch_outer
 */

extern LispValue is_funcclause ARGS((LispValue clause));
extern LispValue make_funcclause ARGS((LispValue func, LispValue funcargs));
extern LispValue get_function ARGS((LispValue func));
extern LispValue get_funcargs ARGS((LispValue func));
extern LispValue or_clause ARGS((LispValue clause));
extern LispValue make_orclause ARGS((LispValue orclauses));
extern LispValue get_orclauseargs ARGS((LispValue orclause));
extern LispValue not_clause ARGS((LispValue clause));
extern LispValue make_notclause ARGS((LispValue notclause));
extern LispValue get_notclausearg ARGS((LispValue notclause));
extern LispValue and_clause ARGS((LispValue clause));
extern LispValue make_andclause ARGS((LispValue andclauses));
extern LispValue get_andclauseargs ARGS((LispValue andclause));


#define is_clause is_opclause
/*  .. ExecEvalExpr, cleanup, compute_clause_selec, find-nots, fix-opid
 *  .. get_relattval, get_relsatts, join-clause-p, match-index-orclause
 *  .. normalize, pull-args, pull_var_clause, push-nots, qual-clause-p
 *  .. relation-level-clause-p, remove-ands, replace-clause-joinvar-refs
 */

#define make_clause make_opclause
/*  .. cleanup, find-nots, flatten-tlistentry, index-outerjoin-references
 *  .. normalize, pull-args, push-nots, remove-ands
 *  .. replace-clause-joinvar-refs, replace-clause-nestvar-refs
 *  .. replace-clause-resultvar-refs, switch_outer
 */


#define AND lispInteger(1)
#define OR lispInteger(2)
#define NOT lispInteger(3)
