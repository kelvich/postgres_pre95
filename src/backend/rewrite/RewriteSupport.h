
/* module: /users/goh/postgres/src/parser/RewriteSupport.c */

struct Prs2LocksData *RelationGetRelationLocks ARGS((struct RelationData *));
char RelationHasLocks ARGS((struct RelationData *));
struct _LispValue *RuleIdGetActionInfo ARGS((unsigned int));
char *OperOidGetName ARGS((ObjectId oproid));
