
/* module: /users/goh/postgres/src/parser/RewriteManip.c 
 * $Header$
 */

/* RewriteManip.c */
void OffsetVarNodes ARGS((List qual_or_tlist , int offset ));
void ChangeVarNodes ARGS((List qual_or_tlist , int old_varno , int new_varno ));
void AddQual ARGS((List parsetree , List qual ));
void AddNotQual ARGS((List parsetree , List qual ));
void FixResdomTypes ARGS((List user_tlist ));
void ResolveNew ARGS((RewriteInfo *info , List targetlist , List tree ));
void FixNew ARGS((RewriteInfo *info , List parsetree ));
void HandleRIRAttributeRule ARGS((List parsetree , List rt , List tl , int rt_index , int attr_num , int *modified , int *badpostquel ));
void HandleViewRule ARGS((List parsetree , List rt , List tl , int rt_index , int *modified ));

