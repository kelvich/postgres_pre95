/* ----------------------------------------------------------------
 *   FILE
 *	pqcomm.c
 *	
 *   DESCRIPTION
 *	Communication functions between the Frontend and the Backend
 *
 *   INTERFACE ROUTINES
 *	pq_gettty 	- return the name of the tty in the given buffer
 *	pq_getport 	- return the PGPORT setting or 4321 if not set
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>

#include "tmp/pqcomm.h"
#include "tmp/c.h"
#include "utils/log.h"

RcsId ("$Header$");

/* ----------------
 *	declarations
 * ----------------
 */
extern char **environ;
char **ep;
char *dp;

FILE *Pfout, *Pfin;
int PQAsyncNotifyWaiting;	/* for async. notification */
extern char PQerrormsg[];

char *strcpy(), *ttyname();

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
	elog(FATAL, "Couldn't initialize socket connection");
    PQnotifies_init();
}

/* --------------------------------
 *	pq_gettty - return the name of the tty in the given buffer
 * --------------------------------
 */
void
pq_gettty(tp)
    char tp[20];
{	
    strcpy(tp, ttyname(0));
}

/* --------------------------------
 *	pq_getport - return the PGPORT setting or 4321 if not set
 * --------------------------------
 */
int
pq_getport()
{
    char *env, *getenv();
    int  port_no;
    
    env = getenv("PGPORT");
    
    if (env) {
	port_no = atoi(env);
    } else
	port_no = 4321;
    
    return
	port_no;
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
 *	pq_getstr - get a null terminateed string from connection
 * --------------------------------
 */
int
pq_getstr(s, maxlen)
    char *s;
    int	 maxlen;
{
    int	c;
    
    if (Pfin == (FILE *) NULL)
    {
	elog(DEBUG, "Input descriptor is null");
	return (EOF);
    }

    while (maxlen-- && (c = getc(Pfin)) != EOF && c)
	*s++ = c;
    *s = '\0';

    /* -----------------
     *     If EOF reached let caller know
     * -----------------
     */
    if (c == EOF)
	return EOF;
    else
	return !EOF;
}

/*
 * USER FUNCTION - gets a newline-terminated string from the backend.
 * 
 * Chiefly here so that applications can use "COPY <rel> () from stdin"
 * and read the output string.  Returns a null-terminated string in s and
 * EOF if it is detected.
 */

int
PQgetline(s, maxlen)

char *s;
int maxlen;

{
    int c;

    if (Pfin == (FILE *) NULL)
	return (EOF);

    while (maxlen-- && (c = getc(Pfin)) != '\n' && c != EOF)
	{
		*s++ = c;
	}
    *s = '\0';

    /* -----------------
     *     If EOF reached let caller know
     * -----------------
     */
    if (c == EOF)
	return EOF;
    else
	return !EOF;
}

/*
 * USER FUNCTION - sends a string to the backend.
 * 
 * Chiefly here so that applications can use "COPY <rel> () to stdout"
 * and read the output string.  Returns a null-terminated string in s and
 * EOF if it is detected.
 */

int
PQputline(s)

char *s;

{
    if (Pfout) {
	(void) fputs(s, Pfout);
	fflush(Pfout);
    }
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
    
    if (Pfin == (FILE *) NULL)
    {
	elog(DEBUG, "Input descriptor is null");
	return (EOF);
    }

    s += off;
    while (maxlen-- && (c = getc(Pfin)) != EOF)
	*s++ = c;

    /* -----------------
     *     If EOF reached let caller know
     * -----------------
     */
    if (c == EOF)
    {
	return EOF;
    }
    else
	return !EOF;
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
    
    if (Pfin == (FILE *) NULL)
    {
	elog(DEBUG, "Input descriptor is null");
	return (EOF);
    }

    n = p = 0;
    while (b-- && (c = getc(Pfin)) != EOF && p < 32) {
	n |= (c & 0xff) << p;
	p += 8;
    }

    return n;
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
	if (status == EOF)
	{
	    strcpy(PQerrormsg,"pq_putstr: write to backend failed\n");
	    fprintf(stderr, PQerrormsg);
	}
	status = fputc('\0', Pfout);
	if (status == EOF)
	{
	    strcpy(PQerrormsg, "pq_putstr: write to backend failed\n");
	    fprintf(stderr, PQerrormsg);
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
	while (n--)
	{
	    status = fputc(*s++, Pfout);
	    if (status == EOF)
	    {
		strcpy(PQerrormsg, "pq_putnchar: write to backend failed\n");
		fprintf(stderr, PQerrormsg);
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
	    if (status == EOF)
	    {
		strcpy(PQerrormsg, "pq_putint: write to backend failed\n");
		fprintf(stderr, PQerrormsg);
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
    return send(fd,msg,len,MSG_OOB);
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
    return len;
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
    struct	hostent	*hs;
    
    bzero((char *)sin, sizeof *sin);
    
    if (host) {
	if (*host >= '0' && *host <= '9')
	    sin->sin_addr.s_addr = inet_addr(host);
	else {
	    if (!(hs = gethostbyname(host))) {
		perror(host);
		return(1);
	    }
	    if (hs->h_addrtype != AF_INET) {
		sprintf(PQerrormsg,"%s: Not Internet\n",host);
		fprintf(stderr,PQerrormsg);
		return(1);
	    }
	    bcopy(hs->h_addr, (char *)&sin->sin_addr, hs->h_length);
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
    struct	sockaddr_in *sin;
    char	*host, *serv;
{
    struct	servent	*ss;
    
    if (*serv >= '0' && *serv <= '9')
	return
	    pq_getinaddr(sin, host, atoi(serv));
    
    if (!(ss = getservbyname(serv, NULL))) {
	fputs(serv, stderr);
	fputs(": Unknown service\n", stderr);
	sprintf(PQerrormsg,"%s: Unknown service\n",serv);
	return(1);
    }
    
    return
	pq_getinaddr(sin, host, ntohs(ss->s_port));
}

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
  int			sock;
  Addr			addr;
  int			status;

  /*
   * Initialize the startup packet.  Packet fields defined in comm.h
   */
  strncpy(startup.database,dbname,sizeof(startup.database));
  strncpy(startup.user,user,sizeof(startup.user));
  strncpy(startup.options,args,sizeof(startup.options));
  strncpy(startup.tty,debugTty,sizeof(startup.tty));
  if (execFile != NULL)
  {
    strncpy(startup.execFile, execFile, sizeof(startup.execFile));
  }
  else
  {
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
  status = StreamOpen(hostName,portName,&sock);
  if (status != STATUS_OK)
    return(STATUS_ERROR);

  /*
   * Save communication port state.
   */
  SendPort = (Port *) malloc(sizeof(Port));
  bzero(SendPort, sizeof(Port));
  SendPort->sock =  sock;

  /* initialize */
  status = PacketSend(SendPort, MyConn, 
		      &startup, STARTUP_MSG, sizeof(startup), BLOCKING);

  /* set up streams over which communic. will flow */
  Pfout = fdopen(sock, "w");
  Pfin = fdopen(dup(sock), "r");
  if (!Pfout && !Pfin)
  {
      strcpy(PQerrormsg, "Couldn't fdopen the socket descriptor\n");
    fprintf(stderr,PQerrormsg);
    return(STATUS_ERROR);
  }
  
  PQAsyncNotifyWaiting = 0;
  PQnotifies_init();
  pq_regoob(pq_async_notify);

  if (status != STATUS_OK)
    return(STATUS_ERROR);

  return(STATUS_OK);
}

/* --------------------------------
 *	pq_accept - accept remote input / output connection
 * --------------------------------
 */
int
pq_accept()
{
    struct sockaddr_in	sin;
    int			fd, nfd, i;
    
    if (!(fd = socket(AF_INET, SOCK_STREAM, 0)))
	return(-2);
    
    pq_getinaddr(&sin, 0, pq_getport());
    if (bind(fd, &sin, sizeof sin))
	return(-3);
    
    listen(fd, SOMAXCONN);
    if ((nfd = accept(fd, NULL, NULL)) < 0)
	return(-1);
    
    close(fd);
    Pfout = fdopen(nfd, "w");
    Pfin = fdopen(dup(nfd), "r");
    
    return(0);
}

/*
 * register an out-of-band listener proc--at most one allowed.
 * This is used for receiving async. notification from the backend.
 */
void 
pq_regoob(fptr)
     void (*fptr)();
{
    int fd = fileno(Pfout);
    fcntl(fd,F_SETOWN,getpid());
    (void) signal(SIGURG,fptr);
}

void pq_unregoob()
{
    signal(SIGURG,SIG_DFL);
}


void pq_async_notify() {
    char msg[20];
    int len = sizeof(msg);
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

#ifdef NOTDEF
  if ((hp = gethostbyname(hostName)) && hp->h_addrtype == AF_INET)
    bcopy(hp->h_addr, (char *)&(sin.sin_addr), hp->h_length);
  else {
    fprintf(stderr, 
        "StreamServerPort: cannot find hostname '%s' for stream port\n", 
        hostName);
    return(STATUS_ERROR);
  }
#endif

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      strcpy(PQerrormsg,"StreamServerPort: cannot make socket descriptor for port\n");
    fprintf(stderr,PQerrormsg);
    return(STATUS_ERROR);
  }

  sin.sin_family = AF_INET;
  sin.sin_port = htons(portName);

  if (bind(fd, (char *)&sin, sizeof sin) < 0) {
    strcpy(PQerrormsg,"StreamServerPort: cannot bind to port\n");
    fprintf(stderr,PQerrormsg);
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
 * This one should be non-blocking.  The client could send us
 * part of a message.  Would not do at all to have the server
 * block waiting for the message to complete.  Not neccesary
 * to return the address, but it could conceivably be used
 * for protection checking.
 * 
 * RETURNS: STATUS_OK or STATUS_ERROR
 */
StreamConnection(server_fd,new_fdP,addrP)
int 			server_fd;
int			*new_fdP;
struct sockaddr_in	*addrP;
{
  long 			len = sizeof (struct sockaddr_in);

  if ((*new_fdP = accept(server_fd, (char *)addrP, &len)) < 0) {
    strcpy(PQerrormsg,"StreamConnection: accept failed\n");
    fprintf(stderr,PQerrormsg);
    return(STATUS_ERROR);
  }
  /* reset to non-blocking */
  fcntl(*new_fdP, F_SETFL, 1);	

  return(STATUS_OK);
}

/* 
 * StreamClose -- close a client/backend connection
 */
StreamClose(sock)
int	sock;
{
  close(sock); 
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
 *
 * ASSUME that the client doesn't need the net address of the
 *	server after this routine exits.
 * ---------------------------
 */
StreamOpen(hostName,portName,sockP)
char	*hostName;
short	portName;
int	*sockP;
{
  struct sockaddr_in	addr;
  struct hostent	*hp;
  int			sock;

  if (! hostName)
    hostName = "localhost";

  bzero((char *)&addr, sizeof addr);

  if ((hp = gethostbyname(hostName)) && hp->h_addrtype == AF_INET)
    bcopy(hp->h_addr, (char *)&(addr.sin_addr), hp->h_length);
  else {
    sprintf(PQerrormsg,
	"StreamPort: cannot find hostname '%s' for stream port\n",
	hostName);
    fprintf(stderr,PQerrormsg);
    return(STATUS_ERROR);
  }

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      strcpy(PQerrormsg,"StreamPort: cannot make socket descriptor for port\n");
    fprintf(stderr,PQerrormsg);
    return(STATUS_ERROR);
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(portName);

  *sockP = sock;
  if (! connect(sock, &addr, sizeof(addr)))
    return(STATUS_OK);
  else
    return(STATUS_ERROR);
}
