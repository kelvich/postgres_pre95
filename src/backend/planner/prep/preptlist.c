/*     
 *      FILE
 *     	preptlist
 *     
 *      DESCRIPTION
 *     	Routines to preprocess the parse tree target list
 *     
 */

/*  RcsId ("$Header$");  */

/*     
 *      EXPORTS
 *     		preprocess-targetlist
 */

#include "planner/internal.h"
#include "pg_lisp.h"
#include "planner/preptlist.h"
#include "lsyscache.h"
#include "relation.h"
#include "relation.a.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "tags.h"
#include "parse.h"

extern LispValue number_list();  /* XXX should #include temp.h in l-lisp */

/*    
 *    	preprocess-targetlist
 *    
 *    	Driver for preprocessing the parse tree targetlist.
 *    
 *    	1. Deal with appends and replaces by filling missing attributes
 *            in the target list.
 *    	2. Reset operator OIDs to the appropriate regproc ids.
 *    
 *    	Returns the new targetlist.
 *    
 */

/*  .. init-query-planner   */

List
preprocess_targetlist (tlist,command_type,result_relation,range_table)
     List tlist;
     int command_type;
     LispValue result_relation;
     List range_table ;
{
     List expanded_tlist = NULL;
     LispValue relid = LispNil;
     LispValue t_list = LispNil;
     LispValue temp = LispNil;

     if ( result_relation )
	  relid = getrelid (result_relation,range_table);
     
     if ( integerp (relid) ) {
	  expanded_tlist = 
	    expand_targetlist (tlist,relid,command_type,result_relation);
     } 
     else {
	  expanded_tlist = tlist;
     } 

     /*    XXX should the fix-opids be this early?? */
     /*    was mapCAR  */
     
     foreach (temp,expanded_tlist) {
	  t_list = nappend1(t_list,fix_opids (CAR(temp)));
     }

     return(t_list);
}  /* function end  */

/*     	====================
 *     	TARGETLIST EXPANSION
 *     	====================
 */

/*    
 *    	expand-targetlist
 *    
 *    	Given a target list as generated by the parser and a result relation,
 *    	add targetlist entries for the attributes which have not been used.
 *    
 *    	XXX This code is only supposed to work with unnested relations.
 *    
 *    	'tlist' is the original target list
 *    	'relid' is the relid of the result relation
 *    	'command' is the update command
 *    
 *    	Returns the expanded target list, sorted in resno order.
 *    
 */

/*  .. preprocess-targetlist    */

LispValue
expand_targetlist (tlist,relid,command_type,result_relation)
     List tlist;
     LispValue relid;
     int command_type;
     LispValue result_relation ;
{
    int node_type;
    
    switch (command_type) {
      case APPEND : 
	{ 
	    node_type = T_Const;
	}
	break;
	
       case REPLACE : 
	 { 
	     node_type = T_Var;
	 }
	break;
    } 
    
    if(node_type) {
	  return (replace_matching_resname (new_relation_targetlist 
					    (relid,
					     result_relation,
					     node_type),
					    tlist));
	  
      } 
    else 
       return (tlist);
    
}  /* function end   */


LispValue
replace_matching_resname (new_tlist,old_tlist)
     LispValue new_tlist,old_tlist ;
{
     /*  lisp mapCAR    */
     LispValue new_tl = LispNil;
     LispValue temp = LispNil;
     LispValue old_tl = LispNil; 
     LispValue t_list = LispNil;

     foreach (new_tl,new_tlist) {
	  /* XXX - let form, maybe incorrect */
	  LispValue matching_old_tl = LispNil;
     
	  foreach (temp,old_tlist) {
	       if (equal (get_resname (tl_resdom(old_tl))),
			 (get_resname (tl_resdom(new_tl)))) {
		 matching_old_tl = LispTrue;
		 break;
	    }
	  }
	  
	  if(matching_old_tl) {
	       set_resno (tl_resdom (matching_old_tl),
			  get_resno (tl_resdom (new_tl)));
	      t_list = nappend1(t_list,matching_old_tl);
	  } 
	  else {
	       t_list = nappend1(t_list,new_tl);
	  } 
     }
     return (t_list);
}  /* function end   */

/*    
 *    	new-relation-targetlist
 *    
 *    	Generate a targetlist for the relation with relation OID 'relid'
 *    	and rangetable index 'rt-index'.
 *    
 *    	Returns the new targetlist.
 *    
 */

/*  .. expand-targetlist     */

LispValue
new_relation_targetlist (relid,rt_index,node_type)
     LispValue relid,rt_index;
     NodeTag node_type ;
{
     LispValue attno = LispNil;
     LispValue t_list = LispNil;

     foreach(attno, number_list(1,(get_relnatts(relid)))) {
	  
	  Name attname = get_attname (relid,attno);
	  ObjectId atttype = get_atttype (relid,attno);
	  int16 typlen = get_typlen (atttype);


	  switch (node_type) {
	       
	     case T_Const:
		 { 
		      struct varlena *typedefault = get_typdefault (atttype);
		      int temp ;
		      Const temp2;
		      TLE temp3;

		      if ( null (typedefault) ) 
			temp = 0;
		      else 
			temp = typlen;

		      temp2 = MakeConst (atttype,temp,
					   typedefault,lispNullp(typedefault));

		      temp3 = MakeTLE (MakeResdom (attno,atttype,
						    typlen,attname,0,LispNil),
				       temp2);
		      t_list = nappend1(t_list,temp3);
		      break;
		   } 
	       case T_Var:
		 { 
		      Var temp;
		      temp = MakeVar (rt_index,attno,atttype,
				       LispNil,LispNil,
				       lispCons (rt_index,
						 lispCons(attno,LispNil)));
		      t_list = nappend1(t_list,temp);
		      break;
		 }
	       
	     default: { /* do nothing */
		  break;
	     }
	  }
     }
     return(t_list);
} /* function end   */
		  

