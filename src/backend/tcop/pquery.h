/* ----------------------------------------------------------------
 *   FILE
 *	pquery.h
 *
 *   DESCRIPTION
 *	prototypes for pquery.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef pqueryIncluded		/* include this file only once */
#define pqueryIncluded	1

extern List MakeQueryDesc ARGS((List operation, List parsetree, Plan plantree, List state, List feature, List arglist, List typelist, LispValue nargs, CommandDest dest));
extern List CreateQueryDesc ARGS((List parsetree, Plan plantree, char **argv, ObjectId *typev, int nargs, CommandDest dest));
extern EState CreateExecutorState ARGS((void));
extern String CreateOperationTag ARGS((int operationType));
extern void ProcessPortal ARGS((String portalName, int portalType, List parseTree, Plan plan, EState state, List attinfo, CommandDest dest));
extern void ProcessQueryDesc ARGS((List queryDesc));
extern void ProcessQuery ARGS((List parsertree, Plan plan, char *argv, ObjectId *typev, int nargs, CommandDest dest));
extern void ParallelProcessQueries ARGS((List parsetree_list, List plan_list, CommandDest dest));

#endif /* pqueryIncluded */
