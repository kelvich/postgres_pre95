/*     
 *      FILE
 *     	storeplan
 *     
 *      DESCRIPTION
 *     	Routines to manipulate param nodes in rule plans.
 *     
 */
#include "tmp/postgres.h"

#include "nodes/nodes.h"
#include "nodes/pg_lisp.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"

#include "utils/lsyscache.h"
#include "planner/paramutils.h"


/* $Header$ */

/*
 * EXPORTS
 * find_parameters
 * substitute_parameters
 */

/*
 *    	find-parameters
 *    
 *    	Extracts all paramids from a plan tree.
 *    
 */

LispValue
  find_parameters (plan)
LispValue plan ;
{
    return (remove_duplicates (find_all_parameters (plan)));
}

/*  .. find-all-parameters, find-parameters   */

LispValue
  find_all_parameters (tree)
LispValue tree ;
{
    if(IsA(tree,Param)) 
	return (lispCons(lispString(get_paramname(tree)),
			 LispNil));
    else 
      if (consp (tree)) {
	  /* MAPCAN  */
	  LispValue t_list = LispNil;
	  LispValue temp = LispNil;
	  
	  foreach (temp, tree) 
	    t_list = nconc(t_list, find_all_parameters(CAR(temp)));
	  return(t_list);
      }
    return(NULL);
  /*
   * XXX Vector plans are not yet supported.  

   else if (vectorp (tree)) {
   LispValue plan_list = LispNil;
   dotimes (i (vsize (tree),plan_list),
   plan_list = nconc (find_all_parameters (aref (tree,i)),
   plan_list););
   
   */
}
 
/*
 *    	substitute-parameters
 *    
 *    	Find param nodes in a structure and change them to constants, as
 *    	specified by a list of parameter ids and values.
 *    
 *    	'plan' is a query plan (or part of a query plan) which (may) contain
 *    		param nodes.
 *    	'params' is a list of values to substitute for parameters in the form:
 *    		(fixnum consttype constvalue)		for $1 params
 *    		("attname" consttype constvalue) 	for $.attname params
 *    
 *    	Returns a new copy of the plan data structure with the parameters
 *    	substituted in.
 *    
 */
LispValue
  substitute_parameters (plan,params)
LispValue plan,params ;
{
    if (null (plan)) 
      return(LispNil);
    else 
      if (null (params)) 
	return (plan);
    else {
	LispValue temp = LispNil;
	LispValue tmpnode = LispNil;
	LispValue param_alist = LispNil;
	LispValue paramnode = LispNil;

	foreach(temp, params) {
	    paramnode = CAR(temp);
	    tmpnode = lispCons(CAR(paramnode),  /* XXX is this right? */
			       MakeConst(CInteger(CDR(paramnode)),
					 get_typlen(CInteger(CDR(paramnode))),
					 CInteger(CDR(CDR(paramnode))),
					 LispNil));
	    param_alist = nappend1(param_alist,tmpnode);
	}
	return(assoc_params (plan,param_alist));
    }
}  /* function end  */


/*
 *    	assoc-params
 *    
 *    	Substitutes the const nodes generated by param-alist into the plan.
 *    
 *    	Returns a new copy of the plan data structure with the parameters
 *    	substituted in.
 *    
 */

/*  .. assoc-params, substitute-parameters  */
 
LispValue
  assoc_params (plan,param_alist)
LispValue plan,param_alist ;
{
    if(IsA(plan,Param)) {
	LispValue tmp = LispNil;
	
	/*  ASSOC  */
	foreach (tmp,param_alist) {
	    if (equal(CAR(CAR(tmp)), get_paramname(plan)))
	      return(CDR(CAR(tmp)));
	}
	return(plan);
    } else if (consp (plan)) {
	LispValue t_list = LispNil;
	LispValue temp = LispNil;
	
	foreach(temp,plan) 
	  t_list = nappend1(t_list, assoc_params(CAR(temp),
						param_alist));
	return(t_list);
    }
    else
      return(plan);

    /*
     *  XXX Vector plans not supported yet!

     else if (vectorp (plan)) {
     LispValue vector_size = vsize (plan);
     LispValue newplan = new_vector (vector_size);
     dotimes (i (vector_size),setf (aref (newplan,i),assoc_params (aref (plan,i),param_alist)));
     vsetprop (newplan,vprop (plan));
     newplan;
     
     */

}  /* function end */     
  
   
