
/*     
 *      FILE
 *     	relnode
 *     
 *      DESCRIPTION
 *     	Relation manipulation routines
 *     
 *      EXPORTS
 *     		get_rel
 *     		rel-member
 */

#include "internal.h"
#include "relnode.h"
#include "relation.h"
#include "plancat.h"

extern void PrintRel();

/*    
 *    	get_rel
 *    
 *    	Returns relation entry corresponding to 'relid', creating a new one if
 *    	necessary.  
 *    
 */

/*  .. add-clause-to-rels, add-join-clause-info-to-rels, add-vars-to-rels
 *  .. find-all-join-paths, find-clause-joins, find-clauseless-joins
 *  .. initialize-targetlist
 */

Rel
get_rel (relid)
     LispValue relid ;
{
    Rel rel = rel_member (lispCons(relid,LispNil),
			  _query_relation_list_);

    if /*when */ ( null (rel)) {
	rel = CreateNode(Rel);
	rel->printFunc = PrintRel;
	set_relids (rel,lispCons (relid,LispNil));

	_query_relation_list_ = lispCons (rel,_query_relation_list_);

	if(listp (relid)) {
	    /*    If the relation is a materialized relation, assume 
		  constants for sizes. */
	    set_pages (rel,_TEMP_RELATION_PAGES_);
	    set_tuples (rel,_TEMP_RELATION_TUPLES_);

	} else {
	    /*    Otherwise, retrieve relation characteristics from the */
	    /*    system catalogs. */
	    
	    LispValue relinfo = relation_info((ObjectId)CInteger(relid));
	    set_indexed (rel, !zerop (nth (0,relinfo)));
	    set_pages (rel,nth (1,relinfo));
	    set_tuples (rel,nth (2,relinfo));
	} 
    }
    return(rel);
}

/*    
 *    	rel-member
 *    
 *    	Determines whether a relation of id 'relid' is contained within a list
 *    	'rels'.  
 *    
 *    	Returns the corresponding entry in 'rels' if it is there.
 *    
 */

/*  .. find-all-join-paths, get_rel
 */

Rel
rel_member (relid,rels)
     LispValue relid;
     List rels;
{
     Rel retval;
     if ( consp (relid) && consp (rels)) {
	  retval = (Rel)Contents(find (relid,rels,"get_relids","same"));
     } 
     else {
	  retval = (Rel)NULL;
     } 
     return(retval);
}
