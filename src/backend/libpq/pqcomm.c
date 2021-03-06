/* ----------------------------------------------------------------
 *   FILE
 *	pqcomm.c
 *	
 *   DESCRIPTION
 *	Communication functions between the Frontend and the Backend
 *
 *   INTERFACE ROUTINES
 *	pq_gettty 	- return the name of the tty in the given buffer
 *	pq_getport 	- return the PGPORT setting
 *	pq_close 	- close input / output connections
 *	pq_flush 	- flush pending output
 *	pq_getstr 	- get a null terminated string from connection
 *	pq_getnchar 	- get n characters from connection
 *	pq_getint 	- get an integer from connection
 *	pq_putstr 	- send a null terminated string to connection
 *	pq_putnchar 	- send n characters to connection
 *	pq_putint 	- send an integer to connection
 *	pq_getinaddr 	- initialize address from host and port number
 *	pq_getinserv 	- initialize address from host and service name
 *	pq_connect 	- create remote input / output connection
 *	pq_accept 	- accept remote input / output connection
 *      pq_async_notify - receive notification from backend.
 *
 *   NOTES
 * 	These functions are used by both frontend applications and
 *	the postgres backend.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "libpq/pqsignal.h"	/* substitute for <signal.h> */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>		/* for ttyname() */

#ifdef PORTNAME_linux
#ifndef SOMAXCONN
#define SOMAXCONN 5		/* from Linux listen(2) man page */
#endif /* SOMAXCONN */
#endif /* PORTNAME_linux */

#include "libpq/auth.h"
#include "tmp/c.h"
#include "tmp/libpq.h"
#include "tmp/pqcomm.h"
#include "utils/log.h"

RcsId ("$Header$");

/* ----------------
 *	declarations
 * ----------------
 */
FILE *Pfout, *Pfin;
int PQAsyncNotifyWaiting;	/* for async. notification */

/* forward declarations */
void pq_regoob ARGS((void (*fptr )()));
void pq_unregoob ARGS((void ));
void pq_async_notify ARGS((void ));


/* --------------------------------
 *	pq_init - open portal file descriptors
 * --------------------------------
 */
void
pq_init(fd)
    int fd;
{
    Pfin = fdopen(fd, "r");
    Pfout = fdopen(dup(fd), "w");
    if (!Pfin || !Pfout)
	elog(FATAL, "pq_init: Couldn't initialize socket connection");
    PQnotifies_init();
}

/* --------------------------------
 *	pq_gettty - return the name of the tty in the given buffer
 * --------------------------------
 */
void
pq_gettty(tp)
    char *tp;
{	
    (void) strncpy(tp, ttyname(0), 19);
}

/* --------------------------------
 *	pq_getport - return the PGPORT setting
 * --------------------------------
 */
int
pq_getport()
{
    char *envport = getenv("PGPORT");

    if (envport)
	return(atoi(envport));
    return(atoi(POSTPORT));
}

/* --------------------------------
 *	pq_close - close input / output connections
 * --------------------------------
 */
void
pq_close()
{
    if (Pfin) {
	fclose(Pfin);
	Pfin = NULL;
    }
    if (Pfout) {
	fclose(Pfout);
	Pfout = NULL;
    }
    PQAsyncNotifyWaiting = 0;
    PQnotifies_init();
    pq_unregoob();
}

/* --------------------------------
 *	pq_flush - flush pending output
 * --------------------------------
 */
void
pq_flush()
{
    if (Pfout)
	fflush(Pfout);
}

/* --------------------------------
 *	pq_getstr - get a null terminated string from connection
 * --------------------------------
 */
int
pq_getstr(s, maxlen)
    char *s;
    int	 maxlen;
{
    int	c;

    if (Pfin == (FILE *) NULL) {
	elog(DEBUG, "Input descriptor is null");
	return(EOF);
    }

    while (maxlen-- && (c = getc(Pfin)) != EOF && c)
	*s++ = c;
    *s = '\0';

    /* -----------------
     *     If EOF reached let caller know.
     *     (This will only happen if we hit EOF before the string
     *     delimiter is reached.)
     * -----------------
     */
    if (c == EOF)
	return(EOF);
    return(!EOF);
}

/*
 * USER FUNCTION - gets a newline-terminated string from the backend.
 * 
 * Chiefly here so that applications can use "COPY <rel> to stdout"
 * and read the output string.  Returns a null-terminated string in s.
 *
 * PQgetline reads up to maxlen-1 characters (like fgets(3)) but strips
 * the terminating \n (like gets(3)).
 *
 * RETURNS:
 *	EOF if it is detected or invalid arguments are given
 *	0 if EOL is reached (i.e., \n has been read)
 *		(this is required for backward-compatibility -- this
 *		 routine used to always return EOF or 0, assuming that
 *		 the line ended within maxlen bytes.)
 *	1 in other cases
 */

int
PQgetline(s, maxlen)
    char *s;
    int maxlen;
{
    int c = '\0';

    if (!Pfin || !s || maxlen <= 1)
	return(EOF);

    for (; maxlen > 1 && (c = getc(Pfin)) != '\n' && c != EOF; --maxlen) {
	*s++ = c;
    }
    *s = '\0';

    if (c == EOF) {
	return(EOF);		/* error -- reached EOF before \n */
    } else if (c == '\n') {
	return(0);		/* done with this line */
    }
    return(1);			/* returning a full buffer */
}

/*
 * USER FUNCTION - sends a string to the backend.
 * 
 * Chiefly here so that applications can use "COPY <rel> from stdin".
 *
 * RETURNS:
 *	0 in all cases.
 */

int
PQputline(s)
    char *s;
{
    if (Pfout) {
	(void) fputs(s, Pfout);
	fflush(Pfout);
    }
    return(0);
}

/* --------------------------------
 *	pq_getnchar - get n characters from connection
 * --------------------------------
 */
int
pq_getnchar(s, off, maxlen)
    char *s;
    int	 off, maxlen;
{
    int	c;

    if (Pfin == (FILE *) NULL) {
	elog(DEBUG, "Input descriptor is null");
	return(EOF);
    }

    s += off;
    while (maxlen-- && (c = getc(Pfin)) != EOF)
	*s++ = c;

    /* -----------------
     *     If EOF reached let caller know
     * -----------------
     */
    if (c == EOF)
	return(EOF);
    return(!EOF);
}

/* --------------------------------
 *	pq_getint - get an integer from connection
 * --------------------------------
 */
int
pq_getint(b)
    int	b;
{
    int	n, c, p;

    if (Pfin == (FILE *) NULL) {
	elog(DEBUG, "pq_getint: Input descriptor is null");
	return(EOF);
    }

    n = p = 0;
    while (b-- && (c = getc(Pfin)) != EOF && p < 32) {
	n |= (c & 0xff) << p;
	p += 8;
    }

    return(n);
}

/* --------------------------------
 *	pq_putstr - send a null terminated string to connection
 * --------------------------------
 */
void
pq_putstr(s)
    char *s;
{
    int status;

    if (Pfout) {
	status = fputs(s, Pfout);
	if (status == EOF) {
	    (void) sprintf(PQerrormsg,
			   "FATAL: pq_putstr: fputs() failed: errno=%d\n",
			   errno);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	}
	status = fputc('\0', Pfout);
	if (status == EOF) {
	    (void) sprintf(PQerrormsg,
			   "FATAL: pq_putstr: fputc() failed: errno=%d\n",
			   errno);
	    fputs(PQerrormsg, stderr);
	    pqdebug("%s", PQerrormsg);
	}
    }
}

/* --------------------------------
 *	pq_putnchar - send n characters to connection
 * --------------------------------
 */
void
pq_putnchar(s, n)
    char *s;
    int n;
{
    int status;

    if (Pfout) {
	while (n--) {
	    status = fputc(*s++, Pfout);
	    if (status == EOF) {
		(void) sprintf(PQerrormsg,
			       "FATAL: pq_putnchar: fputc() failed: errno=%d\n",
			       errno);
		fputs(PQerrormsg, stderr);
		pqdebug("%s", PQerrormsg);
	    }
	}
    }
}

/* --------------------------------
 *	pq_putint - send an integer to connection
 * --------------------------------
 */
void
pq_putint(i, b)
    int	i, b;
{
    int status;

    if (b > 4)
	b = 4;

    if (Pfout) {
	while (b--) {
	    status = fputc(i & 0xff, Pfout);
	    i >>= 8;
	    if (status == EOF) {
		(void) sprintf(PQerrormsg,
			       "FATAL: pq_putint: fputc() failed: errno=%d\n",
			       errno);
		fputs(PQerrormsg, stderr);
		pqdebug("%s", PQerrormsg);
	    }
	}
    }
}

/* ---
 *     pq_sendoob - send a string over the out-of-band channel
 *     pq_recvoob - receive a string over the oob channel
 *  NB: Fortunately, the out-of-band channel doesn't conflict with
 *      buffered I/O because it is separate from regular com. channel.
 * ---
 */
int
pq_sendoob(msg,len)
    char *msg;
    int len;
{
    int fd = fileno(Pfout);

    return(send(fd,msg,len,MSG_OOB));
}

int
pq_recvoob(msgPtr,lenPtr)
    char *msgPtr;
    int *lenPtr;
{
    int fd = fileno(Pfout);
    int len = 0, n;

    len = recv(fd,msgPtr+len,*lenPtr,MSG_OOB);
    *lenPtr = len;
    return(len);
}

/* --------------------------------
 *	pq_getinaddr - initialize address from host and port number
 * --------------------------------
 */
int
pq_getinaddr(sin, host, port)
    struct	sockaddr_in *sin;
    char	*host;
    int		port;
{
    struct hostent	*hs;

    bzero((char *) sin, sizeof(*sin));

    if (host) {
	if (*host >= '0' && *host <= '9')
	    sin->sin_addr.s_addr = inet_addr(host);
	else {
	    if (!(hs = gethostbyname(host))) {
		perror(host);
		return(1);
	    }
	    if (hs->h_addrtype != AF_INET) {
		(void) sprintf(PQerrormsg,
			       "FATAL: pq_getinaddr: %s not on Internet\n",
			       host);
		fputs(PQerrormsg, stderr);
		pqdebug("%s", PQerrormsg);
		return(1);
	    }
	    bcopy(hs->h_addr, (char *) &sin->sin_addr, hs->h_length);
	}
    }
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    return(0);
}

/* --------------------------------
 *	pq_getinserv - initialize address from host and servive name
 * --------------------------------
 */
int
pq_getinserv(sin, host, serv)
    struct sockaddr_in *sin;
    char *host, *serv;
{
    struct servent *ss;

    if (*serv >= '0' && *serv <= '9')
	return(pq_getinaddr(sin, host, atoi(serv)));
    if (!(ss = getservbyname(serv, NULL))) {
	(void) sprintf(PQerrormsg,
		       "FATAL: pq_getinserv: unknown service: %s\n",
		       serv);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(1);
    }
    return(pq_getinaddr(sin, host, ntohs(ss->s_port)));
}

#ifdef FRONTEND
/* ----------------------------------------
 * pq_connect  -- initiate a communication link between client and
 *	POSTGRES backend via postmaster.
 *
 * RETURNS: STATUS_ERROR, if arguments are wrong or local communication
 *	status is screwed up (can't create socket, etc).  STATUS_OK
 *	otherwise.
 *
 * SIDE_EFFECTS: initiates connection.  
 *               SIGURG handler is set (async notification)
 *
 * NOTE: we don't wait for any error messages from the backend/postmaster.
 *	That means that if the fork fails or the startup message is corrupted,
 *	we won't find out until the first Send/Receive message cal.
 * ----------------------------------------
 */
int
pq_connect(dbname,user,args,hostName,debugTty,execFile,portName)
    char	*dbname;
    char	*user;
    char	*args;
    char	*hostName;
    char	*debugTty;
    char	*execFile;
    short	portName;
{
    /*
     * This data structure is used for the seq-packet protocol.  It
     * describes the frontend-backend connection.
     */
    Connection		*MyConn = NULL;
    Port			*SendPort = NULL; /* This is a TCP or UDP socket */
    StartupPacket		startup;
    int			status;
    MsgType		msgtype;

    /*
     * Initialize the startup packet.  Packet fields defined in comm.h
     */
    strncpy(startup.database,dbname,sizeof(startup.database));
    strncpy(startup.user,user,sizeof(startup.user));
    strncpy(startup.options,args,sizeof(startup.options));
    strncpy(startup.tty,debugTty,sizeof(startup.tty));
    if (execFile != NULL) {
	strncpy(startup.execFile, execFile, sizeof(startup.execFile));
    } else {
	strncpy(startup.execFile, "", sizeof(startup.execFile));
    }

    /* If no port  was suggested grab the default or PGPORT value */
    if (!portName)
	portName = pq_getport();
    /*
     * initialize connection structure.  This is really needed
     * only for the sequenced packet protocol, but these initializations
     * are important to the packet.c library.
     */
    MyConn = (Connection *) malloc(sizeof(Connection));
    bzero(MyConn, sizeof(Connection));
    MyConn->id = INVALID_ID;
    MyConn->seqno = INITIAL_SEQNO;

    /*
     * Open a connection to postmaster/backend.
     */
    SendPort = (Port *) malloc(sizeof(Port));
    bzero((char *) SendPort, sizeof(Port));
    status = StreamOpen(hostName, portName, SendPort);
    if (status != STATUS_OK)
	/* StreamOpen already set PQerrormsg */
	return(STATUS_ERROR);

    /* initialize */
    msgtype = fe_getauthsvc();
    status = PacketSend(SendPort, MyConn, &startup, msgtype,
			sizeof(startup), BLOCKING);

    /* authenticate as required */
    if (fe_sendauth(msgtype, SendPort, hostName) != STATUS_OK) {
	(void) sprintf(PQerrormsg,
		       "FATAL: pq_connect: authentication failed with %s\n",
		       hostName);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }

    /* set up the socket file descriptors */
    Pfout = fdopen(SendPort->sock, "w");
    Pfin = fdopen(dup(SendPort->sock), "r");
    if (!Pfout && !Pfin) {
	(void) sprintf(PQerrormsg,
		       "FATAL: pq_connect: fdopen() failed: errno=%d\n",
		       errno);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }

    PQAsyncNotifyWaiting = 0;
    PQnotifies_init();
/*    pq_regoob(pq_async_notify);*/

    if (status != STATUS_OK)
	/* PacketSend already set PQerrormsg */
	return(STATUS_ERROR);

    return(STATUS_OK);
}
#endif /* FRONTEND */

/*
 * register an out-of-band listener proc--at most one allowed.
 * This is used for receiving async. notification from the backend.
 */
void 
pq_regoob(fptr)
    void (*fptr)();
{
    int fd = fileno(Pfout);
#ifdef PORTNAME_hpux
    ioctl(fd, FIOSSAIOOWN, getpid());
#else /* PORTNAME_hpux */
    fcntl(fd, F_SETOWN, getpid());
#endif /* PORTNAME_hpux */
    (void) signal(SIGURG,fptr);
}

void pq_unregoob()
{
    signal(SIGURG,SIG_DFL);
}


void
pq_async_notify() {
    char msg[20];
/*    int len = sizeof(msg);*/
    int len = 20;

    if (pq_recvoob(msg,&len) >= 0) {
	/* debugging */
	printf("received notification: %s\n",msg);
	PQAsyncNotifyWaiting = 1;
	/*	PQappendNotify(msg+1);*/
    } else {
	extern int errno;
	printf("SIGURG but no data: len = %d, err=%d\n",len,errno);
    }
}

/*
 * Streams -- wrapper around Unix socket system calls
 *
 *
 *	Stream functions are used for vanilla TCP connection protocol.
 */

/*
 * StreamServerPort -- open a sock stream "listening" port.
 *
 * This initializes the Postmaster's connection
 *	accepting port.  
 *
 * ASSUME: that this doesn't need to be non-blocking because
 *	the Postmaster uses select() to tell when the socket
 *	is ready.
 *
 * RETURNS: STATUS_OK or STATUS_ERROR
 */
StreamServerPort(hostName,portName,fdP)
    char	*hostName;
    short	portName;
    int	*fdP;
{
    struct sockaddr_in	sin;
    int			fd;
    struct hostent	*hp;

    if (! hostName)
	hostName = "localhost";

    bzero((char *)&sin, sizeof sin);

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	(void) sprintf(PQerrormsg,
		       "FATAL: StreamServerPort: socket() failed: errno=%d\n",
		       errno);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(portName);

    if (bind(fd, (struct sockaddr *)&sin, sizeof sin) < 0) {
	(void) sprintf(PQerrormsg,
		       "FATAL: StreamServerPort: bind() failed: errno=%d\n",
		       errno);
	pqdebug("%s", PQerrormsg);
	(void) strcat(PQerrormsg, "\tIs another postmaster already running on that port?\n");
	(void) strcat(PQerrormsg, "\tIf not, wait a few seconds and retry.\n");
	fputs(PQerrormsg, stderr);
	return(STATUS_ERROR);
    }

    listen(fd, SOMAXCONN);

    /* MS: I took this code from Dillon's version.  It makes the 
     * listening port non-blocking.  That is not necessary (and
     * may tickle kernel bugs).

     (void) fcntl(fd, F_SETFD, 1);
     (void) fcntl(fd, F_SETFL, FNDELAY);
     */

    *fdP = fd;
    return(STATUS_OK);
}

/*
 * StreamConnection -- create a new connection with client using
 *	server port.
 *
 * This one should be non-blocking.
 * 
 * RETURNS: STATUS_OK or STATUS_ERROR
 */
StreamConnection(server_fd, port)
    int	server_fd;
    Port *port;
{
    int	addrlen;

    /* accept connection (and fill in the client (remote) address) */
    addrlen = sizeof(struct sockaddr_in);
    if ((port->sock = accept(server_fd,
			     (struct sockaddr *) &port->raddr,
			     &addrlen)) < 0) {
	elog(WARN, "postmaster: StreamConnection: accept: %m");
	return(STATUS_ERROR);
    }

    /* fill in the server (local) address */
    addrlen = sizeof(struct sockaddr_in);
    if (getsockname(port->sock, (struct sockaddr *) &port->laddr,
		    &addrlen) < 0) {
	elog(WARN, "postmaster: StreamConnection: getsockname: %m");
	return(STATUS_ERROR);
    }

    port->sock = port->sock;
    port->mask = 1 << port->sock;

    /* reset to non-blocking */
    fcntl(port->sock, F_SETFL, 1);	

    return(STATUS_OK);
}

/* 
 * StreamClose -- close a client/backend connection
 */
StreamClose(sock)
    int	sock;
{
    (void) close(sock); 
}

/* ---------------------------
 * StreamOpen -- From client, initiate a connection with the 
 *	server (Postmaster).
 *
 * RETURNS: STATUS_OK or STATUS_ERROR
 *
 * NOTE: connection is NOT established just because this
 *	routine exits.  Local state is ok, but we haven't
 *	spoken to the postmaster yet.
 * ---------------------------
 */
StreamOpen(hostName, portName, port)
    char	*hostName;
    short	portName;
    Port	*port;
{
    struct hostent	*hp;
    int			laddrlen = sizeof(struct sockaddr_in);
    extern int		errno;

    if (!hostName)
	hostName = "localhost";

    /* set up the server (remote) address */
    if (!(hp = gethostbyname(hostName)) || hp->h_addrtype != AF_INET) {
	(void) sprintf(PQerrormsg,
		       "FATAL: StreamOpen: unknown hostname: %s\n",
		       hostName);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }
    bzero((char *) &port->raddr, sizeof(port->raddr));
    bcopy((char *) hp->h_addr, (char *) &(port->raddr.sin_addr),
	  hp->h_length);
    port->raddr.sin_family = AF_INET;
    port->raddr.sin_port = htons(portName);

    /* connect to the server */
    if ((port->sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	(void) sprintf(PQerrormsg,
		       "FATAL: StreamOpen: socket() failed: errno=%d\n",
		       errno);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }
    if (connect(port->sock, (struct sockaddr *)&port->raddr,
		sizeof(port->raddr)) < 0) {
	(void) sprintf(PQerrormsg,
		       "FATAL: StreamOpen: connect() failed: errno=%d\n",
		       errno);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }

    /* fill in the client address */
    if (getsockname(port->sock, (struct sockaddr *) &port->laddr,
		    &laddrlen) < 0) {
	(void) sprintf(PQerrormsg,
		       "FATAL: StreamOpen: getsockname() failed: errno=%d\n",
		       errno);
	fputs(PQerrormsg, stderr);
	pqdebug("%s", PQerrormsg);
	return(STATUS_ERROR);
    }

    return(STATUS_OK);
}
