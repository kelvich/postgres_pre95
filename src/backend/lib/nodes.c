/*
 * nodes.c --
 *	Single inheritance hierachy of tagged node code.
 * $Header$
 */

#include "nodes.h"
#include "palloc.h"
#include "c.h"
#include "log.h"
#include "relation.h"
#include "primnodes.h"

void
SetNodeType(thisNode, tag)
	Node	thisNode;
	TypeId	tag;
{
	Assert(NodeIsValid(thisNode));
	Assert(TypeIdIsValid(tag));

	NodeType(thisNode) = tag;
}


Node
NewNode(size, tag)
	Size	size;
	TypeId	tag;
{
	Node	newNode;
	
	Assert(size > 0);

	newNode = (Node) palloc(size);
	bzero((char *)newNode, size);
	NodeSetTag(newNode, tag);
	return(newNode);
}

Size
NodeTagGetSize(tag)
     TypeId tag;
{
    Size node_size;
    switch(tag) {
      case T_Resdom:
	node_size = sizeof(struct _Resdom);
	break;
      case T_Var:
	node_size = sizeof(struct _Var);
	break;
      case T_Const:
	node_size = sizeof(struct _Const);
	break;
      case T_Oper:
	node_size = sizeof(struct _Oper);
	break;
      default:
	elog(NOTICE,"calling NodeTagGetSize with unknown tag");
	node_size = 48;
	break;
    }
    return(node_size);
}

