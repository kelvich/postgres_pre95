/* ----------------------------------------------------------------
 *   FILE
 *	parse_query.h
 *
 *   DESCRIPTION
 *	prototypes for parse_query.c.
 *
 *   NOTES
 *	Automatically generated using mkproto
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef parse_queryIncluded		/* include this file only once */
#define parse_queryIncluded	1

/* parse_query.c */
extern LispValue ModifyQueryTree ARGS((LispValue query, LispValue priority, LispValue ruletag));
extern Name VarnoGetRelname ARGS((int vnum));
extern LispValue any_ordering_op ARGS((Resdom resdom));
extern Resdom find_tl_elt ARGS((char *varname, List tlist));
extern LispValue MakeRoot ARGS((int NumLevels, LispValue query_name, LispValue result, LispValue rtable, LispValue priority, LispValue ruleinfo, LispValue unique_flag, LispValue sort_clause, LispValue targetlist));
extern LispValue MakeRangeTableEntry ARGS((Name relname, List options, Name refname));
extern LispValue ExpandAll ARGS((Name relname, int *this_resno));
extern LispValue MakeTimeRange ARGS((LispValue datestring1, LispValue datestring2, int timecode));
extern void disallow_setop ARGS((char *op, Type optype, LispValue operand));
extern LispValue make_operand ARGS((char *opname, LispValue tree, int orig_typeId, int true_typeId));
extern LispValue make_op ARGS((LispValue op, LispValue ltree, LispValue rtree, int optype));
extern List make_concat_var ARGS((List list_of_varnos, int attid, int vartype));
extern LispValue fix_param ARGS((LispValue l));
extern int find_atttype ARGS((ObjectId relid, Name attrname));
extern LispValue make_var ARGS((Name relname, Name attrname));
extern LispValue make_array_ref ARGS((LispValue expr, LispValue upperIndexpr, LispValue lowerIndexpr));
extern LispValue make_array_set ARGS((LispValue target_expr, LispValue upperIndexpr, LispValue lowerIndexpr, LispValue expr));
extern int SkipForwardToFromList ARGS((void));
extern LispValue SkipBackToTlist ARGS((void));
extern LispValue SkipForwardPastFromList ARGS((void));
extern int StripRangeTable ARGS((void));
extern LispValue make_const ARGS((LispValue value));
extern LispValue parser_ppreserve ARGS((char *temp));
extern LispValue make_param ARGS((int paramKind, char *relationName, char *attrName));
extern void param_type_init ARGS((ObjectId *typev, int nargs));
extern ObjectId param_type ARGS((int t));
extern ObjectId param_type_complex ARGS((Name n));
extern LispValue FindVarNodes ARGS((LispValue list));
extern void IsPlanUsingNewOrCurrent ARGS((List parsetree, bool *currentUsed, bool *newUsed));
extern int SubstituteParamForNewOrCurrent ARGS((List parsetree, ObjectId relid));

/* ylib.c */
extern int parser ARGS((char *str, LispValue l, ObjectId *typev, int nargs));
extern void fixupsets ARGS((LispValue parse));
extern void define_sets ARGS((LispValue clause));
extern LispValue new_filestr ARGS((LispValue filename));
extern int lispAssoc ARGS((LispValue element, LispValue list));
extern LispValue parser_typecast ARGS((LispValue expr, LispValue typename));
extern LispValue parser_typecast2 ARGS((LispValue expr, Type tp));
extern char *after_first_white_space ARGS((char *input));
extern List ParseFunc ARGS((char *funcname, List fargs, int *curr_resno));
extern List ParseAgg ARGS((char *aggname, List query, List tlist));
extern List MakeFromClause ARGS((List from_list, LispValue base_rel));
extern LispValue HandleNestedDots ARGS((List dots, int *curr_resno));
extern LispValue setup_tlist ARGS((Name attname, ObjectId relid));
extern LispValue setup_base_tlist ARGS((ObjectId typeid));
extern void make_arguments ARGS((int nargs, List fargs, ObjectId *input_typeids, ObjectId *function_typeids));

/* defined in gram.y, used in ylib.c and gram.y */
extern int NumLevels;

/* define in parse_query.c, used in gram.y */
extern ObjectId *param_type_info;
extern int pfunc_num_args;

/* useful macros */
#define ISCOMPLEX(type) (typeid_get_relid((ObjectId)type) ? true : false)

#endif /* parse_queryIncluded */
