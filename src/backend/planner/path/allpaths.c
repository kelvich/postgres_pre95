/*     
 *      FILE
 *     	path
 *     
 *      DESCRIPTION
 *     	Routines to find possible search paths for processing a query
 *     
 */
/* RcsId("$Header$"); */

/*     
 *      EXPORTS
 *     		find-paths
 */

#include <strings.h>

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

#include "planner/allpaths.h"
#include "planner/internal.h"
#include "planner/indxpath.h"
#include "planner/orindxpath.h"
#include "planner/joinrels.h"
#include "planner/prune.h"
#include "planner/indexnode.h"
#include "planner/pathnode.h"
#include "planner/clauses.h"
#include "planner/xfunc.h"
#include "tcop/creatinh.h"
#include "lib/copyfuncs.h"

/*    
 *    	find-paths
 *    
 *    	Finds all possible access paths for executing a query, returning the
 *    	top level list of relation entries.  
 *    
 *    	'rels' is the list of single relation entries appearing in the query
 *    	'nest-level' is the current attribute nesting level being processed
 *    	'sortkeys' contains info on sorting of the result
 *    
 */

/*  .. subplanner   */

LispValue
find_paths(rels,nest_level,sortkeys)
     LispValue rels,sortkeys ;
     int nest_level;
{
    if( length(rels) > 0 && nest_level > 0 ) {
	/* Set the number of join(not nesting) levels yet to be processed. */
	
	int levels_left = length(rels);

	/* Find the base relation paths. */
	find_rel_paths(rels,nest_level,sortkeys);
	
	if( !sortkeys && (levels_left <= 1)) {
	    /* Unsorted single relation, no more processing is required. */
	    return(rels);   
	} 
	else {
	    LispValue x;
	    /* 
	     * this means that joins or sorts are required.
	     * set selectivities of clauses that have not been set
	     * by an index.
	     */
	    set_rest_relselec(rels);
	    if(levels_left <= 1) {
		return(rels); 
	      } 
	    else {
		return(find_join_paths(rels,levels_left - 1,nest_level));
	      }
	  }
      }
    return(NULL);
}  /* end find_paths */

/*    
 *    	find-rel-paths
 *    
 *    	Finds all paths available for scanning each relation entry in 
 *    	'rels'.  Sequential scan and any available indices are considered
 *    	if possible(indices are not considered for lower nesting levels).
 *    	All unique paths are attached to the relation's 'pathlist' field.
 *    
 *    	'level' is the current attribute nesting level, where 1 is the first.
 *    	'sortkeys' contains sort result info.
 *    
 *    	RETURNS:  'rels'.
 *	MODIFIES: rels
 *    
 */

/*  .. find-paths      */

LispValue
find_rel_paths(rels,level,sortkeys)
     LispValue rels,sortkeys ;
     int level;
{
     LispValue temp;
     Rel rel;
     LispValue tmppath;
     
     foreach(temp,rels) {
	  LispValue sequential_scan_list;

	  rel = (Rel)CAR(temp);
	  sequential_scan_list = lispCons((LispValue)create_seqscan_path(rel),
					  LispNil);

	  if(level > 1) {
	       set_pathlist(rel,sequential_scan_list);
	       /* XXX casting a list to a Path */
	       set_unorderedpath(rel, (PathPtr)sequential_scan_list);
	       set_cheapestpath(rel, (PathPtr)sequential_scan_list);
	    } 
	  else {
	       LispValue rel_index_scan_list = 
		 find_index_paths(rel,find_relation_indices(rel),
				   get_clauseinfo(rel),
				   get_joininfo(rel),sortkeys);
	       LispValue or_index_scan_list = 
		 create_or_index_paths(rel,get_clauseinfo(rel));

	       set_pathlist(rel,add_pathlist(rel,sequential_scan_list,
					       append(rel_index_scan_list,
						       or_index_scan_list)));
	    } 
    
	  /* The unordered path is always the last in the list.  
	   * If it is not the cheapest path, prune it.
	   */
	  prune_rel_path(rel, (Path)last_element(get_pathlist(rel))); 
       }
     foreach(temp, rels) {
	 rel = (Rel)CAR(temp);
	 set_size(rel, compute_rel_size(rel));
	 set_width(rel, compute_rel_width(rel));
       }
     return(rels);

}  /* end find_rel_path */

/*    
 *    	find-join-paths
 *    
 *    	Find all possible joinpaths for a query by successively finding ways
 *    	to join single relations into join relations.  
 *
 *	if BushyPlanFlag is set, bushy tree plans will be generated:
 *	Find all possible joinpaths(bushy trees) for a query by systematically
 *	finding ways to join relations(both original and derived) together.
 *    
 *    	'outer-rels' is	the current list of relations for which join paths 
 *    		are to be found, i.e., he current list of relations that 
 *		have already been derived.
 *    	'levels-left' is the current join level being processed, where '1' is
 *    		the "last" level
 *    	'nest-level' is the current attribute nesting level
 *    
 *    	Returns the final level of join relations, i.e., the relation that is
 *	the result of joining all the original relations togehter.
 *    
 */

/*  .. find-join-paths, find-paths
 */
LispValue
find_join_paths(outer_rels,levels_left,nest_level)
     LispValue outer_rels;
     int levels_left, nest_level;
{
    LispValue x, y;
    Rel rel;
    Path curpath;
    LispValue tmp_rels;

    /*
    * Determine all possible pairs of relations to be joined at this level.
    * Determine paths for joining these relation pairs and modify 'new-rels'
    * accordingly, then eliminate redundant join relations.
    */

    LispValue new_rels = find_join_rels(outer_rels);

    find_all_join_paths(new_rels, outer_rels, nest_level);

    /* for each expensive predicate in each path in each rel, consider 
       doing pullup  -- JMH */
    if (XfuncMode != XFUNC_OFF)
      foreach(x, new_rels)
	xfunc_trypullup((Rel)CAR(x));

    new_rels = prune_joinrels(new_rels);
    prune_rel_paths(new_rels);

    if(BushyPlanFlag) {
      /*
       * In case of bushy trees
       * if there is still a join between a join relation and another
       * relation, add a new joininfo that involves the join relation
       * to the joininfo list of the other relation
       */
      add_new_joininfos(new_rels,outer_rels);
    }

    foreach(x, new_rels) {
       rel = (Rel)CAR(x);
       set_size(rel, compute_rel_size(rel));
       set_width(rel, compute_rel_width(rel));
    }

    if(BushyPlanFlag) {
      /* 
       * prune rels that have been completely incorporated into
       * new join rels
       */
      outer_rels = prune_oldrels(outer_rels);
      /* 
       * merge join rels if then contain the same list of base rels
       */
      outer_rels = merge_joinrels(new_rels,outer_rels);
      _join_relation_list_ = outer_rels;
    }
   else {
       _join_relation_list_ = new_rels;
     }

    if(levels_left == 1) {
      if(BushyPlanFlag)
	 return(final_join_rels(outer_rels));
      else
	 return(new_rels);
    } 
    else {
      if(BushyPlanFlag)
	  return(find_join_paths(outer_rels, levels_left - 1, nest_level));
      else
	 return(find_join_paths(new_rels, levels_left - 1, nest_level));
    } 
}


/*
 * The following functions are solely for the purpose of debugging.
 */

/*
 * xStrcat --
 *	Special strcat which returns concatenation into a static buffer.
 */
static
char *xStrcat(s1, s2)
char *s1, *s2;
{
    static char stringbuf[100];
    sprintf(stringbuf, "%s%s",s1,s2);
    return stringbuf;
}

void
printclauseinfo(ind,cl)
char *ind;
List cl;
{
    LispValue xc;
    char indent[100];
    strcpy(indent, ind);
    foreach(xc,cl) {
       CInfo c = (CInfo)CAR(xc);
       if(c == (CInfo)NULL) continue;
       printf("\n%sClauseInfo:	CInfo%lx",indent,(long)c);
       printf("\n%sOp:		%u",indent,get_opno((Oper)get_op((List)get_clause(c))));
       printf("\n%sLeftOp:	",indent);
       lispDisplay(get_varid(get_leftop((List)get_clause(c))));
       printf("\n%sRightOp:	",indent);
       lispDisplay(get_varid(get_rightop((List)get_clause(c))));
       printf("\n%sSelectivity:	%lg\n",indent,get_selectivity(c));
    }
}

void
printjoininfo(ind,jl)
char *ind;
List jl;
{
    LispValue xj;
    char indent[100];
    strcpy(indent, ind);
    foreach(xj,jl) {
	JInfo j = (JInfo)CAR(xj);
	if(joininfo_inactive(j)) continue;
	printf("\n%sJoinInfo:	JInfo%lx",indent,(long)j);
	printf("\n%sOtherRels:	",indent);
	lispDisplay(get_otherrels(j));
	printf("\n%sClauseInfo:	",indent);
	printclauseinfo(xStrcat(indent, "	"),get_jinfoclauseinfo(j));
    }
}

void
printpath(ind,p)
char *ind;
Path p;
{
    char indent[100];
    strcpy(indent, ind);
	if(p == (Path)NULL) return;
	printf("%sPath:	0x%lx",indent,(long)p);
	printf("\n%sParent:	",indent);
	lispDisplay(get_relids(get_parent(p)));
	printf("\n%sPathType:	%ld",indent,get_pathtype(p));
	printf("\n%sCost:	%lg",indent,get_path_cost(p));
	switch(get_pathtype(p))  {
	case T_NestLoop:
		printf("\n%sClauseInfo:	\n",indent);
		printclauseinfo(xStrcat(indent,"	"),get_pathclauseinfo((JoinPath)p));
		printf("%sOuterJoinPath:	\n",indent);
		printpath(xStrcat(indent,"	"),get_outerjoinpath((JoinPath)p));
		printf("%sInnerJoinPath:	\n",indent);
		printpath(xStrcat(indent,"	"),get_innerjoinpath((JoinPath)p));
		printf("%sOuterJoinCost:	%lg",indent,get_outerjoincost(p));
		printf("\n%sJoinId:	",indent);
		lispDisplay(get_joinid(p));
		break;
	case T_MergeJoin:
		printf("\n%sClauseInfo: \n",indent);
                printclauseinfo(xStrcat(indent,"       "),get_pathclauseinfo((JoinPath)p));
                printf("%sOuterJoinPath:        \n",indent);
                printpath(xStrcat(indent," "),get_outerjoinpath((JoinPath)p));
                printf("%sInnerJoinPath:        \n",indent);
                printpath(xStrcat(indent," "),get_innerjoinpath((JoinPath)p));
                printf("%sOuterJoinCost:        %lg",indent,get_outerjoincost(p));
                printf("\n%sJoinId:     ",indent);
                lispDisplay(get_joinid(p));
		printf("\n%sMergeClauses:	",indent);
		lispDisplay(get_path_mergeclauses((MergePath)p));
		printf("\n%sOuterSortKeys:	",indent);
		lispDisplay(get_outersortkeys((MergePath)p));
		printf("\n%sInnerSortKeys:	",indent);
		lispDisplay(get_innersortkeys((MergePath)p));
		break;
	case T_HashJoin:
                printf("\n%sClauseInfo: \n",indent);
                printclauseinfo(xStrcat(indent,"       "),get_pathclauseinfo((JoinPath)p));
                printf("%sOuterJoinPath:        \n",indent);
                printpath(xStrcat(indent," "),get_outerjoinpath((JoinPath)p));
		printf("%sInnerJoinPath:        \n",indent);
                printpath(xStrcat(indent," "),get_innerjoinpath((JoinPath)p));
                printf("%sOuterJoinCost:        %lg",indent,get_outerjoincost(p));
                printf("\n%sJoinId:     ",indent);
                lispDisplay(get_joinid(p));
		printf("\n%sHashClauses:	",indent);
		lispDisplay(get_path_hashclauses((HashPath)p));
		printf("\n%sOuterHashKeys:	",indent);
		lispDisplay(get_outerhashkeys((HashPath)p));
		printf("\n%sInnerHashKeys:	",indent);
		lispDisplay(get_innerhashkeys((HashPath)p));
		break;
	case T_IndexScan:
		printf("\n%sIndexQual:	",indent);
		lispDisplay(get_indexqual((IndexPath)p));
		break;
	}
	printf("\n");
}

void
printpathlist(ind, pl)
char *ind;
LispValue pl;
{
    LispValue xp;
    char indent[100];
    strcpy(indent,ind);
    foreach(xp,pl) {
	Path p = (Path)CAR(xp);
	printpath(indent,p);
    }
}

void
printrels(ind,rl)
char *ind;
List rl;
{
    LispValue xr;
    char indent[100];
    strcpy(indent,ind);
    foreach(xr,rl) {
	Rel r = (Rel)CAR(xr);
	if(r == (Rel)NULL) continue;
	printf("\n%sRel:	Rel%lx",indent,(long)r);
	printf("\n%sRelid:	",indent);
	lispDisplay(get_relids(r));
	printf("\n%sSize:	%u",indent,get_size(r));
	printf("\n%sPathlist:	\n",indent);
	printpathlist(xStrcat(indent,"	"),get_pathlist(r));
	printf("%sClauseInfo:	\n",indent);
	printclauseinfo(xStrcat(indent,"	"),get_clauseinfo(r));
	printf("%sJoinInfo:	",indent);
	printjoininfo(xStrcat(indent,"	"),get_joininfo(r));
     }
}

void
PrintRels(rl)
List rl;
{
    printrels("",rl);
    printf("\n");
}

void
PRel(r)
Rel r;
{
    printrels("",lispCons((LispValue)r,LispNil));
}
