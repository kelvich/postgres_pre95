
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

#define NODE	SLNode
#define LIST	SLList
#define SBASE(list,node) ((Pointer)((char *)(node) - (list)->sl_Offset))

void
SLNewList(list, offset)
LIST *list;
Offset offset;
{
    list->sl_Head = (NODE *)&list->sl_Term;
    list->sl_Term = NULL;
    list->sl_Tail = (NODE *)&list->sl_Head;
    list->sl_Offset = offset;
    list->sl_Magic= LIST_MAGIC;
}

void
SLNewNode(node)
NODE *node;
{
    node->sn_List = NULL;
    node->sn_Magic= NODE_MAGIC;
}

Pointer
SLGetHead(list)
register LIST *list;
{
    register NODE *node;

    Assert(list->sl_Magic == LIST_MAGIC);
    node = list->sl_Head;
    if (node->sn_Next) {
	Assert(node->sn_List == list);
	Assert(node->sn_Magic == NODE_MAGIC);
	return(SBASE(list,node));
    }
    return(NULL);
}

Pointer
SLGetTail(list)
register LIST *list;
{
    register NODE *node;

    Assert(list->sl_Magic == LIST_MAGIC);
    node = list->sl_Tail;
    if (node->sn_Prev) {
	Assert(node->sn_List == list);
	Assert(node->sn_Magic == NODE_MAGIC);
	return(SBASE(list,node));
    }
    return(NULL);
}

Pointer
SLGetPred(node)
register NODE *node;
{
    Assert(node->sn_Magic == NODE_MAGIC);
    node = node->sn_Prev;
    if (node->sn_Prev) {
	Assert(node->sn_Magic == NODE_MAGIC);
	return(SBASE(node->sn_List,node));
    }
    return(NULL);
}

Pointer
SLGetSucc(node)
register NODE *node;
{
    Assert(node->sn_Magic == NODE_MAGIC);
    node = node->sn_Next;
    if (node->sn_Next)  {
	Assert(node->sn_Magic == NODE_MAGIC);
	return(SBASE(node->sn_List,node));
    }
    return(NULL);
}

void
SLRemove(node)
register NODE *node;
{
    Assert(node->sn_Magic == NODE_MAGIC);
    Assert(node->sn_List  != NULL);
    node->sn_Next->sn_Prev = node->sn_Prev;
    node->sn_Prev->sn_Next = node->sn_Next;
    node->sn_List = NULL;
}

void
SLAddHead(list, node)
register LIST *list;
register NODE *node;
{
    Assert(node->sn_List  == NULL);
    Assert(node->sn_Magic == NODE_MAGIC);
    Assert(list->sl_Magic == LIST_MAGIC);
    node->sn_Next = list->sl_Head;
    node->sn_Prev = (NODE *)&list->sl_Head;
    node->sn_Next->sn_Prev = node;
    node->sn_Prev->sn_Next = node;
    node->sn_List = list;
}

void
SLAddTail(list, node)
register LIST *list;
register NODE *node;
{
    Assert(node->sn_List  == NULL);
    Assert(node->sn_Magic == NODE_MAGIC);
    Assert(list->sl_Magic == LIST_MAGIC);

    node->sn_Next = (NODE *)&list->sl_Term;
    node->sn_Prev = list->sl_Tail;
    node->sn_Next->sn_Prev = node;
    node->sn_Prev->sn_Next = node;
    node->sn_List = list;
}

SLList *
SetGetList(node)
register NODE *node;
{
    Assert(node->sn_Magic == NODE_MAGIC);
    return(node->sn_List);
}

void
SLInsertAfter(node, newnode)
register NODE *node, *newnode;
{
    Assert(node->sn_List    != NULL);
    Assert(newnode->sn_List == NULL);
    Assert(node->sn_Magic   == NODE_MAGIC);
    Assert(newnode->sn_Magic== NODE_MAGIC);

    newnode->sn_Next = node->sn_Next;
    newnode->sn_Prev = node;
    node->sn_Next = newnode;
    newnode->sn_Next->sn_Prev = newnode;
    newnode->sn_List = node->sn_List;
}

void
SLInsertBefore(node, newnode)
register NODE *node, *newnode;
{
    Assert(node->sn_List    != NULL);
    Assert(newnode->sn_List == NULL);
    Assert(node->sn_Magic   == NODE_MAGIC);
    Assert(newnode->sn_Magic== NODE_MAGIC);

    newnode->sn_Next = node;
    newnode->sn_Prev = node->sn_Prev;
    node->sn_Prev = newnode;
    newnode->sn_Prev->sn_Next = newnode;
    newnode->sn_List = node->sn_List;
}

Pointer
SLRemHead(list)
register LIST *list;
{
    register NODE *node;

    Assert(list->sl_Magic == LIST_MAGIC);
    node = list->sl_Head;
    if (node->sn_Next) {
	Assert(node->sn_Magic == NODE_MAGIC);
	Assert(node->sn_List == list);
	SLRemove(node);
	return(SBASE(list,node));
    }
    return(NULL);
}

Pointer
SLRemTail(list)
register LIST *list;
{
    register NODE *node;

    Assert(list->sl_Magic == LIST_MAGIC);
    node = list->sl_Tail;
    if (node->sn_Prev) {
	Assert(node->sn_Magic == NODE_MAGIC);
	Assert(node->sn_List == list);
	SLRemove(node);
	return(SBASE(list,node));
    }
    return(NULL);
}

