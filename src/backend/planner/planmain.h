/* ----------------------------------------------------------------
 *   FILE
 *	planmain.h
 *
 *   DESCRIPTION
 *	prototypes for planmain.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef planmainIncluded		/* include this file only once */
#define planmainIncluded	1

extern Plan query_planner ARGS((int command_type, List tlist, List qual, int currentlevel, int maxlevel));
extern Plan subplanner ARGS((LispValue flat_tlist, LispValue original_tlist, LispValue qual, int level, LispValue sortkeys));
extern Result make_result ARGS((List tlist, List resrellevelqual, List resconstantqual, Plan left, Plan right));
extern Plan make_aggplan ARGS((Plan subplan, List agg_tlist, int aggidnum));
extern bool plan_isomorphic ARGS((Plan p1, Plan p2));
extern List group_planlist ARGS((List planlist));
extern bool allNestLoop ARGS((Plan plan));
extern Plan pickNestLoopPlan ARGS((List plangroup));
extern void setPlanStats ARGS((Plan p1, Plan p2));
extern void resetPlanStats ARGS((Plan p));
extern void setPlanGroupStats ARGS((Plan plan, List planlist));
extern List setRealPlanStats ARGS((List parsetree, List planlist));
extern bool plan_contain_redundant_hashjoin ARGS((Plan plan));
extern List pruneHashJoinPlans ARGS((List planlist));

#endif /* planmainIncluded */
