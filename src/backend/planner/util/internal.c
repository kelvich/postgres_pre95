
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

/*    
 *    	---------- GLOBAL VARIABLES
 *    
 *    	These *should* disappear eventually...shouldn't they?
 *    
 */

/* from the parse tree: */
LispValue _query_result_relation_ = LispNil; 
					/*   relid of the result relation */
LispValue _query_command_type_ = LispNil;    /*   command type as a symbol */
LispValue _query_max_level_ = LispNil;      /*   max query nesting level */
LispValue _query_range_table_;     /*   relations to be scanned */

/*     internal to planner:  */
LispValue _query_relation_list_;    /*   global relation list */
LispValue _query_is_archival_;       /*   archival query flag */

/*  .. init-query-planner   */


/* (defmacro with_saved_globals (&rest,body)  XXX fix me */

LispValue 
with_saved_globals (body)
     LispValue body;
{
	LispValue global_save = gensym ();
	LispValue body_result = gensym ();
	LispValue global_save = list (_query_result_relation_,
				      _query_command_type_,
				      _query_max_level_,
				      _query_range_table_,
				      _query_relation_list_,
				      _query_is_archival_);
	LispValue body_result = body;
	_query_result_relation_ = nth (0,global_save);
	_query_command_type_ = nth (1,global_save);
	_query_max_level_ = nth (2,global_save);
	_query_range_table_ = nth (3,global_save);
	_query_relation_list_ = nth (4,global_save);
	_query_is_archival_ = nth (5,global_save);

	return(body_result);
}
