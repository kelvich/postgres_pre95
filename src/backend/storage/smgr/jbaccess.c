/*
 *  jbaccess.c -- Access interface code for the Sony WORM jukebox.
 *
 *	This file is only used if you're at Berkeley and you have
 *	SONY_JUKEBOX defined.  This file was swiped from Andy McFadden's
 *	jukebox management library and adapted to Postgres.
 */

#include "tmp/c.h"
#include "tmp/postgres.h"

#ifdef SONY_JUKEBOX

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include "storage/jbstruct.h"
#include "storage/jbcomm.h"
#include "storage/jblib.h"

RcsId("$Header$");

/* debugging aid */
/* # define DBUG(x) if (jb_debug) printf x */
# define DBUG(x)

/*
 * One of these for every connection to the server.
 * For now, only one of these is needed.
 */

typedef struct {
    int sock;			/* fd of server socket connection */
    int error;			/* last error result for this connection */
} SERVER;

/*
 * globals
 *
 * These can be set from the client program if desired.
 */

int jb_debug = 0;			/* 0=no, 1=debug messages */
int jb_server_port = JB_SERVER_PORT;	/* server port# */
char *jb_server_mach = JB_SERVER_MACH;	/* server machine name */

/*
 * private variables
 */

static SERVER server;		/* connection to server */

/*
 * ----------------------------------------------------------------------
 *	subroutines
 * ----------------------------------------------------------------------
 */

/*
 * Send a message to the server (blocking).
 *
 * "buf" should have JB_DATA_START bytes of free space at the front
 * (i.e., the buffer copying should be performed by the caller).  This
 * routine sticks the generic header data into the buffer, and then
 * sends it to the server.
 */
static int
send_to_server(nbytes, type, unused, buf)
int nbytes;
short type, unused;
char *buf;
{
    int count, cc;

    DBUG((">+> Sending %d bytes\n", nbytes));

    *((int   *) (buf+0)) = htonl(nbytes);
    *((short *) (buf+4)) = htons(type);
    *((short *) (buf+6)) = htons(unused);

    for (count = 0; count < nbytes; count += cc)
	if ((cc = write(server.sock, buf+count, nbytes-count)) <= 0) {
	    if (jb_debug) perror(">+> unable to write to server");
	    server.error = JB_ESERVER;
	    return (-1);
	}

    return (0);
}


/*
 * Force a non-blocking read of nbytes bytes on a socket.
 *
 * Returns 0 on success, -1 on error.
 */
static int
nb_read(csock, buf, nbytes)
int csock, nbytes;
char *buf;
{
    int count, cc;

    for (count = 0; count < nbytes; count += cc)
	if ((cc = read(csock, buf+count, nbytes-count)) <= 0) {
	    if (!cc) {
		return (-1);	/* EOF - disconnected? */
	    } else {
		perror(">+> Read error");
		return (-1);
	    }
	}

    return (0);
}


/*
 * receive a message from the server (blocking)
 *
 * This interprets the first word of the "data" area as the return code,
 * and acts accordingly (sets server.error and returns -1 if a nonzero
 * error code is returned).  "exp_type" should be the type of REPLY
 * expected; mismatched types cause a "JB_EINTERNAL" error.
 *
 * Note that "buf" must be large enough to hold the incoming message.
 * This is somewhat dangerous during JB_READ_REPLYs.
 */
static int
recv_from_server(type_p, size_p, buf, exp_type)
short *type_p;
int *size_p, exp_type;
char *buf;
{
    int size, error;
    short type;

    /* wait on socket until JB_DATA_START bytes are read (header only) */
    if (nb_read(server.sock, buf, JB_DATA_START) < 0) {
	DBUG((">+> first part of read failed\n"));
	server.error = JB_ESERVER;
	return (-1);
    }

    size = ntohl(*((int   *) (buf +0)));
    type = ntohs(*((short *) (buf +4)));

    if (size > JB_DATA_START) {
	if (nb_read(server.sock, buf+JB_DATA_START, size-JB_DATA_START) < 0) {
	    DBUG((">+> second part of read failed\n"));
	    server.error = JB_ESERVER;
	    return (-1);
	}
    }

    *size_p = size;
    *type_p = type;

    /* check return type */
    if (type != exp_type) {
	server.error = JB_EINTERNAL;
	return (-1);
    }

    /* check return status (+00 in data area) */
    error = ntohl(*((int *) (buf + JB_DATA_START +0)));

    if (error) {
	DBUG((">+> Got an error response, code = %d\n", error));
	server.error = error;
	return (-1);
    }

    DBUG((">+> Received %d bytes\n", size));

    return (0);
}


/*
 * ----------------------------------------------------------------------
 *	main library routines
 * ----------------------------------------------------------------------
 */

/*
 * Implementation note:
 *   The server sends back a 4-byte magic cookie (which is actually a
 *   pointer to a JBPLATTER structure on the server machine) for jb_open()
 *   calls, and expects it to be passed on subsequent calls.  However,
 *   the client code is not allowed to interpret this value in any way,
 *   so future implementations of this library may instead pass a pointer
 *   to a local structure holding stuff like a file descriptor (useful if
 *   we get multiple jukeboxes).
 */

/*
 * jb_init
 */
int
jb_init(version)
char *version;
{
    char init_buf[JB_DATA_START + 8];
    int length, rval, size;
    short type;
    struct sockaddr_in server_addr;
    struct hostent *hp;

    DBUG((">+> Initializing client v%s\n", version));
    server.sock = -1;
    server.error = JB_ENOSERVER;	/* only possible error for now */

    /* create the socket */
    if ((server.sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	if (jb_debug) perror(">+> unable to open stream socket");
	return (-1);
    }

    /* locate host */
    server_addr.sin_family = AF_INET;
    if ((hp = gethostbyname(jb_server_mach)) == 0) {
	return (-1);
    }
    bcopy(hp->h_addr, &server_addr.sin_addr, hp->h_length);
    server_addr.sin_port = htons((short) jb_server_port);

    if ((connect(server.sock, (struct sockaddr *) &server_addr,
						sizeof(server_addr))) < 0) {
	perror(">>> connecting stream socket");
	return (-1);
    }

    /* send init request to server */
    if (send_to_server(8, JB_INIT_REQ, 0, init_buf) < 0) {
	return (-1);
    }

    /* wait for response */
    if (recv_from_server(&type, &size, init_buf, JB_INIT_REPLY) < 0) {
	return (-1);
    }

    server.error = 0;

    return (0);
}


JBPLATTER *
jb_open(platter_id, format, flags)
char *platter_id;
char *format;
int flags;
{
    JBPLATTER *jbp;
    long hold;
    char req[JB_DATA_START + 4 + JB_MAX_NAME+1 + JB_MAX_FORMAT+1];
    char reply[JB_DATA_START + 8];
    char *r = &reply[0]; /* necessary for casts to work on Sparcstation */
    int size, len;
    short type;

    server.error = 0;
    len = JB_DATA_START + 4 + strlen(platter_id)+1 + strlen(format)+1;
    *((long *) (req+JB_DATA_START)) = htonl(flags);
    strcpy(req+4+JB_DATA_START, platter_id);
    strcpy(req+4+JB_DATA_START+strlen(platter_id)+1, format);

    if (send_to_server(len, JB_OPEN_REQ, 0, req) < 0)
	return (NULL);
    if (recv_from_server(&type, &size, reply, JB_OPEN_REPLY) < 0)
	return (NULL);

    /* what a mess */
    hold = *((long *)(r+JB_DATA_START+4));
    jbp = (JBPLATTER *) ntohl(hold);

    return (jbp);
}


int
jb_read(platter, buf, blockno, nblocks)
JBPLATTER *platter;
char *buf;
long blockno, nblocks;
{
    char req[JB_DATA_START + 12], *reply;
    int size, len;
    short type;

    /* skip over label blocks */
    if (blockno >= 0)
	blockno += JB_MAX_LABELS;

    server.error = 0;
    if (blockno < 0 || nblocks < 0) {
	server.error = JB_ERANGE;
	return (-1);
    }

    *((long *) (req+JB_DATA_START+0)) = htonl((long) platter);
    *((long *) (req+JB_DATA_START+4)) = htonl(blockno);
    *((long *) (req+JB_DATA_START+8)) = htonl(nblocks);

    if (send_to_server(JB_DATA_START+12, JB_READ_REQ, 0, req) < 0)
	return (-1);
    len = nblocks * JB_BLOCK_SIZE;
    reply = (char *) malloc(JB_DATA_START+4 +len);
    if (recv_from_server(&type, &size, reply, JB_READ_REPLY) < 0) {
	free (reply);
	return (-1);
    }

    if (size != len + JB_DATA_START+4) {
	free(reply);
	return (-1);
    }

    bcopy(reply+JB_DATA_START+4, buf, len);
    free(reply);

    return (0);
}


int
jb_write(platter, buf, blockno, nblocks)
JBPLATTER *platter;
char *buf;
long blockno, nblocks;
{
    char *req, reply[JB_DATA_START+4];
    int size, len, data_len;
    short type;

    /* skip over label blocks */
    if (blockno >= 0)
	blockno += JB_MAX_LABELS;

    server.error = 0;
    if (blockno < 0 || nblocks < 0) {
	server.error = JB_ERANGE;
	return (-1);
    }

    data_len = nblocks * JB_BLOCK_SIZE;
    len = data_len + JB_DATA_START +12;
    if ((req = (char *) malloc(len)) == NULL) {
	server.error = JB_EINTERNAL;
	return (-1);
    }
    *((long *) (req+JB_DATA_START+0)) = htonl((long) platter);
    *((long *) (req+JB_DATA_START+4)) = htonl(blockno);
    *((long *) (req+JB_DATA_START+8)) = htonl(nblocks);
    bcopy(buf, req+JB_DATA_START+12, data_len);

    if (send_to_server(len, JB_WRITE_REQ, 0, req) < 0) {
	free(req);
	return (-1);
    }
    free(req);
    if (recv_from_server(&type, &size, reply, JB_WRITE_REPLY) < 0)
	return (-1);

    return (0);
}


int
jb_close(platter)
JBPLATTER *platter;
{
    char req[JB_DATA_START + 4];
    char reply[JB_DATA_START + 4];
    short type;
    int size;

    server.error = 0;
    *((long *) (req+JB_DATA_START)) = htonl((long) platter);

    if (send_to_server(JB_DATA_START+4, JB_CLOSE_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_CLOSE_REPLY) < 0)
	return (-1);

    return (0);
}


int
jb_istat(platter_id, jbsp)
char *platter_id;
JBSTATUS *jbsp;
{
    char req[JB_DATA_START + JB_MAX_NAME+1];
    char reply[JB_DATA_START + 4 + JBSTATUS_SIZE+sizeof(JBLABEL)];
    short type;
    int size, data_len;

    server.error = 0;
    data_len = JB_DATA_START + strlen(platter_id)+1;
    strcpy(req + JB_DATA_START, platter_id);

    if (send_to_server(data_len, JB_ISTAT_REQ, 0, req) < 0) {
	free(req);
	return (-1);
    }
    free(req);
    if (recv_from_server(&type, &size, reply, JB_ISTAT_REPLY) < 0)
	return (-1);

    jbsp->location = ntohl(*((long *) (reply+JB_DATA_START+ 4)));
    jbsp->access   = ntohl(*((long *) (reply+JB_DATA_START+ 8)));
    jbsp->locked   = ntohl(*((long *) (reply+JB_DATA_START+12)));
    jbsp->offset   = ntohl(*((long *) (reply+JB_DATA_START+16)));
    bcopy(reply+JB_DATA_START+4+JBSTATUS_SIZE, &(jbsp->label), sizeof(JBLABEL));

    return (0);
}


int
jb_fstat(platter, jbsp)
JBPLATTER *platter;
JBSTATUS *jbsp;
{
    char req[JB_DATA_START + 4];
    char reply[JB_DATA_START + 4 + JBSTATUS_SIZE+sizeof(JBLABEL)];
    short type;
    int size;

    server.error = 0;
    *((long *) (req+JB_DATA_START)) = htonl((long) platter);

    if (send_to_server(JB_DATA_START+4, JB_FSTAT_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_FSTAT_REPLY) < 0)
	return (-1);

    jbsp->location = ntohl(*((long *) (reply+JB_DATA_START+ 4)));
    jbsp->access   = ntohl(*((long *) (reply+JB_DATA_START+ 8)));
    jbsp->locked   = ntohl(*((long *) (reply+JB_DATA_START+12)));
    jbsp->offset   = ntohl(*((long *) (reply+JB_DATA_START+16)));
    bcopy(reply+JB_DATA_START+4+JBSTATUS_SIZE, &(jbsp->label), sizeof(JBLABEL));

    return (0);
}


long
jb_scanb(platter, start_block, search_max)
JBPLATTER *platter;
long start_block, search_max;
{
    char req[JB_DATA_START +12];
    char *reply = &req[0];		/* only need JDS+8 for reply */
    short type;
    int size;
    long result;

    /* skip over label blocks */
    if (start_block >= 0)
	start_block += JB_MAX_LABELS;

    server.error = 0;
    *((long *) (req+JB_DATA_START+0)) = htonl((long) platter);
    *((long *) (req+JB_DATA_START+4)) = htonl(start_block);
    *((long *) (req+JB_DATA_START+8)) = htonl(search_max);

    if (send_to_server(JB_DATA_START+12, JB_SCANB_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_SCANB_REPLY) < 0)
	return (-1);

    result = ntohl(*((long *) (reply+JB_DATA_START+ 4)));

    if (result < 0)
	return (-2L);

    /* restore the user's notion of block zero */
    result -= JB_MAX_LABELS;

    return (result);
}


long
jb_scanw(platter, start_block, search_max)
JBPLATTER *platter;
long start_block, search_max;
{
    char req[JB_DATA_START +12];
    char *reply = &req[0];		/* only need JDS+8 for reply */
    short type;
    int size;
    long result;

    /* skip over label blocks */
    if (start_block >= 0)
	start_block += JB_MAX_LABELS;

    server.error = 0;
    *((long *) (req+JB_DATA_START+0)) = htonl((long) platter);
    *((long *) (req+JB_DATA_START+4)) = htonl(start_block);
    *((long *) (req+JB_DATA_START+8)) = htonl(search_max);

    if (send_to_server(JB_DATA_START+12, JB_SCANW_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_SCANW_REPLY) < 0)
	return (-1);

    result = ntohl(*((long *) (reply+JB_DATA_START+ 4)));

    if (result < 0)
	return (-2L);

    /* restore the user's notion of block zero */
    result -= JB_MAX_LABELS;

    return (result);
}


int
jb_eject(platter_id)
char *platter_id;
{
    char req[JB_DATA_START + JB_MAX_NAME+1];
    char reply[JB_DATA_START + 4];
    short type;
    int size, data_len;

    server.error = 0;
    data_len = JB_DATA_START + strlen(platter_id)+1;
    strcpy(req + JB_DATA_START, platter_id);

    if (send_to_server(data_len, JB_EJECT_REQ, 0, req) < 0) {
	return (-1);
    }
    if (recv_from_server(&type, &size, reply, JB_EJECT_REPLY) < 0)
	return (-1);

    return (0);
}


int
jb_load()
{
    char reply[JB_DATA_START+4];
    char *req = reply;		/* only JB_DATA_START needed */
    short type;
    int size;

    if (send_to_server(JB_DATA_START, JB_LOAD_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_LOAD_REPLY) < 0)
	return (-1);

    return (0);
}


int
jb_label(platter_id, jblp)
char *platter_id;
JBLABEL *jblp;
{
    JBPLATTER *jbp;
    char req[JB_DATA_START + JB_MAX_NAME+1 + sizeof(JBLABEL)];
    char reply[JB_DATA_START + 4];
    int size, len;
    short type;
    char *id;

    server.error = 0;
    if (platter_id == NULL)		/* label brand new platter? */
	id = "";
    else
	id = platter_id;

    len = JB_DATA_START + strlen(id)+1 + sizeof(JBLABEL);
    strcpy(req+JB_DATA_START, id);
    bcopy((char *) jblp, req+JB_DATA_START+strlen(id)+1, sizeof(JBLABEL));

    if (send_to_server(len, JB_LABEL_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_LABEL_REPLY) < 0)
	return (-1);

    return (0);
}


int
jb_exit()
{
    char req[JB_DATA_START], reply[JB_DATA_START+4];
    short type;
    int size;

    server.error = 0;
    if (send_to_server(JB_DATA_START, JB_EXIT_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_EXIT_REPLY) < 0)
	return (-1);

    return (0);
}


int
jb_error()
{
    return (server.error);
}


char *
jb_strerror(errnum)
int errnum;
{
    static char ebuf[40];
    static char *jb_errlist[JB_NERR] = {
	/* 0*/ "no error",
	/* 1*/ "unable to connect to server",
	/* 2*/ "platter name not recognized",
	/* 3*/ "platter is offline",
	/* 4*/ "JBPLATTER pointer invalid",
	/* 5*/ "format type mismatch",
	/* 6*/ "invalid argument",
	/* 7*/ "platter is open",
	/* 8*/ "permission denied",
	/* 9*/ "I/O error",
	/*10*/ "error on server connection",
	/*11*/ "tried to read blank sector",
	/*12*/ "tried to write written sector",
	/*13*/ "internal error",
	/*14*/ "platter has no label or invalid label",
	/*15*/ "can't change platter name",
	/*16*/ "can't relabel platter in I/O port",
	/*17*/ "invalid label",
	/*18*/ "no more room for labels",
	/*19*/ "server is rejecting debug commands",
	/*20*/ "platter is open with nonzero offset"
    };

    if ((unsigned int) errnum < JB_NERR)
	return (jb_errlist[errnum]);

    sprintf(ebuf, "Unknown error: %d", errnum);
    return (ebuf);
}


/*
 * ----------------------------------------------------------------------
 *	Debugging calls
 * ----------------------------------------------------------------------
 */
int
jb_offset(platter, offset)
JBPLATTER *platter;
long offset;
{
    char req[JB_DATA_START + 8];
    char reply[JB_DATA_START + 4];
    short type;
    int size;

    server.error = 0;
    *((long *) (req+JB_DATA_START+0)) = htonl((long) platter);
    *((long *) (req+JB_DATA_START+4)) = htonl(offset);

    if (send_to_server(JB_DATA_START+8, JB_OFFSET_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_OFFSET_REPLY) < 0)
	return (-1);

    return (0);
}

int
jb_shelf_offset(shelf, side, offset)
int shelf, side;
long offset;
{
    char req[JB_DATA_START + 12];
    char reply[JB_DATA_START + 4];
    short type;
    int size;

    server.error = 0;
    *((long *) (req+JB_DATA_START+0)) = htonl(shelf);
    *((long *) (req+JB_DATA_START+4)) = htonl(side);
    *((long *) (req+JB_DATA_START+8)) = htonl(offset);

    if (send_to_server(JB_DATA_START+12, JB_SHELF_OFFSET_REQ, 0, req) < 0)
	return (-1);
    if (recv_from_server(&type, &size, reply, JB_SHELF_OFFSET_REPLY) < 0)
	return (-1);

    return (0);
}

#endif /* SONY_JUKEBOX */
