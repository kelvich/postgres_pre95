/*
 * $Header$
 *
 * This file handles the nodes associated with flattening sets in the
 * target list of queries containing functions returning sets.
 *
 * ExecEvalIter() -
 *   Iterate through the all return tuples/base types from a function one
 *   at time (i.e. one per ExecEvalIter call).  Not really needed for
 *   postquel functions, but for reasons of orthogonality, these nodes
 *   exist above pq functions as well as c functions.
 *
 * ExecEvalFjoin() -
 *   Given N Iter nodes return a vector of all combinations of results
 *   one at a time (i.e. one result vector per ExecEvalFjoin call).  This
 *   node does the actual flattening work.
 */

#include "tmp/postgres.h"
#include "nodes/primnodes.h"
#include "nodes/relation.h"
#include "nodes/execnodes.h"
#include "parser/parsetree.h"
#include "executor/flatten.h"


Datum
ExecEvalIter(iterNode, econtext, resultIsNull, iterIsDone)
    Iter		iterNode;
    ExprContext	 	econtext;
    bool		*iterIsDone;
    bool		*resultIsNull;
{
    List expression;

    expression = get_iterexpr(iterNode);

    /*
     * Really Iter nodes are only needed for C functions, postquel function
     * by their nature return 1 result at a time.  For now we are only worrying
     * about postquel functions, c functions will come later.
     */
    return ExecEvalExpr((Node)expression, econtext, resultIsNull, iterIsDone);
}

void
ExecEvalFjoin(tlist, econtext, isNullVect, fj_isDone)
    List		tlist;
    ExprContext	 	econtext;
    bool		*isNullVect;
    bool		*fj_isDone;
{
    bool     isDone;
    int      curNode;
    List     tlistP;
    Fjoin    fjNode     = (Fjoin)CAR(tlist);
    DatumPtr resVect    = get_fj_results(fjNode);
    BoolPtr  alwaysDone = get_fj_alwaysDone(fjNode);

    if (fj_isDone) *fj_isDone = false;
    /*
     * For the next tuple produced by the plan, we need to re-initialize
     * the Fjoin node.
     */
    if (!get_fj_initialized(fjNode))
    {
	/*
	 * Initialize all of the Outer nodes
	 */
	curNode = 1;
	foreach(tlistP, CDR(tlist))
	{
	    List tle = CAR(tlistP);

	    resVect[curNode] = ExecEvalIter(tl_expr(tle),
					    econtext,
					    &isNullVect[curNode],
					    &isDone);
	    if (isDone)
		isNullVect[curNode] = alwaysDone[curNode] = true;
	    else
		alwaysDone[curNode] = false;

	    curNode++;
	}

	/*
	 * Initialize the inner node
	 */
	resVect[0] = ExecEvalIter(tl_expr(get_fj_innerNode(fjNode)),
				  econtext,
				  &isNullVect[0],
				  &isDone);
	if (isDone)
	    isNullVect[0] = alwaysDone[0] = true;
	else
	    alwaysDone[0] = false;

	/*
	 * Mark the Fjoin as initialized now.
	 */
	set_fj_initialized(fjNode, true)
    }
    else
    {
	/*
	 * If we're already initialized, all we need to do is get the
	 * next inner result and pair it up with the existing outer node
	 * result vector.  Watch out for the degenerate case, where the
	 * inner node never returns results.
	 */

	/*
	 * Fill in nulls for every function that is always done.
	 */
	for (curNode=get_fj_nNodes(fjNode)-1; curNode >= 0; curNode--)
	    isNullVect[curNode] = alwaysDone[curNode];

	if (alwaysDone[0] == true)
	{
	    *fj_isDone = FjoinBumpOuterNodes(tlist,
					     econtext,
					     resVect,
					     isNullVect);
	    return;
	}
	else
	    resVect[0] = ExecEvalIter(tl_expr(get_fj_innerNode(fjNode)),
				      econtext,
				      &isNullVect[0],
				      &isDone);
    }

    /*
     * if the inner node is done
     */
    if (isDone)
    {
	*fj_isDone = FjoinBumpOuterNodes(tlist,
					 econtext,
					 resVect,
					 isNullVect);
	if (*fj_isDone)
	    return;

	resVect[0] = ExecEvalIter(tl_expr(get_fj_innerNode(fjNode)),
				  econtext,
				  &isNullVect[0],
				  &isDone);

    }
    return;
}

bool
FjoinBumpOuterNodes(tlist, econtext, results, nulls)
    List		tlist;
    ExprContext	 	econtext;
    DatumPtr		results;
    String		nulls;
{
    bool   funcIsDone = true;
    Fjoin  fjNode     = (Fjoin)CAR(tlist);
    String alwaysDone = get_fj_alwaysDone(fjNode);
    List   outerList  = CDR(tlist);
    List   trailers   = CDR(tlist);
    int    trailNode  = 1;
    int    curNode    = 1;

    /*
     * Run through list of functions until we get to one that isn't yet
     * done returning values.  Watch out for funcs that are always done.
     */
    while ((funcIsDone == true) && (outerList != LispNil))
    {
	if (alwaysDone[curNode] == true)
	{
	    nulls[curNode] = 'n';
	    curNode++;
	}
	else
	    results[curNode] = ExecEvalIter(tl_expr(CAR(outerList)),
					    econtext,
					    &nulls[curNode],
					    &funcIsDone);
	outerList = CDR(outerList);
    }

    /*
     * If every function is done, then we are done flattening.
     * Mark the Fjoin node unitialized, it is time to get the
     * next tuple from the plan and redo all of the flattening.
     */
    if (funcIsDone)
    {
	set_fj_initialized(fjNode, false);
	return (true);
    }

    /*
     * We found a function that wasn't done.  Now re-run every function
     * before it.  As usual watch out for functions that are always done.
     */
    trailNode = 1;
    while (trailNode != curNode)
    {
	if (alwaysDone[curNode] == true)
	    trailNode++;
	else
	    results[curNode] = ExecEvalIter(tl_expr(CAR(trailers)),
					    econtext,
					    &nulls[curNode],
					    &funcIsDone);
	trailers = CDR(trailers);
    }
    return false;
}
