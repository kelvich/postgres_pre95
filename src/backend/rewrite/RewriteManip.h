
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

