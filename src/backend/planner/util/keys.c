
/*     
 *      FILE
 *     	keys
 *     
 *      DECLARATION
 *     	Key manipulation routines
 *     
 *      EXPORTS
 *     		match-indexkey-operand
 *     		equal_indexkey_var
 *     		extract-subkey
 *     		samekeys
 *     		collect-index-pathkeys
 *     		match-sortkeys-pathkeys
 *     		valid-sortkeys
 *     		valid-numkeys
 *	$Header$
 */

#include "pg_lisp.h"
#include "planner/internal.h"
#include "nodes.h"
#include "relation.h"
#include "planner/keys.h"
#include "c.h"
#include "log.h"
#include "planner/tlist.h"



/*    
 *    	1. index key
 *    		one of:
 *    			attnum
 *    			(attnum arrayindex)
 *    	2. path key
 *    		(subkey1 ... subkeyN)
 *    			where subkeyI is a var node
 *    		note that the 'Keys field is a list of these
 *    	3. join key
 *    		(outer-subkey inner-subkey)
 *    			where each subkey is a var node
 *    	4. sort key
 *    		one of:
 *    			SortKey node
 *    			number
 *    			nil
 *    		(may also refer to the 'SortKey field of a SortKey node,
 *    		 which looks exactly like an index key)
 *    
 */

/*    
 *    	match-indexkey-operand
 *    
 *    	Returns t iff an index key 'index-key' matches the given clause 
 *    	operand.
 *    
 */

/*  .. in-line-lambda%598037345, match-index-orclause
 */

bool
match_indexkey_operand (indexkey,operand,rel)
     LispValue indexkey,operand,rel ;
{
     if (IsA (operand,Var) &&
	 equal (get_relid (rel),get_varno (operand)) &&
	 equal_indexkey_var (indexkey,operand))
       return(true);
     else
       return(false);
}

/*    
 *    	equal_indexkey_var
 *    
 *    	Returns t iff an index key 'index-key' matches the corresponding 
 *    	fields of var node 'var'.
 *    
 */

/*  .. in-line-lambda%598037345, in-line-lambda%598037462
 *  .. match-indexkey-operand
 */
bool
equal_indexkey_var (index_key,var)
LispValue index_key,var ;
{
     if ((consp (index_key) &&
	  equal (get_attribute_number (index_key), get_varattno (var)) &&
	  equal (get_array_index (index_key),get_vararrayindex (var))) ||
	 (equal (index_key,get_varattno (var)) &&
	  null (get_vararrayindex (var))))
       return(true);
     else
       return(false);
}

/*    
 *    	extract-subkey
 *    
 *    	Returns the subkey in a join key corresponding to the outer or inner
 *    	relation.
 *    
 */

/*  .. extract-path-keys, in-line-lambda%598037445, in-line-lambda%598037447
 */
LispValue
extract_subkey (joinkey,which_subkey)
     LispValue joinkey;
     int which_subkey ;
{
     LispValue retval;

     switch (which_subkey) {
	case OUTER: 
	  retval = nth (0,joinkey);
	  break;
	case INNER: 
	  retval = nth (1,joinkey);
	  break;
	default: /* do nothing */
	  elog(DEBUG,"extract_subkey with neither INNER or OUTER");
	  retval = LispNil;
     }
}

/*    
 *    	samekeys
 *    
 *    	Returns t iff two sets of path keys are equivalent.  They are 
 *    	equivalent if the first subkey (var node) within each sublist of 
 *    	list 'keys1' is contained within the corresponding sublist of 'keys2'.
 *    
 *    	XXX 	It isn't necessary to check that each sublist exactly contain 
 *    		the same elements because if the routine that built these
 *    		sublists together is correct, having one element in common 
 *    		implies having all elements in common.
 *    
 */

/*  .. in-line-lambda%598037501
 */
bool
samekeys (keys1,keys2)
     LispValue keys1,keys2 ;
{
     bool allmember = true;
     LispValue key1,key2;

     for(key1=keys1,key2=keys2 ; key1 != LispNil && key2 !=LispNil ; 
	 key1 = CDR(key1), key2=CDR(key2)) 
	  if ( ! member( CAR(key1),CAR(key2)))
	    allmember = false;

     if ( (length (keys2) >= length (keys1)) && allmember)
       return(true);
     else
       return(false);
}

/*    
 *    	collect-index-pathkeys
 *    
 *    	Creates a list of subkeys by retrieving var nodes corresponding to
 *    	each index key in 'index-keys' from the relation's target list
 *    	'tlist'.  If the key is not in the target list, the key is irrelevant
 *    	and is thrown away.  The returned subkey list is of the form:
 *    		((var1) (var2) ... (varn))
 *    
 *    	'index-keys' is a list of index keys
 *    	'tlist' is a relation target list
 *    
 *    	Returns the list of cons'd subkeys.
 *    
 */

/*  .. create_index_path
 */
LispValue
collect_index_pathkeys (index_keys,tlist)
     LispValue index_keys,tlist ;
{
     LispValue index_key,retval = LispNil;

     foreach(index_key,index_keys) {
	  Expr mvar;
	  mvar = matching_tlvar (CAR(index_key),tlist,equal_indexkey_var);
	  if ( mvar ) 
	    retval = nconc(retval,lispCons (lispCons (mvar,LispNil),
					    LispNil));
     }
     return(retval);
}

/*    
 *    	match-sortkeys-pathkeys
 *    
 *    	Returns t iff the sort required by a result as specified by
 *    	'relid' and 'sortkeys' matches the 'pathkeys' of a path.
 *    
 */

/*  .. sort-relation-paths
 */
#define every2(a,x,b,y) for(a=x,b=y;a!=LispNil&&b!=LispNil;a=CDR(a),b=CDR(b))

bool
match_sortkeys_pathkeys (relid,sortkeys,pathkeys)
     LispValue relid,sortkeys,pathkeys ;
{
     /* declare (special (relid)); */
     LispValue sortkey,pathkey;
     bool every_val = true;

     every2 (sortkey,sortkeys, pathkey,pathkeys) {
	  if ( ! equal_sortkey_pathkey (relid,CAR(sortkey),CAR(pathkey)))
	    every_val = false;
     }

     if ( every_val  && (length (pathkeys) >= length (sortkeys)))
       return(true);
     else
       return(false);
}

/*    
 *    	equal-sortkey-pathkey
 *    
 *    	Returns t iff a sortkey as specified by 'relid' and 'sortkey' matches
 *    	one of the subkeys (var nodes) within 'pathkey'.
 *    
 */

/*  .. in-line-lambda%598037461
 */

bool
equal_sortkey_pathkey (relid,sortkey,pathkey)
     LispValue relid,sortkey,pathkey ;
{
     /* declare (special (relid,sortkey)); */
     LispValue subkey;
     bool retval = false;

     foreach(subkey,pathkey) {
	  if (equal (relid,get_varno (CAR(subkey))) &&
	      equal_indexkey_var (sortkey,CAR(subkey)))
	    retval = true;
     }
     return(retval);
}

/*    
 *    	valid-sortkeys
 *    
 *    	Returns t if 'sortkeys' appears to be a valid SortKey node.
 *    
 */

/*  .. find-index-paths, query_planner, subplanner
 */

bool
valid_sortkeys (node)
     LispValue node ;
{
    if (node)
      return ((bool)NodeIsType(node, T_SortKey));
    else
      return(false);   /* if sortkeys is nil  */
}

/*    
 *    	valid-numkeys
 *    
 *    	Returns t if 'sortkeys' appears to be a valid numkeys value
 *    
 */

/*  .. query_planner
 */
bool
valid_numkeys (sortkeys)
     LispValue sortkeys ;
{
     if ( integerp (sortkeys) && CInteger(sortkeys) <= _MAX_KEYS_)
       return(true);
     else
       return(false);
}
