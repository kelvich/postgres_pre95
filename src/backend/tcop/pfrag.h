/* $Header$ */
/* pfrag.c */
Fragment InitialPlanFragments ARGS((List parsetree , Plan plan ));
bool plan_is_parallelizable ARGS((Plan plan ));
void OptimizeAndExecuteFragments ARGS((List fragmentlist , CommandDest destination ));
