
/*
 *  SIMPLELISTS.C
 *
 *	Matt Dillon
 *
 *	These list routines support nodes which are not at the beginning
 *	of structures.  When initializing a new list node, an offset is
 *	calculated, and all returned pointers point to the base of
 *	the structure rather than the base of the list node.
 */

#include "c.h"

RcsId("$Header$");

#include "simplelists.h"

#define NODE	SimpleNode
#define LIST	SimpleList
#define SBASE(node)	((void *)((char *)(node) - (node)->sn_Offset))

void
SListNewList(list)
LIST *list;
{
    list->sl_Head = (NODE *)&list->sl_Term;
    list->sl_Term = NULL;
    list->sl_Tail = (NODE *)&list->sl_Head;
}

void
SListNewNode(base, node)
void *base;
NODE *node;
{
    node->sn_Offset = (char *)node - (char *)base;
    node->sn_Next = NULL;	/* mark is unattached */
}

void *
SListGetHead(list)
register LIST *list;
{
    register NODE *node;
    node = list->sl_Head;
    if (node->sn_Next)
	return(SBASE(node));
    return(NULL);
}

void *
SListGetTail(list)
register LIST *list;
{
    register NODE *node;
    node = list->sl_Tail;
    if (node->sn_Prev)
	return(SBASE(node));
    return(NULL);
}

void *
SListGetPred(node)
register NODE *node;
{
    node = node->sn_Prev;
    if (node->sn_Prev)
	return(SBASE(node));
    return(NULL);
}

void *
SListGetSucc(node)
register NODE *node;
{
    node = node->sn_Next;
    if (node->sn_Next) 
	return(SBASE(node));
    return(NULL);
}

void
SListRemove(node)
register NODE *node;
{
    node->sn_Next->sn_Prev = node->sn_Prev;
    node->sn_Prev->sn_Next = node->sn_Next;
    node->sn_Next = NULL;	/* Marker, used only for error detection */
}

void
SListAddHead(list, node)
register LIST *list;
register NODE *node;
{
    node->sn_Next = list->sl_Head;
    node->sn_Prev = (NODE *)&list->sl_Head;
    node->sn_Next->sn_Prev = node;
    node->sn_Prev->sn_Next = node;
}

void
SListAddTail(list, node)
register LIST *list;
register NODE *node;
{
    node->sn_Next = (NODE *)&list->sl_Term;
    node->sn_Prev = list->sl_Tail;
    node->sn_Next->sn_Prev = node;
    node->sn_Prev->sn_Next = node;
}

void
SListInsertAfter(node, newnode)
register NODE *node, *newnode;
{
    newnode->sn_Next = node->sn_Next;
    newnode->sn_Prev = node;
    node->sn_Next = newnode;
    newnode->sn_Next->sn_Prev = newnode;
}

void
SListInsertBefore(node, newnode)
register NODE *node, *newnode;
{
    newnode->sn_Next = node;
    newnode->sn_Prev = node->sn_Prev;
    node->sn_Prev = newnode;
    newnode->sn_Prev->sn_Next = newnode;
}

void *
SListRemHead(list)
register LIST *list;
{
    register NODE *node = list->sl_Head;

    if (node->sn_Next) {
	SListRemove(node);
	return(SBASE(node));
    }
    return(NULL);
}

void *
SListRemTail(list)
register LIST *list;
{
    register NODE *node = list->sl_Tail;
    if (node->sn_Prev) {
	SListRemove(node);
	return(SBASE(node));
    }
    return(NULL);
}

