
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
void AddQual ARGS((List parsetree, List qual));
void AddNotQual ARGS((List parsetree, List qual));
Const make_null ARGS((ObjectId type));
List FindMatchingNew ARGS(( List user_tlist , int attno ));
List FindMatchingTLEntry ARGS(( List user_tlist , Name e_attname));
void ResolveNew ARGS((RewriteInfo *info, List targetlist,List tree));
void FixNew ARGS((RewriteInfo *info, List parsetree));
void HandleRIRAttributeRule ARGS((List parsetree, List rt, List tl, int rt_index, int attr_num,int *modified));
void HandleViewRule ARGS((List parsetree, List rt,List tl, int rt_index,int *modified));
