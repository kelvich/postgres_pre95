
/* module: /users/goh/postgres/o/rewrite/RewriteHandler.c */
/* $Id$ */

int ChangeTheseVars ARGS((int, short, struct _LispValue *, struct _LispValue *));
int ReplaceVarWithMulti ARGS((int, short, struct _LispValue *, struct _LispValue *));
int FixResdom ARGS((struct _LispValue *));
char ThisLockWasTriggered ARGS((int, short, struct _LispValue *));
struct _LispValue *MatchRetrieveLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *TheseLocksWereTriggered ARGS((struct Prs2LocksData *, struct _LispValue *, int, int));
struct _LispValue *ModifyVarNodes ARGS((struct _LispValue *, int, int, struct RelationData *, struct _LispValue *, struct _LispValue *, struct _LispValue *));
struct _LispValue *MatchLocks ARGS((char, struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *MatchReplaceLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *MatchAppendLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *MatchDeleteLocks ARGS((struct Prs2LocksData *, int, struct _LispValue *));
struct _LispValue *ModifyUpdateNodes ARGS((struct _LispValue *, struct _LispValue *, char *, int));
struct _LispValue *ProcessOneLock ARGS((struct _LispValue *, struct RelationData *, struct _LispValue *, int, int, char *));
struct _LispValue *QueryRewrite ARGS((struct _LispValue *));
