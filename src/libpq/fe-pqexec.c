/* ----------------------------------------------------------------
 *   FILE
 *	fe-pqexec.c
 *	
 *   DESCRIPTION
 *	support for executing POSTGRES commands and functions
 *	from a frontend application.
 *
 *   SUPPORT ROUTINES
 *	read_initstr, read_remark, EstablishComm, process_portal,
 *	StringPointerSet, InitVacuumDemon
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

#include "tmp/libpq-fe.h"
#include "utils/exception.h"

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
 *	PQoption:	PGOPTION
 * ----------------------------------------------------------------
 */

#define DefaultHost	"localhost"
#define DefaultPort	"4321"
#define DefaultTty	"/dev/null"
#define DefaultOption	""
#define DefaultVacuum	"~/bin/vacuumd"
	
char 	*PQhost;		/* the machine on which POSTGRES backend 
				   is running */
char 	*PQport = NULL;		/* the communication port with the 
				   POSTGRES backend */
char	*PQtty;			/* the tty on PQhost backend message 
				   is displayed */
char	*PQoption;		/* the optional arguments to POSTGRES 
				   backend */
char 	*PQdatabase;	 	/* the POSTGRES database to access */
int 	PQportset = 0;		/* 1 if the communication with backend
				   is set */
int	PQxactid = 0;		/* the transaction id of the current 
				   transaction */
/*char	*PQinitstr = NULL;	/* the initialization string passed 
				   to backend (obsolete 2/21/91 -mer) */

extern char *getenv();

/* ----------------------------------------------------------------
 *			PQ utility routines
 * ----------------------------------------------------------------
 */

/* ----------------
 *	read_initstr
 *
 *	This is now a misnomer. PQinistr is no longer used and this routine
 *	really just initializes the PQ global variables if they need it.
 *
 * 	Read in the initialization string to be passed to the POSTGRES backend.
 * 	The initstr has the format of
 *
 *		USER,DATABASE,TTY,OPTION\n
 *
 * 	If the variables do not have values yet, read in the values from the 
 * 	environment variables.  If the environment variable does not have a
 * 	value, use the default value.
 * ----------------
 */

void
read_initstr()
{
    if ((PQdatabase == NULL) 
	&& ((PQdatabase = getenv ("PGDATABASE")) == NULL)) 
	libpq_raise(ProtocolError,
		    form("Fatal Error -- No database is specified."));
    
    if ((PQhost == NULL) && ((PQhost = getenv("PGHOST")) == NULL))
	PQhost = DefaultHost;
    
    if ((PQtty == NULL) && ((PQtty = getenv("PGTTY")) == NULL))
	PQtty = DefaultTty;
    
    if ((PQoption == NULL) && ((PQoption = getenv("PGOPTION")) == NULL))
	PQoption = DefaultOption;
    
    if (PQport == NULL) {
	if (getenv("PGPORT") == NULL)
	   PQport = DefaultPort;
	else PQport = getenv("PGPORT");
    }
    
/* The function of PQinitstr is now done via a message to the postmaster
 *
 *  PQinitstr = addValues(initstr_length);
 *  
 *  sprintf(PQinitstr, "%s,%s,%s,%s\n",
 *	    getenv("USER"), PQdatabase, PQtty, PQoption);
 */
}

/* ----------------
 *	read_remark - Read and discard remarks. 
 * ----------------
 */
void
read_remark(id)
    char id[];
{
    char remarks[remark_length];

    while (id[0] == 'R') {
	pq_getstr(remarks, remark_length);
	pq_getnchar(id, 0, 1);
    }
}

/* ----------------
 *	EstablishComm
 * ----------------
 */
static
void
EstablishComm()
{
    if (!PQportset) { 
	read_initstr();

	if ( pq_connect( PQdatabase, 
		getenv("USER"), 
		PQoption, 
		PQhost, 
		PQtty,
		(short)atoi(PQport) ) == -1 ) {
	    libpq_raise(ProtocolError,
	      form("Failed to connect to backend (host=%s, port=%s)",
		   PQhost, PQport));
	}
	
/*	Now a message.
 *
 *	pqdebug("\nInitstr sent to the backend: %s", PQinitstr);
 *	pq_putstr(PQinitstr);
 */
	pq_flush();
	PQportset = 1;
    }
}

/* ----------------
 *	process_portal
 *	
 * 	Process protal queries. 
 * 	Return values are the same as PQexec().
 * ----------------
 */

char *
process_portal(rule_p)
    int rule_p;
{
    char pname[portal_name_length];
    char id[2];
    char *errormsg;
    char command[command_length];
    char PQcommand[portal_name_length+1];
    static char retbuf[portal_name_length + 1];
    
    /* Read in the portal name. */
    pq_getstr(pname, portal_name_length);
    pqdebug("Portal name = %s", pname);

    /* Read in the identifier following the portal name. */
    pq_getnchar(id, 0, 1);
    read_remark(id);
    pqdebug("Identifier is: %c", id[0]);

    switch (id[0]) {
    case 'T':
	/* Tuples are returned, dump data into a portal buffer. */
	dump_data(pname, rule_p);
	sprintf(PQcommand, "P%s", pname);
	strcpy(retbuf, PQcommand);
	return(retbuf);
	
	/* Pending data inquiry - return nothing */
    case 'C':
	/* Portal query command (e.g., retrieve, close), no tuple returned. */
	PQxactid = pq_getint (4);
	pqdebug("Transaction Id is: %d", PQxactid);
	pq_getstr(command, command_length);
	pqdebug("Query command: %s", command);
	
	/* Process the portal commands. */
	if (strcmp(command, "retrieve") == 0) {
	    /* Allocate a portal buffer, if portal table is full, error. */
	    pbuf_setup(pname);
	    return
		"Cretrieve";
	} 
	else if (strcmp (command, "close") == 0) 
	    return
		"Cclose";
	else {
	    sprintf(retbuf, "C%s", command);
	    return
		retbuf;
	}
	
    default:
	{
	    char s[45];

	    PQreset();
	    sprintf(s, "Unexpected identifier in process_portal: %c", id[0]);
	    libpq_raise(ProtocolError, form (s));
	}
    }
}
	
/* ----------------
 *	StringPointerSet
 * ----------------
 */
static
void
StringPointerSet(stringInOutP, newString, environmentString, defaultString)
    String	*stringInOutP;
    String	newString;
    String	environmentString;
    String	defaultString;
{
    Assert(PointerIsValid(stringInOutP));
    Assert(!PointerIsValid(*stringInOutP));
    Assert(PointerIsValid(environmentString));
    
    if (PointerIsValid(newString)) {
	*stringInOutP = newString;
    } else {
	*stringInOutP = getenv(environmentString);
	
	if (!PointerIsValid(*stringInOutP)) {
	    *stringInOutP = defaultString;
	}
    }
}

/* ----------------
 *	InitVacuumDemon
 * ----------------
 */
void
InitVacuumDemon(host, database, terminal, option, port, vacuum)
    String	host;
    String	database;
    String	terminal;
    String	option;
    String	port;
    String	vacuum;
{
    String	path = NULL;
    
    Assert(!PQportset);
    Assert(!PointerIsValid(PQdatabase));
    
    StringPointerSet(&PQhost, host, "PGHOST", DefaultHost);
    
    if (!PointerIsValid(database)) {
	database = getenv ("PGDATABASE");
    }
    if (!PointerIsValid(database)) {
	libpq_raise(ProtocolError,
		    form("InitVacuumDemon: No database specified"));
    }
    PQsetdb(database);
    
    StringPointerSet(&PQtty, terminal, "PGTTY", DefaultTty);
    StringPointerSet(&PQoption, option, "PGOPTION", DefaultOption);
    StringPointerSet(&PQport, port, "PGPORT", DefaultPort);
    StringPointerSet(&path, vacuum, "PGVACUUM", DefaultVacuum);
    
/*    
 *  PQinitstr = pbuf_addValues(initstr_length);
 *  sprintf(PQinitstr, "%s,%s,%s,%s,%s\n", getenv ("USER"),
 *	    PQdatabase, PQtty, PQoption, path);
 */
    
    if ( pq_connect( PQdatabase, 
	getenv("USER"), 
	PQoption, 
	PQhost, 
	(short)atoi(PQport) ) == -1 ) {
	    libpq_raise(ProtocolError,
		    form("Fatal Error -- No POSTGRES backend to connect to"));
    }
    
/*
 *  pqdebug("\nInitstr sent to the backend: %s", PQinitstr);
 *  pq_putstr(PQinitstr);
 */
    pq_flush();
    PQportset = 1;
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
    return PQdatabase;
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
    PQdatabase = dbname;
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
/*
 *  if (PQinitstr != NULL)
 *      pq_free(PQinitstr);
 *
 *  PQinitstr = NULL;
 */
}

/* ----------------
 *	PQfinish - Close communication ports with the backend. 
 * ----------------
 */
void
PQfinish()
{
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
    char holder[1];

    for (i = 0; i < i_count; i++) {
	pq_getnchar(holder, 0, 1);
	if (holder[0] != 'I')
	    fprintf(stderr, "PQFlushI: read bad protocol entity");
	pq_getint(4); /* throw this away */
    }
}

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
    char id[2];
    char errormsg[error_msg_length];
    char command[command_length];
    char PQcommand[command_length+1];
    void EstablishComm();
    int  actual_len;
    short i;

    if (!PQportset)
	EstablishComm();
    
    pq_putnchar("F", 1);	/*	function		*/
    pq_putint(PQxactid, 4);	/*	transaction id ?	*/
    pq_putint(fnid, 4);		/*	function id		*/
    pq_putint(result_len, 4);	/*	length of return value  */
    pq_putint(nargs, 4);	/*	# of args		*/
    
    for (i=0; i < nargs; ++i) { /*	len.int4 + contents	*/
	pq_putint(args[i].len, 4);
	if (args[i].isint)
	    pq_putint(args[i].u.integer, 4);
	else
	    pq_putnchar(args[i].u.ptr, args[i].len);
    }

    pq_flush();

    /* process return value from the backend	*/

    id[0] = '?';

    pq_getnchar(id, 0, 1);
    read_remark(id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %c", id[0]);
    
    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", PQxactid);

    if (id[0] == 'V')
	pq_getnchar(id, 0, 1);

    switch (id[0]) {
    case 'G':		/* PQFN: simple return value	*/
	actual_len = pq_getint(4);
	pqdebug2("LENGTH act/usr %ld/%ld\n", actual_len, result_len);
	if (actual_len < 0 || actual_len > result_len) {
	    pqdebug("RESET CALLED FROM CASE G", 0);
	    PQreset();
	    libpq_raise(ProtocolError, form ("Buffer Too Small: %s", id));
	}
	if (result_is_int)
	    *(int *)result_buf = pq_getint(4);
	else
	    pq_getnchar(result_buf, 0, actual_len);
	if (actual_len != result_len)	/* if wouldn't overflow the buf */
	    ((char *)result_buf)[actual_len] = 0;	/* add a \0 */
	pq_getnchar(id, 0, 1);
	return("G");

    case '0':		/* PQFN: no return value	*/
	return("V");

    default:
	/* The backend violates the protocol. */
	pqdebug("RESET CALLED FROM CASE G", 0);
	pqdebug("Protocol Error, bad form, got '%c'", id[0]);
	PQreset();
	libpq_raise(ProtocolError, form("Unexpected identifier: %s", id));
    }
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
    char id[2];
    char errormsg[error_msg_length];
    char command[command_length];
    char PQcommand[command_length+1];
    void EstablishComm();

    /* If the communication is not established, establish it. */
    if (!PQportset)
	EstablishComm();

    /* Send a query to backend. */
    pq_putnchar("Q", 1);
    pq_putint(PQxactid, 4);
    pq_putstr(query);
    pqdebug("The query sent to the backend: %s", query);
    pq_flush();

    /* forever (or at least until we get something besides a notice) */
    for (;;) {  

    	/* Process return values from the backend. 
	 * The code in this function is the implementation of
	 * the communication protocol.
	 */
    	id[0] = '?';

    	/* Get the identifier. */
    	pq_getnchar(id,0,1); 

    	read_remark(id);
    	pqdebug("The Identifier is: %c", id[0]);

    	/* Read in the transaction id. */
    	PQxactid = pq_getint(4);
    	pqdebug("The Transaction Id is: %d", PQxactid);

    	switch (id[0]) {
    	case 'I':
	    return("I");
	    
    	case 'E':
	    /* An error, return 0. */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s error encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    /* PQportset = 0;
	       EstablishComm(); */
	    return("R");
	    
    	case 'N': /* print notice and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s notice encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    break;
	    
    	case 'A':
	    /* Asynchronized portal. */
	    pqdebug("%s portal encountered.", "Asynchronized");
	    /* Processed the same way as synchronized portal. */
	    return
		process_portal(1);
	    
    	case 'P':
	    /* Synchronized (normal) portal. */
	    return
		process_portal(0);
	    
    	case 'C':
	    /* Query executed successfully. */
	    pq_getstr (command, command_length);
	    pqdebug ("Query command: %s", command);
	    sprintf (PQcommand, "C%s", command);
	    return
		PQcommand;
	    
    	default:
	    /* The backend violates the protocol. */
	    libpq_raise(ProtocolError, 
		form("Fatal errors in the backend, exitting...\n"));
	    exit(1);
    	}
    }
}
