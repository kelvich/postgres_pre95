
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
 /* provide ("internal"); */

#include "nodes/nodes.h"
#include "nodes/primnodes.h"
#include "nodes/plannodes.h"
#include "parser/parsetree.h"
#include "nodes/relation.h"
#include "catalog/pg_index.h
#include "tmp/c.h"

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
extern LispValue _query_relation_list_;    /*   global relation list */
extern bool _query_is_archival_;       /*   archival query flag */

extern void save_globals();
extern void restore_globals();

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

/*     	=======================
 *     	Internal planner nodes.
 *     	=======================
 */

/*    
 *    	"Keys" of various flavors.
 *    
 */

typedef struct order_key {
    int attribute_number;
    LispValue  array_index ;
} orderkey;        /* XXX lisp constructor name: create_orderkey () */


typedef struct join_key {
    LispValue outer ;
    LispValue inner ;
} joinkey;        /* XXX lisp constructor name: create_joinkey */


typedef struct merge_order {
    LispValue join_operator;
    LispValue left_operator;
    LispValue right_operator;
    LispValue left_type;
    LispValue right_type;
} mergeorder;  /* XXX lisp constructor name: create_mergeorder. */


/*    
 *    	Join method information nodes.
 *    
 */

typedef struct join_method {
     LispValue keys;
     LispValue clauses;
} *joinmethod;     

/*
typedef struct merge_info {
     LispValue ordering;
} mergeinfo;


typedef struct hash_info {
     LispValue op;
} hashinfo;
*/

extern void set_unorderedpath ARGS((Rel node; Path unorderedpath));
extern Path get_unorderedpath ARGS((Rel node));
extern LispValue joinmethod_clauses ARGS((joinmethod node));
extern LispValue joinmethod_keys ARGS((joinmethod node));

extern LispValue SearchSysCacheGetAttribute();
extern LispValue TypeDefaultRetrieve();

#define retrieve_cache_attribute SearchSysCacheGetAttribute


#define or_clause_p or_clause
/*
#define INDEXSCAN  20
#define SEQSCAN    21
#define NESTLOOP   22
#define HASHJOIN   23
#define MERGESORT  24
#define SORT       25    
#define HASH       26
*/
extern TLE MakeTLE();
extern void set_joinlist();

#define TOLERANCE 0.000001

#define FLOAT_EQUAL(X,Y) ((X) - (Y) < TOLERANCE)
#define FLOAT_IS_ZERO(X) (FLOAT_EQUAL(X,0.0))
