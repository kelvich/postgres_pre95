
/*     
 *      FILE
 *     	cfi
 *     
 *      DESCRIPTION
 *     	All planner routines that (directly) call C routines.
 *     
 *	$Header$
 *      EXPORTS
 *     		intermediate-rule-lock-data
 *     		intermediate-rule-lock
 *     		print-rule-lock-intermediate
 *     		rule-insert-catalog
 *     		relation-info
 *     		index-info
 *     		index-selectivity
 *     		restriction-selectivity
 *     		join-selectivity
 *     		find-inheritance-children
 *     		find-version-parents
 *     		function-index-info
 */

#include <stdio.h>
#include "pg_lisp.h"
#include "planner/internal.h"
#include "c.h"
#include "planner/plancat.h"


/*    
 *    	intermediate-rule-lock-data
 *    	intermediate-rule-lock
 *    	print-rule-lock-intermediate
 *    	rule-insert-catalog
 *    
 *    	Return pointers to malloc'd copies of the appropriate rule lock
 *    	structures.
 *    
 */

/*  .. exec-make-intermediate-locks, make-rule-locks     */

LispValue
intermediate_rule_lock_data (type,attribute,plan)
     LispValue type,attribute,plan ;
{
    MakeRuleLockIntermediateLock (type,attribute,plan);
}

/*  .. exec-make-intermediate-locks, make-rule-locks
 */
LispValue
intermediate_rule_lock (rule_id,priority,type,is_early,
			rule_lock_intermediate_data_list)
     LispValue rule_id,priority,type,is_early,
     rule_lock_intermediate_data_list ;
{
    MakeRuleLockIntermediate (rule_id,priority,type,is_early,
			      rule_lock_intermediate_data_list);
}

/*  .. print_rtentries
 */
LispValue
print_rule_lock_intermediate (rule_lock_intermediate)
     LispValue rule_lock_intermediate ;
{
     RuleLockIntermediateDump (rule_lock_intermediate);
}

/*  .. make-rule-locks
 */
LispValue
rule_insert_catalog ()
{
    RuleInsertCatalog ();
}

/*    
 *    	index-info
 *    
 *    	Retrieves catalog information on an index on a given relation.
 *    
 *    	The index relation is opened on the first invocation of
 *    	util/plancat.c:IndexCatalogInformation.  The current call retrieves
 *    	the next index relation within the catalog that has not already been
 *    	retrieved by a previous call.  The index catalog is closed by
 *    	IndexCatalogInformation when no more indices for 'relid' can be
 *    	found.
 *    
 *    	See util/plancat.c:IndexCatalogInformation for the format of 
 *    	'indexinfo'.
 *    
 *    	'not-first' is 0 if this is the first call 
 *    	'relid' is the OID of the relation for which indices are being located
 *    
 *    	Returns the list of index info.
 *    
 */

/*  .. find-secondary-index    */


List
index_info (not_first,relid)
     bool not_first;
     ObjectId  relid ;
{
    int indexinfo[32];
    int i = 0;
    LispValue ikey = LispNil;
    LispValue iord = LispNil;
    LispValue am_ops = LispNil;
    
    /*    Retrieve information for a single index (if there is another one). */
    if(1 == IndexCatalogInformation (not_first,
				     CInteger(getrelid(relid,
						       _query_range_table_)),
				     _query_is_archival_,indexinfo)) {
	/*    First extract the index OID and the basic statistics (pages,
	 *     tuples, and indexrelid), then collect the index keys, operators,
	 *     and operator classes into lists.  Only include the 
	 *     actual keys in the list, i.e., no 0's.
	 */
	
	/*    Keys over which we're indexing: */
	
	for (i = _MAX_KEYS_+2 ; i > 2; --i ) 
	  ikey = lispCons(lispInteger(indexinfo[i]),ikey);
	
	/*    Operators used by the index (for ordering purposes): */
	
	for (i = _MAX_KEYS_+10 ; i > 10; --i ) 
	  iord = lispCons(lispInteger(indexinfo[i]),iord);
	
	/*    Classes of the AM operators used by index: */
	
	for (i = _MAX_KEYS_+18 ; i > 18; --i ) 
	  am_ops = lispCons(lispInteger(indexinfo[i]),iord);
	
	return(lispCons (lispInteger(indexinfo[0]),
			 lispCons(lispInteger(indexinfo[1]),
				  lispCons(lispInteger(indexinfo[2]),
					   lispCons(ikey,
						    lispCons(iord,
							     lispCons
							     (am_ops,
							      LispNil)
							     ))))));
    }
}
	       
/*    
 *    	index-selectivity
 *    
 *    	Call util/plancat.c:IndexSelectivity with the indicated arguments.
 *    
 *    	'indid' is the index OID
 *    	'classes' is a list of index key classes
 *    	'opnos' is a list of index key operator OIDs
 *    	'relid' is the OID of the relation indexed
 *    	'attnos' is a list of the relation attnos which the index keys over
 *    	'values' is a list of the values of the clause's constants
 *    	'flags' is a list of fixnums which describe the constants
 *    	'nkeys' is the number of index keys
 *    
 *    	Returns a list of two floats: index pages and index selectivity.
 *    
 */

/*  .. best-or-subclause-index, create_index_path, index-innerjoin
 */
List
index_selectivity (indid,classes,opnos,relid,attnos,values,flags,nkeys)
     ObjectId 	indid,relid;
     List	attnos;
     List 	values;
     List	opnos;
     List	classes;
     List	flags;
     int32	nkeys ;
{
    int *class_array,*opno_array,*attno_array,*value_array,*flag_array;
    if(nkeys == length (classes) &&
       nkeys == length (opnos)  &&
       nkeys == length (attnos) &&
       nkeys == length (values) && 
       nkeys == length (flags)) {
	
	float param[2];
	int i = 0;
	LispValue xclass,xopno,xattno,value,flag;

	class_array = (int *)palloc(nkeys*sizeof(ObjectId));
	opno_array = (int *)palloc(nkeys*sizeof(ObjectId));
	attno_array = (int *)palloc(nkeys*sizeof(int32));
	value_array = (int *)palloc(nkeys*sizeof(char *));
	flag_array = (int *)palloc(nkeys*sizeof(int32));

	foreach(xclass,classes) {
	    class_array[i++] = CInteger(CAR(xclass));
	}
	i = 0;
	foreach(xopno,opnos) {
	    opno_array[i++] = CInteger(CAR(xopno));
	}
	  i = 0;
	foreach(xattno,attnos) {
	    attno_array[i++] = CInteger(CAR(xattno));
	}
	i = 0;
	foreach(value,values) {
	    value_array[i++] = CInteger(CAR(value));
	}
	i = 0;
	foreach(flag,flags) {
	    flag_array[i++] = CInteger(CAR(flag));
	}
	
	IndexSelectivity (indid,relid,nkeys,
			  class_array,
			  opno_array,
			  attno_array,
			  value_array,
			  flag_array,
			  param);
	
	return (lispCons(lispFloat(param[0]),
			 lispCons(lispFloat(param[1]),LispNil)));
    } else 
      return(lispCons(lispFloat(0.0),
		      lispCons(lispFloat(1.0),LispNil)));
}

/*    
 *    	restriction-selectivity  
 *    
 *    	Returns the selectivity of an operator, given the restriction clause
 *    	information.
 *      The routine is now merged with 
 *      RestrictionClauseSelectivity as defined in plancat.c
 *
 *      This routine is called by comput_selec
 */

/*    
 *    	join-selectivity
 *    
 *    	Returns the selectivity of an operator, given the join clause
 *    	information.
 *
 *      Similarly, this routine is merged with JoinClauseSelectivity in
 *      plancat.c
 *      Routin is used in compute_selec.
 */


/*    
 *    	find-version-parents
 *    
 *    	Returns all relations that are base relations for the version relation
 *    	represented by 'relation-oid'.
 *    
 */

/*  .. plan-union-queries
 */
LispValue
find_version_parents (relation_oid)
     LispValue relation_oid ;
{
    return( VersionGetParents (relation_oid,
				    lispList()));
}

/*    
 *    	function-index-info		XXX FUNCS
 *    
 *    	Returns an integer iff 'function-oid' and 'index-oid' correspond
 *    	to a valid function-index mapping.
 *    
 */
#ifdef funcindex
/*  .. function-index-clause-p     */

int32
function_index_info (function_oid,index_oid)
     int32 function_oid,index_oid ;
{
     int32      info[4];
     if (!zerop (FunctionIndexInformation (index_oid,info)) && 
	 info[0] == function_oid)
       return(function_oid);

}
#endif
#ifndef funcindex

/*  .. function-index-clause-p    */

int32
function_index_info (function_oid,index_oid)
     int32 function_oid,index_oid ;
{
     return((int32)LispNil);
}
#endif
 
