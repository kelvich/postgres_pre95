
/* module: /users/goh/postgres/o/rewrite/RewriteHandler.c
 * $Header$
 */

int ChangeTheseVars ARGS((int, short, struct _LispValue *, struct _LispValue *));
int ReplaceVarWithMulti ARGS((int, short, struct _LispValue *, struct _LispValue *));
int FixResdom ARGS((struct _LispValue *));
struct _LispValue *ModifyVarNodes ARGS((struct _LispValue *, int, int, struct RelationData *, struct _LispValue *, struct _LispValue *, struct _LispValue *));
struct _LispValue *ModifyUpdateNodes ARGS((struct _LispValue *, struct _LispValue *, char *, int));
struct _LispValue *ProcessEntry ARGS((struct _LispValue *, struct RelationData *, struct _LispValue *, int, int, char *));
struct _LispValue *QueryRewrite ARGS((struct _LispValue *));
