/*
 * ExecNodes.h --
 *	Definitions for executor nodes.
 *
 * Identification:
 *	$Header$
 */

#ifndef ExecNodesIncluded
#define	ExecNodesIncluded

#include "pg_lisp.h"
#include "nodes.h"	/* bogus inheritance system */

/*
 *  This #define means that we have supplied a print function for EState
 *  nodes.  The routine that does the printing is in lib/C/printfuncs.c;
 *  it's called by an automatically-generated routine in execnodes.c.  This
 *  print support is intended for use in getting the C->lisp port done
 *  quickly, and should be replaced by something better designed when time
 *  permits.
 */

#define	PrintEStateExists

extern void	PrintEState();
extern bool	EqualEState();

#include "item.h"
#include "sdir.h"
#include "htup.h"
#include "tim.h"
#include "rel.h"

#define abstime AbsoluteTime

class (EState) public (Node) {
	inherits(Node);
	ScanDirection	direction;
	abstime		time;
	ObjectId	owner;
	List		locks;
	List		subplan_info;
	Name		error_message;
	List		range_table;
	HeapTuple	qualification_tuple;
	ItemPointer	qualification_tuple_id;
	Relation	relation_relation_descriptor;
	ObjectId	result_relation;
	Relation	result_relation_descriptor;
};

#endif /* ExecNodesIncluded */
