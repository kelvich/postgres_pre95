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

    if (equal(_query_result_relation_,get_relids (rel)) && 
	(_query_command_type_ != RETRIEVE )) {
	return (LispNil);
    } else if (get_indexed (rel)) {
	LispValue temp = CAR(get_relids(rel));
	if (IsA(temp,LispInt)) 
	  return(find_secondary_index (false,CInteger(temp)));
    } else {
	return (LispNil);
    }
}

/*    
 *    	find-secondary-index
 *    
 *    	Creates a list of index path nodes containing information for each
 *    	secondary index defined on a relation by searching through the index
 *    	catalog.
 *    
 *    	'notfirst' is 0 if this is the first call to find-secondary-index
 *    	'relid' is the OID of the relation for which indices are being located
 *    
 *    	Returns a list of new index nodes.
 *    
 */

/*  .. find-relation-indices, find-secondary-index    */

 LispValue
find_secondary_index (notfirst,relid)
     bool notfirst;
     ObjectId relid ;
{
    LispValue indexinfo = index_info (notfirst,relid);
    if  ( consp (indexinfo)) {
	Rel indexnode = CreateNode(Rel);
	set_relids (indexnode,lispCons(CAR(indexinfo),LispNil));
	set_pages (indexnode,CInteger(CADR (indexinfo)));
	set_tuples (indexnode,CInteger(nth (2,indexinfo)));
	set_indexkeys (indexnode,nth (3,indexinfo));
	set_ordering (indexnode,nth (4,indexinfo));
	set_classlist (indexnode,nth (5,indexinfo));
	
	set_indexed(indexnode,false);  /* XXX should it be true instead */
	set_size (indexnode,0);
	set_width(indexnode,0);
	set_targetlist(indexnode,LispNil);
	set_pathlist(indexnode,LispNil);
	set_unorderedpath(indexnode,(Path)NULL);
	set_cheapestpath(indexnode,(Path)NULL);
	set_clauseinfo(indexnode,LispNil);
	set_joininfo(indexnode,LispNil);
	set_innerjoin(indexnode,LispNil);
	
	indexnode->printFunc = PrintRel;
	indexnode->equalFunc = EqualRel;

	return(lispCons (indexnode,find_secondary_index (true,relid)));
    } else
      return(LispNil);
}
