/* ----------------------------------------------------------------
 *   FILE
 *     	internal
 *     
 *   DESCRIPTION
 *     	Definitions required throughout the query optimizer.
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	"$Header$"
 * ----------------------------------------------------------------
 */

#ifndef internalIncluded		/* include this file only once */
#define internalIncluded	1

/*    
 *    	---------- SHARED MACROS
 *    
 *     	Macros common to modules for creating, accessing, and modifying
 *    	query tree and query plan components.
 *    	Shared with the executor.
 *    
 */

#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "parser/parsetree.h"
#include "nodes/relation.h"
#include "catalog/pg_index.h"
#include "catalog/syscache.h"	/* for SearchSysCacheGetAttribute, etc. */

/* XXX - versions don't work yet, so parsetree doesn't have them,
   I define the constant here just for the heck of it - jeff */

/*   parse tree manipulation routines */

/*    
 *    	---------- GLOBAL VARIABLES
 *    
 *    	These *should* disappear eventually...shouldn't they?
 *    
 */

/* from the parse tree: */
extern LispValue _query_result_relation_; /*   relid of the result relation */
extern int _query_command_type_;    /*   command type as a symbol */
extern int _query_max_level_;      /*   max query nesting level */
extern LispValue _query_range_table_;     /*   relations to be scanned */

/*     internal to planner:  */
extern List _base_relation_list_;    /*   base relation list */
extern List _join_relation_list_;    /*   join relation list */
extern bool _query_is_archival_;       /*   archival query flag */

extern int NBuffers;

/*
 *    	System-dependent tuning constants
 *    
 */

#define _CPU_PAGE_WEIGHT_  0.065      /* CPU-to-page cost weighting factor */
#define _PAGE_SIZE_    8192           /* BLCKSZ (from ../h/bufmgr.h) */
#define _MAX_KEYS_     INDEX_MAX_KEYS /* maximum number of keys in an index */
#define _TID_SIZE_     6              /* sizeof(itemid) (from ../h/itemid.h) */

/*    
 *    	Size estimates
 *    
 */

/*     The cost of sequentially scanning a materialized temporary relation
 */
#define _TEMP_SCAN_COST_ 	10

/*     The number of pages and tuples in a materialized relation
 */
#define _TEMP_RELATION_PAGES_ 		1
#define _TEMP_RELATION_TUPLES_ 	10

/*     The length of a variable-length field in bytes
 */
#define _DEFAULT_ATTRIBUTE_WIDTH_ (2 * _TID_SIZE_)

/*    
 *    	Flags and identifiers
 *    
 */

/*     Identifier for UNION (nested dot) variable type  */

#define _UNION_TYPE_ID_    UNION

/*     Identifier for (sort) temp relations   */
#define _TEMP_RELATION_ID_   -1

/*     Identifier for invalid relation OIDs and attribute numbers for use by
 *     selectivity functions
 */
#define _SELEC_VALUE_UNKNOWN_   -1

/*     Flag indicating that a clause constant is really a parameter (or other 
 *     	non-constant?), a non-parameter, or a constant on the right side
 *    	of the clause.
 */
#define _SELEC_NOT_CONSTANT_   0
#define _SELEC_IS_CONSTANT_    1
#define _SELEC_CONSTANT_LEFT_  0
#define _SELEC_CONSTANT_RIGHT_ 2

#define retrieve_cache_attribute SearchSysCacheGetAttribute

#define or_clause_p or_clause

#define TOLERANCE 0.000001

#define FLOAT_EQUAL(X,Y) ((X) - (Y) < TOLERANCE)
#define FLOAT_IS_ZERO(X) (FLOAT_EQUAL(X,0.0))

extern int BushyPlanFlag;
#define deactivate_joininfo(joininfo)	set_inactive(joininfo, true)
#define joininfo_inactive(joininfo)	get_inactive(joininfo)

/*
 *	prototypes for internal.c.
 *	Automatically generated using mkproto
 */

extern char *save_globals ARGS((void));
extern void restore_globals ARGS((char *pgh));
extern List joinmethod_clauses ARGS((JoinMethod method));
extern List joinmethod_keys ARGS((JoinMethod method));
extern Node rule_lock_plan ARGS((void));
extern Node make_rule_lock ARGS((void));
extern Node rule_lock_type ARGS((void));
extern Node rule_lock_attribute ARGS((void));
extern Node rule_lock_relation ARGS((void));
extern Node rule_lock_var ARGS((void));
extern TLE MakeTLE ARGS((Resdom foo, Expr bar));
extern void set_joinlist ARGS((TL foo, List bar));
extern Var get_expr ARGS((TLE foo));
extern Resdom get_resdom ARGS((TLE foo));
extern TLE get_entry ARGS((TL foo));
extern void set_entry ARGS((TL node, TLE foo));
extern List get_joinlist ARGS((TL foo));
extern void init_planner ARGS((void));

#endif /* internalIncluded */
