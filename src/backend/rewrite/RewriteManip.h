
/* module: /users/goh/postgres/src/parser/RewriteManip.c 
 * $Header$
 */

#ifdef OLD_REWRITE
void OffsetVarNodes ARGS(( List , int ));
void FixRangeTable ARGS(( List , List ));
void AddQualifications ARGS(( List , List , int ));
void HandleVarNodes ARGS(( List, List, int, int, int ));
void UpdateRangeTableOfViewParse ARGS((Name name, List parsetree));
#endif  OLD_REWRITE


void OffsetVarNodes ARGS(( List qual_or_tlist , int offset )) ;
void ChangeVarNodes ARGS(( List qual_or_tlist , int old_varno, int new_varno));
void AddQualARGS((List parsetree, List qual));
void AddNotQualARGS((List parsetree, List qual));
Const make_nullARGS((ObjectId type));
List FindMatchingNew ARGS(( List user_tlist , int attno ));
List FindMatchingTLEntry ARGS(( List user_tlist , Name e_attname));
void ResolveNewARGS((RewriteInfo *info, List targetlist,List tree));
void FixNewARGS((RewriteInfo *info, List parsetree));
void HandleRIRAttributeRuleARGS((List parsetree, List rt, List tl, int rt_index, int attr_num,int *modified));
void HandleViewRuleARGS((List parsetree, List rt,List tl, int rt_index,int *modified));
