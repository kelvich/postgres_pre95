/* ----------------------------------------------------------------
 *   FILE
 *	fastpath.c
 *	
 *   DESCRIPTION
 * 	routines to handle function requests from the frontend
 *
 *   INTERFACE ROUTINES
 *	HandleFunctionRequest
 *
 *   NOTES
 *	This cruft is the server side of PQfn.
 *
 *	The VAR_LENGTH_{ARGS,RESULT} stuff is limited to MAX_STRING_LENGTH
 *	(see src/backend/tmp/fastpath.h) for no obvious reason.  Since its
 *	primary use (for us) is for Inversion path names, it should probably
 *	be increased to 256 (MAXPATHLEN for Inversion, hidden in pg_type
 *	as well as utils/adt/filename.c).
 *
 *	Quoth PMA on 08/15/93:
 *
 *	This code has been almost completely rewritten with an eye to
 *	keeping it as compatible as possible with the previous (broken)
 *	implementation.
 *
 *	The previous implementation would assume (1) that any value of
 *	length <= 4 bytes was passed-by-value, and that any other value
 *	was a struct varlena (by-reference).  There was NO way to pass a
 *	fixed-length by-reference argument (like char16) or a struct
 *	varlena of size <= 4 bytes.
 *	
 *	The new implementation checks the catalogs to determine whether
 *	a value is by-value (type "0" is null-delimited character string,
 *	as it is for, e.g., the parser).  The only other item obtained
 *	from the catalogs is whether or not the value should be placed in
 *	a struct varlena or not.  Otherwise, the size given by the
 *	frontend is assumed to be correct (probably a bad decision, but
 *	we do strange things in the name of compatibility).
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
#include "utils/builtins.h"	/* for oideq */
#include "tmp/fastpath.h"
#include "tmp/libpq.h"

#include "access/xact.h"	/* for TransactionId/CommandId protos */

#include "catalog/syscache.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"


/* ----------------
 *	SendPortalResult
 * ----------------
 */
static void
SendPortalResult()
{
    pq_putnchar("V", 1);
    pq_putint(0, 4);		/* bogus function id */
}


/* ----------------
 *	SendFunctionResult
 * ----------------
 */
static void
SendFunctionResult(fid, retval, retbyval, retlen, retsize)
    ObjectId fid;	/* function id */
    char *retval;	/* actual return value */
    bool retbyval;
    int retlen;		/* the length according to the catalogs */
    int retsize;	/* whatever the frontend passed */
{
    pq_putnchar("V", 1);
    pq_putint(fid, 4);
    
    switch (retsize) {
    case 0: 			/* void retval */
    case PORTAL_RESULT:
        break;
	
    case VAR_LENGTH_RESULT:	/* string */
	/*
	 *  XXX 10/27/92:  mao says this is bogus, and predicts that this code
	 *  never gets executed.  retdata may contain nulls.  the caller always
	 *  passes the real length of the return value, and never -1, to this
	 *  routine.
	 *  XXX 08/15/93:  pma says mao's probably right about it not being
	 *  used much, since it was passing an uninitialized char * to
	 *  pq_putstr before today. :-)   However, there's nothing too
	 *  unreasonable about returning a null-delimited string with
	 *  known bounded length (just like VAR_LENGTH_ARGS) -- for example,
	 *  a function returning a pathname.  We can't just turn this off
	 *  since the user (frontend code) is the one who might specify
	 *  VAR_LENGTH_RESULT...
	 */
	pq_putnchar("G", 1);
	pq_putint(VAR_LENGTH_RESULT, 4);
	pq_putstr(retval);
	break;

    default:
	pq_putnchar("G", 1);
	if (retbyval) {		/* by-value */
	    pq_putint(retlen, 4);
	    pq_putint(retval, retlen);
	} else {		/* by-reference ... */
	    if (retlen < 0) {		/* ... varlena */
		pq_putint(VARSIZE(retval) - VARHDRSZ, 4);
		pq_putnchar(VARDATA(retval), VARSIZE(retval) - VARHDRSZ);
	    } else {			/* ... fixed */
		/* XXX cross our fingers and trust "retsize" */
		pq_putint(retsize, 4);
		pq_putnchar(retval, retsize);
	    }
	}
    }

    pq_putnchar("0", 1);
    pq_flush();
}

/*
 * This structure saves enough state so that one can avoid having to
 * do catalog lookups over and over again.  (Each RPC can require up
 * to MAXFMGRARGS+2 lookups, which is quite tedious.)
 *
 * The previous incarnation of this code just assumed that any argument
 * of size <= 4 was by value; this is not correct.  There is no cheap
 * way to determine function argument length etc.; one must simply pay
 * the price of catalog lookups.
 */
struct fp_info {
    ObjectId		funcid;
    int			nargs;
    bool		argbyval[MAXFMGRARGS];
    int32		arglen[MAXFMGRARGS];	/* signed (for varlena) */
    bool		retbyval;
    int32		retlen;			/* signed (for varlena) */
    TransactionId	xid;
    CommandId		cid;
};

/*
 * We implement one-back caching here.  If we need to do more, we can.
 * Most routines in tight loops (like PQfswrite -> F_LOWRITE) will do
 * the same thing repeatedly.
 */
static struct fp_info last_fp = { InvalidObjectId };

/*
 * valid_fp_info
 *
 * RETURNS:
 *	1 if the state in 'fip' is valid
 *	0 otherwise
 *
 * "valid" means:
 * The saved state was either uninitialized, for another function,
 * or from a previous command.  (Commands can do updates, which
 * may invalidate catalog entries for subsequent commands.  This
 * is overly pessimistic but since there is no smarter invalidation
 * scheme...).
 */
static
int
valid_fp_info(func_id, fip)
    ObjectId func_id;
    struct fp_info *fip;
{
    Assert(ObjectIdIsValid(func_id));
    Assert(fip != (struct fp_info *) NULL);
    
    return(ObjectIdIsValid(fip->funcid) &&
	   oideq(func_id, fip->funcid) &&
	   TransactionIdIsCurrentTransactionId(fip->xid) &&
	   CommandIdIsCurrentCommandId(fip->cid));
}

/*
 * update_fp_info
 *
 * Performs catalog lookups to load a struct fp_info 'fip' for the
 * function 'func_id'.
 *
 * RETURNS:
 *	The correct information in 'fip'.  Sets 'fip->funcid' to
 *	InvalidObjectId if an exception occurs.
 */
static
void
update_fp_info(func_id, fip)
    ObjectId func_id;
    struct fp_info *fip;
{
    oid8		*argtypes;
    ObjectId		rettype;
    HeapTuple		func_htp, type_htp;
    Form_pg_type	tp;
    Form_pg_proc	pp;
    int			i;
    
    Assert(ObjectIdIsValid(func_id));
    Assert(fip != (struct fp_info *) NULL);

    /*
     * Since the validity of this structure is determined by whether
     * the funcid is OK, we clear the funcid here.  It must not be
     * set to the correct value until we are about to return with
     * a good struct fp_info, since we can be interrupted (i.e., with
     * an elog(WARN, ...)) at any time.
     */
    bzero((char *) fip, (int) sizeof(struct fp_info));
    fip->funcid = InvalidObjectId;

    func_htp = SearchSysCacheTuple(PROOID, (char *) func_id,
				   (char *) NULL, (char *) NULL,
				   (char *) NULL);
    if (!HeapTupleIsValid(func_htp)) {
	elog(WARN, "update_fp_info: cache lookup for function %d failed",
	     func_id);
	/*NOTREACHED*/
    }
    pp = (Form_pg_proc) GETSTRUCT(func_htp);
    fip->nargs = pp->pronargs;
    rettype = pp->prorettype;
    argtypes = &(pp->proargtypes);

    for (i = 0; i < fip->nargs; ++i) {
	if (ObjectIdIsValid(argtypes->data[i])) {
	    type_htp = SearchSysCacheTuple(TYPOID, (char *) argtypes->data[i],
					   (char *) NULL, (char *) NULL,
					   (char *) NULL);
	    if (!HeapTupleIsValid(type_htp)) {
		elog(WARN, "update_fp_info: bad argument type %d for %d",
		     argtypes->data[i], func_id);
		/*NOTREACHED*/
	    }
	    tp = (Form_pg_type) GETSTRUCT(type_htp);
	    fip->argbyval[i] = tp->typbyval;
	    fip->arglen[i] = tp->typlen;
	} /* else it had better be VAR_LENGTH_ARG */
    }

    if (ObjectIdIsValid(rettype)) {
	type_htp = SearchSysCacheTuple(TYPOID, (char *) rettype,
				       (char *) NULL, (char *) NULL,
				       (char *) NULL);
	if (!HeapTupleIsValid(type_htp)) {
	    elog(WARN, "update_fp_info: bad return type %d for %d",
		 rettype, func_id);
	    /*NOTREACHED*/
	}
	tp = (Form_pg_type) GETSTRUCT(type_htp);
	fip->retbyval = tp->typbyval;
	fip->retlen = tp->typlen;
    } /* else it had better by VAR_LENGTH_RESULT */

    fip->xid = GetCurrentTransactionId();
    fip->cid = GetCurrentCommandId();

    /*
     * This must be last!
     */
    fip->funcid = func_id;
}
	

/*
 * HandleFunctionRequest
 *
 * Server side of PQfn (fastpath function calls from the frontend).
 * This corresponds to the libpq protocol symbol "F".
 *
 * RETURNS:
 *	nothing of significance.
 *	All errors result in elog(WARN,...).
 */
int
HandleFunctionRequest()
{
    ObjectId		fid;
    int			argsize, retsize;
    Count		nargs;
    char		*arg[8];
    char		*retval;
    int			i;
    uint32		palloced;
    char		*p;
    struct fp_info	*fip;

    (void) pq_getint(4);		/* xactid */
    fid = (ObjectId) pq_getint(4);	/* function oid */
    retsize = pq_getint(4);		/* size of return value */
    nargs = (Count) pq_getint(4);	/* # of arguments */

    /*
     * This is where the one-back caching is done.
     * If you want to save more state, make this a loop around an array.
     */
    fip = &last_fp;
    if (!valid_fp_info(fid, fip)) {
	update_fp_info(fid, fip);
    }

    if (fip->nargs != nargs) {
	elog(WARN, "HandleFunctionRequest: actual arguments (%d) != registered arguments (%d)",
	     nargs, fip->nargs);
	/*NOTREACHED*/
    }

    /*
     *  Copy arguments into arg vector.  If we palloc() an argument, we need
     *  to remember, so that we pfree() it after the call.
     */
    palloced = 0x0;
    for (i = 0; i < 8; ++i) {
	if (i >= nargs) {
	    arg[i] = (char *) NULL;
	} else {
	    argsize = pq_getint(4);
	    
	    if (argsize == VAR_LENGTH_ARG) {	/* string */
		/*
		 * This is a special interface that should be deprecated
		 * and removed.  There is no particular reason why you
		 * can't just pass the actual string length from the
		 * frontend instead of this magic value.
		 */
		if (!(p = palloc(MAX_STRING_LENGTH))) {
		    elog(WARN, "HandleFunctionRequest: palloc failed");
		    /*NOTREACHED*/
		}
		palloced |= (1 << i);
		pq_getstr(p, MAX_STRING_LENGTH);
		arg[i] = p;
	    } else {
		Assert(argsize > 0);
		if (fip->argbyval[i]) {		/* by-value */
		    Assert(argsize <= 4);
		    arg[i] = (char *) pq_getint(argsize);
		} else {			/* by-reference ... */
		    if (fip->arglen[i] < 0) {		/* ... varlena */
			if (!(p = palloc(argsize + VARHDRSZ))) {
			    elog(WARN, "HandleFunctionRequest: palloc failed");
			    /*NOTREACHED*/
			}
			VARSIZE(p) = argsize + VARHDRSZ;
			pq_getnchar(VARDATA(p), 0, argsize);
		    } else {				/* ... fixed */
			/* XXX cross our fingers and trust "argsize" */
			if (!(p = palloc(argsize))) {
			    elog(WARN, "HandleFunctionRequest: palloc failed");
			    /*NOTREACHED*/
			}
			pq_getnchar(p, 0, argsize);
		    }
		    palloced |= (1 << i);
		    arg[i] = p;
		}
	    }
	}
    }

    /*
     *	Old comment:
     *		fake it out by putting the stuff out first
     *	I have no idea what that is supposed to mean.   Evidently it 
     *  is a bad idea to move this down to where SendFunctionResult is.
     *  -- PMA 08/15/93
     */
    if (retsize == PORTAL_RESULT) {
	SendPortalResult();
    }

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
	SendFunctionResult(fid, retval, fip->retbyval, fip->retlen, retsize);
	if (!fip->retbyval)
	    pfree(retval);
    }
    return(0);
}
