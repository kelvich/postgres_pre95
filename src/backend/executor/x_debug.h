/* ----------------------------------------------------------------
 *   FILE
 *	ex_debug.h
 *
 *   DESCRIPTION
 *	prototypes for ex_debug.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ex_debugIncluded		/* include this file only once */
#define ex_debugIncluded	1

extern bool DebugVariableSet ARGS((String debug_variable, int debug_value));
extern bool DebugVariableProcessCommand ARGS((char *buf));
extern void DebugVariableFileSet ARGS((String filename));
extern int PrintParallelDebugInfo ARGS((FILE *statfp));
extern int ResetParallelDebugInfo ARGS((void));
extern Pointer FQdGetCommand ARGS((List queryDesc));
extern Pointer FQdGetCount ARGS((List queryDesc));
extern Pointer FGetOperation ARGS((List queryDesc));
extern Pointer FQdGetParseTree ARGS((List queryDesc));
extern Pointer FQdGetPlan ARGS((List queryDesc));
extern Pointer FQdGetState ARGS((List queryDesc));
extern Pointer FQdGetFeature ARGS((List queryDesc));
extern Pointer Fparse_tree_range_table ARGS((List queryDesc));
extern Pointer Fparse_tree_result_relation ARGS((List queryDesc));
extern void say_at_init ARGS((void));
extern void say_pre_proc ARGS((void));
extern void say_pre_end ARGS((void));
extern void say_post_init ARGS((void));
extern void say_post_proc ARGS((void));
extern void say_post_end ARGS((void));
extern void say_yow ARGS((void));
extern void noop ARGS((void));
extern void InitHook ARGS((HookNode hook));
extern void NoisyHook ARGS((HookNode hook));
extern String GetNodeName ARGS((Node node));
extern Plan AddMaterialNode ARGS((Plan plan));
extern Plan AddSortNode ARGS((Plan plan, int op));
extern Plan AddUniqueNode ARGS((Plan plan));

#endif /* ex_debugIncluded */
