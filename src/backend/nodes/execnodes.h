/*
 * ExecNodes.h --
 *	Definitions for executor nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef ExecNodesIncluded
#define	ExecNodesIncluded

#include "nodes.h"	/* bogus inheritance system */

#include "item.h"
#include "sdir.h"
#include "htup.h"

class (EState) public (Node) {
	inherits(Node);
	ScanDirection	direction;
	abstime		time;
	ObjectId	owner;
	List		locks;
	List		subplan_info;
	char		*error_message;
	List		range_table;
	HeapTuple	qualification_tuple;
	ItemPointer	qualification_tuple_id;
	Relation	relation_relation_descriptor;
	ObjectId	result_relation;
	Relation	result_relation_descriptor;
};

#endif /* ExecNodesIncluded */
