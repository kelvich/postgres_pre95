
/*
 *  simplelists.h
 */

#ifndef SimpleListsIncluded
#define SimpleListsIncluded

#define SimpleNode	struct _SimpleNode
#define SimpleList	struct _SimpleList

SimpleNode {
    SimpleNode *sn_Next;	/* Next node or &sn_Term	*/
    SimpleNode *sn_Prev;	/* Previous node or &sn_Head	*/
    short	sn_Offset;	/* structural offset.		*/
};

SimpleList {
    SimpleNode *sl_Head;	/* First node or &sn_Term	*/
    SimpleNode *sl_Term;	/* Terminator == NULL		*/
    SimpleNode *sl_Tail;	/* Last node or &sn_Head	*/
};

extern void SListNewList();
extern void SListNewNode();
extern void *SListGetHead();
extern void *SListGetTail();
extern void *SListGetSucc();
extern void *SListGetPred();
extern void SListRemove();
extern void SListAddHead();
extern void SListAddTail();
extern void *SListRemHead();
extern void *SListRemTail();

#endif

