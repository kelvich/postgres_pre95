/*
 * simplelists.h --
 *	Linked list primitives.
 *
 * Identification:
 *	$Header$
 */

#ifndef SIMPLELISTS_H
#define SIMPLELISTS_H

#ifndef C_H
#include "c.h"
#endif

#define SLNode	struct _SetNode
#define SLList	struct _SetList

#define NODE_MAGIC  0x41424344
#define LIST_MAGIC  0x45464748

SLNode {
    SLNode  *sn_Next;	/* Next node or &sn_Term	*/
    SLNode  *sn_Prev;	/* Previous node or &sn_Head	*/
    SLList  *sn_List;	/* node's list or NULL		*/ 
    uint32   sn_Magic;	/* NODE_MAGIC			*/
};

SLList {
    SLNode *sl_Head;	/* First node or &sn_Term	*/
    SLNode *sl_Term;	/* Terminator == NULL		*/
    SLNode *sl_Tail;	/* Last node or &sn_Head	*/
    Offset  sl_Offset;	/* structural offset.		*/
    uint32  sl_Magic;	/* LIST_MAGIC			*/
    uint32  sl_Pad0;	/* pad byte for 8 char align.	*/
};

extern void SLNewList();
extern void SLNewNode();
extern Pointer SLGetHead();
extern Pointer SLGetTail();
extern Pointer SLGetSucc();
extern Pointer SLGetPred();
extern void SLRemove();
extern void SLAddHead();
extern void SLAddTail();
extern Pointer SLRemHead();
extern Pointer SLRemTail();
extern void SLInsertAfter();
extern void SLInsertBefore();
extern SLList *SLGetList();

#endif

