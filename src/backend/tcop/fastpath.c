/* ----------------------------------------------------------------
 *   FILE
 *	fastpath.c
 *	
 *   DESCRIPTION
 * 	routines to handle function requests from the frontend
 *
 *   INTERFACE ROUTINES
 *	
 *   NOTES
 *	this protocol was hacked up quickly - should decide this
 *	and then synchronize code in lib/libpq/{fe,be}-pqexec.c 
 *
 *	error condition may be asserted by fmgr code and
 *   	an "elog" propagated to the frontend independent of the
 *   	code in this module
 *
 *      for no good reason, function names are limited to 80 chars
 *      by this module.
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) tcopdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "tcop/tcopdebug.h"

#include "utils/palloc.h"
#include "utils/fmgr.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/pg_type.h"

#include "tmp/fastpath.h"

/* ----------------
 *	external_to_internal
 * ----------------
 */
static int
external_to_internal( string )
     char *string;
{
    return((int)string);
}

/* ----------------
 *	internal_to_external
 * ----------------
 */
static char * 
internal_to_external( string )
     char *string;
{
    return(string);
}

/* ----------------
 *	SendFunctionResult
 * ----------------
 */
static void
SendFunctionResult ( fid, retval, rettype )
     int fid;
     char *retval;
     int rettype;
{
    char *retdata;

#if 0    
    printf("Sending results %d, %d, %d",fid,retval,rettype);
#endif    
    
    pq_putnchar("V",1);
    pq_putint(fid,4);
    
    switch (rettype) {

      case 0: 			/* void retval */
        break;
      case PORTAL_RESULT:
	break;

      case VAR_LENGTH_RESULT:
	retdata = internal_to_external(retval, MAX_STRING_LENGTH, 0);
	pq_putnchar("G", 1);
	pq_putint(rettype, 4);
	pq_putstr(retdata);
	break;

      case 1: case 2: case 4:
	pq_putnchar("G",1);
	pq_putint(rettype,4);
	pq_putint(retval, rettype);
	break;

      default:
	pq_putnchar("G",1);
	pq_putint(VARSIZE(retval),4);
	pq_putnchar(retval+4, VARSIZE(rettype));
    }
    pq_putnchar("0", 1);
    pq_flush();
} /* SendFunctionResult */

/* 
    {
	Datum sendproc;
	bool attr_is_null;

	tuple = SearchSysCacheTuple (TYPOID,rettype);
	if (HeapTupleIsValid(tuple)) {


	    sendproc = HeapTupleGetAttributeValue(tuple,
						  InvalidBuffer,
						  13,
						  tupledesc,
						  attr_is_null);
						  
	    if ( RegProcedureIsValid ( sendproc )  == false )
	      elog(WARN,"invalid sending procedure");

    } */

/* ---------------
 * 
 * HandleFunctionRequest
 * - externally callable routine for handling function requests
 * 
 * Desc : This code is called only when the backend recieves
 *        a function request (current protocol is "F") we then
 *        handle the parameters of the request here.
 *        Requests come in 2 forms, by name, and by id.
 *        If requests come by id, we just call the fmgr with the id
 *        and the required number of arguments.
 *        If requests come by name, we do a catalog lookup to determine
 *        the id, which we subsequently return to the frontend so that
 *        it may cache it , and subsequent calls to the same function
 *        can be made by id.
 *
 *        XXX - note that if the catalog changes in the meantime, 
 *              the function id cannot be invalidated here, and the
 *              frontend must take care of it by itself  ( - jeff)
 *	  XXX - someday, maybe we should standardize on an RPC 
 *		mechanism that everyone else uses
 *
 *        If any error condition exists, we terminate with an elog
 *        which causes an "E" followed by the message to be sent to the
 *        frontend.  
 *
 * BUGS:  for no particular reasion, function names are limited to 80
 *        characters, and VARLENGTHARGS has a max of 100 (inherited from
 *	  the lisp backend)
 * CAVEAT:the function_by_name code was given to me by Mike H, I am taking
 *        it on faith since it looks ok & I don't have the time to verify it. 
 *
 * ----------------
 */

int
HandleFunctionRequest()
{
    int xactid;
    int fid;
    char function_name[MAX_FUNC_NAME_LENGTH+1];
    char *retval;		/* XXX - should be datum, maybe ? */
    int  arg[8];
    int  arg_length[8];
    int  rettype;
    int  nargs;
    int  i;

    xactid = pq_getint(4);
    fid = pq_getint(4);

    /* XXX FUNCTION_BY_NAME is unsupported by PQfn() in fe-pqexec.c -cim */
    
    if ( fid == FUNCTION_BY_NAME ) { 	
	HeapTuple	tuple;

	pq_getstr(function_name, MAX_FUNC_NAME_LENGTH);
	tuple = SearchSysCacheTuple(PRONAME, function_name);
	if (!HeapTupleIsValid(tuple)) {
	    elog(WARN, "Function name lookup failed for \"%s\"",
		 function_name);
	}
	fid = tuple->t_oid;
    } 

    rettype = pq_getint(4);
    nargs = pq_getint(4);

    /* copy arguments into arg[] */
    
    for (i = 0 ; i < nargs ; i++ ) {
	arg_length[i] = pq_getint(4);
	
	if (arg_length[i] == VAR_LENGTH_ARG ) {
	    char *data = (char *) palloc(MAX_STRING_LENGTH);
	    pq_getstr(data,MAX_STRING_LENGTH);
	    arg[i] = external_to_internal(data,MAX_STRING_LENGTH,PASS_BY_REF);
	} else if (arg_length[i] > 4)  {
	    char *vl;
	    elog(WARN,"arg_length of %dth argument too long",i);
	    vl = palloc(arg_length[i]+4);
	    VARSIZE(vl) = arg_length[i];
	    pq_getnchar(VARDATA(vl),0,arg_length[i]);
	    arg[i] = (int)vl;
	} else {
	    arg[i] = pq_getint ( arg_length[i]);
	}
    }

    /* lookup rettype in type catalog, and check send function */

    if (rettype == PORTAL_RESULT) {
	/* fake it out by putting the stuff out first */
	pq_putnchar("V",1);
	pq_putint(0,4);
    }

#if 0    
    printf("\n arguments are %d %d %d \n",fid,arg[0],arg[1]);
    printf("rettype = %d\n",rettype);
#endif 
    
    retval = fmgr (fid, arg[0],arg[1],arg[2],arg[3],
		   arg[4],arg[5],arg[6],arg[7] );

    if (rettype != PORTAL_RESULT)
	SendFunctionResult ( fid, retval, rettype );

}
