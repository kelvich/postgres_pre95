
/*
 * $Header$
 */

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


/* RewriteHandler.c */
RewriteInfo *GatherRewriteMeta ARGS((List parsetree , List rule_action , List rule_qual , int rt_index , int event , int *instead_flag ));
List OptimizeRIRRules ARGS((List locks ));
List OrderRules ARGS((List locks ));
int AllRetrieve ARGS((List actions ));
List StupidUnionRetrieveHack ARGS((List parsetree , List actions ));
List FireRetrieveRulesAtQuery ARGS((List parsetree , int rt_index , Relation relation , int *instead_flag , int rule_flag ));
int ApplyRetrieveRule ARGS((List parsetree , List rule , int rt_index , int relation_level , int attr_num , int *modified ));
List ProcessRetrieveQuery ARGS((List parsetree , List rt , int *instead_flag , int rule ));
List CopyAndAddQual ARGS((List parsetree , List actions , List rule_qual , int rt_index , int event ));
List FireRules ARGS((List parsetree , int rt_index , int event , int *instead_flag , List locks , List *qual_products ));
List ProcessUpdateNode ARGS((List parsetree , int rt_index , int event , int *instead_flag , RuleLock relation_locks , List *qual_products ));
List RewriteQuery ARGS((List parsetree , int *instead_flag , List *qual_products ));
List QueryRewrite ARGS((List parsetree ));



