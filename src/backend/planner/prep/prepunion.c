
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

#include "internal.h"
#include "pg_lisp.h"

extern LispValue find_all_inheritors();
extern LispValue plan_union_queries();
extern LispValue plan_union_query();
extern LispValue new_rangetable_entry();
extern LispValue subst_rangetable();
extern LispValue fix_parsetree_attnums();

/*    		=======
 *    		ARCHIVE
 *    		=======
 */

#define ARCHIVE 1

/*    		===========
 *    		INHERITANCE
 *    		===========
 */

#define INHERITANCE 2
#define VERSION 3
#define UNION 4

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

LispValue
first_matching_rt_entry (rangetable,flag)
     LispValue rangetable,flag ;
{

/* XXX - let form, maybe incorrect */
     int count = 0;
     int position = -1;
     LispValue temp = LispNil;
     Boolean first = 1;
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
	  if (member (flag,rt_flags (temp)) && first) {
	       position = count;
	       first = 0;
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
 *    
 */

/*  .. planner     */

LispValue
plan_union_queries (rt_index,flag,root,tlist,qual,rangetable)
     LispValue rt_index,flag,root,tlist,qual,rangetable ;
{
     switch (flag) {

	  /*    Most queries require us to iterate over a list of relids. */
	  /*    Archives are handled differently.  */
	  /*	Instead of a list of different */
	  /*    relids, we need to plan the same query  */
	  /*    twice in different modes. */

	case ARCHIVE :
	  { /* XXX - let form, maybe incorrect */
	       LispValue primary_plan = LispNil;
	       LispValue archive_plan = LispNil;
	       _query_is_archival_ = LispNil;
	       primary_plan = init_query_planner (root,tlist,qual);;
	       _query_is_archival_ = 1 ;
	       archive_plan = init_query_planner (root,tlist,qual);;
	       return (make_append (list (primary_plan,archive_plan),
				    rt_index,
				    list (rt_fetch (rt_index,rangetable),
					  rt_fetch (rt_index,rangetable)),
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
		    
		  case INHERITANCE :
		    union_relids = 
		      find_all_inheritors (list (rt_relid (rt_entry)),
						LispNil);
		    break;

		  case UNION :
		    /*    XXX What goes here?? */
		    union_relids = LispNil;
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

	       rt_flags (rt_fetch (rt_index,rangetable)) = 
		 remove (flag,rt_flags (rt_fetch (rt_index,rangetable)));

	       union_info = plan_union_query (sort (union_relids, <),
					      rt_index,rt_entry,
					      root,tlist,qual,rangetable);
	       
	       foreach(temp,union_info) {
		 union_plans = nappend1(union_plans,CAR (temp));
		 union_rt_entries = nappend1(union_rt_entries,
					     CADR (temp));
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
     LispValue relids,rt_index,rt_entry,root,tlist,qual,rangetable ;
{
  if ( consp (relids) ) {
    LispValue relid = CAR (relids);
    LispValue new_rt_entry = new_rangetable_entry (relid,rt_entry);
    LispValue new_root = subst_rangetable (root,rt_index,new_rt_entry);
    LispValue new_parsetree = LispNil;

    if ( listp(rt_relid(rt_entry))) 
      new_parsetree = fix_parsetree_attnums (rt_index, 
					     /* XX temporary for inheritance */
					     CAR (rt_relid(rt_entry)),
					     relid,
					     list(new_root,
						  copy_seq_tree(tlist),
						  copy_seq_tree(qual)));
    else 
      new_parsetree = fix_parsetree_attnums (rt_index, 
					     rt_relid(rt_entry),
					     relid,
					     list(new_root,
						  copy_seq_tree(tlist),
						  copy_seq_tree(qual)));


    return (cons (list (planner (new_parsetree),new_rt_entry),
		  plan_union_query (CDR (relids),
				    rt_index,rt_entry,
				    root,tlist,qual,rangetable)));
  }
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
     LispValue new_relid,old_entry ;
{
  /* XXX - let form, maybe incorrect */
  LispValue new_entry = copy_seq_tree (old_entry);
  /* XXX setf, may be incorrect  */
  rt_relname (new_entry) = get_rel_name (new_relid);
  rt_relid (new_entry) = new_relid;
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
     LispValue root,index,new_entry ;
{
  /* XXX - let form, maybe incorrect */
  LispValue new_root = copy_seq_tree (root);
  /* XXX - setf  */
  rt_fetch (index,root_rangetable (new_root)) = new_entry;
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

/*  .. fix-parsetree-attnums, plan-union-query
 */
LispValue
fix_parsetree_attnums (rt_index,old_relid,new_relid,parsetree)
LispValue rt_index,old_relid,new_relid,parsetree ;
{
if(and (var_p (parsetree),equal (get_varno (parsetree),rt_index))) {
setf (get_varattno (parsetree),get_attnum (new_relid,get_attname (old_relid,get_varattno (parsetree))));

} else if (vectorp (parsetree)) {
dotimes (i (vsize (parsetree)),setf (aref (parsetree,i),fix_parsetree_attnums (rt_index,old_relid,new_relid,aref (parsetree,i))));

} else if (listp (parsetree)) {
dotimes (i (length (parsetree)),setf (nth (i,parsetree),fix_parsetree_attnums (rt_index,old_relid,new_relid,nth (i,parsetree))));

} else if ( true ) ;  /* end-cond */;
parsetree;
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

LispValue
fix_rangetable (rangetable,index,new_entry)
     LispValue rangetable,index,new_entry ;
{
  /* XXX - let form, maybe incorrect */
  LispValue new_rangetable = copy_seq_tree (rangetable);
  /* XXX was setf  */
  rt_fetch (index,new_rangetable) = new_entry;
  return (new_rangetable);

}
