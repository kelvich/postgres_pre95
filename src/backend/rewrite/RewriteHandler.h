
/* module: /users/goh/postgres/o/rewrite/RewriteHandler.c */
/* $Id$ */

int ChangeTheseVars(int, short, struct _LispValue *, struct _LispValue *);
int ReplaceVarWithMulti(int, short, struct _LispValue *, struct _LispValue *);
int FixResdom(struct _LispValue *);
char ThisLockWasTriggered(int, short, struct _LispValue *);
struct _LispValue *MatchRetrieveLocks(struct Prs2LocksData *, int, struct _LispValue *);
struct _LispValue *TheseLocksWereTriggered(struct Prs2LocksData *, struct _LispValue *, int, int);
struct _LispValue *ModifyVarNodes(struct _LispValue *, int, int, struct RelationData *, struct _LispValue *, struct _LispValue *, struct _LispValue *);
struct _LispValue *MatchLocks(char, struct Prs2LocksData *, int, struct _LispValue *);
struct _LispValue *MatchReplaceLocks(struct Prs2LocksData *, int, struct _LispValue *);
struct _LispValue *MatchAppendLocks(struct Prs2LocksData *, int, struct _LispValue *);
struct _LispValue *MatchDeleteLocks(struct Prs2LocksData *, int, struct _LispValue *);
struct _LispValue *ModifyUpdateNodes(struct _LispValue *, struct _LispValue *, char *, int);
struct _LispValue *ProcessOneLock(struct _LispValue *, struct RelationData *, struct _LispValue *, int, int, char *);
struct _LispValue *QueryRewrite(struct _LispValue *);
