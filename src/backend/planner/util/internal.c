
/*     
 *      FILE
 *     	internal
 *     
 *      DESCRIPTION
 *     	Definitions required throughout the query optimizer.
 *     
 */
/* RcsId ("$Header$"); */

/*     
 */

/*    
 *    	---------- SHARED MACROS
 *    
 *     	Macros common to modules for creating, accessing, and modifying
 *    	query tree and query plan components.
 *    	Shared with the executor.
 *    
 */

#include "internal.h"
#include "relation.h"
#include "plannodes.h"

/*    
 *    	---------- GLOBAL VARIABLES
 *    
 *    	These *should* disappear eventually...shouldn't they?
 *    
 */

/* from the parse tree: */

LispValue _query_result_relation_;
					/*   relid of the result relation */
int  _query_command_type_ = 0;    /*   command type as a symbol */
int  _query_max_level_ = 0;      /*   max query nesting level */
LispValue  _query_range_table_;     /*   relations to be scanned */

/*     internal to planner:  */
LispValue _query_relation_list_;    /*   global relation list */
bool _query_is_archival_;       /*   archival query flag */

/*  .. init-query-planner   */


/* (defmacro with_saved_globals (&rest,body)  XXX fix me */

LispValue 
with_saved_globals (body)
     LispValue (* body)();
{
	LispValue global_save;
/*
	global_save = list (lispInteger(_query_result_relation_),
				      lispInteger(_query_command_type_),
				      lispInteger(_query_max_level_),
				      _query_range_table_,
				      _query_relation_list_,
				      _query_is_archival_);
*/
	LispValue body_result = (* body)();
	_query_result_relation_ = nth (0,global_save);
	_query_command_type_ = CInteger(nth (1,global_save));
	_query_max_level_ = CInteger(nth (2,global_save));
	_query_range_table_ = nth (3,global_save);
	_query_relation_list_ = nth (4,global_save);
	_query_is_archival_ = (bool) CInteger(nth (5,global_save));

	return(body_result);
}

void
set_unorderedpath(node,path)
     Rel node;
     Path path;
{
    AssertArg(IsA(node,Rel));
    (Path)(node->unorderedpath) = path;
}

Path
get_unorderedpath(node)
     Rel node;
{
    return((Path)(node->unorderedpath));
}

List
joinmethod_clauses(method)
     joinmethod method;
{
    return(method->clauses);
}

List
joinmethod_keys(method)
     joinmethod method;
{
    return(method->keys);
}
     
/* XXX - these should go away once spyros turns in some code */

Node
rule_lock_plan()
{
    return((Node)NULL);
}

Node 
make_rule_lock()
{
    return((Node)NULL);
}

Node
rule_lock_type()
{
    return((Node)NULL);
}

Node 
rule_lock_attribute()
{
    return((Node)NULL);
}

Node
rule_lock_relation()
{
    return((Node)NULL);
}

Node
rule_lock_var()
{
    return((Node)NULL);
}

int _rule_lock_type_alist_ = 0;
int _rule_type_alist_ = 0;

TLE
MakeTLE(foo,bar)
     Resdom foo;
     Expr bar;
{
    return(lispCons(foo,lispCons(bar,LispNil)));
}

void
set_joinlist(foo,bar)
     TL foo;
     List bar;
{
    CADR(foo) = bar;
}
