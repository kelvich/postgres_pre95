/*
 *	parsetree.l
 *
 *	Routines to access various components and subcomponents of parse trees.
 *
 *	XXX These will replaced with defstruct macros in Version 2.
 *  $Header$
 */



#include "pg_lisp.h"


#define parse_root(parse)                   CAR(parse)
#define parse_targetlist(parse)             CADR(parse)
#define parse_qualification(parse)          CADDR(parse)
#define root_numlevels(root)                CInteger(CAR(root))
#define root_command_type(root)             ((int)CAtom(CADR(root)))
#define root_result_relation(root)          CADDR(root)

/* .. subst-rangetable */
#define root_rangetable(root)                   CADDR(CDR(root))
#define root_priority(root)                     CADDR(CDR(CDR(root)))
#define root_ruleinfo(root)                     CADDR(CDR(CDR(CDR(root))))
#define resultrelation_name(resultrelation)     if (lisp (resultrelation)) nth (0, resultrelation)

#define resultrelation_archive(resultrelation)  if (lisp (resultrelation)) nth (1, resultrelation)


/* .. GetIDFromRangeTbl, InitPlan, print_rtentries, print_subplan
 * .. print_var
 */
#define rt_relname(rt_entry)          CAR(rt_entry)


/* .. ExecutePlan, best-or-subclause-index, create_index_path
 * .. exec-set-lock, in-line-lambda%598040865, index-info, index-innerjoin
 * .. parameterize, plan-union-queries, plan-union-query
 * .. preprocess-targetlist, print_rtentries, print_var, relation-info
 * .. translate-relid, write-decorate
 */
#define rt_relid(rt_entry)             CADR(rt_entry)


/* .. ExecBeginScan, ExecCreatR, ExecOpenR, InitPlan
 * .. in-line-lambda%598040864, print_rtentries 
 */
#define rt_time(rt_entry)              CADDR(rt_entry)

/*.. in-line-lambda%598040864 */
#define rt_archive_time(rt_entry)      ( (CADDDR(rt_entry)) ? \
					rt_time (rt_entry) : LispNil )

/* .. in-line-lambda%598040866, plan-union-queries, print_rtentries */
#define rt_flags(rt_entry)         CADDR(CDR(rt_entry))

/* .. add-read-locks, exec-make-intermediate-locks, exec-set-lock
 *.. make-parameterized-plans, make-rule-locks, print_rtentries
 * .. write-decorate
 */
#define rt_rulelocks(rt_entry)     CADDR(CDR(CDR(rt_entry)))


/*  .. ExecCreatR, ExecOpenR, ExecutePlan, InitPlan, add-read-locks
 * .. exec-set-lock, plan-union-queries, write-decorate
#define rt_fetch(inhrelid,rangetable)  nth ( inhrelid -1, rangetable)
#define rt_store(inhrelid,rangetable,rt_entry)  \
setf (nth (inhrelid -1,rangetable) ,rt_entry)
*/

/* .. MakeAttlist, compute-attribute-width, copy-vars, flatten-tlist-vars
 * .. in-line-lambda%598040855, new-result-tlist, new-unsorted-tlist
 * .. print_tlistentry, relation-sortkeys, replace-resultvar-refs
 * .. set-join-tlist-references, sort-level-result
 * .. targetlist-resdom-numbers, tlist-member, tlist-temp-references
 */
#define tl_resdom(tl_entry)          CAR(tl_entry)


/* .. MakeAttlist, copy-vars, find-tlist-read-vars, flatten-tlist
 * .. flatten-tlist-vars, flatten-tlistentry, in-line-lambda%598041218
 * .. in-line-lambda%598041219, in-line-lambda%598041221
 * .. initialize-targetlist, matching_tlvar, new-join-tlist
 * .. new-level-tlist, new-result-tlist, print_tlistentry
 * .. relation-sortkeys, set-join-tlist-references, tlist-temp-references
 * .. update-relations 
 */
#define tl_expr(tl_entry)             CADR(tl_entry)

/* .. exec-make-intermediate-locks, exec-set-lock */
#define ruleinfo_ruleid(ruleinfo)     CAR(ruleinfo)

/* .. exec_make_intermediate_locks, exec_set_lock, make_rule_locks 
 * .. process_rules
 */
#define ruleinfo_ruletag(ruleinfo)    CADR(ruleinfo)


/*
 *	NEW STUFF
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

/*
 *	rt_archival_time
 *
 *	Return the archival time range for a rangetable entry.
 */

#define rt_archival_time(rt)       if (rt_archival_p(rt)) rt_time(rt)
  

/*
 *	rt_fetch
 *	rt_store
 *
 *	Access and (destructively) replace rangetable entries.
 */

#define rt_fetch(rangetable_index,rangetable)  nth (rangetable_index -1, \
						    rangetable)

#define rt_store(rangetable_index,rangetable, rt) \
setf (nth (rangetable_index - 1,rangetable) ,rt)

/*
 *	getrelid
 *	getrelname
 *
 *	Given the range index of a relation, return the corresponding
 *	relation id or relation name.
 */

 /* .. best_or_subclause_index, create_index_path, index_info
  * .. index_innerjoin, parameterize, preprocess_targetlist, print_var
  * .. relation_info, translate_relid, write_decorate
  */
#define getrelid(rangeindex,rangetable)  rt_relid ( (nth ((rangeindex -1), rangetable)))


/* .. GetIDFromRangeTbl, print_subplan, print_var */
#define getrelname(rangeindex,rangetable) rt_relname( nth( rangeindex -1, rangetable)

