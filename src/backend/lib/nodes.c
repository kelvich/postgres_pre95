/*
 * nodes.c --
 *	Single inheritance hierachy of tagged node code.
 * $Header$
 */
#include "tmp/c.h"

RcsId("$Header$");

#include "utils/palloc.h"
#include "utils/log.h"

#include "nodes/nodes.h"
#include "nodes/relation.h"
#include "nodes/primnodes.h"

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
      case T_Param:
	node_size = sizeof (struct _Param);
	break;
      case T_Func:
	node_size = sizeof (struct _Func);
	break;
      case T_Iter:
	node_size = sizeof (struct _Iter);
	break;
      default:
	elog(NOTICE,"calling NodeTagGetSize with unknown tag");
	node_size = 48;
	break;
    }
    return(node_size);
}

