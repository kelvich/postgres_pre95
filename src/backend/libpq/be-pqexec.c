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
 * 	If there is an error: return "E error-message".
 * 	If tuples are fetched from the backend, return "P portal-name".
 * 	If a query is executed successfully but no tuples fetched, 
 * 	return "C query-command".
 * ----------------
 */

char *
PQexec(query)
    char *query;
{
    PortalEntry *entry = NULL;
    char *pname = NULL;

    /* ----------------
     *	pg_eval will put the query results in a portal which will
     *  end up on the top of the portal stack.
     * ----------------
     */
    pg_eval(query, Local);
    
    /* ----------------
     *	pop the portal off the portal stack and return it's name.
     * ----------------
     */
    entry = (PortalEntry *) be_portalpop();
    pname = entry->name;

    return pname;
}

/* ----------------
 *	pqtest takes a text query and returns the number of
 *	tuples it returns.
 * ----------------
 */
int
pqtest(vlena)
    struct varlena	*vlena;
{
    char *pname;
    PortalBuffer *a;
    char *q;
    int g, k, t;

    q = textout(vlena);
    
    pname = PQexec(q);
    a = PQparray(pname);
    g = PQngroups(a);
    
    t = 0;
    for (k=0; k < g-1; k++) {
	t += PQntuplesGroup(a,k);
    }

    return t;
}
