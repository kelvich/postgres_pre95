
/* module: /users/goh/postgres/o/rewrite/RewriteHandler.c
 * $Header$
 */

#ifdef OLD_REWRITE
int ChangeTheseVars ARGS((int, short, struct _LispValue *, struct _LispValue *));
int ReplaceVarWithMulti ARGS((int, short, struct _LispValue *, struct _LispValue *));
int FixResdom ARGS((struct _LispValue *));
struct _LispValue *ModifyVarNodes ARGS((struct _LispValue *, int, int, struct RelationData *, struct _LispValue *, struct _LispValue *, struct _LispValue *));
struct _LispValue *ModifyUpdateNodes ARGS((struct _LispValue *, struct _LispValue *, char *, int));
struct _LispValue *ProcessEntry ARGS((struct _LispValue *, struct RelationData *, struct _LispValue *, int, int, char *));
struct _LispValue *QueryRewrite ARGS((struct _LispValue *));
#endif OLD_REWRITE

struct _rewrite_meta_knowledge {
    List rt;
    int rt_index;
    int instead_flag;
    int event;
    int action;
    int current_varno;
    int new_varno;
    List rule_action,rule_qual;
    int nothing;
};

typedef struct _rewrite_meta_knowledge RewriteInfo;

RewriteInfo *GatherRewriteMeta ARGS((List parsetree, List rule_action, List rule_qual, int rt_index, int event, int *instead_flag));
List OptimizeRIRRules ARGS((List locks));
List OrderRules ARGS((List locks));
List FireRetrieveRulesAtQuery ARGS((List parsetree, int rt_index, Relation relation,int *instead_flag));
ApplyRetrieveRule ARGS((List parsetree, List rule, int rt_index,int relation_level, int attr_num,int *modified));
List ProcessRetrieveQuery ARGS((List parsetree, List rt,int *instead_flag, int rule));
List FireRules ARGS((List parsetree, int rt_index, int event, int *instead_flag, List locks, List *products));
List ProcessUpdateNode ARGS((List parsetree, int rt_index, int event, int *instead_flag,RuleLock relation_locks, List *products));
List RewriteQuery ARGS((List parsetree,int *instead_flag, List *products));
List QueryRewrite ARGS(( List parsetree ));
List CopyAndAddQual ARGS((List parsetree, List actions, List rule_qual, int rt_index, int event));



