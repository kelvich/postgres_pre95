/* sets.h
 *
 * $Header$
 */

/* Temporary name of set, before SetDefine changes it. */

#ifndef SetsIncluded
#define SetsIncluded 1

#define GENERICSETNAME "zyxset"
extern ObjectId SetDefine ARGS((char *querystr, Name typename));
extern int32 seteval ARGS(());

#endif SetsIncluded
