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
/* extern LispValue	pg_out(); */
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
	extern LispValue lispReadString();

	return(lispReadString(string));

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

