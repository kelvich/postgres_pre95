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
 *	StringPointerSet
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

#include "tmp/simplelists.h"
#include "tmp/libpq-fe.h"
#include "tmp/fastpath.h"
#include "utils/fmgr.h"
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
char 	PQdatabase[17] = {0};	/* the POSTGRES database to access */
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
    char *envDB;

    if (PQdatabase[0] == NULL) 
    {
	if ((envDB = getenv ("PGDATABASE")) == NULL)
	    libpq_raise(&ProtocolError,
			form((int)"Fatal Error -- No database is specified."));
	else
	{
	    PQdatabase[16] = '\0';
	    strncpy(PQdatabase, envDB, 16);
	}
    }
    if ((PQhost == NULL) && ((PQhost = getenv("PGHOST")) == NULL))
	PQhost = DefaultHost;
    
    if ((PQtty == NULL) && ((PQtty = getenv("PGTTY")) == NULL))
	PQtty = DefaultTty;
    
    /*
     *  As of release 4, PQoption is no longer taken from the environment
     *  var PGOPTION if it's not already defined.  This change was made
     *  in order to be consistent with the documentation and to make frew
     *  happy.
     *
     *  In general, users shouldn't be screwing around with PQoption, in
     *  any case.
     */

    if (PQoption == NULL)
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
    char errormsg[error_msg_length];

    while (id[0] == 'R') {
	pq_getstr(remarks, remark_length);
	if (pq_getnchar(id, 0, 1) == EOF) 
	   return;
    }
    while(id[0] == 'N') {
        pq_getstr(errormsg,error_msg_length);
        fprintf(stdout,"%s \n",&errormsg[0]+4);
        if (pq_getnchar(id, 0, 1) == EOF)
	   return;
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

	if (pq_connect(PQdatabase, getenv("USER"), PQoption, PQhost, PQtty,
			(char *) NULL, (short)atoi(PQport) ) == -1 ) {
	    libpq_raise(&ProtocolError,
	      form((int)"Failed to connect to backend (host=%s, port=%s)",
		   PQhost, PQport));
	}

	pq_flush();
	PQportset = 1;
    }
}

/* ----------------
 *	process_portal
 *	
 * 	Process portal queries. 
 * 	Return values are the same as PQexec().
 * ----------------
 */

char *
process_portal(rule_p)
    int rule_p;
{
    char pname[portal_name_length];
    char id[2];
    char errormsg[error_msg_length];
    char command[command_length];
    char PQcommand[portal_name_length+1];
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
        pq_getnchar(id, 0, 1);
        read_remark(id);
        pqdebug("Identifier is: %c", (char *)id[0]);

        switch (id[0]) {
        case 'E':
	    /* An error, return 0. */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s error encountered.", errormsg);
	    /* get past gunk at front of errmsg */
	    fprintf(stdout,"%s \n", &errormsg[0] + 4);
	    strcpy(retbuf, "R");
	    return(retbuf);

        case 'N': /* print notice and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s notice encountered.", errormsg);
	    /* get past gunk at front of errmsg */
	    fprintf(stdout,"%s \n", &errormsg[0] + 4);
	    break;
	    
        case 'T':
	    /* Tuples are returned, dump data into a portal buffer. */
	    if (dump_data(pname, rule_p) == -1)
	    {
		    return("R");
	    }
	    sprintf(PQcommand, "P%s", pname);
	    strcpy(retbuf, PQcommand);
	    return(retbuf);
	
	    /* Pending data inquiry - return nothing */
        case 'C':
	    /*
	     * Portal query command (e.g., retrieve, close),
	     * no tuple returned.
	     */
	    PQxactid = pq_getint (4);
	    pqdebug("Transaction Id is: %d", (char *)PQxactid);
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
	        libpq_raise(&ProtocolError, form ((int)s));
	    }
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
 *      actual_result_len: actual length returned. (differs from result_len for
 *                      varlena structures.
 *      result_type     : If the result is an integer, this must be 1,
 *                        If result is opaque, this must be 2.
 * 	args		: pointer to a NULL terminated arg array.
 *			  (length, if integer, and result-pointer)
 * 	nargs		: # of arguments in args array.
 * ----------------
 */
char *
PQfn(fnid, result_buf, result_len, actual_result_len,
     result_type, args, nargs)
    int fnid;
    int *result_buf;    /* can't use void, dec compiler barfs */
    int result_len;
     int *actual_result_len;
    int result_type;
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
	else if (args[i].len == VAR_LENGTH_ARG) {
            pq_putstr((char *)args[i].u.ptr);
	} else
	    pq_putnchar((char *)args[i].u.ptr, args[i].len);
    }

    pq_flush();

    /* process return value from the backend	*/

    id[0] = '?';

    pq_getnchar(id, 0, 1);
    if (id[0] == 'E') {
        char buf[1024];
        pq_getstr(buf,sizeof(buf));
        printf ("Error: %s\n",buf);
    }

    read_remark(id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %c", (char *)id[0]);
    
    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", (char *)PQxactid);

    if (id[0] == 'V')
	pq_getnchar(id, 0, 1);
    for (;;) {
	switch (id[0]) {
	  case 'G':		/* PQFN: simple return value	*/
	    actual_len = pq_getint(4);
	    pqdebug2("LENGTH act/usr %ld/%ld\n", (char*)actual_len, (char*)result_len);
	    if ((actual_len != VAR_LENGTH_RESULT) &&
                (actual_len < 0 || actual_len > result_len)) {
		pqdebug("RESET CALLED FROM CASE G", (char *)0);
		PQreset();
		libpq_raise(&ProtocolError, form ((int)"Buffer Too Small: %s", id));
	    }
	    if (result_type == 1)
	      *(int *)result_buf = pq_getint(4);
	    else if (actual_len == VAR_LENGTH_RESULT) {
		pq_getstr((char *)result_buf,MAX_STRING_LENGTH);
	    } else
	      pq_getnchar((char *)result_buf, 0, actual_len);
	    if (actual_result_len != NULL)
	      *actual_result_len = actual_len;
	    if ((result_type != 2) && /* not a binary result */
		(actual_len != result_len)) /* if wouldn't overflow the buf */
	      ((char *)result_buf)[actual_len] = 0; /* add a \0 */
	    pq_getnchar(id, 0, 1);
	    return("G");
	    
	  case 'E':		/* print error and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s error encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    pq_getnchar(id, 0, 1);
	    break;

	  case 'N':		/* print notice and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s notice encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    pq_getnchar(id, 0, 1);
	    break;

	  case '0':		/* PQFN: no return value	*/
	    return("V");

	    default:
	    /* The backend violates the protocol. */
	    pqdebug("RESET CALLED FROM CASE G", (char *)0);
	    pqdebug("Protocol Error, bad form, got '%c'", (char *)id[0]);
	    PQreset();
	    libpq_raise(&ProtocolError, form((int)"Unexpected identifier: %s", id));
	    return(NULL);
	}
    }
}

/*
 *  PQfsread, PQfswrite -- special-purpose versions of PQfn for file
 *			   system (POSTGRES large object) read and
 *			   write routines.
 *
 *	We need these special versions because the user expects a standard
 *	unix file system interface, and postgres wants to use varlenas
 *	all over the place.
 */

int
PQfsread(fd, buf, nbytes)
    int fd;
    char *buf;
    int nbytes;
{
    int fnid;
    char id[2];
    char errormsg[error_msg_length];
    char command[command_length];
    char PQcommand[command_length+1];
    void EstablishComm();
    int  actual_len;
    short i;

    if (!PQportset)
	EstablishComm();
    
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

    /* process return value from the backend	*/

    id[0] = '?';

    pq_getnchar(id, 0, 1);
    if (id[0] == 'E') {
        char buf[1024];
        pq_getstr(buf,sizeof(buf));
        printf ("Error: %s\n",buf);
    }

    read_remark(id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %c", (char *)id[0]);
    
    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", (char *)PQxactid);

    if (id[0] == 'V')
	pq_getnchar(id, 0, 1);
    for (;;) {
	switch (id[0]) {
	  case 'G':

	    /*
	     *  We know exactly what the return stream looks like, here:
	     *  it's a length, followed by a varlena (which includes the
	     *  length again...).
	     */

	    nbytes = actual_len = pq_getint(4);
#if 0	    
	/* fe/be does NOT transmit varlenas this way */
	    nbytes = pq_getint(4);
	    nbytes -= sizeof(long);	/* compensate for varlena vl->len */
#endif

	    if (nbytes > 0)
		pq_getnchar((char *)buf, 0, nbytes);
	    pq_getnchar(id, 0, 1);
	    return(nbytes);
	    
	  case 'E':		/* print error and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s error encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    pq_getnchar(id, 0, 1);
	    break;

	  case 'N':		/* print notice and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s notice encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    pq_getnchar(id, 0, 1);
	    break;

	  default:
	    /* The backend violates the protocol. */
	    pqdebug("RESET CALLED FROM CASE G", (char *)0);
	    pqdebug("Protocol Error, bad form, got '%c'", (char *)id[0]);
	    PQreset();
	    libpq_raise(&ProtocolError, form((int)"Unexpected identifier: %s", id));
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

    /* process return value from the backend	*/
    id[0] = '?';

    pq_getnchar(id, 0, 1);
    if (id[0] == 'E') {
        char buf[1024];
        pq_getstr(buf,sizeof(buf));
        printf ("Error: %s\n",buf);
    }

    read_remark(id);
    fnid = pq_getint(4);
    pqdebug("The Identifier is: %c", (char *)id[0]);
    
    /* Read in the transaction id. */
    pqdebug("The Transaction Id is: %d", (char *)PQxactid);

    if (id[0] == 'V')
	pq_getnchar(id, 0, 1);

    for (;;) {
	switch (id[0]) {
	  case 'G':		/* PQFN: simple return value	*/
	    actual_len = pq_getint(4);
	    if (actual_len != 4)
		libpq_raise(&ProtocolError,
			    form((int) "wanted 4 bytes in PQfswrite, got %d\n",
			    actual_len));
	    nbytes = pq_getint(4);
	    pq_getnchar(id, 0, 1);
	    return (nbytes);
	    
	  case 'E': /* print error and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s error encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    pq_getnchar(id, 0, 1);
	    break;

	  case 'N': /* print notice and go back to processing return values */
	    pq_getstr(errormsg, error_msg_length);
	    pqdebug("%s notice encountered.", errormsg);
	    fprintf(stdout,"%s \n", errormsg);
	    pq_getnchar(id, 0, 1);
	    break;

	    default:
	    /* The backend violates the protocol. */
	    pqdebug("RESET CALLED FROM CASE G", (char *)0);
	    pqdebug("Protocol Error, bad form, got '%c'", (char *)id[0]);
	    PQreset();
	    libpq_raise(&ProtocolError, form((int)"Unexpected identifier: %s", id));
	    return(NULL);
	}
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
    	pqdebug("The Identifier is: %c", (char *)id[0]);

    	/* Read in the transaction id. */
    	PQxactid = pq_getint(4);
    	pqdebug("The Transaction Id is: %d", (char *)PQxactid);

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

    	case 'A': {
	    char relname[16];
	    extern int PQAsyncNotifyWaiting;
	    int pid;
	    PQAsyncNotifyWaiting = 0;
	    
	    /* Asynchronized portal. */
	    /* No tuple transfer at this stage. */
	    pqdebug("%s portal encountered.", "Asynchronized");
	    /* Processed the same way as synchronized portal. */
/*	    return
		process_portal(1);*/
	    pq_getstr(relname,16);
	    pid =pq_getint(4);
	    PQappendNotify(relname,pid);
	}
	    break;
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

	case 'B':
	    /* Copy command began successfully - it is sending stuff back...  */
	    return "BCOPY";

	case 'D':
	    /* Copy command began successfully - it is waiting to receive... */
	    return "DCOPY";

    	default:
	    /* The backend violates the protocol. */
	    if (id[0] == '?')
	    	libpq_raise(&ProtocolError, 
			form((int)"No response from the backend, exiting...\n"));
	    else
	    	libpq_raise(&ProtocolError, 
		   form((int)"Unexpected response from the backend, exiting...\n"));
	    exit(1);
    	}
    }
}

int
PQendcopy()

{
    char id[2];
    char errormsg[error_msg_length];

    for (;;) 
    {
		id[0] = '?';

        pq_getnchar(id,0,1); 

        switch(id[0])
        {
            case 'N':

                pq_getstr(errormsg, error_msg_length);
                pqdebug("%s notice encountered.", errormsg);
                fprintf(stdout,"%s \n", errormsg);
                break;

            case 'E':

                pq_getstr(errormsg, error_msg_length);
                pqdebug("%s notice encountered.", errormsg);
                fprintf(stdout,"%s \n", errormsg);
                return(0);

            case 'Z': /* backend finished the copy */
                return(1);

            default:
                /* The backend violates the protocol. */
                if (id[0] == '?')
                    libpq_raise(&ProtocolError, 
                        form((int)"No response from the backend, exiting...\n"));
                else
                    libpq_raise(&ProtocolError, 
                    form((int)"Unexpected response from the backend, exiting...\n"));
                exit(1);
        }
    }
}
