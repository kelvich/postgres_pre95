/* ----------------------------------------------------------------
 *   FILE
 *	fe-pqexec.c
 *	
 *   DESCRIPTION
 *	support for executing POSTGRES commands and functions
 *	from a frontend application.
 *
 *   SUPPORT ROUTINES
 *	pq_global_init, read_remark, EstablishComm, process_portal
 *
 *   INTERFACE ROUTINES
 *	PQdb 		- Return the current database being accessed. 
 *	PQsetdb 	- Make the specified database the current database. 
 *	PQreset 	- Reset the communication port with the backend. 
 *	PQfinish 	- Close communication ports with the backend. 
 *	PQFlushI 	- Used for flushing out "poll" queries by the monitor.
 *
 * >>	PQfn 		- Send a function call to the POSTGRES backend.
 * >>	PQexec 		- Send a query to the POSTGRES backend
 *	PQfsread,
 *	PQfswrite	- Special versions of PQfn intended for use by
 *			  the Inversion routines p_read/p_write.
 *
 *   NOTES
 *	These routines are NOT compiled into the postgres backend,
 *	rather they end up in libpq.a.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/c.h"

#include "tmp/simplelists.h"
#include "tmp/libpq-fe.h"
#include "tmp/fastpath.h"
#include "libpq/auth.h"
#include "tmp/postgres.h"
#include "fmgr.h"
#include "utils/exc.h"

#include <pwd.h>

RcsId ("$Header$");

/* ----------------------------------------------------------------
 * Declare Global Variables.  
 *
 * 	The Values of the first four varialbes can be changed by setting
 *	the approriate environment variables.
 *
 *	PQhost:		PGHOST
 *	PQdatabase:	PGDATABASE
 *	PQport:		PGPORT
 *	PQtty:		PGTTY
 * ----------------------------------------------------------------
 */

#define DefaultHost	"localhost"
#define DefaultTty	"/dev/null"
#define DefaultOption	""

char 	*PQhost;		/* the machine on which POSTGRES backend 
				   is running */
char 	*PQport = NULL;		/* the communication port with the 
				   POSTGRES backend */
char	*PQtty;			/* the tty on PQhost backend message 
				   is displayed */
char	*PQoption;		/* the optional arguments to POSTGRES 
				   backend */
char 	PQdatabase[17] = {0};	/* the POSTGRES database to access */
int 	PQportset = 0;		/* 1 if the communication with backend
				   is set */
int	PQxactid = 0;		/* the transaction id of the current 
				   transaction */

/* ----------------------------------------------------------------
 *			PQ utility routines
 * ----------------------------------------------------------------
 */

/*
 * pq_username
 */
static
char *
pq_username()
{
    static char userbuf[17];
    char *user;
    struct passwd *pw;

    /*
     * this is not a security thing.  we check USER first because people
     * dork with USER as a convenience...
     */
    if (!(user = getenv("USER")) &&
	(!(pw = getpwuid(getuid())) || !(user = pw->pw_name))) {
	return((char *) NULL);
    }
    (void) strncpy(userbuf, user, 16);
    userbuf[16] = '\0';
    return(userbuf);
}

/*
 * pq_print_elog
 *	print a backend error message ("E" or "N").
 *
 *	if a function name 'where' is given, prepend that to the backend
 *	message.
 *
 *	elog() sends the xact id followed by the null-delimited string.
 *	some routines (like PQexec) have already read the xact id.
 *	if 'readxid' is true, we've already read the transaction id.
 *	if not, we need to read it from the input stream since it 
 *	precedes the actual error message.
 *
 * RETURNS:
 *	0 on success, -1 on failure.  failure generally indicates a fatal
 *	backend error.
 */
pq_print_elog(where, readxid)
    char *where;
    bool readxid;
{
    char errbuf[error_msg_length];
    int len = error_msg_length;
    
    /*
     * XXX we throw away the xact id here.. but then, elog sends garbage
     * anyway.  (see utils/error/elog.c)
     */
    if ((!readxid && pq_getint(sizeof(int)) == EOF) ||
	pq_getstr(errbuf, error_msg_length) == EOF) {
	(void) strcpy(PQerrormsg,
		      "FATAL: pq_print_elog: unexpected end of connection\n");
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	PQreset();
	return(-1);
    }
    
#define	WHERE_MSG ": detected at "

    (void) strncpy(PQerrormsg, errbuf, len);
    len -= strlen(errbuf);
    if (where) {
	(void) strncat(PQerrormsg, WHERE_MSG, len);
	len -= strlen(WHERE_MSG);
	(void) strncat(PQerrormsg, where, len);
	len -= strlen(where);
	(void) strncat(PQerrormsg, "\n", len);
    }
    fputs(PQerrormsg, stderr);
    pqdebug("%s", PQerrormsg);
    return(0);
}

#define INVALID_BE_ID	'?'

/*
 * pq_read_id
 *	read a single-character protocol identifier.
 *
 * RETURNS:
 *	EOF on success if EOF is reached
 *	0 on success if EOF is not reached
 *	1 on error (e.g., the backend appears to have died)
 */
int
pq_read_id(id, where)
    char *id;
    char *where;
{
    int at_eof = 0;
    unsigned len = error_msg_length;
    
    Assert(id);
    Assert(where);
    
    id[0] = INVALID_BE_ID;
    if (pq_getnchar(id, 0, 1) == EOF) {
	at_eof = EOF;
	if (id[0] == INVALID_BE_ID) {

#define DIE_MSG "FATAL: no response from backend: detected in "

	    /*
	     * we got zero bytes back from the read.
	     * this is the world's stupidest way to detect this, forced
	     * upon us by the crufty pq_getnchar interface.
	     */
	    (void) strncpy(PQerrormsg, DIE_MSG, len);
	    len -= strlen(DIE_MSG);
	    (void) strncat(PQerrormsg, where, len);
	    len -= strlen(where);
	    (void) strncat(PQerrormsg, "\n", len);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    return(1);
	}
    }
    return(at_eof);
}
    
/* ----------------
 * pq_global_init
 * 	If the global variables do not have values yet, read in the values
 *	from the environment variables.  If the environment variable does
 *	not have a value, use the default value (where applicable).
 *
 * RETURNS:
 *	0 on success, -1 on (fatal) error.
 * ----------------
 */

static
pq_global_init()
{
    char *tmpname;

    if (PQdatabase[0] == '\0') {
	if (!(tmpname = getenv("PGDATABASE")) &&
	    !(tmpname = fe_getauthname())) {
	    (void) strcpy(PQerrormsg, 
			  "FATAL: pq_global_init: unable to determine a database name!\n");
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    return(-1);
	}
	PQdatabase[16] = '\0';
	strncpy(PQdatabase, tmpname, 16);
    }
    
    if (!PQhost) {
	if (!(tmpname = getenv("PGHOST"))) {
	    tmpname = DefaultHost;
	}
	if (!(PQhost = malloc(strlen(tmpname) + 1))) {
	    (void) strcpy(PQerrormsg,
			  "FATAL: pq_global_init: unable to allocate memory!\n");
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    return(-1);
	}
	(void) strcpy(PQhost, tmpname);
    }
    
    if (!PQtty) {
	if (!(tmpname = getenv("PGTTY"))) {
	    tmpname = DefaultTty;
	}
	if (!(PQtty = malloc(strlen(tmpname) + 1))) {
	    (void) strcpy(PQerrormsg,
			  "FATAL: pq_global_init: unable to allocate memory!\n");
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    return(-1);
	}
	(void) strcpy(PQtty, tmpname);
    }

    /*
     *  As of release 4, PQoption is no longer taken from the environment
     *  var PGOPTION if it's not already defined.  This change was made
     *  in order to be consistent with the documentation and to make frew
     *  happy.
     *
     *  In general, users shouldn't be screwing around with PQoption, in
     *  any case.
     */
    if (!PQoption) {
	PQoption = DefaultOption;
    }

    if (!PQport) {
	if (!(tmpname = getenv("PGPORT"))) {
	    tmpname = POSTPORT;
	}
	if (!(PQport = malloc(strlen(tmpname) + 1))) {
	    (void) strcpy(PQerrormsg,
			  "FATAL: pq_global_init: unable to allocate memory!\n");
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    return(-1);
	}
	(void) strcpy(PQport, tmpname);
    }
    
    bzero((char *) PQerrormsg, sizeof(PQerrormsg));

    return(0);
}

/* ----------------
 * read_remark
 *	Read and discard remarks, then print pending NOTICEs.
 *
 *	XXX I can't find any place in the backend side of the protocol
 *	where "R" remarks are sent anymore.  we probably need this to
 *	clear any remaining elog(NOTICE) messages, though.  -pma 05/28/93
 * ----------------
 */
void
read_remark(id)
    char id[];
{
    char errbuf[error_msg_length];

    Assert(id);

    while (id[0] == 'R') {
	pq_getstr(errbuf, remark_length);
	if (pq_getnchar(id, 0, 1) == EOF)
	    return;
    }
    while (id[0] == 'N') {
	if (pq_print_elog((char *) NULL, false) < 0 ||
	    pq_getnchar(id, 0, 1) == EOF)
	    return;
    }
}

/* ----------------
 * EstablishComm
 *	Establishes a connection to a backend through the postmaster.
 *
 * RETURNS:
 *	0 on success, -1 on (fatal) error.
 * ----------------
 */
static
EstablishComm()
{
    extern void PQufs_init();

    if (!PQportset) { 
	if (pq_global_init() < 0) {
	    return(-1);
	}
	if (pq_connect(PQdatabase,
		       fe_getauthname(),
		       PQoption,
		       PQhost,
		       PQtty,
		       (char *) NULL,
		       (short) atoi(PQport)) == -1 ) {
	    (void) sprintf(PQerrormsg, 
			   "FATAL: Failed to connect to postmaster (host=%s, port=%s)\n",
			   PQhost, PQport);
	    pqdebug("%s", PQerrormsg);
	    (void) strcat(PQerrormsg, "\tIs the postmaster running?\n");
	    fputs(PQerrormsg, stderr);
	    return(-1);
	}
	pq_flush();
	PQufs_init();
	PQportset = 1;
    }
    return(0);
}

/* ----------------
 * process_portal
 * 	Process portal queries. 
 *
 * RETURNS:
 * 	The same values as PQexec().
 * ----------------
 */
static
char *
process_portal(rule_p)
    int rule_p;
{
    char pname[portal_name_length];
    char id;
    char command[command_length];
    static char retbuf[portal_name_length + 1];

    /* Read in the portal name. */
    pq_getstr(pname, portal_name_length);
    pqdebug("Portal name = %s", pname);

    /*
     * This for loop is necessary so that NOTICES out of portal processing
     * stuff are handled properly.
     */

    for (;;) {
        /* Read in the identifier following the portal name. */
	if (pq_read_id(&id, "process_portal") > 0) {
	    (void) strcpy(retbuf, "E");
	    return(retbuf);
	}
        read_remark(&id);
        pqdebug("Identifier is: %x", (char *) id);

        switch (id) {
        case 'E':
	    if (pq_print_elog((char *) NULL, false) < 0) {
		(void) strcpy(retbuf, "E");
		return(retbuf);
	    }
	    (void) strcpy(retbuf, "R");
	    return(retbuf);
        case 'N':
	    /*
	     * print the NOTICE and go back to processing return values.
	     * If we get an EOF (i.e. the backend quickdies) return "E" to
	     * the application.
	     */
	    if (pq_print_elog((char *) NULL, false) < 0) {
		(void) strcpy(retbuf, "E");
		return(retbuf);
	    }
	    break;
        case 'T':
	    /* Tuples are returned, dump data into a portal buffer. */
	    if (dump_data(pname, rule_p) == -1) {
		(void) strcpy(retbuf, "R");
		return(retbuf);
	    }
	    sprintf(retbuf, "P%s", pname);
	    return(retbuf);
	    /* Pending data inquiry - return nothing */
        case 'C':
	    /*
	     * Portal query command (e.g., retrieve, close),
	     * no tuple returned.
	     */
	    PQxactid = pq_getint(4);
	    pqdebug("process_portal: Transaction Id is: %d",
		    (char *) PQxactid);
	    pq_getstr(command, command_length);
	    pqdebug("process_portal: Query command: %s", command);

	    /* Process the portal commands. */
	    if (strcmp(command, "retrieve") == 0) {
	        pbuf_setup(pname);
		(void) strcpy(retbuf, "Cretrieve");
	    } else {
	        sprintf(retbuf, "C%s", command);
	    }
	    return(retbuf);
	case 'A':
	    /* I have no reason to believe that this will ever happen
	     * But just in case...
	     *   -- jw, 1/7/94
	     */	    
	    {
		char relname[NAMEDATALEN+1];
		extern int PQAsyncNotifyWaiting;
		
		PQAsyncNotifyWaiting = 1;
		pq_getstr(relname,NAMEDATALEN);
		relname[NAMEDATALEN] = '\0';
		pqdebug2("Asynchronous notification encountered. (%s, %d)",
			 relname, PQxactid);
		PQappendNotify(relname, PQxactid);
	    }
	    break;
        default:
	    sprintf(PQerrormsg,
		    "FATAL: process_portal: protocol error: id=%x\n",
		    id);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    (void) strcpy(retbuf, "E");
	    return(retbuf);
	}
    }
}

/* ----------------------------------------------------------------
 *			PQ interface routines
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	PQdb - Return the current database being accessed. 
 * --------------------------------
 */
char *
PQdb()
{
    return(PQdatabase);
}

/* ----------------
 *	PQsetdb - Make the specified database the current database. 
 * ----------------
 */
void
PQsetdb(dbname)
    char *dbname;
{
    PQreset();
    PQdatabase[16] = '\0';
    strncpy(PQdatabase, dbname, 16);
}

/* ----------------
 *	PQreset - Reset the communication port with the backend. 
 * ----------------
 */
void
PQreset()
{
    pq_close();
    PQportset = 0;
}

/* ----------------
 *	PQfinish - Close communication ports with the backend. 
 * ----------------
 */
void
PQfinish()
{
    if (!PQportset)
	return;

    pq_putnchar("X", 1);	/* exiting */
    pq_flush();
    pq_close();

    PQportset = 0;
}

/* ----------------
 *	PQFlushI - Used for flushing out "poll" queries by the monitor.
 * ----------------
 */
int
PQFlushI(i_count)
    int i_count;
{
    int i;
    char id;

    for (i = 0; i < i_count; i++) {
	if (pq_read_id(&id, "PQFlushI") > 0)
	    return(-1);
	if (id != 'I') {
	    (void) strcpy(PQerrormsg,
			  "FATAL: PQFlushI: read bad protocol entity\n");
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return(-1);
	}
	pq_getint(4); /* throw this away */
    }
    return(0);
}

/* ----------------
 *	PQfn -  Send a function call to the POSTGRES backend.
 *
 *	fnid		: function id
 * 	result_buf      : pointer to result buffer (&int if integer)
 * 	result_len	: length of return value.
 *      actual_result_len: actual length returned. (differs from result_len
 *			  for varlena structures.)
 *      result_type     : If the result is an integer, this must be 1,
 *                        If result is opaque, this must be 2.
 * 	args		: pointer to a NULL terminated arg array.
 *			  (length, if integer, and result-pointer)
 * 	nargs		: # of arguments in args array.
 *
 * RETURNS
 *	NULL on failure.  PQerrormsg will be set.
 *	"G" if there is a return value.
 *	"V" if there is no return value.
 * ----------------
 */
char *
PQfn(fnid, result_buf, result_len, actual_result_len,
     result_type, args, nargs)
    int fnid;
    int *result_buf;
    int result_len;
    int *actual_result_len;
    int result_type;
    PQArgBlock *args;
    int nargs;
{
    char id;
    char command[command_length];
    int  actual_len;
    short i;
    char retbuf[command_length + 1];

    if (!PQportset && EstablishComm() < 0) {
	return((char *) NULL);
    }

    pq_putnchar("F", 1);	/*	function		*/
    pq_putint(PQxactid, 4);	/*	transaction id ?	*/
    pq_putint(fnid, 4);		/*	function id		*/
    pq_putint(result_len, 4);	/*	length of return value  */
    pq_putint(nargs, 4);	/*	# of args		*/

    for (i = 0; i < nargs; ++i) { /*	len.int4 + contents	*/
	pq_putint(args[i].len, 4);
	if (args[i].isint) {
	    pq_putint(args[i].u.integer, 4);
	} else if (args[i].len == VAR_LENGTH_ARG) {
            pq_putstr((char *)args[i].u.ptr);
	} else {
	    pq_putnchar((char *)args[i].u.ptr, args[i].len);
	}
    }

    pq_flush();

    /* process return value from the backend	*/
    if (pq_read_id(&id, "PQfn") > 0)
	return((char *) NULL);
    if (id == 'E') {
	(void) pq_print_elog((char *) NULL, false);
	return ((char *) NULL);
    }

    read_remark(&id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %x", (char *) id);

    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", (char *) PQxactid);

    if (id == 'V') {
	if (pq_read_id(&id, "PQfn") > 0)
	    return((char *) NULL);
    }
    for (;;) {
	switch (id) {
	case 'G':		/* simple return value	*/
	    actual_len = pq_getint(4);
	    pqdebug2("LENGTH act/usr %ld/%ld\n",
		     (char *) actual_len,
		     (char *) result_len);
	    if ((actual_len != VAR_LENGTH_RESULT) &&
                (actual_len < 0 || actual_len > result_len)) {
		(void) sprintf(PQerrormsg,
			       "FATAL: PQfn: bogus return value (expected size %d, actually %d)\n",
			       result_len, actual_len);
		fputs(PQerrormsg, stderr);
		pqdebug("%s", PQerrormsg);
		PQreset();
		return((char *) NULL);
	    }
	    if (result_type == 1) {
		*((int *)result_buf) = pq_getint(4);
	    } else if (actual_len == VAR_LENGTH_RESULT) {
		pq_getstr((char *) result_buf, MAX_STRING_LENGTH);
	    } else {
		pq_getnchar((char *) result_buf, 0, actual_len);
	    }
	    if (actual_result_len != NULL)
		*actual_result_len = actual_len;
	    if ((result_type != 2) && /* not a binary result */
		(actual_len != result_len)) /* if wouldn't overflow the buf */
		((char *)result_buf)[actual_len] = 0; /* add a \0 */
	    if (pq_read_id(&id, "PQfn") > 0)
		return((char *) NULL);
	    (void) strcpy(retbuf, "G");
	    return(retbuf);
	case 'E':
	    (void) pq_print_elog((char *) NULL, false);
	    return((char *) NULL);
	case 'N':
	    /* print notice and go back to processing return values */
	    (void) pq_print_elog((char *) NULL, false);
	    (void) pq_getnchar(&id, 0, 1);
	    if (pq_read_id(&id, "PQfn") > 0)
		return((char *) NULL);
	    break;
	case '0':		/* no return value */
	    (void) strcpy(retbuf, "V");
	    return(retbuf);
	default:
	    /* The backend violates the protocol. */
	    (void) sprintf(PQerrormsg,
			   "FATAL: PQfn: protocol error: id=%x\n",
			   id);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return((char *) NULL);
	}
    }
    /*NOTREACHED*/
}

/*
 *  PQfsread, PQfswrite -- special-purpose versions of PQfn for file
 *			   system (POSTGRES large object) read and
 *			   write routines.
 *
 *	We need these special versions because the user expects a standard
 *	unix file system interface, and postgres wants to use varlenas
 *	all over the place.
 *
 *	These are NOT intended for direct use by users.  Use the
 *	documented interfaces (p_read and p_write) instead.
 */
int
PQfsread(fd, buf, nbytes)
    int fd;
    char *buf;
    int nbytes;
{
    int fnid;
    char id;
    char command[command_length];
    int  actual_len;
    short i;

    if (!PQportset && EstablishComm() < 0) {
	return(-1);
    }

    pq_putnchar("F", 1);	/* function */
    pq_putint(PQxactid, 4);	/* transaction id? */
    pq_putint(F_LOREAD, 4);	/* function id */

    /* size of return value -- += sizeof(int) because we expect a varlena */
    pq_putint(nbytes + sizeof(int), 4);

    pq_putint(2, 4);		/* nargs */

    /* now put arguments */
    pq_putint(4, 4);		/* length of fd */
    pq_putint(fd, 4);

    pq_putint(4, 4);		/* length of nbytes */
    pq_putint(nbytes, 4);

    pq_flush();

    /* process return value from the backend */
    if (pq_read_id(&id, "PQfsread") > 0)
	return(-1);
    if (id == 'E') {
	(void) pq_print_elog((char *) NULL, false);
	return(-1);
    }

    read_remark(&id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %x", (char *) id);

    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", (char *) PQxactid);

    if (id == 'V')
	if (pq_read_id(&id, "PQfsread") > 0)
	    return(-1);
    for (;;) {
	switch (id) {
	case 'G':
	    nbytes = actual_len = pq_getint(4);
	    if (nbytes > 0)
		pq_getnchar((char *) buf, 0, nbytes);
	    if (pq_read_id(&id, "PQfsread") > 0)
		return(-1);
	    return(nbytes);
	case 'E':
	    (void) pq_print_elog((char *) NULL, false);
	    return(-1);
	case 'N':
	    /* print notice and go back to processing return values */
	    (void) pq_print_elog((char *) NULL, false);
	    if (pq_read_id(&id, "PQfsread") > 0)
		return(-1);
	    break;
	default:
	    /* The backend violates the protocol. */
	    sprintf(PQerrormsg,
		    "FATAL: PQfsread: protocol error: id=%x\n",
		    id);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return(-1);
	}
    }
}

int
PQfswrite(fd, buf, nbytes)
    int fd;
    char *buf;
    int nbytes;
{
    int fnid;
    char id;
    char command[command_length];
    int  actual_len;
    short i;

    if (!PQportset && EstablishComm() < 0) {
	return(-1);
    }

    pq_putnchar("F", 1);	/*	function		*/
    pq_putint(PQxactid, 4);	/*	transaction id ?	*/
    pq_putint(F_LOWRITE, 4);	/*	function id		*/
    pq_putint(4, 4);		/*	length of return value  */
    pq_putint(2, 4);		/*	# of args		*/

    /* now put arguments */
    pq_putint(4, 4);		/* size of fd */
    pq_putint(fd, 4);
    /*
     * The method of transmitting varlenas is:
     * Send vl_len-4
     * Send data consisting of vl_len-4 bytes.
     */
    pq_putint(nbytes, 4);	/* size of varlena */
#if 0
    /* The fe/be protocol does NOT transmit varlenas this way */
    pq_putint(nbytes + 4, 4);	/* vl_len */
#endif
    pq_putnchar(buf, nbytes);	/* vl_dat */

    pq_flush();

    /* process return value from the backend */
    if (pq_read_id(&id, "PQfswrite") > 0)
	return(-1);
    if (id == 'E') {
	(void) pq_print_elog((char *) NULL, false);
	return(-1);
    }

    read_remark(&id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %x", (char *) id);

    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", (char *) PQxactid);

    if (id == 'V')
	if (pq_read_id(&id, "PQfswrite") > 0)
	    return(-1);
    for (;;) {
	switch (id) {
	case 'G':		/* simple return value */
	    actual_len = pq_getint(4);
	    if (actual_len != 4)
		libpq_raise(&ProtocolError,
			    form("wanted 4 bytes in PQfswrite, got %d\n",
				 actual_len));
	    nbytes = pq_getint(4);
	    if (pq_read_id(&id, "PQfswrite") > 0)
		return(-1);
	    return(nbytes);
	case 'E':
	    (void) pq_print_elog((char *) NULL, false);
	    return(-1);
	case 'N':
	    /* print notice and go back to processing return values */
	    (void) pq_print_elog((char *) NULL, false);
	    if (pq_read_id(&id, "PQfswrite") > 0)
		return(-1);
	    break;
	default:
	    /* The backend violates the protocol. */
	    sprintf(PQerrormsg,
		    "FATAL: PQfswrite: protocol error: id=%x\n",
		    id);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return(-1);
	}
    }
}

/* ----------------
 *	PQexec -  Send a query to the POSTGRES backend.
 *
 * RETURNS:
 *	"E" on a fatal error (e.g., backend died).  A LIBPQ error
 *		message is stored in PQerrormsg.
 *	"R" on a non-fatal error (elog(WARN)).  The backend error 
 *		message is stored in PQerrormsg.
 *	"P<portal-name>" if tuples are available.
 *	"C<query-command>" if the command was successful but no tuples
 *		were returned.
 *	"BCOPY" if a "copy from" command was executed and the backend is
 *		sending data.
 *	"DCOPY" if a "copy to" command was executed and the backend is
 *		expecting data.
 * ----------------
 */
char *
PQexec(query)
    char *query;
{
    char id;
    char command[command_length];
    static char retbuf[command_length+1];

    if (!PQportset && EstablishComm() < 0) {
	(void) strcpy(retbuf, "E");
	return(retbuf);
    }

    /* Send a query to backend. */
    pq_putnchar("Q", 1);
    pq_putint(PQxactid, 4);
    pq_putstr(query);
    pqdebug("The query sent to the backend: %s", query);
    pq_flush();

    /* forever (or at least until we get something besides a notice) */
    for (;;) {  
    	/*
	 * Process return values from the backend. 
	 * The code in this function is the implementation of
	 * the communication protocol.
	 */
    	/* Get the identifier. */
	if (pq_read_id(&id, "PQexec") > 0) {
	    (void) strcpy(retbuf, "E");
	    return(retbuf);
	}
    	read_remark(&id);
    	pqdebug("The Identifier is: %x", (char *) id);

    	/* Read in the transaction id. */
    	PQxactid = pq_getint(4);
    	pqdebug("The Transaction Id is: %d", (char *) PQxactid);

    	switch (id) {
    	case 'I':
	    (void) strcpy(retbuf, "I");
	    return(retbuf);
    	case 'E':
	    if (pq_print_elog((char *) NULL, true) < 0) {
		(void) strcpy(retbuf, "E");
		return(retbuf);
	    }
	    (void) strcpy(retbuf, "R");
	    return(retbuf);
    	case 'N':
	    if (pq_print_elog((char *) NULL, true) < 0) {
		(void) strcpy(retbuf, "E");
		return(retbuf);
	    }
	    break;
    	case 'A':
	    {
		char relname[NAMEDATALEN+1];
		extern int PQAsyncNotifyWaiting;
		
		PQAsyncNotifyWaiting = 1;
		pq_getstr(relname,NAMEDATALEN);
		relname[NAMEDATALEN] = '\0';
		pqdebug2("Asynchronous notification encountered. (%s, %d)",
			 relname, PQxactid);
		PQappendNotify(relname, PQxactid);
	    }
	    break;
    	case 'P':
	    /* Synchronized (normal) portal. */
	    return(process_portal(0));
    	case 'C':
	    /* Query executed successfully but returned nothing. */
	    pq_getstr(command, command_length);
	    pqdebug("Query command: %s", command);
	    sprintf(retbuf, "C%s", command);
	    return(retbuf);
	case 'B':
	    /* Copy command began successfully - it is sending stuff back */
	    (void) strcpy(retbuf, "BCOPY");
	    return(retbuf);
	case 'D':
	    /* Copy command began successfully - it is waiting to receive */
	    (void) strcpy(retbuf, "DCOPY");
	    return(retbuf);
    	default:
	    (void) sprintf(PQerrormsg, 
			   "FATAL: PQexec: protocol error: id=%x\n",
			   id);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    (void) strcpy(retbuf, "E");
	    return(retbuf);
    	}
    }
}

/*
 * PQendcopy
 *	called while waiting for the backend to respond with success/failure
 *	to a "copy".
 *
 * RETURNS:
 *	0 on failure
 *	1 on success
 */
int
PQendcopy()
{
    char id;

    for (;;) {
	if (pq_read_id(&id, "PQendcopy") > 0)
	    return(0);
	switch (id) {
	case 'E':
	    (void) pq_print_elog((char *) NULL, false);
	    return(0);
	case 'N':
	    (void) pq_print_elog((char *) NULL, false);
	    break;
	case 'Z': /* backend finished the copy */
	    return(1);
	default:
	    (void) sprintf(PQerrormsg, 
			   "FATAL: PQendcopy: protocol error: id=%x\n",
			   id);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	    PQreset();
	    return(0);
	}
    }
    /*NOTREACHED*/
}
