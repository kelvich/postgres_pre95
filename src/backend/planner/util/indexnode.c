/*     
 *      FILE
 *     	indexnode
 *     
 *      DESCRIPTION
 *     	Routines to find all indices on a relation
 * $Header$     
 */

/*
 *      EXPORTS
 *     		find-relation-indices
 */
/* declare (localf (find_secondary_index)); */

#include "planner/internal.h"
#include "parse.h"
#include "planner/indexnode.h"
#include "plannodes.h"
#include "relation.h"
#include "relation.a.h"
#include "planner/cfi.h"



/*    
 *    	find-relation-indices
 *    
 *    	Returns a list of index nodes containing appropriate information for
 *    	each (secondary) index defined on a relation.
 *    
 */

/*  .. find-rel-paths
 */

LispValue
find_relation_indices (rel)
     Rel rel ;
{
    /*    XXX Cheap temporary hack: 
	  if the relation is the result relation, */
    /* 	 don't use an index to update it! */

    if ((_query_result_relation_ == (LispValue)get_relids (rel)) && 
	(_query_command_type_ != RETRIEVE )) {
	return (LispNil);
    } else if (get_indexed (rel)) {
	Relid temp = (Relid) get_relids(rel);

	if (IsA(temp,LispInt))
	    find_secondary_index (0,CInteger(temp));
    } else {
	return (LispNil);
    }
}

/*    
 *    	find-secondary-index
 *    
 *    	Creates a list of index nodes containing information for each
 *    	secondary index defined on a relation by searching through the index
 *    	catalog.
 *    
 *    	'notfirst' is 0 if this is the first call to find-secondary-index
 *    	'relid' is the OID of the relation for which indices are being located
 *    
 *    	Returns a list of new index nodes.
 *    
 */

/*  .. find-relation-indices, find-secondary-index
 */
LispValue
find_secondary_index (notfirst,relid)
     LispValue notfirst;
     int relid ;
{
    LispValue indexinfo = index_info (notfirst,relid);
    if /*when */ ( consp (indexinfo)) {
	IndexScan indexnode = CreateNode(IndexScan);
	set_indexid (indexnode,lispCons(CAR(indexinfo),LispNil));
	set_pages (indexnode,nth (1,indexinfo));
	set_tuples (indexnode,nth (2,indexinfo));
	set_indexkeys (indexnode,nth (3,indexinfo));
	set_ordering (indexnode,nth (4,indexinfo));
	set_classlist (indexnode,nth (5,indexinfo));
	return(lispCons (indexnode,find_secondary_index (1,relid)));
    } else
      return(LispNil);
}
