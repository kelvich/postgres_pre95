
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
#include "internal.h"
#include "c.h"
#include "pg_lisp.h"


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

/*  .. exec-make-intermediate-locks, make-rule-locks
 */
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

/*  .. find-secondary-index
 */

List
index_info (not_first,relid)
     bool not_first;
     ObjectId relid ;
{
    int indexinfo[32];
    /*    Retrieve information for a single index (if there is another one). */
    if(1 == IndexCatalogInformation (not_first,
				     getrelid (relid,_query_range_table_),
				     _query_is_archival_,indexinfo)) {
	/*    First extract the index OID and the basic statistics (pages,
	      tuples, and indexrelid), then collect the index keys, operators,
	      and operator classes into lists.  Only include the 
	      actual keys in the list, i.e., no 0's.
	      */

	/*    Keys over which we're indexing: */

	for (i = _MAX_KEYS_+3 ; i > 2; --i ) 
	  ikey = lispCons(lispInteger(indexinfo[i]),ikey);

	/*    Operators used by the index (for ordering purposes): */

	for (i = _MAX_KEYS_+18 ; i > 10; --i ) 
	  iord = lispCons(lispInteger(indexinfo[i]),iord);

	/*    Classes of the AM operators used by index: */

	for (i = _MAX_KEYS_+25 ; i > 18; --i ) 
	  iord = lispCons(lispInteger(indexinfo[i]),iord);

	return(list (lispInteger(indexinfo[0]),
		     lispInteger(indexinfo[1]),
		     lispInteger(indexinfo[2]),
		     lispInteger(indexinfo[3]),ikey,iord,am_ops));
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
     ObjectId 	indid, classes,opnos,relid;
     int 32	attnos[];
     char 	*values[];
     int32	flags;
     int32	nkeys ;
{
    if(nkeys == length (classes) &&
       nkeys == length (opnos)  &&
       nkeys == length (attnos) &&
       nkeys == length (values) && 
       nkeys == length (flags)) {

	float param[2];

	IndexSelectivity (indid,relid,nkeys,
			  classes,opnos,attnos,values,flags,param);

	return (list(lispDouble(param[0]),
		     lispDouble(param[1]));
    } else 
      return(lispCons(lispDouble(0.0),
		      lispCons(lispDouble(1.0))));
}

/*    
 *    	restriction-selectivity
 *    
 *    	Returns the selectivity of an operator, given the restriction clause
 *    	information.
 *    
 */

/*  .. compute_selec
 */
LispValue
restriction_selectivity (oprrest,operator,relation,attribute_number,value,flags)
LispValue oprrest,operator,relation,attribute_number,value,flags ;
{
RestrictionClauseSelectivity (oprrest,operator,relation,attribute_number,value,flags);
}

/*    
 *    	join-selectivity
 *    
 *    	Returns the selectivity of an operator, given the join clause
 *    	information.
 *    
 */

/*  .. compute_selec
 */
LispValue
join_selectivity (oprjoin,operator,relation1,attribute_number1,relation2,attribute_number2)
LispValue oprjoin,operator,relation1,attribute_number1,relation2,attribute_number2 ;
{
JoinClauseSelectivity (oprjoin,operator,relation1,attribute_number1,relation2,attribute_number2);
}

/*    
 *    	find-inheritance-children
 *    
 *    	Returns all relations that directly inherit from the relation
 *    	represented by 'relation-oid'.
 *    
 */

/*  .. find-all-inheritors
 */
LispValue
find_inheritance_children (relation_oid)
LispValue relation_oid ;
{
car (InheritanceGetChildren (relation_oid,list (nil)));
}

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
car (VersionGetParents (relation_oid,list (nil)));
}

/*    
 *    	function-index-info		XXX FUNCS
 *    
 *    	Returns an integer iff 'function-oid' and 'index-oid' correspond
 *    	to a valid function-index mapping.
 *    
 */
#ifdef 
funcindex;

#endif
;

/*  .. function-index-clause-p
 */
LispValue
function_index_info (function_oid,index_oid)
LispValue function_oid,index_oid ;
{
/* XXX - let form, maybe incorrect */
LispValue info = new_vectori_long (4);
and (not (zerop (FunctionIndexInformation (index_oid,info))),vrefi_long (info,0) == function_oid);
;
}
#ifndef 
funcindex;

#endif
;

/*  .. function-index-clause-p
 */
LispValue
function_index_info (function_oid,index_oid)
LispValue function_oid,index_oid ;
{
nil;
}
