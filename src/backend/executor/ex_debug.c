/* ----------------------------------------------------------------
 *   FILE
 *	ex_debug.c
 *	
 *   DESCRIPTION
 *	routines for manipulation of global debugging variables and
 *	other miscellanious stuff.
 *
 *   INTERFACE ROUTINES
 *	DebugVariableProcessCommand	- process a debugging command
 *	DebugVariableFileSet		- process debugging cmds from a file
 *
 *   NOTES
 *	None of the code in this file is essential to the executor.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) execdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h
 *	6) externs.h comes last
 * ----------------
 */
#include "executor/execdebug.h"

#include "parser/parsetree.h"
#include "utils/mcxt.h"

#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/execnodes.h"
#include "nodes/execnodes.a.h"

#include "executor/execdefs.h"
#include "executor/execmisc.h"

#include "executor/externs.h"

/* ----------------
 *	planner cost variables
 * ----------------
 */
extern bool _enable_indexscan_;
extern bool _enable_seqscan_;
extern bool _enable_indexscan_;
extern bool _enable_sort_;
extern bool _enable_hash_;
extern bool _enable_nestloop_;
extern bool _enable_mergesort_;
extern bool _enable_hashjoin_;
 
 
/* ----------------
 *	executor debugging hooks
 * ----------------
 */
int _debug_hook_id_ = 0;
HookNode _debug_hooks_[10];
 
/* ----------------
 *	execution repetition
 * ----------------
 */
int _exec_repeat_ = 1;

/* ----------------
 *	Quiet flag - used to silence unwanted output
 * ----------------
 */
int Quiet;

/* ----------------------------------------------------------------
 *		   debuging variable declaration
 * ----------------------------------------------------------------
 */
/* ----------------
 *	cpp hokum to make the table look nicer
 *	(note: lack of spaces here is critical)
 * ----------------
 */
#define debugconstantquote(x)"
#define debugprefixquote(x)debugconstantquote(x)x
#define debugquote(x)debugprefixquote(x)"
#define DV(x) { debugquote(x), (Pointer) &x }
 
/* ----------------
 *	DebuggingVariables is our table of debugging variables
 * ----------------
 */
DebugVariable DebuggingVariables[] = {
    /* ----------------
     *	these first set are planner cost variables
     * ----------------
     */
    DV(_enable_indexscan_),
    DV(_enable_seqscan_),
    DV(_enable_sort_),
    DV(_enable_hash_),
    DV(_enable_nestloop_),
    DV(_enable_mergesort_),
    DV(_enable_hashjoin_),
    
    
    /* ----------------
     *	these next are executor debug hook variables
     * ----------------
     */
    DV(_debug_hook_id_),

    /* ----------------
     *	next comes the executor repeat count
     * ----------------
     */
    DV(_exec_repeat_),
    
    /* ----------------
     *	an then there's the Quiet flag
     * ----------------
     */
    DV(Quiet),
};
 
#define NumberDebuggingVariables \
    (sizeof(DebuggingVariables)/sizeof(DebuggingVariables[0]))
 
/* ----------------------------------------------------------------
 *			support functions
 * ----------------------------------------------------------------
 */
/* ----------------
 *	SearchDebugVariables scans the table for the debug variable
 *	entry given the name of the debugging variable.  It returns
 *	NULL if the search fails.
 *
 *	Note: if the table gets larger than 50 items, then 
 *	linear search should be replaced by matt's hash lookup.
 *	Even so, it is only called by DebugVariableSet which is called
 *	explicitly by the user so it only needs to be
 *	interactively-acceptable.
 * ----------------
 */
/**** xxref:
 *           DebugVariableSet
 *           DebugVariableProcessCommand
 ****/
static
DebugVariablePtr
SearchDebugVariables(debug_variable)
    String debug_variable;
{
    int i;
    String s;
    
    for (i=0; i<NumberDebuggingVariables; i++) {
	s = DebuggingVariables[i].debug_variable;
	if (strcmp(debug_variable, s) == 0)
	    return &(DebuggingVariables[i]);
    }
    
    return NULL;
}
 
/* ----------------
 *	DebugVariableSet sets the value of the specified variable.
 *	We assume debug_address is a pointer to an int.
 * ----------------
 */
/**** xxref:
 *           DebugVariableProcessCommand
 ****/
bool
DebugVariableSet(debug_variable, debug_value)
    String debug_variable;
    int	   debug_value;
{
    DebugVariablePtr p;
    
    p = SearchDebugVariables(debug_variable);
    if (p == NULL)
	return false;
    
    *((int *) p->debug_address) = debug_value;
    
    return true;
}
 
/* ----------------
 *	DebugVariableProcessCommand processes one of three
 *	types of commands:
 *
 *	DEBUG variable value	- assign value to variable
 *	DEBUG ? variable	- print value of variable
 *	DEBUG P *		- print all variables & values
 *	DEBUG X *		- initialize X debugging hook
 *
 *	It returns false if it couldn't parse the command.
 * ----------------
 */
/**** xxref:
 *           DebugVariableFileSet
 ****/
bool
DebugVariableProcessCommand(buf)
    char *buf;
{
    DebugVariablePtr p;
    int 	i;
    char 	s[128];
    int 	v;
    bool 	result;
    
    if (sscanf(buf, "DEBUG %s%d", s, &v) == 2) {
	/* ----------------
	 *	set the variable in s to the value in v
	 * ----------------
	 */
	result = DebugVariableSet(s, v);
	if (! Quiet)
	    printf("DEBUG %s = %d\n", s, v);
	return result;
	
    } else if (sscanf(buf, "DEBUG ? %s", s) == 1) {
	/* ----------------
	 *	print the value of the variable in s
	 * ----------------
	 */
        p = SearchDebugVariables(s);
	if (p == NULL) {
	    printf("DEBUG [%s] no such variable\n", s);
	    return false;
	}
	v = *((int *) p->debug_address);
	if (! Quiet)
	    printf("DEBUG %s = %d\n", s, v);
	return true;
	
    } else if (sscanf(buf, "DEBUG P %s", s) == 1) {
	/* ----------------
	 *	print the value of all variables
	 * ----------------
	 */
	for (i=0; i<NumberDebuggingVariables; i++) {
	    v = *((int *) DebuggingVariables[i].debug_address);
	    if (! Quiet)
		printf("DEBUG %s = %d\n",
		       DebuggingVariables[i].debug_variable,
		       v);
	}
	return true;
	
    } else if (sscanf(buf, "DEBUG X %s", s) == 1) {
	MemoryContext oldContext;
	extern void InitXHook();
	
	/* ----------------
	 *	assign the X debugging hooks in the top memory context
	 *	so that they aren't wiped out at end-transaction time.
	 * ----------------
	 */
	oldContext = MemoryContextSwitchTo(TopMemoryContext);
    
	_debug_hook_id_ = 1;
	_debug_hooks_[ _debug_hook_id_ ] = (HookNode) RMakeHookNode();
	InitXHook( _debug_hooks_[ _debug_hook_id_ ] );

	(void) MemoryContextSwitchTo(oldContext);
	return true;
    }
    
    return false;
}
 
/* ----------------
 *	DebugVariableFileSet reads an entire file of
 *	assignments and sets the values of the variables
 *	in the file one at a time..
 * ----------------
 */
/**** xxref:
 *           InitializeExecutor
 ****/
void
DebugVariableFileSet(filename)
    String filename;
{
    FILE *dfile;
    char buf[128];
    char s[128];
    int  v;
    
    dfile = fopen(filename, "r");
    if (dfile == NULL)
	return;
    
    bzero(buf, sizeof(buf));
    
    while (fgets(buf, 127, dfile) != NULL) {
	if (buf[0] != '#')
	    DebugVariableProcessCommand(buf);
	
	bzero(buf, sizeof(buf));
    }
    
    fclose(dfile);
}

/* ----------------
 *	parallel debugging stuff.  XXX comment me
 * ----------------
 */
#ifdef PARALLELDEBUG
#include "executor/paralleldebug.h"
struct paralleldebuginfo ParallelDebugInfo[] = {
/* 0 */ {"SharedLock", 0, 0, 0},
/* 1 */ {"ExclusiveLock", 0, 0, 0},
/* 2 */ {"FileRead", 0, 0, 0},
/* 3 */ {"FileSeek", 0, 0, 0},
};
#define NPARALLELDEBUGINFO      4

PrintParallelDebugInfo()
{
    int i;
    for (i=0; i<NPARALLELDEBUGINFO; i++) {
	if (ParallelDebugInfo[i].count > 0)
	   fprintf(stderr, "!\t%sCount %d %sTime %.6f\n",
		ParallelDebugInfo[i].name, ParallelDebugInfo[i].count,
		ParallelDebugInfo[i].name, ParallelDebugInfo[i].time/1e6);
      }
}

ResetParallelDebugInfo()
{
    int i;
    for (i=0; i<NPARALLELDEBUGINFO; i++) {
	ParallelDebugInfo[i].count = 0;
	ParallelDebugInfo[i].time = 0;
     }
}
#endif /* PARALLELDEBUG */

/* ----------------------------------------------------------------
 *			debugging functions
 * ----------------------------------------------------------------
 */
/* ----------------
 *	dbx does not understand macros so these functions are useful...
 * ----------------
 */
Pointer
FQdGetCommand(queryDesc)
    List queryDesc;
{
    return (Pointer) QdGetCommand(queryDesc);
}
 
Pointer
FQdGetCount(queryDesc)
    List queryDesc;
{
    return (Pointer) QdGetCount(queryDesc);
}
 
Pointer
FGetOperation(queryDesc)
    List queryDesc;
{
    return (Pointer) GetOperation(queryDesc);
}
 
Pointer
FQdGetParseTree(queryDesc)
    List queryDesc;
{
    return (Pointer) QdGetParseTree(queryDesc);
}
 
Pointer
FQdGetPlan(queryDesc)
    List queryDesc;
{
    return (Pointer) QdGetPlan(queryDesc);
}
 
Pointer
FQdGetState(queryDesc)
    List queryDesc;
{
    return (Pointer) QdGetState(queryDesc);
}
 
Pointer
FQdGetFeature(queryDesc)
    List queryDesc;
{
    return (Pointer) QdGetFeature(queryDesc);
}
 
Pointer
Fparse_tree_range_table(queryDesc)
    List queryDesc;
{
    return (Pointer) parse_tree_range_table(queryDesc);
}
 
Pointer
Fparse_tree_result_relation(queryDesc)
    List queryDesc;
{
    return (Pointer) parse_tree_result_relation(queryDesc);
}
 
/* ----------------------------------------------------------------
 *	simple hook utility functions
 * ----------------------------------------------------------------
 */
 
/* ----------------
 *	Simple hook functions
 * ----------------
 */
void say_at_init()	{ puts("at_init"); }
void say_pre_proc() 	{ puts("pre_proc"); }
void say_pre_end()	{ puts("pre_end"); }
void say_post_init()	{ puts("post_init"); }
void say_post_proc()	{ puts("post_proc"); }
void say_post_end()	{ puts("post_end"); }
void say_yow()		{ puts("yow"); }
void noop()		{ return; }
 
/* ----------------
 *	InitHook just initializes all the hook functions to noop
 *	for the given hook node.
 * ----------------
 */
void
InitHook(hook)
    HookNode hook;
{
    set_hook_at_initnode(hook, noop);
    set_hook_pre_procnode(hook, noop);
    set_hook_pre_endnode(hook, noop);
    set_hook_post_initnode(hook, noop);
    set_hook_post_procnode(hook, noop);
    set_hook_post_endnode(hook, noop);
}
 
/* ----------------
 *	NoisyHook just initializes all the hook functions to
 *	various say_xxx() routines for the given hook node.
 * ----------------
 */
void
NoisyHook(hook)
    HookNode hook;
{
    set_hook_at_initnode(hook,   say_at_init);
    set_hook_pre_procnode(hook,  say_pre_proc);
    set_hook_pre_endnode(hook,   say_pre_end);
    set_hook_post_initnode(hook, say_post_init);
    set_hook_post_procnode(hook, say_post_proc);
    set_hook_post_endnode(hook,  say_post_end);
}
 
/* ----------------
 *	GetNodeName
 * ----------------
 */
/**** xxref:
 *           nice_pre_proc
 *           nice_pre_end
 *           nice_post_init
 *           nice_post_proc
 *           nice_post_end
 ****/
String
GetNodeName(node)
    Node node;
{
    switch(NodeType(node)) {
    case classTag(Result):
	return (String) "Result";
    
    case classTag(Append):
	return (String) "Append";
    
    case classTag(NestLoop):
	return (String) "NestLoop";
    
    case classTag(MergeJoin):
	return (String) "MergeJoin";
    
    case classTag(HashJoin):
	return (String) "HashJoin";
    
    case classTag(SeqScan):
	return (String) "SeqScan";
    
    case classTag(IndexScan):
	return (String) "IndexScan";
    
    case classTag(Material):
	return (String) "Material";
    
    case classTag(Sort):
	return (String) "Sort";
    
    case classTag(Unique):
	return (String) "Unique";
    
    case classTag(Hash):
	return (String) "Hash";

    default:
	return (String) "???";
    }
}
 
/* ----------------------------------------------------------------
 *	the AddXXXNode functions are used to make last minute
 *	hacks to the plans before the executor gets at them.
 * ----------------------------------------------------------------
 */
/* ----------------
 *	AddMaterialNode adds a material node to a plan
 *
 *	This is only supposed to be used in dbx for debugging..
 *	-cim 1/26/89
 * ----------------
 */
Plan
AddMaterialNode(plan)
    Plan plan;
{
    Material  material;
    List      targetlist;
    List      tl;
    List      resdom;
    
    material = MakeMaterial();
    targetlist = get_qptargetlist(plan);
    set_qptargetlist(material, targetlist);
    set_lefttree(material, plan);
    return (Plan) material;
}
 
/* ----------------
 *	AddSortNode adds a sort node to a plan
 *
 *	This is only supposed to be used in dbx for debugging..
 *	-cim 10/16/89
 * ----------------
 */
Plan
AddSortNode(plan, op)
    Plan plan;
    int op;
{
    Sort      sort;
    List      targetlist;
    List      tl;
    List      resdom;
    
    sort = MakeSort();
    targetlist = get_qptargetlist(plan);
    foreach (tl, targetlist) {
	resdom = (List) tl_resdom(CAR(tl));
	set_reskey(resdom, 1);
	set_reskeyop(resdom, op);
    }
    set_qptargetlist(sort, targetlist);
    set_lefttree(sort, plan);
    set_tempid(sort, -1);
    set_keycount(sort, 1);
    return (Plan) sort;
}
 
/* ----------------
 *	AddUniqueNode adds a unique node to a plan
 *
 *	This is only supposed to be used in dbx for debugging..
 *	-cim 1/30/89
 * ----------------
 */
Plan
AddUniqueNode(plan)
    Plan plan;
{
    Unique    unique;
    List      targetlist;
    List      tl;
    List      resdom;
    
    unique = MakeUnique();
    targetlist = get_qptargetlist(plan);
    set_qptargetlist(unique, targetlist);
    set_lefttree(unique, plan);
    return (Plan) unique;
}
