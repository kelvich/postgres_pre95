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
#include "parser/parse.h"
#include "utils/log.h"
#include "tmp/portal.h"

#include "nodes/pg_lisp.h"

#include "catalog/pg_type.h"

/* ----------------
 *	output functions
 * ----------------
 */
void
donothing(tuple, attrdesc)
    List tuple;
    List attrdesc;
{
}

extern void debugtup();
extern void printtup();

/* ----------------
 * 	DestToFunction - return the proper "output" function for dest
 * ----------------
 */
void
(*DestToFunction(dest))()
    CommandDest	dest;
{
    switch (dest) {
    case Remote:
	return printtup;
	break;
	
    case Local:
	return donothing;
	break;
	
    case Debug:
	return debugtup;
	break;
	
    case None:
    default:
	return donothing;
	break;
    }	
}

/* ----------------
 * 	BeginCommand - prepare destination for tuples of the given type
 * ----------------
 */
void
BeginCommand(pname, attinfo, operation, isInto, dest)
    char 	*pname;
    LispValue 	attinfo;
    int		operation;
    bool	isInto;
    CommandDest	dest;
{
    struct attribute **attrs;
    int natts;
    int i;

    natts = CInteger(CAR(attinfo));
    attrs  = (struct attribute **) CADR(attinfo);

    switch (dest) {
    case Remote:
	/* ----------------
	 *	send fe info on tuples we're about to send
	 * ----------------
	 */
	pq_flush();
	pq_putnchar("P", 1);	/* new portal.. */
	pq_putint(0, 4);	/* xid */
	pq_putstr(pname);	/* portal name */
	
	/* ----------------
	 *	if this is a retrieve, then we send back the tuple
	 *	descriptor of the tuples.  "retrieve into" is an
	 *	exception because no tuples are returned in that case.
	 * ----------------
	 */
	if (operation == RETRIEVE && !isInto) {
	    pq_putnchar("T", 1);	/* type info to follow.. */
	    pq_putint(natts, 2);	/* number of attributes in tuples */
    
	    for (i = 0; i < natts; ++i) {
		pq_putstr(attrs[i]->attname);	/* if 16 char name oops.. */
		pq_putint((int) attrs[i]->atttypid, 4);
		pq_putint(attrs[i]->attlen, 2);
	    }
	}
	pq_flush();
	break;
	
    case Local:
	break;
	
    case Debug:
	/* ----------------
	 *	show the return type of the tuples
	 * ----------------
	 */
	showatts(pname, natts, attrs);
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
	/* ----------------
	 *	tell the fe that the query is over
	 * ----------------
	 */
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
	/* ----------------
	 *	tell the fe that the last of the queries has finished
	 * ----------------
	 */
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
