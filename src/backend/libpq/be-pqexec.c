/* ----------------------------------------------------------------
 *   FILE
 *	be-pqexec.c
 *	
 *   DESCRIPTION
 *	support for executing POSTGRES commands and functions
 *	from a user-defined function in a backend.
 *
 *   INTERFACE ROUTINES
 * 	PQfn 		- call a POSTGRES function
 * 	PQexec 		- execute a POSTGRES query
 *	
 *   NOTES
 *	These routines are compiled into the postgres backend.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId ("$Header$");

#include "tcop/dest.h"
#include "tmp/libpq-be.h"
#include "utils/exception.h"
#include "utils/builtins.h"
#include "utils/log.h"

/* ----------------------------------------------------------------
 *			PQ interface routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	PQfn -  Send a function call to the POSTGRES backend.
 *
 *	fnid		: function id
 * 	result_buf      : pointer to result buffer (&int if integer)
 * 	result_len	: length of return value.
 * 	result_is_int	: If the result is an integer, this must be non-zero
 * 	args		: pointer to a NULL terminated arg array.
 *			  (length, if integer, and result-pointer)
 * 	nargs		: # of arguments in args array.
 * ----------------
 */
char *
PQfn(fnid, result_buf, result_len, result_is_int, args, nargs)
    int fnid;
    int *result_buf;	/* can't use void, dec compiler barfs */
    int result_len;
    int result_is_int;
    PQArgBlock *args;
    int nargs;
{
    PortalEntry *entry = NULL;
    char *pname = NULL;

    return pname;
}

/* ----------------
 *	PQexec -  Send a query to the POSTGRES backend
 *
 * 	The return value is a string.  
 * 	If 0 or more tuples fetched from the backend, return "P portal-name".
 * 	If a query is does not return tuples, return "C query-command".
 * 	If there is an error: return "E error-message".
 *
 *	Note: if we get a serious error or an elog(WARN), then PQexec never
 *	returns because the system longjmp's back to the main loop.
 * ----------------
 */

char *
PQexec(query)
    char *query;
{
    PortalEntry *entry = NULL;
    char *result = NULL;

    /* ----------------
     *	create a new portal and put it on top of the portal stack.
     * ----------------
     */
    entry = (PortalEntry *) be_newportal();
    be_portalpush(entry);
    
    /* ----------------
     *	pg_eval will put the query results in a portal which will
     *  end up on the top of the portal stack.
     * ----------------
     */
    pg_eval(query, Local);
    
    /* ----------------
     *	pop the portal off the portal stack and return the
     *  result.  Note if result is null, we return E.
     * ----------------
     */
    entry = (PortalEntry *) be_portalpop();
    result = entry->result;
    if (result == NULL) {
	char *PQE = "Enull PQexec result";
	result = strcpy(palloc(strlen(PQE)), PQE);
    }
    
    return result;
}

/* ----------------
 *	pqtest takes a text query and returns the number of
 *	tuples it returns.  Note: there is no need to PQclear()
 *	here - the memory will go away at end transaction.
 * ----------------
 */
int
pqtest(vlena)
    struct varlena	*vlena;
{
    PortalBuffer *a;
    char 	 *q;
    char 	 *res;
    int 	 t;

    /* ----------------
     *	get the query text and
     *	execute the postgres query
     * ----------------
     */
    q = textout(vlena);
    res = PQexec(q);

    /* ----------------
     *	return number of tuples in portal or 0 if command returns no tuples.
     * ----------------
     */
    t = 0;
    switch(res[0]) {
    case 'P':
	a = PQparray(&res[1]);
	if (a == NULL)
	    elog(WARN, "pqtest: PQparray could not find portal %s", res);
	
	t = PQntuples(a);
	break;
    case 'C':
	break;
    default:
	elog(NOTICE, "pqtest: PQexec(%s) returns %s", q, res);
	break;
    }
    
    return t;
}
