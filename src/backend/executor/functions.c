/* ------------------------------------------------
 *   FILE
 *     functions.c
 *
 *   DESCRIPTION
 *	Routines to handle functions called from the executor
 *      Putting this stuff in fmgr makes the postmaster a mess....
 *
 *
 *   IDENTIFICATION
 *   	$Header$
 * ------------------------------------------------
 */

#include "utils/fmgr.h"
#include "utils/fcache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_language.h"
#include "catalog/syscache.h"

#include "rules/params.h"

#include "utils/log.h"


char *postquel_lang_func_call_array(procedureId,pronargs,args)
     ObjectId procedureId;
     int	pronargs;
     char *args[];    
{
    List query_descriptor = LispNil, qd = LispNil;
    HeapTuple   procedureTuple;
    ParamListInfo paramlist;
    char *plan_str;
    int status,x;
    Datum *value;
    Boolean *isnull;
    

    plan_str = (char *)
	SearchSysCacheGetAttribute(PROOID,Anum_pg_proc_prosrc, procedureId);
    qd = StringToPlanWithParams(textout(plan_str),&paramlist);
    x=0; 
    while(paramlist[x].kind != PARAM_INVALID) {
	paramlist[x].value = (Datum) args[x];
	x++;
    }
    if (prs2RunOnePlanAndGetValue(qd,paramlist, NULL, &value, &isnull))
	return (char *) value;
    else return (char *)NULL;
}
char *postquel_lang_func_call(procedureId,pronargs,values)
     ObjectId procedureId;
     int	pronargs;
     FmgrValues	values;
{


    
}

postquel_lang_func_call_array_fcache() {}


char *
ExecCallFunction(fcache,args)
     FunctionCachePtr fcache;
     char * args[];
{
	func_ptr user_fn;
	int true_arguments;
	char *returnValue;
	bool  isNull;

	switch (fcache->language) {
	case INTERNALlanguageId:
	case ClanguageId:
	    return
		c_lang_func_call_ptr_array(fcache->func,fcache->nargs,args, &isNull);
	    break;
	case POSTQUELlanguageId:
	    return postquel_lang_func_call_array_fcache(fcache);
	    break;
	default:
	    elog(WARN,
		 "No function caller for arrays args registered for language");
	}
}


