/* ----------------------------------------------------------------
 *   FILE
 *	dest.h
 *	
 *   DESCRIPTION
 *	Whenever the backend is submitted a query, the results
 *	have to go someplace - either to the standard output,
 *	to a local portal buffer or to a remote portal buffer.
 *
 *    -	stdout is the destination only when we are running a
 *	backend without a postmaster and are returning results
 *	back to the user.
 *
 *    -	a local portal buffer is the destination when a backend
 *	executes a user-defined function which calls PQexec() or
 *	PQfn().  In this case, the results are collected into a
 *	PortalBuffer which the user's function may diddle with.
 *
 *    -	a remote portal buffer is the destination when we are
 *	running a backend with a frontend and the frontend executes
 *	PQexec() or PQfn().  In this case, the results are sent
 *	to the frontend via the pq_ functions.
 *
 *    - None is the destination when the system executes
 *	a query internally.  This is not used now but it may be
 *	useful for the parallel optimiser/executor.
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef DestIncluded
#define DestIncluded

/* ----------------
 *	CommandDest is used to allow the results of calling
 *	pg_eval() to go to the right place.
 * ----------------
 */
typedef enum {
    None,		/* results are discarded */
    Debug,		/* results go to debugging output */
    Local,		/* results go in local portal buffer */
    Remote,		/* results sent to frontend process */
	CopyBegin,	/* results sent to frontend process but are strings */
	CopyEnd		/* results sent to frontend process but are strings */
} CommandDest;

/* dest.c */
void donothing ARGS((List tuple , List attrdesc ));
void EndCommand ARGS((String commandTag , CommandDest dest ));
void SendCopyBegin ARGS((void ));
void ReceiveCopyBegin ARGS((void ));
void NullCommand ARGS((CommandDest dest ));
void BeginCommand ARGS((char *pname , int operation , LispValue attinfo , bool isIntoRel , bool isIntoPortal , char *tag , CommandDest dest ));
#endif  DestIncluded
