
/*     
 *      FILE
 *     	prepunion
 *     
 *      DESCRIPTION
 *     	Routines to plan archive, inheritance, union, and version queries
 *     
 */

/* RcsId ("$Header$");  */

/*     
 *      EXPORTS
 *     		first-matching-rt-entry
 *     		plan-union-queries
 *     		fix-targetlist
 */

#include "tmp/c.h"

#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/execnodes.h"
#include "nodes/plannodes.h"
#include "nodes/plannodes.a.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"

#include "parser/parse.h"
#include "parser/parsetree.h"
#include "tmp/utilities.h"
#include "utils/log.h"
#include "utils/lsyscache.h"

#include "planner/internal.h"
#include "planner/cfi.h"
#include "planner/plancat.h"
#include "planner/planner.h"
#include "planner/prepunion.h"
#include "planner/handleunion.h"

/* ----------------
 *	Append creator declaration
 * ----------------
 */
extern Append RMakeAppend();

/*    
 *    	find-all-inheritors
 *    
 *    	Returns a list of relids corresponding to relations that inherit
 *    	attributes from any relations listed in either of the argument relid
 *    	lists.
 *    
 */

/*  .. find-all-inheritors, plan-union-queries    */

LispValue
find_all_inheritors (unexamined_relids,examined_relids)
     LispValue unexamined_relids,examined_relids ;
{
    LispValue new_inheritors = LispNil;
    LispValue new_examined_relids = LispNil;
    LispValue new_unexamined_relids = LispNil;
    
    /*    Find all relations which inherit from members of */
    /*    'unexamined-relids' and store them in 'new-inheritors'. */
    
    LispValue rels = LispNil;
    LispValue newrels = LispNil;
    
     /* was lispDO */
    foreach(rels,unexamined_relids) {
	newrels = LispUnion(find_inheritance_children (CAR (rels)),
			    newrels);
    }
    new_inheritors = newrels;
    
    new_examined_relids = LispUnion (examined_relids,unexamined_relids);
    new_unexamined_relids = set_difference (new_inheritors,
					    new_examined_relids);
    
    if(null (new_unexamined_relids)) {
	return(new_examined_relids);
    } else {
	return (find_all_inheritors (new_unexamined_relids,
				     new_examined_relids));
    }
} /* function end   */

/*    		=====
 *    		UNION
 *    		=====
 */

/*    		=======
 *    		VERSION
 *    		=======
 */

/*    		===============
 *    		COMMON ROUTINES
 *    		===============
 */

/*    
 *    	first-matching-rt-entry
 *    
 *    	Given a rangetable, find the first rangetable entry that represents
 *    	the appropriate special case.
 *    
 *    	Returns a rangetable index.
 *    
 */

/*  .. planner    */

int
first_matching_rt_entry (rangetable,flag)
     LispValue rangetable,flag ;
{
    

    int count = 0;
    int position = -1;
    LispValue temp = LispNil;
    bool first = true;
/*     XXX the bogus way to handle things (old parser)
 *    	 (position-if (case flag
 *    			    (archive
 *    			     #'(lambda (x)
 *    				 (rt_archive_time x)))
 *    			    (inheritance
 *    			     #'(lambda (x)
 *    				 (and (rt_relid x)
 *    				      (listp (rt_relid x)))))
 *    			    ((union version)
 *    			     #'(lambda (x)
 *    				 #+opus43 (declare (special flag))
 *    				 (member flag (rt_flags x)))))
 *    )
 *     XXX new spiffy way to handle things (new parser)
 */

    foreach (temp,rangetable) {
	if (member (flag,rt_flags (CAR(temp))) && first) {
	    position = count;
	    first = false;
	}
	count++;
      }
    
    if(position != -1) 
      position++;
     
    return(position);
} /* function end   */


/*    
 *    	plan-union-queries
 *    
 *    	Plans the queries for a given parent relation.
 *    
 *    	Returns a list containing a list of plans and a list of rangetable
 *    	entries to be inserted into an APPEND node.
 *     XXX - what exactly does this mean, look for make_append
 */

/*  .. planner     */

Append
plan_union_queries (rt_index,flag,root,tlist,qual,rangetable)
     Index  rt_index;
     int flag;
     LispValue root,tlist,qual,rangetable ;
{
    LispValue temp_flag = LispNil;
    switch (flag) {
	
	/*    Most queries require us to iterate over a list of relids. */
	/*    Archives are handled differently.  */
	/*	Instead of a list of different */
	/*    relids, we need to plan the same query  */
	/*    twice in different modes. */
	
      case ARCHIVE :
	{ /* XXX - let form, maybe incorrect */
	    Plan primary_plan = (Plan)NULL;
	    Plan archive_plan = (Plan)NULL;

	    _query_is_archival_ = false;
	    primary_plan = init_query_planner (root,tlist,qual);
	    _query_is_archival_ = true;
	    archive_plan = init_query_planner (root,tlist,qual);
	    return (make_append (lispCons(primary_plan,
					  lispCons(archive_plan,LispNil)),
				 rt_index,
				 lispCons(rt_fetch (rt_index,rangetable),
					  lispCons(rt_fetch (rt_index,
							     rangetable),
						   LispNil)),
				 get_qptargetlist (primary_plan)));
	    
	  }
	break;
	
      default:
	{ 
	    LispValue rt_entry = rt_fetch (rt_index,rangetable);
	    LispValue union_relids = LispNil;

	    LispValue union_info = LispNil;
	    LispValue union_plans = LispNil;
	    LispValue union_rt_entries = LispNil;
	    LispValue temp = LispNil;
	    
	    switch (flag) {
		
	      case INHERITS :
		union_relids = 
		  find_all_inheritors (lispCons(rt_relid (rt_entry),
						LispNil),
				       LispNil);
		break;

	      case UNION :
		  union_plans = handleunion(root,rangetable,tlist,qual);
		  return (make_append (union_plans,
				       -1, rangetable,
				       get_qptargetlist (CAR(union_plans))));
		break;
		
	      case VERSION :
		union_relids = 
		  find_version_parents (rt_relid (rt_entry));
		break;
		
	      default: 
		/* do nothing */
		break;
	    }
	    
	    /*    XXX temporary for inheritance: */
	    /*     relid is listified by the parser. */
	    
	    /*    	   (case flag
	     *    		 (inheritance
	     *    		  (setf (rt_relid (rt_fetch rt-index 
	     *                                  rangetable))
	     *    			(CAR (rt_relid 
	     *                         (rt_fetch rt-index rangetable))))))
	     */
	    
	    /*    Remove the flag for this relation, */
	    /*     since we're about to handle it */
	    /*    (do it before recursing!). */
	    /*    XXX destructive parse tree change */
	    /*   was setf -- is this right?  */
	    
	    temp_flag = lispInteger(flag);
	    NodeSetTag(temp_flag,classTag(LispSymbol));
	    rt_flags (rt_fetch (rt_index,rangetable)) = 
	      LispRemove (temp_flag,
		      rt_flags (rt_fetch (rt_index,rangetable)));
	    
	    /* XXX - can't find any reason to sort union-relids
	       as paul did, so we're leaving it out for now
	       (maybe forever) - jeff & lp */

	    union_info = plan_union_query (union_relids,
					   rt_index,rt_entry,
					   root,tlist,qual,rangetable);
	    
	    foreach(temp,union_info) {
	      union_plans = nappend1(union_plans,CAR(CAR(temp)));
	      union_rt_entries = nappend1(union_rt_entries,
					  CADR (CAR(temp)));
	    }
	    
	    return (make_append (union_plans,
				 rt_index,union_rt_entries,
				 get_qptargetlist (CAR (union_plans))));
	  }
      }
    
  }   /* function end  */


/*    
 *    	plan-union-query
 *    
 *    	Returns a list of plans for 'relids'.
 *    
 */

/*  .. plan-union-queries, plan-union-query    */

LispValue
plan_union_query (relids,rt_index,rt_entry,root,tlist,qual,rangetable)
     LispValue relids,rt_entry,root,tlist,qual,rangetable ;
     Index rt_index;
{
    LispValue i = LispNil;
    LispValue union_plans = LispNil;

    foreach (i,relids) {
	LispValue relid = CAR (i);
	LispValue new_rt_entry = new_rangetable_entry (CInteger(relid),
						       rt_entry);
	LispValue new_root = subst_rangetable (root,
					       rt_index,
					       new_rt_entry);
	LispValue new_parsetree = LispNil;
	LispValue new_tlist = copy_seq_tree(tlist);
	LispValue new_qual = copy_seq_tree(qual);

	if ( listp(rt_relid(rt_entry))) 
	  new_parsetree = fix_parsetree_attnums (rt_index, 
					     /* XX temporary for inheritance */
						 CInteger(CAR 
							 (rt_relid(rt_entry))),
						 CInteger(relid),
						 lispCons(new_root,
							  lispCons
							       (new_tlist,
								lispCons
								(new_qual,
								 LispNil))));
	else 
	  new_parsetree = fix_parsetree_attnums (rt_index, 
						 CInteger(rt_relid(rt_entry)),
						 CInteger(relid),
						 lispCons(new_root,
							  lispCons
							  (new_tlist,
							   lispCons
							   (new_qual,
							    LispNil))));


	union_plans = nappend1(union_plans,
			       lispCons(planner(new_parsetree),
					 lispCons(new_rt_entry,
						  LispNil)));
    }
    return(union_plans);

}  /* function end   */

/*    
 *    	new-rangetable-entry
 *    
 *    	Replaces the name and relid of 'old-entry' with the values for 
 *    	'new-relid'.
 *    
 *    	Returns a copy of 'old-entry' with the parameters substituted.
 *    
 */

/*  .. plan-union-query     */

LispValue
new_rangetable_entry (new_relid,old_entry)
     ObjectId new_relid;
     LispValue old_entry ;
{

    LispValue new_entry = copy_seq_tree (old_entry);
/*
 *  following stmt had to be changed to ensuing if stmt.
 *
 *   rt_relname (new_entry) = lispString(get_rel_name (new_relid));
 *   
 *  i know it's not a good idea to manipulate range table entry
 *  without using macros defined in lib/H/parsetree.h, but i couldn't
 *  think of a better way to do it.  i'll come with a better fix when
 *  i get more familiar with the parser.   8/1/90 ron choi
 */
    if (!strcmp(CString(CAR(new_entry)), "*CURRENT*") ||
        !strcmp(CString(CAR(new_entry)), "*NEW*"))
      CAR(new_entry) =  lispString(get_rel_name (new_relid));
    else
      CADR(new_entry) =  lispString(get_rel_name (new_relid));


    rt_relid (new_entry) = lispInteger(new_relid);
    return(new_entry);
}

/*    
 *    	subst-rangetable
 *    
 *    	Replaces the 'index'th rangetable entry in 'root' with 'new-entry'.
 *    
 *    	Returns a new copy of 'root'.
 *    
 */

/*  .. plan-union-query    */

LispValue
subst_rangetable (root,index,new_entry)
     LispValue root,new_entry ;
     Index index;
{
    LispValue new_root = copy_seq_tree (root);
    LispValue temp = LispNil;
    int i = 0;

    for(temp = root_rangetable(new_root),i =1; i < index; temp =CDR(temp),i++)
      ;
    CAR(temp) = new_entry;
    /*  setf(rt_fetch (index,root_rangetable (new_root)),new_entry); */
    return (new_root);

}

/*    
 *    	fix-parsetree-attnums
 *     
 *    	Replaces attribute numbers from the relation represented by
 *    	'old-relid' in 'parsetree' with the attribute numbers from
 *    	'new-relid'.
 *    
 *    	Returns the destructively-modified parsetree.
 *    
 */

/*  .. fix-parsetree-attnums, plan-union-query    */

LispValue
fix_parsetree_attnums (rt_index,old_relid,new_relid,parsetree)
     LispValue old_relid,new_relid,parsetree ;
     Index rt_index;
{
    LispValue i = LispNil;
    Node foo = (Node)NULL;
    
    
    /* If old_relid == new_relid, we shouldn't have to 
     * do anything ---- is this right??
     */

    if (old_relid != new_relid)
      foreach(i,parsetree) {
	  foo = (Node)CAR(i);
	  if (!null(foo) && IsA(foo,Var) && 
	       (get_varno (foo) == rt_index)) {
	      set_varattno (foo, 
			    get_attnum (new_relid,
					get_attname(old_relid,
						    get_varattno(foo))));
	  } 
	  if (!null(foo) && IsA(foo,LispList)) {
	      foo = (Node)fix_parsetree_attnums(rt_index,old_relid,
						new_relid,foo);
	  }
	  /* vectors ? */
	  /* all other cases are not interesting, I believe - jeff */
      }

    return (parsetree);
}

/*    
 *    	fix-rangetable
 *    
 *    	Replaces the 'index'th rangetable entry with 'new-entry'.
 *    
 *    	Returns a new copy of 'rangetable'.
 *    
 */

/*  .. print_plan    */

List
fix_rangetable (rangetable,index,new_entry)
     List rangetable;
     int index;
     List new_entry ;
{
  LispValue new_rangetable = copy_seq_tree (rangetable);
  /* XXX was setf  */
  rt_store (index,new_rangetable,new_entry);

  return (new_rangetable);

}

/*  XXX dummy function -- HELP fix me !  */
TL
fix_targetlist (oringtlist, tlist)
     TL oringtlist,tlist;
{
    elog(WARN,"unsupported function, fix_targetlist");
    return((TL)tlist);
}
		

Append
make_append (unionplans,rt_index,union_rt_entries, tlist)
     List unionplans,union_rt_entries,tlist;
     Index rt_index;
{
  Append node = RMakeAppend();

  set_unionplans(node,unionplans);
  set_unionrelid(node,rt_index);
  set_unionrtentries(node,union_rt_entries);
  set_cost(node,0.0);
  set_fragment(node,0);
  set_state(node,(EState)NULL);
  set_qptargetlist(node,tlist);
  set_qpqual(node,LispNil);
  set_lefttree(node,(Plan)NULL);
  set_righttree(node,(Plan)NULL);

  return(node);
}
