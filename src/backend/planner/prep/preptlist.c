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

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/relation.a.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "tags.h"

#include "parser/parse.h"
#include "utils/lsyscache.h"

#include "planner/internal.h"
#include "planner/preptlist.h"


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

	if (!null(result_relation) && command_type != RETRIEVE) {
		relid = getrelid(CInteger(result_relation), range_table);
	}
     
     if ( integerp (relid) ) {
	  expanded_tlist = 
	    expand_targetlist (tlist,CInteger(relid),
			       command_type,
			       CInteger(result_relation));
     } 
     else {
	  expanded_tlist = tlist;
     } 

     /*    XXX should the fix-opids be this early?? */
     /*    was mapCAR  */
     
     foreach (temp,expanded_tlist) {
	  t_list = nappend1(t_list,fix_opids (CAR(temp)));
      }
     
    /* ------------------
     *  for "replace" or "delete" queries, add ctid of the result
     *  relation into the target list so that the ctid can get
     *  propogate through the execution and in the end ExecReplace()
     *  will find the right tuple to replace or delete.  This
     *  extra field will be removed in ExecReplace().
     *  For convinient, we append this extra field to the end of
     *  the target list.
     * ------------------
     */
    if (command_type == REPLACE || command_type == DELETE) {
       LispValue ctid;
       ctid = lispCons(MakeResdom(length(t_list) + 1,
                                  27,
                                  6,
                                  "ctid",
                                  0,
                                  0,
				  1),  /* set resjunk to 1 */
                        lispCons(MakeVar(CInteger(result_relation),
                                          -1,
                                          27,
                                          LispNil,
					  0,
                                          lispCons(result_relation,
                                                   lispCons(lispInteger(-1),
                                                            LispNil)),
                                         0), LispNil));
        t_list = nappend1(t_list, ctid);
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
    int node_type = -1;
    
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
    
    if(node_type != -1) {
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
     LispValue i = LispNil;
     LispValue matching_old_tl = LispNil;
     LispValue j = LispNil;
     
     foreach (i,new_tlist) {
	 new_tl = CAR(i);
	 matching_old_tl = LispNil;

	 foreach (temp,old_tlist) {
	     old_tl = CAR(temp);
	     if (!strcmp(get_resname (tl_resdom(old_tl)),
			get_resname (tl_resdom(new_tl)))) {
		 matching_old_tl = old_tl;
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
     ObjectId relid;
     Index rt_index;
     NodeTag node_type ;
{
     LispValue attno = LispNil;
     LispValue t_list = LispNil;
     LispValue i = LispNil;
     Name attname = (Name)NULL;
     ObjectId atttype = 0;
     int16 typlen = 0;

     foreach(i, number_list(1,(get_relnatts(relid)))) {
	 attno = CAR(i);
	 attname = get_attname (relid,CInteger(attno));
	 atttype = get_atttype (relid,CInteger(attno));
	 typlen = get_typlen (atttype);

	 switch (node_type) {
	       
	   case T_Const:
	     { 
		 struct varlena *typedefault = get_typdefault (atttype);
		 int temp = 0;
		 Const temp2 = (Const)NULL;
		 TLE temp3 = (TLE)NULL;

		 if ( null (typedefault) ) 
		   temp = 0;
		 else 
		   temp = typlen;
		 
		 temp2 = MakeConst (atttype,temp,
				    typedefault,lispNullp(typedefault));
		 
		 temp3 = MakeTLE (MakeResdom (CInteger(attno),atttype,
					      typlen,
					      attname,0,LispNil,0),
				  temp2);
		 t_list = nappend1(t_list,temp3);
		 break;
	     } 
	   case T_Var:
	     { 
		 Var temp_var = (Var)NULL;
		 LispValue temp_list = LispNil;

		 temp_var = MakeVar (rt_index,
				 CInteger(attno),
				 atttype,
				 LispNil,LispNil,
				 lispCons (lispInteger(rt_index),
					   lispCons(attno,
						    LispNil)), 0);
		 temp_list = MakeTLE (MakeResdom (CInteger(attno),atttype,
					      typlen,
					      attname,0,LispNil,0),
				      temp_var);
		 t_list = nappend1(t_list,temp_list);
		 break;
	     }
	       
	   default: { /* do nothing */
	       break;
	   }
	 }
     }
     return(t_list);
 } /* function end   */
		  

