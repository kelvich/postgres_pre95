/* $Header$ */
/* pquery.c */
List MakeQueryDesc ARGS((List operation , List parsetree , Plan plantree , List state , List feature , List arglist , List typelist , LispValue nargs , CommandDest dest ));
List CreateQueryDesc ARGS((List parsetree , Plan plantree , char *argv, ObjectId *typev, int nargs, CommandDest dest ));
EState CreateExecutorState ARGS((void ));
String CreateOperationTag ARGS((int operationType ));
void ProcessPortal ARGS((String portalName , int portalType, List parseTree , Plan plan , EState state , List attinfo , CommandDest dest ));
void ProcessQueryDesc ARGS((List queryDesc ));
void ProcessQuery ARGS((List parsertree , Plan plan , char *argv, ObjectId *typev, int nargs, CommandDest dest ));
void ParallelProcessQueries ARGS((List parsetree_list , List plan_list , CommandDest dest ));
