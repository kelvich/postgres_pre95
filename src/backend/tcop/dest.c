/* ----------------------------------------------------------------
 *   FILE
 *	dest.c
 *	
 *   DESCRIPTION
 *	support for various command destinations - see lib/H/tcop/dest.h
 *
 *   INTERFACE ROUTINES
 * 	BeginCommand - prepare destination for tuples of the given type
 * 	EndCommand - tell destination that no more tuples will arrive
 * 	NullCommand - tell dest that the last of a query sequence was processed
 *	
 *   NOTES
 *	These routines do the appropriate work before and after
 *	tuples are returned by a query to keep the backend and the
 *	"destination" portals synchronized.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

RcsId("$Header$");

#include "tcop/dest.h"
#include "utils/log.h"
#include "tmp/portal.h"

#include "nodes/pg_lisp.h"

#include "catalog/pg_type.h"

/* ----------------
 *	initport
 *
 *	XXX change all these "P"'s and "T"'s to use #define constants
 * ----------------
 */
void
initport(name, natts, attinfo)
    char		*name;
    int			natts;
    struct attribute	*attinfo[];
{
    register int	i;
    
    /* ----------------
     *	send fe info on tuples we're about to send
     * ----------------
     */
    pq_putnchar("P", 1);	/* new portal.. */
    pq_putint(0, 4);		/* xid */
    pq_putstr(name);		/* portal name */
    pq_putnchar("T", 1);	/* type info to follow.. */
    pq_putint(natts, 2);	/* number of attributes in tuples */
    
    /* ----------------
     *	send tuple attribute types to fe
     * ----------------
     */
    for (i = 0; i < natts; ++i) {
	pq_putstr(attinfo[i]->attname);	/* if 16 char name oops.. */
	pq_putint((int) attinfo[i]->atttypid, 4);
	pq_putint(attinfo[i]->attlen, 2);
    }
}

/* ----------------
 * 	BeginCommand - prepare destination for tuples of the given type
 * ----------------
 */
void
BeginCommand(pname, attinfo, dest)
    char 	*pname;
    LispValue 	attinfo;
    CommandDest	dest;
{
    struct attribute **attrs;
    int nattrs;

    nattrs = CInteger(CAR(attinfo));
    attrs  = (struct attribute **) CADR(attinfo);

    switch (dest) {
    case Remote:
	pq_flush();
	initport(pname, nattrs, attrs);
	pq_flush();
	break;
	
    case Local:
	break;
	
    case Debug:
	showatts(pname, nattrs, attrs);
	break;
	
    case None:
    default:
	break;
    }
}

/* ----------------
 * 	EndCommand - tell destination that no more tuples will arrive
 * ----------------
 */
void
EndCommand(commandTag, dest)
    String  commandTag;
    CommandDest	dest;
{
    switch (dest) {
    case Remote:
	pq_putnchar("C", 1);
	pq_putint(0, 4);
	pq_putstr(commandTag);
	pq_flush();
	break;

    case Local:
    case Debug:
    case None:
    default:
	break;
    }
}

/* ----------------
 * 	NullCommand - tell dest that the last of a query sequence was processed
 * 
 * 	Necessary to implement the hacky FE/BE interface to handle
 *	multiple-return queries.
 * ----------------
 */
void
NullCommand(dest)
    CommandDest	dest;
{
    switch (dest) {
    case Remote:
	pq_putnchar("I", 1);
	pq_putint(0, 4);
	pq_flush();
	break;

    case Local:
    case Debug:
    case None:
    default:
	break;
    }	
}
