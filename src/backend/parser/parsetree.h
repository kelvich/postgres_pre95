/* ----------------------------------------------------------------
 *   FILE
 *	parsetree.h
 *
 *   DESCRIPTION
 *	Routines to access various components and subcomponents of
 *	parse trees.  
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PARSETREE_H
#define PARSETREE_H		/* include once only */

/* ----------------
 *	need pg_lisp.h for definitions of CAR(), etc. macros
 * ----------------
 */
#include "nodes/pg_lisp.h"

/* ----------------
 *	top level parse tree macros
 *
 *  parse tree:
 *	(root targetlist qual)
 * ----------------
 */
#define parse_root(parse)                   CAR(parse)
#define parse_targetlist(parse)             CADR(parse)
#define parse_qualification(parse)          CADDR(parse)
#define parse_parallel(parse)	((length(parse)<4)?LispNil:CAR(nthCdr(3,parse)))

/* eliminate this when tcop/pquery.c is unlocked */
#define parse_tree_root(parse)              parse_root(parse)

/* ----------------
 *	root macros
 *
 *  parse tree:
 *	(root targetlist qual)
 *	 ^^^^
 *  parse root:
 *	(numlevels cmdtype resrel rangetable priority ruleinfo nestdotinfo)
 * ----------------
 */
#define root_numlevels(root)                CInteger(CAR(root))
#define root_command_type_atom(root)	    CADR(root)
#define root_command_type(root)    ((int) CAtom(root_command_type_atom(root)))
#define root_result_relation(root)          CADDR(root)
#define root_rangetable(root)               CADDR(CDR(root))
#define root_priority(root)                 CADDR(CDR(CDR(root)))
#define root_ruleinfo(root)                 CADDR(CDR(CDR(CDR(root))))
#define root_nestdotinfo(root)              CADDR(CDR(CDR(CDR(CDR(root)))))
#define root_uniqueflag(root)		    nth(6, root)
#define root_sortclause(root)		    nth(7, root)

/* ----------------
 *	result relation macros
 *
 *  parse tree:
 *	(root targetlist qual)
 *	 ^^^^
 *  parse root:
 *	(numlevels cmdtype resrel rangetable priority ruleinfo nestdotinfo)
 *			   ^^^^^^
 * ----------------
 */
#define parse_tree_result_relation(parse_tree) \
    root_result_relation(parse_root(parse_tree))

/* ----------------
 *	range table macros
 *
 *  parse tree:
 *	(root targetlist qual)
 *	 ^^^^
 *  parse root:
 *	(numlevels cmdtype resrel rangetable priority ruleinfo nestdotinfo)
 *			          ^^^^^^^^^^
 *  range table:
 *	(rtentry ...)
 *
 *  rtentry:
 *	note: this might be wrong, I don't understand how
 *	rt_time / rt_archive_time work together.  anyways it
 *      looks something like:
 *
 *	   (relname ?       relid timestuff flags rulelocks)
 *	or (new/cur relname relid timestuff flags rulelocks)
 *
 *	someone who knows more should correct this -cim 6/9/91
 * ----------------
 */
#define parse_tree_range_table(parse_tree) \
    root_rangetable(parse_root(parse_tree))

/* .. GetIDFromRangeTbl, InitPlan, print_rtentries, print_subplan
 * .. print_var
 */
#define rt_relname(rt_entry) \
      ((!strcmp(CString(CAR(rt_entry)),"*CURRENT*") ||\
        !strcmp(CString(CAR(rt_entry)),"*NEW*")) ? CAR(rt_entry) : \
        CADR(rt_entry))

/* .. ExecutePlan, best-or-subclause-index, create_index_path
 * .. exec-set-lock, in-line-lambda%598040865, index-info, index-innerjoin
 * .. parameterize, plan-union-queries, plan-union-query
 * .. preprocess-targetlist, print_rtentries, print_var, relation-info
 * .. translate-relid, write-decorate
 */
#define rt_relid(rt_entry)             CADDR(rt_entry)


/* .. ExecBeginScan, ExecCreatR, ExecOpenR, InitPlan
 * .. in-line-lambda%598040864, print_rtentries 
 */
#define rt_time(rt_entry)              CADDR(CDR(rt_entry))

/*.. in-line-lambda%598040864 */
#define rt_archive_time(rt_entry) \
    ( nth(5, rt_entry) ? rt_time (rt_entry) : LispNil )

/* .. in-line-lambda%598040866, plan-union-queries, print_rtentries */
#define rt_flags(rt_entry)         CADDR(CDR(CDR(rt_entry)))

/* .. add-read-locks, exec-make-intermediate-locks, exec-set-lock
 *.. make-parameterized-plans, make-rule-locks, print_rtentries
 * .. write-decorate
 */
#define rt_rulelocks(rt_entry)     CADDR(CDR(CDR(CDR(rt_entry))))


/*
 *	rt_fetch
 *	rt_store
 *
 *	Access and (destructively) replace rangetable entries.
 *
 * .. ExecCreatR, ExecOpenR, ExecutePlan, InitPlan, add-read-locks
 * .. exec-set-lock, plan-union-queries, write-decorate
 */
#define rt_fetch(rangetable_index, rangetable) \
    nth((rangetable_index)-1, rangetable)

#define rt_store(rangetable_index, rangetable, rt) \
    nth((rangetable_index)-1, rangetable) =  rt

/*
 *	getrelid
 *	getrelname
 *
 *	Given the range index of a relation, return the corresponding
 *	relation id or relation name.
 *
 * .. best_or_subclause_index, create_index_path, index_info
 * .. index_innerjoin, parameterize, preprocess_targetlist, print_var
 * .. relation_info, translate_relid, write_decorate
 */
#define getrelid(rangeindex,rangetable) \
    rt_relid(nth((rangeindex)-1, rangetable))

/* .. GetIDFromRangeTbl, print_subplan, print_var */
#define getrelname(rangeindex, rangetable) \
    rt_relname(nth((rangeindex)-1, rangetable))

/* ----------------
 *	rule info macros
 *
 *  parse tree:
 *	(root targetlist qual)
 *	 ^^^^
 *  parse root:
 *	(numlevels cmdtype resrel rangetable priority ruleinfo nestdotinfo)
 *			                              ^^^^^^^^
 *  ruleinfo:
 *  	(ruleid ruletag)
 * ----------------
 */
/* .. exec-make-intermediate-locks, exec-set-lock */
#define ruleinfo_ruleid(ruleinfo)     CAR(ruleinfo)

/* .. exec_make_intermediate_locks, exec_set_lock, make_rule_locks 
 * .. process_rules
 */
#define ruleinfo_ruletag(ruleinfo)    CADR(ruleinfo)


/* ----------------
 *	target list macros
 *
 *  parse tree:
 *	(root targetlist qual)
 *	      ^^^^^^^^^^
 *  target list:
 *	(tl_entry ...)
 *	 
 *  tl_entry:
 *	(resdom expr)
 * ----------------
 */
/* .. MakeAttlist, compute-attribute-width, copy-vars, flatten-tlist-vars
 * .. in-line-lambda%598040855, new-result-tlist, new-unsorted-tlist
 * .. print_tlistentry, relation-sortkeys, replace-resultvar-refs
 * .. set-join-tlist-references, sort-level-result
 * .. targetlist-resdom-numbers, tlist-member, tlist-temp-references
 */
#define tl_resdom(tl_entry)          CAR(tl_entry)
#define tl_node(tl_entry)            CAR(tl_entry)
#define tl_is_resdom(tl_entry)       IsA(CAR(tl_entry),Resdom)


/* .. MakeAttlist, copy-vars, find-tlist-read-vars, flatten-tlist
 * .. flatten-tlist-vars, flatten-tlistentry, in-line-lambda%598041218
 * .. in-line-lambda%598041219, in-line-lambda%598041221
 * .. initialize-targetlist, matching_tlvar, new-join-tlist
 * .. new-level-tlist, new-result-tlist, print_tlistentry
 * .. relation-sortkeys, set-join-tlist-references, tlist-temp-references
 * .. update-relations 
 */
#define tl_expr(tl_entry)             CADR(tl_entry)

/* ----------------------------------------------------------------
 *	NEW STUFF
 * ----------------------------------------------------------------
 */

/*
typedef struct parse_struct {
	LispValue root;
	LispValue targetlist;
	LispValue qualification;
} parse;   /* XXX constr name : new_parse */

/*
typedef struct root_struct {
  LispValue levels;
  LispValue command_type ;
  LispValue result_relation ;
  LispValue rangetable ;
  LispValue priority;
  LispValue ruleinfo;
} root;  /* XXX constr name: new_root */


/*
typedef struct resultrelation {

  LispValue id;
  LispValue archive_level;
} result_relation;
*/
/*
typedef struct rule_info {
  LispValue id;
  LispValue tag;
} ruleinfo;
*/
/*
typedef struct rt_struct {
  LispValue relation_name;
  LispValue relation_id;
  LispValue time;
  LispValue archival_p ;
} rt;
*/
/*
typedef struct tl_struct {
  LispValue  resdom ;
  LispValue  expr ; 
} tl;
*/

#endif PARSETREE_H
	     
