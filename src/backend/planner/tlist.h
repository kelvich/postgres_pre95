/* ----------------------------------------------------------------
 *   FILE
 *	tlist.h
 *
 *   DESCRIPTION
 *	prototypes for tlist.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef tlistIncluded		/* include this file only once */
#define tlistIncluded	1

extern LispValue tlistentry_member ARGS((Var var, List targetlist));
extern Expr matching_tlvar ARGS((Var var, List targetlist));
extern LispValue add_tl_element ARGS((Rel rel, Var var, List joinlist));
extern TL create_tl_element ARGS((Var var, int resdomno, List joinlist));
extern LispValue get_actual_tlist ARGS((LispValue tlist));
extern Resdom tlist_member ARGS((Var var, List tlist));
extern Resdom tlist_resdom ARGS((LispValue tlist, Resdom resnode));
extern LispValue match_varid ARGS((Var test_var, LispValue tlist));
extern List new_unsorted_tlist ARGS((List targetlist));
extern LispValue targetlist_resdom_numbers ARGS((LispValue targetlist));
extern LispValue copy_vars ARGS((LispValue target, LispValue source));
extern LispValue flatten_tlist ARGS((LispValue tlist));
extern LispValue flatten_tlist_vars ARGS((LispValue full_tlist, LispValue flat_tlist));

#endif /* tlistIncluded */
