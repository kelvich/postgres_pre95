
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

#include "planner/internal.h"
#include "relation.h"
#include "plannodes.h"
#include "primnodes.h"
#include "log.h"

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

void
save_globals ()
{
/*
  LispValue 	qrr = _query_result_relation_;
  LispValue	qtt = lispInteger(_query_command_type_);
  LispValue	qml = lispInteger(_query_max_level_);
  LispValue 	qrt = _query_range_table_;
  LispValue	qrl = _query_relation_list_;
  LispValue	qia = lispInteger(_query_is_archival_);
  LispValue 	foo;

  /* saved_global_list = nappend1(saved_global_list,foo); */
}

void
restore_globals()
{
/*
	_query_result_relation_ = nth (0,global_save);
	_query_command_type_ = CInteger(nth (1,global_save));
	_query_max_level_ = CInteger(nth (2,global_save));
	_query_range_table_ = nth (3,global_save);
	_query_relation_list_ = nth (4,global_save);
	_query_is_archival_ = (bool) CInteger(nth (5,global_save));

	return(body_result);
*/
}

List
joinmethod_clauses(method)
     JoinMethod method;
{
  return(method->clauses);
}

List
joinmethod_keys(method)
     JoinMethod method;
{
    return(method->jmkeys);
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
    CDR(foo) = bar;
}

Var
get_expr (foo)
     TLE foo;
{
    Assert(!null(foo));
    Assert(!null(CDR(foo)));
    /* Assert(IsA(foo,LispValue)); */
    return((Var)CADR(foo));
}

Resdom
get_resdom (foo)
     TLE foo;
{
    /* Assert(IsA(foo,LispValue)); */
    return((Resdom)CAR(foo));
}

TLE
get_entry(foo)
     TL foo;
{
    /* Assert(IsA(foo,LispValue)); */

    return(CAR(foo));
}
void
set_entry(node,foo)
     TL node;
     TLE foo;
{
    CAR(node) = foo;
}

List
get_joinlist(foo)
     TL foo;
{
    /* Assert(IsA(foo,LispValue)); */

    return (CDR(foo));
}

void
init_planner()
{
    _query_result_relation_ = LispNil;
    _query_command_type_ = 0;
    _query_max_level_ = 1;
    _query_range_table_ = LispNil;
    _query_relation_list_ = LispNil;
    _query_is_archival_ = false;
}
