
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

#include "pg_lisp.h";
#include "relation.h"
#include "relation.a.h"
#include "planner/internal.h";
#include "planner/indxpath.h"
#include "planner/clauses.h"
#include "lsyscache.h"
#include "planner/clauseinfo.h"
#include "planner/cfi.h"


/* #define INDEXSCAN 1
   #define List LispValue
 */

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

/*  .. find-index-paths, find-rel-paths */

LispValue
find_index_paths (rel,indices,clauseinfo_list,joininfo_list,sortkeys)
     LispValue rel,indices,clauseinfo_list,joininfo_list,sortkeys ;

{
	LispValue scanclausegroups = LispNil;
	LispValue scanpaths = LispNil;
	LispValue index = LispNil;
	LispValue joinclausegroups = LispNil;
	LispValue joinpaths = LispNil;
	LispValue sortpath = LispNil;
	
	if(consp (indices)) {
	     
	     /*  1. If this index has only one key, try matching it against 
	      * subclauses of an 'or' clause.  The fields of the clauseinfo
	      * nodes are marked with lists of the matching indices no path
	      * are actually created. 
	      */

	     if ( 1 == length (get_indexkeys (CAR (indices)))) {
		  match_index_orclauses (rel,CAR (indices),
					 CAR ( get_indexkeys 
					      (CAR (indices))),
					 CAR (get_classlist(CAR (indices))),
					 clauseinfo_list);
	     }
	     
	     index = CAR (indices);
	     /* 2. If the keys of this index match any of the available 
	      * restriction clauses, then create pathnodes corresponding 
	      * to each group of usable clauses.
	      */
	     
	     scanclausegroups =
	       group_clauses_by_indexkey (rel,index,get_indexkeys (index),
					  get_classlist (index),clauseinfo_list,
					  LispNil,LispNil);
	     scanpaths = LispNil;
	     if ( consp (scanclausegroups) ) 
	       scanpaths = create_index_paths (rel,index,
					       scanclausegroups,
					       LispNil);
	     
	     /* 3. If this index can be used with any join clause, then 
	      * create pathnodes for each group of usable clauses.  An 
	      * index can be used with a join clause if its ordering is 
	      * useful for a mergejoin, or if the index can possibly be 
	      * used for scanning the inner relation of a nestloop join. 
	      */
	     
	     joinclausegroups = 
	       indexable_joinclauses (rel,index,joininfo_list);
	     joinpaths = LispNil;
	     if ( consp (joinclausegroups) ) {
		  LispValue new_join_paths = 
		    create_index_paths (rel,index,joinclausegroups,
					LispTrue);
		  LispValue innerjoin_paths = 
		    index_innerjoin (rel,joinclausegroups,index);
		  set_innerjoin (rel,nconc (get_innerjoin(rel),
					    innerjoin_paths));
		  joinpaths = new_join_paths;
	     }
	     
	     /* 4. If this particular index hasn't already been used above,
	      *   then check to see if it can be used for ordering a  
	      *   user-specified sort.  If it can, add a pathnode for the 
	      *   sorting scan. 
	      */
	     
	     sortpath = LispNil;
	     if ( valid_sortkeys(sortkeys) && null(scanclausegroups) && 
		 null(joinclausegroups) && equal(get_relid(sortkeys),
						 get_relid (rel)) && 
		 equal_path_path_ordering(get_ordering (sortkeys),
					  get_ordering (index)) && 
		 equal (get_sortkeys(sortkeys),get_indexkeys (index))) {
		  sortpath =lispCons (create_index_path (rel,index,
							 LispNil,
							 LispNil),
				      LispNil);
	     } 
	     
	     return (append (scanpaths,joinpaths,sortpath,
			     find_index_paths (rel,
					       CDR (indices),
					       clauseinfo_list,
					       joininfo_list,sortkeys)));
	} 
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
LispValue rel,index,indexkey,xclass,clauseinfo_list ;
{
     LispValue clauseinfo;
     foreach (clauseinfo, clauseinfo_list) {
	  if ( valid_or_clause (clauseinfo)) {
	       
	       /* Mark the 'or' clause with a list of indices which 
		* match each of its subclauses.  The list is
		* generated by adding 'index' to the existing
		* list where appropriate.
		*/
	       
	       set_indexids (clauseinfo,
			  match_index_orclause (rel,index,indexkey,
						xclass,
						get_orclauseargs
						(get_clause(clauseinfo)),
						get_indexids (clauseinfo)));
	  }
     }
}  /* function end */

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
 *    
 */

/*  .. match-index-orclauses  	 */

LispValue
match_index_orclause (rel,index,indexkey,xclass,
		      or_clauses,other_matching_indices)
     LispValue rel,index,indexkey,xclass,or_clauses,other_matching_indices ;
{
     /* XXX - lisp mapCAR  -- may be wrong. */
     LispValue clause = LispNil;
     LispValue matched_indices  = other_matching_indices;
     LispValue index_list = LispNil;
     
     foreach (clause,or_clauses) {
	  if (is_clause (clause) && 
	      op_class(get_opno(get_op (clause)),xclass) && 
	      match_indexkey_operand (indexkey,
				      get_leftop (clause),
				      rel) &&
	      IsA(get_rightop (clause),Const)) {
	       index_list = nappend1(index_list,
				     lispCons (index,matched_indices));
	  } 
	  else {
	       index_list = nappend1(index_list,matched_indices);
	  }
     }
     return(index_list);
     
}  /* function end */

/*    		----  ROUTINES TO CHECK RESTRICTIONS  ---- 	 */


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
group_clauses_by_indexkey (rel,index,indexkeys,classes,clauseinfo_list,
			   matched_clauseinfo_list,join)
     LispValue rel,index,indexkeys,classes,clauseinfo_list,
     matched_clauseinfo_list,join ;
{
     if ( consp (clauseinfo_list) ) {
	  
	  /*    If we can't find any matching clauses for the first of 
	   *    the remaining keys, give up now. 
	   */
	  
	  CInfo matched_clause = 
	    match_clauses_to_indexkey (rel,index,
				       CAR(indexkeys),
				       CAR(classes),
				       clauseinfo_list,join);
	  if ( matched_clause ) {
	       LispValue new_matched_clauseinfo_list =
		 cons (matched_clause,matched_clauseinfo_list);
	       LispValue clausegroup = LispNil;
	       if ( CDR (indexkeys) ) {
		    clausegroup = 
		      group_clauses_by_indexkey (rel,index,
						 CDR (indexkeys),
						 CDR (classes),
						 clauseinfo_list,
						 new_matched_clauseinfo_list,
						 join);
	       } 
	       else {
		    clausegroup = 
		      lispCons (new_matched_clauseinfo_list,LispNil);
	       } 

	       if ( consp (clausegroup) ) {
		    return (nconc (clausegroup,

			   /*  See if other clauses can be used for */
			   /*  these remaining keys by ignoring the one */
			   /*  that was just used (i.e., find all  */
			   /*  possible solutions). */

				   group_clauses_by_indexkey (rel,index,
							      indexkeys,
							      classes,
							      remove 
							      (matched_clause,
							      clauseinfo_list),
						      matched_clauseinfo_list,
							      join)));
	       } 
	  }
	  return (LispNil);
     }
     return (LispNil);
}  /* function end */

/*    
 *    	match-clauses-to-indexkey
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
 */

/*  .. group-clauses-by-indexkey  */
 
CInfo
match_clauses_to_indexkey (rel,index,indexkey,xclass,clauses,join)
     LispValue rel,index,indexkey,xclass,clauses,join ;
{
	/* XXX lisp find-if function  --- may be wrong */
     LispValue clauseinfo = LispNil;
     foreach(clauseinfo,clauses) {
	  Expr clause = get_clause (clauseinfo);
	  Var leftop = get_leftop (clause);
	  Var rightop = get_rightop (clause);
	  ObjectId join_op;
	  bool temp1 ;
	  bool temp2 ;
	  if(null (join)) {
	       ;
	  } 
	  else if (match_indexkey_operand (indexkey,rightop,rel)) {
	       join_op = get_commutator (get_opno (get_op (clause)));
	  } 
	  else if (match_indexkey_operand (indexkey,leftop,rel)) {
	       join_op = get_opno (get_op (clause));
	  } 
	  
	  temp1 = (bool) (null(join_op)    /*   (op var const) */
			  && op_class(get_opno(get_op(clause)),xclass)
			  && ( (qual_clause_p(clause)
				&& equal_indexkey_var(indexkey,leftop))
			      || function_index_clause_p(clause,rel,index)));
		
	  temp2 = (bool) (join_op && op_class(join_op,xclass) &&
			  !zerop(join_op) && join_clause_p(clause));
	  
	  if (temp1 || temp2) 
	    return((CInfo)clauseinfo);
     }
} /* function end */


			  
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
     LispValue rel,index,joininfo_list ;
{
	/*  XXX Lisp Mapcan -- may be wrong  */

     LispValue joininfo = LispNil;
     LispValue cg_list = LispNil;

     foreach(joininfo,joininfo_list) {
	  LispValue clausegroups = 
	    group_clauses_by_indexkey (rel,index,
				       get_indexkeys(index),
				       get_classlist (index),
				       get_clauseinfo(joininfo),
				       LispNil,
				       LispTrue);
		
	  if ( consp (clausegroups)) {
	       set_joinid (CAAR(clausegroups),
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
     LispValue rel,clausegroup_list,index ;

{
     LispValue clausegroup = LispNil;
     LispValue cg_list = LispNil;
     
     /*  XXX MapCAR function  */
     foreach(clausegroup,clausegroup_list) {
	  Path pathnode = CreateNode(Path);
	  
	  LispValue relattvals =                
	    get_joinvars (get_relid (rel),clausegroup);
	  LispValue pagesel = 
	    index_selectivity (CAR((LispValue)get_indexid (index)),
			       get_classlist (index),
			       get_opnos (clausegroup),
			       getrelid (get_relid (rel),
					 _query_range_table_),
			       CAR (relattvals),
			       CADR (relattvals),
			       CADDR (relattvals),
			       length (clausegroup));
	  
	  set_pathtype (pathnode,T_IndexScan);
	  set_parent (pathnode,rel);
	  set_indexid (pathnode,get_indexid (index));
	  set_indexqual (pathnode,clausegroup);
	  set_joinid (pathnode,get_joinid (CAR (clausegroup)));
	  set_cost (pathnode,cost_index (nth (0,get_indexid (index)),
					 floor (nth (0,pagesel)),
					 (int)(nth (1,pagesel)),
					 get_pages (rel),
					 get_tuples (rel),
					 get_pages (index),
					 get_tuples (index)));

	  cg_list = nappend1(cg_list,pathnode);
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
     LispValue rel,index,clausegroup_list,join ;
{
     /* XXX Mapcan  */
     LispValue clausegroup = LispNil;
     LispValue ip_list = LispNil;
     LispValue temp = LispTrue;

     foreach(clausegroup,clausegroup_list) {
	  LispValue clauseinfo = LispNil;
	  LispValue temp_node = LispNil;
	  
	  foreach (clauseinfo,clausegroup) {
	       if ( !(join_clause_p(get_clause(clauseinfo))
		      && equal_path_merge_ordering
		      ( get_ordering(index),
		       get_mergesortorder (clauseinfo)))) {
		    return(LispNil);
	       }
	  }

	  if ( !join  || temp ) {  /* restriction, ordering scan */
	       temp_node = 
		 lispCons (create_index_path (rel,index,
					      clausegroup,join),
			   LispNil);
	       ip_list = nconc(ip_list,temp_node);
	  } 
     }
     return(ip_list);
}  /* function end */

