/*
 * simplelists.h --
 *	Linked list primitives.
 *
 * Identification:
 *	$Header$
 */

#ifndef SimpleListsIncluded
#define SimpleListsIncluded

#ifndef C_H
#include "c.h"
#endif

#define SetNode	struct _SetNode
#define SetList	struct _SetList

#define NODE_MAGIC  0x41424344
#define LIST_MAGIC  0x45464748

SetNode {
    SetNode *sn_Next;	/* Next node or &sn_Term	*/
    SetNode *sn_Prev;	/* Previous node or &sn_Head	*/
    SetList *sn_List;
    ulong    sn_Magic;
};

SetList {
    SetNode *sl_Head;	/* First node or &sn_Term	*/
    SetNode *sl_Term;	/* Terminator == NULL		*/
    SetNode *sl_Tail;	/* Last node or &sn_Head	*/
    Offset  sl_Offset;	/* structural offset.		*/
    ulong   sl_Magic;
};

extern void SetNewList();
extern void SetNewNode();
extern void *SetGetHead();
extern void *SetGetTail();
extern void *SetGetSucc();
extern void *SetGetPred();
extern void SetRemove();
extern void SetAddHead();
extern void SetAddTail();
extern void *SetRemHead();
extern void *SetRemTail();
extern void SetInsertAfter();
extern void SetInsertBefore();
extern SetList *SetGetList();

#endif

