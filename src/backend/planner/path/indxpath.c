
/*     
 *      FILE
 *     	indxpath
 *     
 *      DESCRIPTION
 *     	Routines to determine which indices are usable for 
 *     	scanning a given relation
 *     
 */
 /* RcsId("$Header$"); */

/*     
 *      EXPORTS
 *     		find-index-paths
 */
#include <math.h>

#include "tmp/postgres.h"
#include "access/att.h"
#include "access/heapam.h"
#include "access/ftup.h"
#include "access/nbtree.h"

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.a.h"

#include "utils/lsyscache.h"
#include "utils/log.h"

#include "planner/internal.h"
#include "planner/indxpath.h"
#include "planner/clauses.h"
#include "planner/clauseinfo.h"
#include "planner/cfi.h"
#include "planner/costsize.h"
#include "planner/pathnode.h"
#include "planner/xfunc.h"

#include "catalog/catname.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_proc.h"

#include "executor/x_qual.h"

/* If Spyros can use a constant PRS2_BOOL_TYPEID, I can use this */
#define BOOL_TYPEID ((ObjectId) 16)

/*    
 *    	find-index-paths
 *    
 *    	Finds all possible index paths by determining which indices in the
 *    	list 'indices' are usable.
 *    
 *    	To be usable, an index must match against either a set of 
 *    	restriction clauses or join clauses.
 *    
 *    	Note that the current implementation requires that there exist
 *    	matching clauses for every key in the index (i.e., no partial
 *    	matches are allowed).
 *    
 *    	If an index can't be used with restriction clauses, but its keys
 *    	match those of the result sort order (according to information stored
 *    	within 'sortkeys'), then the index is also considered. 
 *    
 *    	'rel' is the relation entry to which these index paths correspond
 *    	'indices' is a list of possible index paths
 *    	'clauseinfo-list' is a list of restriction clauseinfo nodes for 'rel'
 *    	'joininfo-list' is a list of joininfo nodes for 'rel'
 *    	'sortkeys' is a node describing the result sort order (from
 *    		(find_sortkeys))
 *    
 *    	Returns a list of index nodes.
 *    
 */

/*  .. find_index_paths, find_rel_paths */

LispValue
find_index_paths (rel,indices,clauseinfo_list,joininfo_list,sortkeys)
     Rel rel;
     LispValue indices,clauseinfo_list,joininfo_list,sortkeys ;

{
    LispValue scanclausegroups = LispNil;
    LispValue scanpaths = LispNil;
    Rel       index = (Rel)NULL;
    LispValue joinclausegroups = LispNil;
    LispValue joinpaths = LispNil;
    LispValue sortpath = LispNil;
    LispValue retval = LispNil;
    LispValue temp = LispNil;
    LispValue add_index_paths();
    
    if(!consp(indices))
	return(NULL);
	
    index = (Rel)CAR (indices);

    retval = find_index_paths (rel, CDR (indices),
				    clauseinfo_list,
				    joininfo_list,sortkeys);

    /* If this is a partial index, return if it fails the predicate test */
    if (index->indpred != LispNil)
	if (!pred_test(index->indpred, clauseinfo_list, joininfo_list))
	    return retval;

    /*  1. If this index has only one key, try matching it against 
     * subclauses of an 'or' clause.  The fields of the clauseinfo
     * nodes are marked with lists of the matching indices no path
     * are actually created. 
     *
     * XXX NOTE:  Currently btrees dos not support indices with
     * > 1 key, so the following test will always be true for
     * now but we have decided not to support index-scans 
     * on disjunction . -- lp
     */

    if (SingleAttributeIndex(index)) 
    {
	match_index_orclauses (rel,
			       index,
			       CAR(get_indexkeys(index)),
			       CAR(get_classlist(index)),
			       clauseinfo_list);
    }

    /* 2. If the keys of this index match any of the available
     * restriction clauses, then create pathnodes corresponding
     * to each group of usable clauses.
     */

    scanclausegroups = group_clauses_by_indexkey (rel,
						  index,
						  get_indexkeys(index),
						  get_classlist(index),
						  clauseinfo_list,
						  false);

    scanpaths = LispNil;
    if (consp (scanclausegroups))
	scanpaths = create_index_paths (rel,
					index,
					scanclausegroups,
					false);
	     
    /* 3. If this index can be used with any join clause, then 
     * create pathnodes for each group of usable clauses.  An 
     * index can be used with a join clause if its ordering is 
     * useful for a mergejoin, or if the index can possibly be 
     * used for scanning the inner relation of a nestloop join. 
     */
	     
    joinclausegroups = indexable_joinclauses(rel,index,joininfo_list);
    joinpaths = LispNil;

    if (consp (joinclausegroups))
    {
	LispValue new_join_paths = create_index_paths(rel,
						      index,
						      joinclausegroups, 
						      true);
	LispValue innerjoin_paths = index_innerjoin(rel,joinclausegroups,index);

	set_innerjoin (rel,nconc (get_innerjoin(rel), innerjoin_paths));
	joinpaths = new_join_paths;
    }
	     
    /* 4. If this particular index hasn't already been used above,
     *   then check to see if it can be used for ordering a  
     *   user-specified sort.  If it can, add a pathnode for the 
     *   sorting scan. 
     */
	     
    sortpath = LispNil;
    if ( valid_sortkeys(sortkeys)
	 && null(scanclausegroups)
	 && null(joinclausegroups)
	 && (CInteger(get_relid((SortKey)sortkeys)) == 
	     CInteger(CAR(get_relids(rel))))
	 && equal_path_path_ordering(get_sortorder((SortKey)sortkeys),
				     get_ordering(index))
	 && equal ((Node)get_sortkeys((SortKey)sortkeys),
		   (Node)get_indexkeys (index)) )
    {
	sortpath = lispCons((LispValue)create_index_path(rel,
					      index,
					      LispNil,
					      false),
			    LispNil);
    }
	
    /*
     *  Some sanity checks to make sure that
     *  the indexpath is valid.
     */
    if (!null(sortpath))
	retval = add_index_paths(sortpath,retval);
    if ( !null(joinpaths) )
	retval = add_index_paths(joinpaths,retval);
    if (!null(scanpaths))
	retval = add_index_paths(scanpaths,retval);
	
    return retval;

}  /* function end */


/*    		----  ROUTINES TO MATCH 'OR' CLAUSES  ----   */


/*    
 *    	match-index-orclauses
 *    
 *    	Attempt to match an index against subclauses within 'or' clauses.
 *    	If the index does match, then the clause is marked with information
 *    	about the index.
 *    
 *    	Essentially, this adds 'index' to the list of indices in the
 *    	ClauseInfo field of each of the clauses which it matches.
 *    
 *    	'rel' is the node of the relation on which the index is defined.
 *    	'index' is the index node.
 *    	'indexkey' is the (single) key of the index
 *    	'class' is the class of the operator corresponding to 'indexkey'.
 *    	'clauseinfo-list' is the list of available restriction clauses.
 *    
 *    	Returns nothing.
 *    
 */

/*  .. find-index-paths   */

void
match_index_orclauses (rel,index,indexkey,xclass,clauseinfo_list)
Rel rel,index;
LispValue indexkey,xclass,clauseinfo_list ;
{
    CInfo clauseinfo = (CInfo)NULL;
    LispValue i = LispNil;

    foreach (i, clauseinfo_list) {
	clauseinfo = (CInfo)CAR(i);
	if ( valid_or_clause((LispValue)clauseinfo)) {
	    
	    /* Mark the 'or' clause with a list of indices which 
	     * match each of its subclauses.  The list is
	     * generated by adding 'index' to the existing
	     * list where appropriate.
	     */
	    set_indexids (clauseinfo,
			  match_index_orclause (rel,index,indexkey,
						xclass,
						get_orclauseargs
						((List)get_clause(clauseinfo)),
						get_indexids (clauseinfo)));
	}
   }
}  /* function end */

/*
 *	match_index_operand()
 *
 *	Generalize test for a match between an existing index's key
 *	and the operand on the rhs of a restriction clause.  Now check
 *	for functional indices as well.
 */
bool
match_index_to_operand(indexkey, operand, rel, index)
    LispValue indexkey;
    LispValue operand;
    Rel rel;
    Rel index;
{
    /*
     * Normal index.
     */
    if (index->indproc == InvalidObjectId)
	return match_indexkey_operand(indexkey, operand, rel);

    /*
     * functional index check
     */
    return (function_index_operand(operand, rel, index));
}
/*    
 *    	match-index-orclause
 *    
 *    	Attempts to match an index against the subclauses of an 'or' clause.
 *    
 *    	A match means that:
 *    	(1) the operator within the subclause can be used with one
 *         	of the index's operator classes, and
 *    	(2) there is a usable key that matches the variable within a
 *         	sargable clause.
 *    
 *    	'or-clauses' are the remaining subclauses within the 'or' clause
 *    	'other-matching-indices' is the list of information on other indices
 *    		that have already been matched to subclauses within this
 *    		particular 'or' clause (i.e., a list previously generated by
 *    		this routine)
 *    
 *    	Returns a list of the form ((a b c) (d e f) nil (g h) ...) where
 *    	a,b,c are nodes of indices that match the first subclause in
 *    	'or-clauses', d,e,f match the second subclause, no indices
 *    	match the third, g,h match the fourth, etc.
 */

/*  .. match-index-orclauses  	 */

LispValue
match_index_orclause (rel,index,indexkey,xclass,
		      or_clauses,other_matching_indices)
     Rel rel,index;
     LispValue indexkey,xclass,or_clauses,other_matching_indices ;
{
    /* XXX - lisp mapCAR  -- may be wrong. */
    LispValue clause = LispNil;
    LispValue matched_indices  = other_matching_indices;
    LispValue index_list = LispNil;
    LispValue clist;
    LispValue ind;

    if (!matched_indices)
	matched_indices = lispCons(LispNil, LispNil);

    for (clist = or_clauses, ind = matched_indices; 
	 clist; 
	 clist = CDR(clist), ind = CDR(ind))
    {
	clause = CAR(clist);
	if (is_clause (clause) && 
	    op_class(get_opno((Oper)get_op(clause)),CInteger(xclass)) && 
	    match_index_to_operand (indexkey,
				    get_leftop(clause),
				    rel,
				    index) &&
	    IsA(get_rightop (clause),Const)) {
	  
	  matched_indices =  lispCons((LispValue)index, matched_indices);
	  index_list = nappend1(index_list,
				matched_indices);
	}
    }
    return(index_list);
     
}  /* function end */

/*    		----  ROUTINES TO CHECK RESTRICTIONS  ---- 	 */


/*
 * DoneMatchingIndexKeys() - MACRO
 *
 * Determine whether we should continue matching index keys in a clause.
 * Depends on if there are more to match or if this is a functional index.
 * In the latter case we stop after the first match since the there can
 * be only key (i.e. the function's return value) and the attributes in
 * keys list represent the arguments to the function.  -mer 3 Oct. 1991
 */
#define DoneMatchingIndexKeys(indexkeys, index) \
	((CDR (indexkeys) == LispNil) || \
	 (CInteger(CADR(indexkeys)) == 0) || \
	 (get_indproc(index) != InvalidObjectId))

/*    
 *    	group-clauses-by-indexkey
 *    
 *    	Determines whether there are clauses which will match each and every
 *    	one of the remaining keys of an index.  
 *    
 *    	'rel' is the node of the relation corresponding to the index.
 *    	'indexkeys' are the remaining index keys to be matched.
 *    	'classes' are the classes of the index operators on those keys.
 *    	'clauses' is either:
 *    		(1) the list of available restriction clauses on a single
 *    			relation, or
 *    		(2) a list of join clauses between 'rel' and a fixed set of
 *    			relations,
 *    		depending on the value of 'join'.
 *    	'startlist' is a list of those clause nodes that have matched the keys 
 *    		that have already been checked.
 *    	'join' is a flag indicating that the clauses being checked are join
 *    		clauses.
 *    
 *    	Returns all possible groups of clauses that will match (given that
 *    	one or more clauses can match any of the remaining keys).
 *    	E.g., if you have clauses A, B, and C, ((A B) (A C)) might be 
 *    	returned for an index with 2 keys.
 *    
 */

/*  .. find-index-paths, group-clauses-by-indexkey, indexable-joinclauses  */

LispValue
group_clauses_by_indexkey (rel,index,indexkeys,classes,clauseinfo_list,join)
    Rel rel,index;
    LispValue indexkeys,classes,clauseinfo_list;
    bool join;
{
    LispValue curCinfo		= LispNil;
    CInfo matched_clause	= (CInfo)NULL;
    LispValue retlist		= LispNil;
    LispValue clausegroup	= LispNil;


    if ( !consp (clauseinfo_list) )
	return LispNil;

    foreach (curCinfo,clauseinfo_list) {
	CInfo temp		= (CInfo)CAR(curCinfo);
	LispValue curIndxKey	= indexkeys;
	LispValue curClass	= classes;

	do {
	    /*
	     * If we can't find any matching clauses for the first of 
	     * the remaining keys, give up.
	     */
	    matched_clause = match_clause_to_indexkey (rel, 
						       index, 
						       CAR(curIndxKey),
						       CAR(curClass),
						       temp,
						       join);
	    if ( !matched_clause )
		break;

	    clausegroup = cons ((LispValue)matched_clause,clausegroup);
	    curIndxKey = CDR(curIndxKey);
	    curClass = CDR(curClass);

	} while ( !DoneMatchingIndexKeys(indexkeys, index) );
    }

    if (consp(clausegroup))
	return(lispCons(clausegroup,LispNil));
    return LispNil;
}

/*
 * IndexScanableClause ()  MACRO
 *
 * Generalize condition on which we match a clause with an index.
 * Now we can match with functional indices.
 */
#define IndexScanableOperand(opnd, indkeys, rel, index) \
    ((index->indproc == InvalidObjectId) ? \
	equal_indexkey_var(indkeys,opnd) : \
	function_index_operand(opnd,rel,index))

/*    
 *    	match_clause_to-indexkey
 *    
 *    	Finds the first of a relation's available restriction clauses that
 *    	matches a key of an index.
 *    
 *    	To match, the clause must:
 *    	(1) be in the form (op var const) if the clause is a single-
 *    		relation clause, and
 *    	(2) contain an operator which is in the same class as the index
 *    		operator for this key.
 *    
 *    	If the clause being matched is a join clause, then 'join' is t.
 *    
 *    	Returns a single clauseinfo node corresponding to the matching 
 *    	clause.
 *
 *      NOTE:  returns nil if clause is an or_clause.
 *    
 */

/*  .. group-clauses-by-indexkey  */
 
CInfo
match_clause_to_indexkey (rel,index,indexkey,xclass,clauseInfo,join)
     Rel rel,index;
     LispValue indexkey,xclass;
     CInfo clauseInfo;
     bool join;
{
    LispValue clause	= get_clause (clauseInfo);
    Var leftop	= get_leftop (clause);
    Var rightop	= get_rightop (clause);
    ObjectId join_op = InvalidObjectId;
    bool isIndexable = false;

    if ( or_clause (clause))
	return ((CInfo)NULL);

     /*
      * If this is not a join clause, check for clauses of the form:
      * (operator var/func constant) and (operator constant var/func)
      */
    if(!join) 
    {
	ObjectId restrict_op = InvalidObjectId;

	/*
	 * Check for standard s-argable clause
	 */
	if (IsA(rightop,Const))
	{
	    restrict_op = get_opno((Oper)get_op(clause));
	    isIndexable =
		( op_class(restrict_op, CInteger(xclass)) &&
		  IndexScanableOperand((LispValue)leftop,indexkey,rel,index) );
	}

	/*
	 * Must try to commute the clause to standard s-arg format.
	 */
	else if (IsA(leftop,Const))
	{
	    restrict_op = get_commutator(get_opno((Oper)get_op(clause)));

	    if ( (restrict_op != InvalidObjectId) &&
		  op_class(restrict_op, CInteger(xclass)) &&
		  IndexScanableOperand((LispValue)rightop,indexkey,rel,index) )
	    {
		isIndexable = true;
		/*
		 * In place list modification.
		 * (op const var/func) -> (op var/func const)
		 */
		/* BUG!  Old version:
		   CommuteClause(clause, restrict_op);
		*/
		CommuteClause(clause);
	    }
	}
    } 

    /*
     * Check for an indexable scan on one of the join relations.
     * clause is of the form (operator var/func var/func)
     */
    else
    {
	if (match_index_to_operand (indexkey,rightop,rel,index))
	    join_op = get_commutator(get_opno((Oper)get_op(clause)));

	else if (match_index_to_operand (indexkey,leftop,rel,index))
	    join_op = get_opno((Oper)get_op(clause));

	if ( join_op && op_class(join_op,CInteger(xclass)) &&
	     join_clause_p(clause))
	{
	    isIndexable = true;

	    /*
	     * If we're using the operand's commutator we must
	     * commute the clause.
	     */
	    if ( join_op != get_opno((Oper)get_op(clause)) )
		CommuteClause(clause);
	}
    }

    if (isIndexable)
	return(clauseInfo);

    return(NULL);
}

/*    		----  ROUTINES TO DO PARTIAL INDEX PREDICATE TESTS  ----  */

/*    
 *    	pred_test
 *    
 *	Does the "predicate inclusion test" for partial indexes.
 *
 *	Recursively checks whether the clauses in clauseinfo_list imply
 *	that the given predicate is true.
 *
 *	This routine (together with the routines it calls) iterates over
 *	ANDs in the predicate first, then reduces the qualification
 *	clauses down to their constituent terms, and iterates over ORs
 *	in the predicate last.  This order is important to make the test
 *	succeed whenever possible (assuming the predicate has been
 *	successfully cnfify()-ed). --Nels, Jan '93
 */

bool
pred_test (predicate_list, clauseinfo_list, joininfo_list)
    List predicate_list, clauseinfo_list, joininfo_list;
{
    List pred, items, item;

    /*
     * Note: if Postgres tried to optimize queries by forming equivalence
     * classes over equi-joined attributes (i.e., if it recognized that a
     * qualification such as "where a.b=c.d and a.b=5" could make use of
     * an index on c.d), then we could use that equivalence class info
     * here with joininfo_list to do more complete tests for the usability
     * of a partial index.  For now, the test only uses restriction
     * clauses (those in clauseinfo_list). --Nels, Dec '92
     */

    if (predicate_list == NULL)
	return true;	/* no predicate: the index is usable */
    if (clauseinfo_list == NULL)
	return false;	/* no restriction clauses: the test must fail */

    foreach (pred, predicate_list) {
	/* if any clause is not implied, the whole predicate is not implied */
	if (and_clause(CAR(pred))) {
	    items = (List) get_andclauseargs(CAR(pred));
	    foreach (item, items) {
		if (!one_pred_test(CAR(item), clauseinfo_list))
		    return false;
	    }
	}
	else if (!one_pred_test(CAR(pred), clauseinfo_list))
	    return false;
    }
    return true;
}


/*    
 *    	one_pred_test
 *    
 *	Does the "predicate inclusion test" for one conjunct of a predicate
 *	expression.
 */

bool
one_pred_test (predicate, clauseinfo_list)
    List predicate, clauseinfo_list;
{
    CInfo clauseinfo;
    List item;

    Assert(predicate != LispNil);
    foreach (item, clauseinfo_list) {
	clauseinfo = (CInfo)CAR(item);
	/* if any clause implies the predicate, return true */
	if (one_pred_clause_expr_test(predicate, get_clause(clauseinfo)))
	    return true;
    }
    return false;
}


/*    
 *    	one_pred_clause_expr_test
 *    
 *	Does the "predicate inclusion test" for a general restriction-clause
 *	expression.
 */

bool
one_pred_clause_expr_test (predicate, clause)
    List predicate, clause;
{
    List items, item;

    if (fast_is_clause(clause))        /* CAR is Oper node */
	return one_pred_clause_test(predicate, clause);
    else if (fast_or_clause(clause)) {
	items = (List) get_orclauseargs(clause);
	foreach (item, items) {
	    /* if any OR item doesn't imply the predicate, clause doesn't */
	    if (!one_pred_clause_expr_test(predicate, CAR(item)))
		return false;
	}
	return true;
    }
    else if (fast_and_clause(clause)) {
	items = (List) get_andclauseargs(clause);
	foreach (item, items) {
	    /* if any AND item implies the predicate, the whole clause does */
	    if (one_pred_clause_expr_test(predicate, CAR(item)))
		return true;
	}
	return false;
    }
    else /* unknown clause type never implies the predicate */ 
	return false;
}


/*    
 *    	one_pred_clause_test
 *    
 *	Does the "predicate inclusion test" for one conjunct of a predicate
 *	expression for a simple restriction clause.
 */

bool
one_pred_clause_test (predicate, clause)
    List predicate, clause;
{
    List items, item;

    if (fast_is_clause(predicate))        /* CAR is Oper node */
	return clause_pred_clause_test(predicate, clause);
    else if (fast_or_clause(predicate)) {
	items = (List) get_orclauseargs(predicate);
	foreach (item, items) {
	    /* if any item is implied, the whole predicate is implied */
	    if (one_pred_clause_test(CAR(item), clause))
		return true;
	}
	return false;
    }
    else if (fast_and_clause(predicate)) {
	items = (List) get_andclauseargs(predicate);
	foreach (item, items) {
	    /* if any item is not implied, the whole predicate is not implied */
	    if (!one_pred_clause_test(CAR(item), clause))
		return false;
	}
	return true;
    }
    else {
	elog(DEBUG, "Unsupported predicate type, index will not be used");
	return false;
    }
}


/*
 * Define an "operator implication table" for btree operators ("strategies").
 * The "strategy numbers" are:  (1) <   (2) <=   (3) =   (4) >=   (5) >
 *
 * The interpretation of:
 *
 *	test_op = BT_implic_table[given_op-1][target_op-1]
 *
 * where test_op, given_op and target_op are strategy numbers (from 1 to 5)
 * of btree operators, is as follows:
 *
 *   If you know, for some ATTR, that "ATTR given_op CONST1" is true, and you
 *   want to determine whether "ATTR target_op CONST2" must also be true, then
 *   you can use "CONST1 test_op CONST2" as a test.  If this test returns true,
 *   then the target expression must be true; if the test returns false, then
 *   the target expression may be false.
 *
 * An entry where test_op==0 means the implication cannot be determined, i.e.,
 * this test should always be considered false.
 */

StrategyNumber BT_implic_table[BTMaxStrategyNumber][BTMaxStrategyNumber] = {
    {2, 2, 0, 0, 0},
    {1, 2, 0, 0, 0},
    {1, 2, 3, 4, 5},
    {0, 0, 0, 4, 5},
    {0, 0, 0, 4, 4}
};


/*    
 *    	clause_pred_clause_test
 *
 *	Use operator class info to check whether clause implies predicate.
 *    
 *	Does the "predicate inclusion test" for a "simple clause" predicate
 *	for a single "simple clause" restriction.  Currently, this only handles
 *	(binary boolean) operators that are in some btree operator class.
 *	Eventually, rtree operators could also be handled by defining an
 *	appropriate "RT_implic_table" array.
 */

bool
clause_pred_clause_test (predicate, clause)
    List predicate, clause;
{
    Var			pred_var, clause_var;
    Const		pred_const, clause_const;
    ObjectId		pred_op, clause_op, test_op;
    ObjectId		opclass_id;
    StrategyNumber	pred_strategy, clause_strategy, test_strategy;
    Oper		test_oper;
    List		test_expr;
    bool		test_result, isNull;
    Relation		relation;
    HeapScanDesc	scan;
    HeapTuple		tuple;
    ScanKeyEntryData	entry[3];
    AccessMethodOperatorTupleForm form;

    pred_var = (Var)get_leftop(predicate);
    pred_const = (Const)get_rightop(predicate);
    clause_var = (Var)get_leftop(clause);
    clause_const = (Const)get_rightop(clause);

    /* Check the basic form; for now, only allow the simplest case */
    if (!IsA(CAR(clause),Oper) ||
	!IsA(clause_var,Var) ||
	!IsA(clause_const,Const) ||
	!IsA(CAR(predicate),Oper) ||
	!IsA(pred_var,Var) ||
	!IsA(pred_const,Const)) {
	return false;
    }

    /*
     * The implication can't be determined unless the predicate and the clause
     * refer to the same attribute.
     */
    if (get_varattno(clause_var) != get_varattno(pred_var))
	return false;

    /* Get the operators for the two clauses we're comparing */
    pred_op = get_opno((Oper)get_op(predicate));
    clause_op = get_opno((Oper)get_op(clause));


    /*** 1. Find a "btree" strategy number for the pred_op ***/

    /* XXX - hardcoded amopid value 403 to find "btree" operator classes */
    ScanKeyEntryInitialize(&entry[0], 0,
			   Anum_pg_amop_amopid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(403));

    ScanKeyEntryInitialize(&entry[1], 0,
			   Anum_pg_amop_amopopr,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(pred_op));

    relation = heap_openr(AccessMethodOperatorRelationName);

    /*
     * The following assumes that any given operator will only be in a single
     * btree operator class.  This is true at least for all the pre-defined
     * operator classes.  If it isn't true, then whichever operator class
     * happens to be returned first for the given operator will be used to
     * find the associated strategy numbers for the test. --Nels, Jan '93
     */
    scan = heap_beginscan(relation, false, NowTimeQual, 2, (ScanKey)entry);
    tuple = heap_getnext(scan, false, (Buffer *)NULL);
    if (! HeapTupleIsValid(tuple)) {
	elog(DEBUG, "clause_pred_clause_test: unknown pred_op");
	return false;
    }
    form = (AccessMethodOperatorTupleForm) HeapTupleGetForm(tuple);

    /* Get the predicate operator's strategy number (1 to 5) */
    pred_strategy = form->amopstrategy;

    /* Remember which operator class this strategy number came from */
    opclass_id = form->amopclaid;

    heap_endscan(scan);


    /*** 2. From the same opclass, find a strategy num for the clause_op ***/

    ScanKeyEntryInitialize(&entry[1], 0,
			   Anum_pg_amop_amopclaid,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(opclass_id));

    ScanKeyEntryInitialize(&entry[2], 0,
			   Anum_pg_amop_amopopr,
			   ObjectIdEqualRegProcedure,
			   ObjectIdGetDatum(clause_op));

    scan = heap_beginscan(relation, false, NowTimeQual, 3, (ScanKey)entry);
    tuple = heap_getnext(scan, false, (Buffer *)NULL);
    if (! HeapTupleIsValid(tuple)) {
	elog(DEBUG, "clause_pred_clause_test: unknown clause_op");
	return false;
    }
    form = (AccessMethodOperatorTupleForm) HeapTupleGetForm(tuple);

    /* Get the restriction clause operator's strategy number (1 to 5) */
    clause_strategy = form->amopstrategy;
    heap_endscan(scan);


    /*** 3. Look up the "test" strategy number in the implication table ***/

    test_strategy = BT_implic_table[clause_strategy-1][pred_strategy-1];
    if (test_strategy == 0)
	return false; /* the implication cannot be determined */


    /*** 4. From the same opclass, find the operator for the test strategy ***/

    ScanKeyEntryInitialize(&entry[2], 0,
			   Anum_pg_amop_amopstrategy,
			   Integer16EqualRegProcedure,
			   Int16GetDatum(test_strategy));

    scan = heap_beginscan(relation, false, NowTimeQual, 3, (ScanKey)entry);
    tuple = heap_getnext(scan, false, (Buffer *)NULL);
    if (! HeapTupleIsValid(tuple)) {
	elog(DEBUG, "clause_pred_clause_test: unknown test_op");
	return false;
    }
    form = (AccessMethodOperatorTupleForm) HeapTupleGetForm(tuple);

    /* Get the test operator */
    test_op = form->amopoprid;
    heap_endscan(scan);


    /*** 5. Evaluate the test ***/

    test_oper = MakeOper(
            test_op,            /* opno */
            InvalidObjectId,    /* opid */
            false,              /* oprelationlevel */
            BOOL_TYPEID,        /* opresulttype */
            0,                  /* opsize */
            NULL);              /* op_fcache */
    (void) replace_opid(test_oper);

    test_expr = lispCons(test_oper,
			 lispCons(lispCopy(clause_const),
				  lispCons(lispCopy(pred_const),
					   LispNil)));

    test_result = ExecEvalExpr(test_expr, NULL, &isNull, NULL);
    if (isNull) {
	elog(DEBUG, "clause_pred_clause_test: null test result");
	return false;
    }
    return test_result;
}


/*    		----  ROUTINES TO CHECK JOIN CLAUSES  ---- 	 */


/*    
 *    	indexable-joinclauses
 *    
 *    	Finds all groups of join clauses from among 'joininfo-list' that can 
 *    	be used in conjunction with 'index'.
 *    
 *    	The first clause in the group is marked as having the other relation
 *    	in the join clause as its outer join relation.
 *    
 *    	Returns a list of these clause groups.
 *    
 */

/*  .. find-index-paths 	 */

LispValue
indexable_joinclauses (rel,index,joininfo_list)
     Rel rel,index;
     List joininfo_list ;
{
	/*  XXX Lisp Mapcan -- may be wrong  */

    JInfo joininfo = (JInfo)NULL;
    LispValue cg_list = LispNil;
    LispValue i = LispNil;
    LispValue clausegroups = LispNil;

    foreach(i,joininfo_list) { 
	joininfo = (JInfo)CAR(i);
	clausegroups = 
	  group_clauses_by_indexkey (rel,
				     index,
				     get_indexkeys(index),
				     get_classlist (index),
				     get_jinfoclauseinfo(joininfo),
				     true);

	if ( consp (clausegroups)) {
	    set_cinfojoinid((CInfo)CAAR(clausegroups),
			get_otherrels(joininfo));
	}
	cg_list = nconc(cg_list,clausegroups);
    }
    return(cg_list);
}  /* function end */

/*    		----  PATH CREATION UTILITIES  ---- */


/*    
 *    	index-innerjoin
 *    
 *    	Creates index path nodes corresponding to paths to be used as inner
 *    	relations in nestloop joins.
 *    
 *    	'clausegroup-list' is a list of list of clauseinfo nodes which can use
 *    	'index' on their inner relation.
 *    
 *    	Returns a list of index pathnodes.
 *    
 */

/*  .. find-index-paths    	 */

LispValue
index_innerjoin (rel,clausegroup_list,index)
     Rel	rel;
     List 	clausegroup_list;
     Rel 	index ;
{
     LispValue clausegroup = LispNil;
     LispValue cg_list = LispNil;
     LispValue i = LispNil;
     LispValue relattvals = LispNil;
     List pagesel = LispNil;
     IndexPath pathnode = (IndexPath)NULL;
     Cost  	temp_selec;
     Count	temp_pages;

     foreach(i,clausegroup_list) {
	 clausegroup = CAR(i);
	 pathnode = RMakeIndexPath();
	 relattvals =                
	   get_joinvars (CAR(get_relids(rel)),clausegroup);
	 pagesel = 
	   index_selectivity (CInteger(CAR(get_relids (index))),
			      get_classlist (index),
			      get_opnos (clausegroup),
			      CInteger(getrelid (CInteger(CAR
							  (get_relids (rel))),
						 _query_range_table_)),
			      CAR (relattvals),
			      CADR (relattvals),
			      CADDR (relattvals),
			      length (clausegroup));
	  
	 set_pathtype((Path)pathnode,T_IndexScan);
	 set_parent((Path)pathnode,rel);
	 set_indexid(pathnode,get_relids (index));
	 set_indexqual(pathnode,clausegroup);
	 set_joinid((Path)pathnode,get_cinfojoinid((CInfo)CAR(clausegroup)));

	 temp_pages = (int)CDouble(CAR(pagesel));
	 temp_selec = CDouble(CADR(pagesel));

	 set_path_cost ((Path)pathnode,cost_index(
					(ObjectId)CInteger(CAR(get_relids (index))),
					temp_pages,
					temp_selec,
					get_pages (rel),
					get_tuples (rel),
					get_pages (index),
					get_tuples (index), true));

	 /* copy clauseinfo list into path for expensive function processing 
	    -- JMH, 7/7/92 */
	 set_locclauseinfo(pathnode,
			   set_difference(CopyObject(get_clauseinfo(rel)),
					  clausegroup,
					  LispNil));

	 /* add in cost for expensive functions!  -- JMH, 7/7/92 */
	 set_path_cost((Path)pathnode, get_path_cost((Path)pathnode) + 
		       xfunc_get_path_cost(pathnode));

	 cg_list = nappend1(cg_list,(LispValue)pathnode);
     }
     return(cg_list);
 }  /*  function end */

/*    
 *    	create-index-paths
 *    
 *    	Creates a list of index path nodes for each group of clauses
 *    	(restriction or join) that can be used in conjunction with an index.
 *    
 *    	'rel' is the relation for which 'index' is defined
 *    	'clausegroup-list' is the list of clause groups (lists of clauseinfo 
 *    		nodes) grouped by mergesortorder
 *    	'join' is a flag indicating whether or not the clauses are join
 *    		clauses
 *    
 *    	Returns a list of new index path nodes.
 *    
 */

/*  .. find-index-paths	 */

LispValue
create_index_paths (rel,index,clausegroup_list,join)
     Rel rel, index;
     LispValue clausegroup_list;
     bool join;
{
     /* XXX Mapcan  */
     LispValue clausegroup = LispNil;
     LispValue ip_list = LispNil;
     LispValue temp = LispTrue;
     LispValue i = LispNil;
     LispValue j = LispNil;
     IndexPath  temp_path;

     foreach(i,clausegroup_list) {
	 CInfo clauseinfo;
	 LispValue temp_node = LispNil;
	 bool temp = true;

	 clausegroup = CAR(i);

	 foreach (j,clausegroup) {
	     clauseinfo = (CInfo)CAR(j); 
	     if ( !(join_clause_p(get_clause(clauseinfo))
		    && equal_path_merge_ordering
		    ( get_ordering(index),
		     get_mergesortorder (clauseinfo)))) {
		 temp = false;
	     }
	 }
	  
	 if ( !join  || temp ) {  /* restriction, ordering scan */
	     temp_path = create_index_path (rel,index,clausegroup,join);
	     temp_node = 
	       lispCons ((LispValue)temp_path,
			LispNil);
	     ip_list = nconc(ip_list,temp_node);
	  } 
      }
     return(ip_list);
}  /* function end */

bool
indexpath_useless(p, plist)
IndexPath p;
List plist;
{
    LispValue x;
    IndexPath path;
    List qual;

    if (get_indexqual(p)) return false;
    foreach (x, plist) {
	path = (IndexPath)CAR(x);
	qual = get_indexqual(path);
	set_indexqual(path, LispNil);
	if (equal((Node)p, (Node)path)) {
	    set_indexqual(path, qual);
	    return true;
	  }
        set_indexqual(path, qual);
      }
    return false;
}

List
add_index_paths(indexpaths, new_indexpaths)
List indexpaths, new_indexpaths;
{
    LispValue x;
    IndexPath path;
    List retlist;
    extern int testFlag;

    if (!testFlag)
	return append(indexpaths, new_indexpaths);
    retlist = indexpaths;
    foreach (x, new_indexpaths) {
	path = (IndexPath)CAR(x);
	if (!indexpath_useless(path, retlist))
	    retlist = nappend1(retlist, (LispValue)path);
      }
    return retlist;
}

bool
function_index_operand(funcOpnd, rel, index)
    LispValue funcOpnd;
    Rel rel;
    Rel index;
{
    ObjectId heapRelid	= CInteger(CAR(get_relids(rel)));
    Func function 	= (Func)get_function(funcOpnd);
    LispValue funcargs	= get_funcargs(funcOpnd);
    LispValue indexKeys	= get_indexkeys(index);
    LispValue arg;

    /*
     * sanity check, make sure we know what we're dealing with here.
     */
    if ( !((consp(funcOpnd) && function) && IsA(function,Func)) )
	return false;

    if (get_funcid(function) != get_indproc(index))
	return false;

    /*
     * Check that the arguments correspond to the same arguments used
     * to create the functional index.  To do this we must check that
     * 1. refer to the right relatiion. 
     * 2. the args have the right attr. numbers in the right order.
     *
     *
     * Check all args refer to the correct relation (i.e. the one with
     * the functional index defined on it (rel).  To do this we can
     * simply compare range table entry numbers, they must be the same.
     */
    foreach (arg, funcargs) 
    {
	if (heapRelid != get_varno((Var)CAR(arg)))
	    return false;
    }

    /*
     * check attr numbers and order.
     */
    foreach (arg, funcargs) 
    {
	int curKey;

	if (indexKeys == LispNil)
	    return (false);

	curKey = CInteger(CAR(indexKeys));
	indexKeys = CDR (indexKeys);
	if (get_varattno((Var)CAR(arg)) != curKey)
	    return (false);
    }

    /*
     * We have passed all the tests.
     */
    return true;
}

bool
SingleAttributeIndex(index)
    Rel index;
{
    /*
     * Non-functional indices.
     */

    /*
     * return false for now as I don't know if we support index scans
     * on disjunction and the code doesn't work
     */
    return (false);

#if 0
    if (index->indproc == InvalidObjectId)
    {
	switch (length(get_indexkeys(index)))
	{
	    case 0:  return false;
	    case 1:  return true;

	    /*
	     * The list of index keys currently always has 8 elements.
	     * It is a SingleAttributeIndex if the second element on
	     * are invalid. It suffices to just check the 2nd -mer Oct 21 1991
	     */
	    default: return (CInteger(CADR(get_indexkeys(index))) == 
							    InvalidObjectId);
	}
    }

    /*
     * We have a functional index which is a single attr index
     */
    return true;
#endif
}
