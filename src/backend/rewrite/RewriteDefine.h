/* 
 * $Header$
 */

/* RewriteDefine.c */
void strcpyq ARGS((char *dest , char *source ));
OID InsertRule ARGS((Name rulname , int evtype , Name evobj , Name evslot , char *evqual , bool evinstead , char *actiontree ));
void ModifyActionToReplaceCurrent ARGS((List retrieve_parsetree ));
void ValidateRule ARGS((int event_type , char *eobj_string , char *eslot_string , List event_qual , List *action , int is_instead , ObjectId event_attype ));
int DefineQueryRewrite ARGS((List args ));
int ShowRuleAction ARGS((LispValue ruleaction ));
