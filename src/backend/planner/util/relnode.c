
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
extern bool EqualRel();

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
	/* XXX - can remove the following code when MakeRel() works */
	rel = CreateNode(Rel);
	rel->printFunc = PrintRel;
	rel->equalFunc = EqualRel;
	
	set_relids (rel,lispCons (relid,LispNil));
	set_indexed (rel,false);
	set_pages (rel, 0);
	set_tuples (rel,0);
	set_width (rel,0);
	set_targetlist (rel,LispNil);
	set_pathlist (rel,LispNil);
	set_unorderedpath (rel,(Path)NULL);
	set_cheapestpath (rel,(Path)NULL);
	set_classlist (rel,(List)NULL);
	set_ordering (rel,LispNil);
	set_clauseinfo (rel,LispNil);
	set_joininfo (rel,LispNil);
	set_innerjoin (rel,LispNil);

	_query_relation_list_ = lispCons (rel,_query_relation_list_);

	if(listp (relid)) {
	    /*    If the relation is a materialized relation, assume 
		  constants for sizes. */
	    set_pages (rel,_TEMP_RELATION_PAGES_);
	    set_tuples (rel,_TEMP_RELATION_TUPLES_);

	} else {
	    /*    Otherwise, retrieve relation characteristics from the */
	    /*    system catalogs. */

	  int reltuples = 0;
	  LispValue relinfo = relation_info((ObjectId)CInteger(relid));

	  reltuples = CInteger(CADDR(relinfo));
	  set_indexed (rel, !zerop (nth (0,relinfo)));
	  set_pages (rel,(Count)CInteger(CADR(relinfo)));
	  set_tuples (rel,(Count)reltuples);
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
     LispValue temp = LispNil;
     LispValue temprelid = LispNil;
     LispValue relid_list = LispNil;
     LispValue temp2 = LispNil;
     LispValue t_list = LispNil;

     foreach (temp2,rels) {
       t_list = nappend1(t_list,get_relids(CAR(temp2)));
     }
     
     if ( consp (relid) && consp (rels)) {
       foreach (temp,rels) {
	 temprelid = (LispValue)get_relids(CAR(temp)); /* XXX assumes it
							*  is a list with
							*  only 1 relid
							*/
	 if (equal(temprelid, relid))   /* XXX used to be "same" function  */
	   return((Rel)(CAR(temp)));
       }
     } 
     else {
       retval = (Rel)NULL;
     } 
     return(retval);
   }
