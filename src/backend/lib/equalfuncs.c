#include "tmp/postgres.h"

RcsId("$Header$");

#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"

#include "utils/log.h"
#include "storage/itemptr.h"
#include "lib/equalfuncs.h"

bool
equal(a, b)
	Node	a;
	Node	b;
{
    if (a == b)
      return(true);
    if ((!a && b) || (a && !b))
      return(false);
    Assert(IsA(a,Node));
    Assert(IsA(b,Node));
    if (a->type != b->type)
      return (false);
    
    return ((a->equalFunc)(a, b));
}

/*
 *  Stuff from primnodes.h
 */

/*
 *  Resdom is a subclass of Node.
 */

bool
_equalResdom(a, b)
	register Resdom	a;
	register Resdom	b;
{
	register OperatorTupleForm at;
	register OperatorTupleForm bt;

	if (a->resno != b->resno)
		return (false);
	if (a->restype != b->restype)
		return (false);
	if (a->reslen != b->reslen)
		return (false);
	if (strcmp(a->resname, b->resname) != 0)
		return (false);
	if (a->reskey != b->reskey)
		return (false);

	/*
	 *  reskeyop is a pointer to an OperatorTupleForm.  We can almost
	 *  do a bcmp() on the two structures, but the oprname field is
	 *  a char16, and bytes after the null may not be the same.  We
	 *  do all this by hand because of that.
	 */
	at = a->reskeyop;
	bt = b->reskeyop;

	/* well, it might happen... */
	if (at == bt)
		return (true);

	if (strncmp(at->oprname, bt->oprname, sizeof (bt->oprname)) != 0)
		return (false);
	if (at->oprowner != bt->oprowner)
		return (false);
	if (at->oprprec != bt->oprprec)
		return (false);
	if (at->oprkind != bt->oprkind)
		return (false);
	if (at->oprisleft != bt->oprisleft)
		return (false);
	if (at->oprcanhash != bt->oprcanhash)
		return (false);
	if (at->oprleft != bt->oprleft)
		return (false);
	if (at->oprright != bt->oprright)
		return (false);
	if (at->oprresult != bt->oprresult)
		return (false);
	if (at->oprcom != bt->oprcom)
		return (false);
	if (at->oprnegate != bt->oprnegate)
		return (false);
	if (at->oprlsortop != bt->oprlsortop)
		return (false);
	if (at->oprrsortop != bt->oprrsortop)
		return (false);
	if (at->oprcode != bt->oprcode)
		return (false);
	if (at->oprrest != bt->oprrest)
		return (false);
	if (at->oprjoin != bt->oprjoin)
		return (false);

	return (true);
}

bool
_equalFjoin(a, b)
	Fjoin a;
	Fjoin b;
{
	int nNodes;

	if (a->fj_initialized != b->fj_initialized)
		return (false);
	if (a->fj_nNodes != b->fj_nNodes)
		return (false);
	if (!equal(a->fj_innerNode, b->fj_innerNode))
		return (false);
	
	nNodes = a->fj_nNodes;
	if (bcmp(a->fj_results, b->fj_results, nNodes*sizeof(Datum)) != 0)
		return (false);
	if (bcmp(a->fj_alwaysDone, b->fj_alwaysDone, nNodes*sizeof(bool)) != 0)
		return (false);

	return(true);
}
/*
 *  Expr is a subclass of Node.
 */
bool
_equalExpr(a, b)
	Expr	a;
	Expr	b;
{
	/* no private data */
	return (true);
}

bool
_equalIter(a, b)
	Iter	a;
	Iter	b;
{
	return (equal((Node)a->iterexpr, (Node)b->iterexpr));
}

/*
 *  Var is a subclass of Expr.
 */

bool
_equalVar(a, b)
	register Var	a;
	register Var	b;
{
	if (!_equalExpr((Expr)a, (Expr)b))
		return (false);

	if (a->varno != b->varno)
		return (false);
	if (a->varattno != b->varattno)
		return (false);
	if (a->vartype != b->vartype)
		return (false);
	if (!_equalLispValue(a->vardotfields, b->vardotfields))
		return (false);
	if (!_equalLispValue(a->vararraylist, b->vararraylist))
		return (false);
	if (!_equalLispValue(a->varid, b->varid))
		return (false);

	return (true);
}

bool
_equalArray(a, b)
	register Array	a;
	register Array	b;

{
	if (!_equalExpr((Expr)a, (Expr)b))
		return (false);
	if (a->arrayelemtype != b->arrayelemtype)
		return (false);
	if (a->arraylow != b->arraylow)
		return (false);
	if (a->arrayhigh != b->arrayhigh)
		return (false);
	if (a->arraylen != b->arraylen)
		return (false);
	return(true);
}

/*
 *  Oper is a subclass of Expr.
 */

bool
_equalOper(a, b)
	register Oper	a;
	register Oper	b;
{
	if (!_equalExpr((Expr)a, (Expr)b))
		return (false);

	if (a->opno != b->opno)
		return (false);
	if (a->oprelationlevel != b->oprelationlevel)
		return (false);
	if (a->opresulttype != b->opresulttype)
		return (false);

	return (true);
}

/*
 *  Const is a subclass of Expr.
 */

bool
_equalConst(a, b)
	register Const	a;
	register Const	b;
{
	if (a == b)
		return (true);

	return (false);

#ifdef NOTYET
	if (a->consttype != b->consttype)
		return (false);
	if (a->constlen != b->constlen)
		return (false);
	/*
	 *  Have to use the function manager to call the equality
	 *  test routine for the datum we've got.  Talk to mike h
	 *  about how to do that.
	 */

	if (a->constisnull != b->constisnull)
		return (false);

	return (true);
#endif
}

/*
 *  Param is a subclass of Expr.
 */

bool
_equalParam(a, b)
	register Param	a;
	register Param	b;
{
	if (!_equalExpr((Expr) a, (Expr) b))
		return (false);

	if (a->paramkind != b->paramkind)
		return (false);
	if (a->paramtype != b->paramtype)
		return (false);

	switch (a->paramkind) {
	    case PARAM_NAMED:
	    case PARAM_NEW:
	    case PARAM_OLD:
		if (strcmp(a->paramname, b->paramname) != 0)
			return (false);
		break;
	    case PARAM_NUM:
		if (a->paramid != b->paramid)
			return (false);
		break;
	    case PARAM_INVALID:
		/*
		 * XXX: Hmmm... What are we supposed to return
		 * in this case ??
		 */
		return(true);
		break;
	    default:
		elog(WARN, "_equalParam: Invalid paramkind value: %d",
		a->paramkind);
	}

	return (true);
}

/*
 *  Func is a subclass of Expr.
 */

bool
_equalFunc(a, b)
	register Func	a;
	register Func	b;
{
	if (!_equalExpr((Expr) a, (Expr) b))
		return (false);

	if (a->funcid != b->funcid)
		return (false);
	if (a->functype != b->functype)
		return (false);
	if (a->funcisindex != b->funcisindex)
		return (false);
	if (a->funcsize != b->funcsize)
		return (false);
	if (!equal(a->func_tlist, b->func_tlist))
		return (false);

	return (true);
}

/*
 * CInfo is a subclass of Node.
 */

bool
_equalCInfo(a,b)
     register CInfo a;
     register CInfo b;
{
    Assert(IsA(a,CInfo));
    Assert(IsA(b,CInfo));
    
    if (!_equalExpr((Expr)(get_clause(a)),
		    (Expr)(get_clause(b))))
      return(false);
    if (a->selectivity != b->selectivity)
      return(false);
    if (a->notclause != b->notclause)
      return(false);
#ifdef EqualMergeOrderExists
    if (!EqualMergeOrder(a->mergesortorder,b->mergesortorder))
      return(false);
#endif
    if(a->hashjoinoperator != b->hashjoinoperator)
      return(false);
    return(equal((Node)(a->indexids),
		 (Node)(b->indexids)));
}

bool
_equalJoinMethod(a,b)
     register JoinMethod a,b;
{
    Assert(IsA(a,JoinMethod));
    Assert(IsA(b,JoinMethod));

    if (!equal((Node)(a->jmkeys),
	       (Node)(b->jmkeys)))
      return(false);
    if (!equal((Node)(a->clauses),
	       (Node)(b->clauses)))
      return(false);
    return(true);
}

bool
_equalPath(a,b)
register Path a,b;
{
    Assert(IsA(a,Path));
    Assert(IsA(b,Path));

    if (a->pathtype != b->pathtype)
	return(false);
    if (a->parent != b->parent)
	return(false);
    /*
    if (a->path_cost != b->path_cost)
        return(false);
    */
    if (!equal((Node)(a->p_ordering), 
	       (Node)(b->p_ordering)))
	return(false);
    if (!equal((Node)(a->keys),
	       (Node)(b->keys)))
	return(false);
    if (!equal((Node)(a->pathsortkey),
	       (Node)(b->pathsortkey)))
	return(false);
    /*
    if (a->outerjoincost != b->outerjoincost)
	return(false);
    */
    if (!equal((Node)(a->joinid),
	       (Node)(b->joinid)))
	return(false);
    return(true);
}

bool
_equalIndexPath(a,b)
register IndexPath a,b;
{
    Assert(IsA(a,IndexPath));
    Assert(IsA(b,IndexPath));

    if (!_equalPath((Path)a,(Path)b))
	return(false);
    if (!equal((Node)(a->indexid), (Node)(b->indexid)))
	return(false);
    if (!equal((Node)(a->indexqual), (Node)(b->indexqual)))
	return(false);
    return(true);
}

bool
_equalJoinPath(a,b)
register JoinPath a,b;
{
    Assert(IsA(a,JoinPath));
    Assert(IsA(b,JoinPath));

    if (!_equalPath((Path)a,(Path)b))
	return(false);
    if (!equal((Node)(a->pathclauseinfo), (Node)(b->pathclauseinfo)))
	return(false);
    if (!equal((Node)(a->outerjoinpath), (Node)(b->outerjoinpath)))
	return(false);
    if (!equal((Node)(a->innerjoinpath), (Node)(b->innerjoinpath)))
	return(false);
    return(true);
}

bool
_equalMergePath(a,b)
register MergePath a,b;
{
    Assert(IsA(a,MergePath));
    Assert(IsA(b,MergePath));

    if (!_equalJoinPath((JoinPath)a,(JoinPath)b))
	return(false);
    if (!equal((Node)(a->path_mergeclauses), (Node)(b->path_mergeclauses)))
	return(false);
    if (!equal((Node)(a->outersortkeys), (Node)(b->outersortkeys)))
	return(false);
    if (!equal((Node)(a->innersortkeys), (Node)(b->innersortkeys)))
	return(false);
    return(true);
}

bool
_equalHashPath(a,b)
register HashPath a,b;
{
    Assert(IsA(a,HashPath));
    Assert(IsA(b,HashPath));

    if (!_equalJoinPath((JoinPath)a,(JoinPath)b))
	return(false);
    if (!equal((Node)(a->path_hashclauses), (Node)(b->path_hashclauses)))
	return(false);
    if (!equal((Node)(a->outerhashkeys), (Node)(b->outerhashkeys)))
	return(false);
    if (!equal((Node)(a->innerhashkeys), (Node)(b->innerhashkeys)))
	return(false);
    return(true);
}

bool
_equalJoinKey(a,b)
register JoinKey a,b;
{
    Assert(IsA(a,JoinKey));
    Assert(IsA(b,JoinKey));

    if (!equal((Node)(a->outer),(Node)(b->outer)))
       return(false);
    if (!equal((Node)(a->inner),(Node)(b->inner)))
       return(false);
    return(true);
}

bool
_equalMergeOrder(a,b)
     register MergeOrder a,b;
{
    if (a == (MergeOrder)NULL && b == (MergeOrder)NULL)
       return(true);
    Assert(IsA(a,MergeOrder));
    Assert(IsA(b,MergeOrder));

    if (a->join_operator != b->join_operator)
      return(false);
    if (a->left_operator != b->left_operator)
      return(false);
    if (a->right_operator != b->right_operator)
      return(false);
    if (a->left_type != b->left_type)
      return(false);
    if (a->right_type != b->right_type)
      return(false);
    return(true);
}

bool
_equalHInfo(a,b)
     register HInfo a,b;
{
    Assert(IsA(a,HInfo));
    Assert(IsA(b,HInfo));

    if (a->hashop != b->hashop)
      return(false);
    return(true);
}

/* XXX  This equality function is a quick hack, should be
 *      fixed to compare all fields.
 */

bool
_equalIndexScan(a,b)
     register IndexScan a,b;
{
    Assert(IsA(a,IndexScan));
    Assert(IsA(b,IndexScan));

    if(a->cost != b->cost)
      return(false);

    if (a->fragment != b->fragment)
      return(false);

    if (!equal((Node)(a->indxqual),(Node)(b->indxqual)))
      return(false);

    if (a->scanrelid != b->scanrelid)
      return(false);

    if (!equal((Node)(a->indxid),(Node)(b->indxid)))
      return(false);
    return(true);
}

bool
_equalJInfo(a,b)
     register JInfo a,b;
{
    Assert(IsA(a,JInfo));
    Assert(IsA(b,JInfo));
    if (!equal((Node)(a->otherrels),(Node)(b->otherrels)))
      return(false);
    if (!equal((Node)(a->jinfoclauseinfo),(Node)(b->jinfoclauseinfo)))
      return(false);
    if (a->mergesortable != b->mergesortable)
      return(false);
    if (a->hashjoinable != b->hashjoinable)
      return(false);
    return(true);
}
/*
 *  Stuff from execnodes.h
 */

/*
 *  EState is a subclass of Node.
 */

bool
_equalEState(a, b)
	register EState	a;
	register EState	b;
{
	if (a->es_direction != b->es_direction)
		return (false);
	if (a->es_time != b->es_time)
		return (false);
	if (a->es_owner != b->es_owner)
		return (false);
	if (!_equalLispValue(a->es_locks, b->es_locks))
		return (false);
	if (!_equalLispValue(a->es_subplan_info, b->es_subplan_info))
		return (false);
	if (a->es_error_message == (Name) NULL
	    || b->es_error_message == (Name) NULL) {
		if (a->es_error_message != b->es_error_message)
			return (false);
	} else {
		if (strcmp(a->es_error_message, b->es_error_message) != 0)
			return (false);
	}
	if (!_equalLispValue(a->es_range_table, b->es_range_table))
		return (false);

	if (!_equalHeapTuple(a->es_qualification_tuple,
			     b->es_qualification_tuple))
		return (false);
	if (!ItemPointerEquals(a->es_qualification_tuple_id,
			       b->es_qualification_tuple_id))
		return (false);
	if (!_equalRelation(a->es_relation_relation_descriptor,
			    b->es_relation_relation_descriptor))
		return (false);
	if (a->es_result_relation_info != b->es_result_relation_info)
		return (false);

	return (true);
}

/*
 *  _equalHeapTuple -- Are two heap tuples equal?
 */

bool
_equalHeapTuple(a, b)
	register HeapTuple	a;
	register HeapTuple	b;
{
	if (a->t_len != b->t_len)
		return (false);
	if (!ItemPointerEquals(&a->t_ctid, &b->t_ctid))
		return (false);

	/*
	 *  We ignore the t_lock entry, because i don't know how to
	 *  decide which entry in the union to look at.
	 */

	if (a->t_oid != b->t_oid)
		return (false);
	if (a->t_xmin != b->t_xmin)
		return (false);
	if (a->t_cmin != b->t_cmin)
		return (false);
	if (a->t_xmax != b->t_xmax)
		return (false);
	if (a->t_cmax != b->t_cmax)
		return (false);

	if (!ItemPointerEquals(&a->t_chain, &b->t_chain))
		return (false);
	if (a->t_tmin != b->t_tmin)
		return (false);
	if (a->t_tmax != b->t_tmax)
		return (false);
	if (a->t_natts != b->t_natts)
		return (false);
	if (a->t_hoff != b->t_hoff)
		return (false);
	if (a->t_vtype != b->t_vtype)
		return (false);

	/*
	 *  We ignore the t_bits field, because i don't know how to
	 *  compute its length.
	 */

	return (true);
}

/*
 *  _equalRelation -- Are two relations equal?
 *
 *	Two relations are considered equal, for our purposes, if they
 *	have the same object id.  This may not be sufficient, in which
 *	case we can write more code.
 */

bool
_equalRelation(a, b)
	Relation	a;
	Relation	b;
{
	if (a->rd_id != b->rd_id)
		return (false);

	return (true);
}
/*
 *  _equalLispValue -- are two lists equal?
 *
 *	This is a comparison by value.  It would be simpler to write it
 *	to be recursive, but it should run faster if we iterate.
 */

bool
_equalLispValue(a, b)
	register LispValue	a;
	register LispValue	b;
{
	while (a != (LispValue) NULL) {

		if (b == (LispValue) NULL)
			return (false);
		if (a == b)
			return(true);
		if (LISP_TYPE(a) != LISP_TYPE(b))
			return (false);

		switch (LISP_TYPE(a)) {

		  case PGLISP_ATOM:
			if (a->val.name != b->val.name)
				return (false);
			break;

		  case PGLISP_DTPR:
			if (!_equalLispValue(a->val.car, b->val.car))
				return (false);
			break;

		  case PGLISP_FLOAT:
			if (a->val.flonum != b->val.flonum)
				return (false);
			break;

		  case PGLISP_INT:
			if (a->val.fixnum != b->val.fixnum)
				return (false);
			break;

		  case PGLISP_STR:
			if (strcmp(a->val.str, b->val.str) != 0)
				return (false);
			break;

		  case PGLISP_VECI:
			if (a->val.veci->size != b->val.veci->size)
				return (false);
			if (bcmp(&(a->val.veci->data[0]), &(b->val.veci->data[0]),
					a->val.veci->size) != 0)
				return (false);
			break;

		  default:
			if (IsA(a,Node) && IsA(b,Node) ) 
			  if(NodeType(a) == NodeType(b)) {
			     return((*(a->equalFunc))(a,b));
			  } else {
			    return(false);
			  }
			elog(NOTICE,"equal: LispAtom type %d unknown",
				a->type);
			return (false);
		}

		/* next */
		a = CDR(a);
		b = CDR(b);
	}

	if (b != (LispValue) NULL)
		return (false);

	return (true);
}

bool
_equalFragment(a,b)
register Fragment a,b;
{
    Assert(IsA(a,Fragment));
    Assert(IsA(b,Fragment));
    if (a->frag_root != b->frag_root)
	return(false);
    if (a->frag_parent_op != b->frag_parent_op)
	return(false);
    if (a->frag_parallel != b->frag_parallel)
	return(false);
    if (!equal((Node)(a->frag_subtrees),(Node)(b->frag_subtrees)))
	return(false);
    if (a->frag_parent_frag != b->frag_parent_frag)
	return(false);
    return(true);
}



