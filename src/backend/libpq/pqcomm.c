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

char *strcpy(), *ttyname();

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
void
pq_getstr(s, maxlen)
    char *s;
    int	 maxlen;
{
    int	c;
    
    while (maxlen-- && (c = getc(Pfin)) != EOF && c)
	*s++ = c;
    *s = '\0';
}

/* --------------------------------
 *	pq_getnchar - get n characters from connection
 * --------------------------------
 */
void
pq_getnchar(s, off, maxlen)
    char *s;
    int	 off, maxlen;
{
    int	c;
    
    s += off;
    while (maxlen-- && (c = getc(Pfin)) != EOF)
	*s++ = c;
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
    fputs(s, Pfout);
    fputc('\0', Pfout);
}

/* --------------------------------
 *	pq_putnchar - send n characters to connection
 * --------------------------------
 */
void
pq_putnchar(s, n)
    char *s;
{
    while (n--)
	fputc(*s++, Pfout);
}

/* --------------------------------
 *	pq_putint - send an integer to connection
 * --------------------------------
 */
void
pq_putint(i, b)
    int	i, b;
{
    if (b > 4)
	b = 4;
    
    while (b--) {
	fputc(i & 0xff, Pfout);
	i >>= 8;
    }
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
		fputs(host, stderr);
		fputs(": Not Internet\n", stderr);
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
	return(1);
    }
    
    return
	pq_getinaddr(sin, host, ntohs(ss->s_port));
}

/* --------------------------------
 *	pq_connect - create remote input / output connection
 * --------------------------------
 */
int
pq_connect(host, port)
    char	*host, *port;
{
    struct sockaddr_in	sin;
    int			fd, port_no;
    
    port_no = atoi(port);
    
    /* pq_getport() returns the environment variable PGPORT, default is 4321 */
    if (!port_no) 
	port_no = pq_getport();
    
    if (pq_getinaddr(&sin, host, port_no))
	return(-1);
    
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	return(-1);
    }
    
    if (connect(fd, &sin, sizeof sin) == -1)
	return(-1);
    
    fcntl(fd,F_SETFL,0);
    Pfout = fdopen(fd, "w");
    Pfin = fdopen(dup(fd), "r");
    
    return(0);
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

/* ----------------
 *	pqtest
 * ----------------
 */
int pqtest() { return 1; }
