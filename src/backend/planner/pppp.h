/* ----------------------------------------------------------------
 *   FILE
 *	pppp.h
 *
 *   DESCRIPTION
 *	prototypes for pppp.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef ppppIncluded		/* include this file only once */
#define ppppIncluded	1

extern void print_parse ARGS((LispValue parse));
extern void print_root ARGS((LispValue root));
extern void print_plan ARGS((Plan plan, int levelnum));
extern char *subplan_type ARGS((int type));
extern void print_subplan ARGS((LispValue subplan, int levels));
extern void print_rtentries ARGS((LispValue rt, int levels));
extern void print_tlist ARGS((LispValue tlist, int levels));
extern void print_tlistentry ARGS((LispValue tlistentry, int level));
extern void print_allqual ARGS((LispValue quals, int levels));
extern void print_qual ARGS((LispValue qual, int levels));
extern void print_var ARGS((LispValue var));
extern void print_clause ARGS((LispValue clause, int levels));
extern void print_const ARGS((LispValue node));
extern void print_param ARGS((LispValue param));
extern void pplan ARGS((Plan plan));
extern void set_query_range_table ARGS((List parsetree));
extern void p_plan ARGS((Plan plan));
extern void p_plans ARGS((List plans));
extern char *path_type ARGS((Path path));
extern void ppath ARGS((Path path));
extern void p_path ARGS((Path path));
extern void p_paths ARGS((List paths));
extern void p_rel ARGS((Rel rel));
extern void p_rels ARGS((List rels));

#endif /* ppppIncluded */
