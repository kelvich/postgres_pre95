/*     
 *      FILE
 *     	storeplan
 *     
 *      DESCRIPTION
 *     	Routines to store and retrieve plans
 *     
 */
#include "c.h"
#include "postgres.h"

RcsId("$Header$");

/*     
 *      EXPORTS
 *     		plan-save
 *     		plan-load
 *     		plan-to-string
 *     		string-to-plan
 *     		find-parameters
 *     		substitute-parameters
 */

/*
 * provide ("storeplan");
 * require ("internal");
 * #ifdef and (opus43,pg_production)
 * declare (localf (pg_out,pg_in,find_all_parameters,assoc_params))
 * #endif
 * ;
 */ 
#include "planner/internal.h"
#include "pg_lisp.h"
#include "nodes.h"

extern LispValue	string_to_plan();
extern char		*plan_to_string();
extern LispValue	pg_out();
/*    
 *    	plan-save
 *    	plan-load
 *    
 *    	Public disk file interface.
 *    
 */

plan_save (plan, path)
     LispValue plan;
     char *path;
{
#ifdef NOTYET
    int fd;
    char *planstring;
    
    fd = fopen(path,"w+");
    planstring = plan_to_string(plan);
    
    write(fd,strlen(planstring),4);
    write(fd,planstring,strlen(planstring));
    
    fclose(fd);
#endif /* NOTYET */
}

LispValue
plan_load (filename)
	LispValue filename;
{
#ifdef NOTYET   
    int fd;
    int planstring_length;
    char *planstring;
    LispValue plan;
  
    fd = fopen(path,"r+");

    read(fd,&planstring_length,4);
    planstring = malloc(planstring_length);
    
    read(fd,planstring,planstring_length);
    plan = string_to_plan(planstring);

    return(plan);
#endif NOTYET
}

/*    
 *    	string-to-plan
 *    	plan-to-string
 *    
 *    	Public string interface.
 *    
 */

/*  .. plan-load
 */
LispValue
string_to_plan (string)
	LispValue string;
{
	LispValue parsePlanString();
#ifdef NOTYET
	pg_in (parsePlanString (string));
#endif NOTYET
}

/*
 * Reads a plan out of string.
 */

LispValue read_from_string(string)

char *string;

{
	extern LispValue lispReadString();

	return(lispReadString(string));
}


/*  .. plan-save
 */
char 
*plan_to_string (plan)
     LispValue plan;
{
#ifdef NOTYET
    static char foo[8192];
    
    pg_out (plan,0,sprintf,foo);
#endif NOTYET
}


pg_out (lv,iscdr,printfunc,string)
     LispValue lv;
     int iscdr;
     void (*printfunc)();
     char *string;
{
#ifdef NOTYET
    pid = getpid();
    static char filestring[20];
    static bool is_first_time = true;

    if (is_first_time == true) 
      sprintf(pidstring,"/tmp/plan.%d",pid);

    planfd = freopen(pidstring,"w+",stdout);
    
    lispDisplay(lv,iscdr);
    
    planfd = fopen("stdout","rw",stdout);
#endif NOTYET
}

/*   will read plans many times */

/*
 *  Mao says:  don't need these anymore.  Only place that _node_types_
 *	       was used was in this file, but we now have other ways of
 *	       determining whether a LispValue is a node.
 *
 * defvar (_node_types_, QUOTE append,const,existential,func,
 *			indexscan,mergesort,nestloop,oper,param,resdom,
 *			result,seqscan,sort,var,rule_lock,);
 */
/*
 * .. pg-in, string-to-plan
 */
LispValue
pg_in (tree)
	LispValue tree ;
{
	return(tree);
    /* Greg gets to fill in the goodies here */
	/* What else is needed here */
}

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
    return ( remove_duplicates (find_all_parameters (plan)) );
}


/*  .. find-all-parameters, find-parameters
 */
LispValue
find_all_parameters (tree)
     LispValue tree ;
{
#ifdef NOTYET
    LispValue retval = LispNil;

    if(param_p (tree)) {
	return (lispCons (get_paramid (tree), LispNil );
    } else if (consp (tree)) {
	foreach (i,tree) {
	    retval = nconc ( retval, find_all_parameters(tree));
	}
	return(retval);
    } else if (vectorp (tree)) {
	LispValue plan_list = LispNil;

	for (i = 0 ; i < vsize (tree ) ; i ++ ) {
	    plan_list = nconc ( find_all_parameters ( tree[i] ),
			       plan_list );
	}
	return (plan_list );
    }
#endif
}

/*    
 *    	substitute-parameters
 *    
 *    	Find param nodes in a structure and change them to constants, as
 *    	specified by an list of parameter ids and values.
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
#ifdef NOTYET
    if( null (plan)) {
	return(LispNil);
    } else if (null (params)) {
	return(plan);
    } else {
	LispValue param_alist;
	
	foreach (i,params) {
	    LispValue param = CAR(i);
	    
	    lispCons (CAR(param),
		      make_const ( CADR(param),
				  get_typlen (CADR(param)),
				  CADDR(param),
				  LispNil ));
	}
	return ( assoc_params (plan,param_alist) );
    }
#endif
}


/*    
 *    	assoc-params
 *    
 *    	Substitutes the const nodes generated by param-alist into the plan.
 *    
 *    	Returns a new copy of the plan data structure with the parameters
 *    	substituted in.
 *    
 */

/*  .. assoc-params, substitute-parameters
 */
LispValue
assoc_params (plan,param_alist)
     LispValue plan,param_alist ;
{
#ifdef NOTYET
    LispValue retval = LispNil;

    if(param_p (plan)) {
	x = CDR ( assoc (get_paramid (plan),param_alist));
	if ( x ) 
	  return ( x) ;
	else 
	  return (plan);
    } else if (consp (plan)) {
	foreach (i,plan) {
	    retval = nappend1 ( retval ,assoc_params( CAR(i) , param_alist ));
	}
	return(retval);
    } else if ( vectorp (plan)) {
	LispValue vector_size = vsize (plan);
	LispValue *newplan = malloc (sizeof( LispValue ) * vector_size );

	for ( i  = 0 ; i < vector_size; i++ ) {
	    newplan[i] = assoc_params ( plan[i], param_alist);
	}

	vsetprop (newplan,vprop (plan));
	return (newplan);
    }

    return (plan);
#endif
}
