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
#include "fmgr.h"
#include "utils/log.h"

#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"

#include "tmp/fastpath.h"

/* ----------------
 *	SendFunctionResult
 * ----------------
 */
static void
SendFunctionResult ( fid, retval, retsize )
     int fid;
     char *retval;
     int retsize;
{
    char *retdata;

#if 0    
    printf("Sending results %d, %d, %d",fid,retval,retsize);
#endif    
    
    pq_putnchar("V",1);
    pq_putint(fid,4);
    
    switch (retsize) {

      case 0: 			/* void retval */
        break;
      case PORTAL_RESULT:
	break;

      /*
       *  XXX 10/27/92:  mao says this is bogus, and predicts that this code
       *  never gets executed.  retdata may contain nulls.  the caller always
       *  passes the real length of the return value, and never -1, to this
       *  routine.
       */

      case VAR_LENGTH_RESULT:
	pq_putnchar("G", 1);
	pq_putint(retsize, 4);
	pq_putstr(retdata);
	break;

      case 1: case 2: case 4:
	pq_putnchar("G",1);
	pq_putint(retsize,4);
	pq_putint(retval, retsize);
	break;

      default:
	pq_putnchar("G",1);
	pq_putint(VARSIZE(retval)-4,4);
	pq_putnchar(retval+4, VARSIZE(retval)-4);
    }

    pq_putnchar("0", 1);
    pq_flush();
}

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
    ObjectId		fid;
    char		function_name[MAX_FUNC_NAME_LENGTH+1];
    HeapTuple		htp;
    ObjectId		prorettype;
    bool		typbyval;
    Count		nargs;
    char		*arg[8];
    char		*retval;
    Size		retsize;
    int			i;
    uint32		palloced;
    char		*p;

    (void) pq_getint(4);		/* xactid */
    fid = (ObjectId) pq_getint(4);	/* function oid */
    retsize = (Size) pq_getint(4);	/* size of return value */
    nargs = (Count) pq_getint(4);	/* # of arguments */

    /*
     *  Copy arguments into arg vector.  If we palloc() an argument, we need
     *  to remember, so that we pfree() it after the call.
     */
    palloced = 0x0;
    for (i = 0; i < 8; ++i) {
	if (i >= nargs) {
	    arg[i] = (char *) NULL;
	} else {
	    Size argsize = (Size) pq_getint(4);
	    
	    if (argsize == VAR_LENGTH_ARG) {	/* string */
		if (!(p = palloc(MAX_STRING_LENGTH)))
		    elog(WARN, "HandleFunctionRequest: palloc failed");
		palloced |= (1 << i);
		pq_getstr(p, MAX_STRING_LENGTH);
		arg[i] = p;
	    } else if (argsize > 4)  {		/* by-reference */
		if (!(p = palloc(argsize + sizeof(VARSIZE(p)))))
		    elog(WARN, "HandleFunctionRequest: palloc failed");
		palloced |= (1 << i);
		VARSIZE(p) = argsize + sizeof(VARSIZE(p));
		pq_getnchar(VARDATA(p), 0, argsize);
		arg[i] = p;
	    } else {				/* by-value */
		arg[i] = (char *) pq_getint(argsize);
	    }
	}
    }

    if (retsize == PORTAL_RESULT) {
	/* fake it out by putting the stuff out first */
	pq_putnchar("V", 1);
	pq_putint(0, 4);
    }

    /*
     *  Figure out the return type for this function, and remember whether
     *  it's pass-by-value or pass-by-reference.  Pass-by-ref values need
     *  to be pfree'd before we exit this routine.
     */
    htp = SearchSysCacheTuple(PROOID, (char *) fid,
			      (char *) NULL, (char *) NULL, (char *) NULL);
    if (!HeapTupleIsValid(htp))
	elog(WARN, "HandleFunctionRequest: procedure id %d does not exist",
	     fid);
    prorettype = ((Form_pg_proc) GETSTRUCT(htp))->prorettype;
    htp = SearchSysCacheTuple(TYPOID, (char *) prorettype,
			      (char *) NULL, (char *) NULL, (char *) NULL);
    if (!HeapTupleIsValid(htp))
	elog(WARN, "HandleFunctionRequest: return type %d for procedure %d does not exist",
	     prorettype, fid);
    typbyval = ((Form_pg_type) GETSTRUCT(htp))->typbyval;
    
    retval = fmgr(fid,
		  arg[0], arg[1], arg[2], arg[3],
		  arg[4], arg[5], arg[6], arg[7]);

    /* free palloc'ed arguments */
    for (i = 0; i < nargs; ++i) {
	if (palloced & (1 << i))
	    pfree(arg[i]);
    }

    /*
     *  If this is an ordinary query (not a retrieve portal p ...), then
     *  we return the data to the user.  If the return value was palloc'ed,
     *  then it must also be freed.
     */
    if (retsize != PORTAL_RESULT) {
	SendFunctionResult(fid, retval, retsize);
	if (!typbyval)
	    pfree(retval);
    }
}
