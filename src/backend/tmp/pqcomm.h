/* ----------------------------------------------------------------
 *   FILE
 *	pqcomm.h
 *	
 *   DESCRIPTION
 *	Parameters for the communication module
 *
 *   NOTES
 *	Some of this should move to libpq.h
 *	I think the (unused) UDP stuff should go away.. -- pma
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef PqcommIncluded		/* Include this file only once */
#define	PqcommIncluded

#include <sys/types.h>
#include <netinet/in.h>
#include "catalog/pg_user.h"

/*
 * startup msg parameters: path length, argument string length
 */
#define PATH_SIZE	64
#define ARGV_SIZE	64

/*
 * For sequenced packet, this is fixed to 2K by UDP protocol.
 * For the stream implementation, it depends only on available
 *	buffering.
 */
#define MAX_PACKET_SIZE	(2*1024)

typedef enum MsgType {
  ACK_MSG = 0,		/* acknowledge a message */
  ERROR_MSG=1,		/* error response to client from server */
  RESET_MSG=2,		/* client must reset connection */
  PRINT_MSG=3,		/* tuples for client from server */
  NET_ERROR=4,		/* error in net system call */
  FUNCTION_MSG=5,	/* fastpath call (unused) */
  QUERY_MSG=6,		/* client query to server */
  STARTUP_MSG=7,	/* initialize a connection with a backend */
  DUPLICATE_MSG=8,/* duplicate message arrived (errors msg only) */
  INVALID_MSG=9,	/* for some control functions */
  STARTUP_KRB4_MSG=10,	/* krb4 session follows startup packet */
  STARTUP_KRB5_MSG=11,	/* krb5 session follows startup packet */
  /* insert new values here -- DO NOT REORDER OR DELETE ENTRIES */
} MsgType;

typedef char *Addr;
typedef int SeqNo;	/* (seq pack protocol) sequence numbers */
typedef int ConnId;	/* (seq pack protocol) connection ids */
typedef int PacketLen;	/* packet length */

typedef struct PacketHdr {
  SeqNo		seqno;
  PacketLen	len;
  ConnId	connId;
  MsgType	type;
} PacketHdr;

typedef struct StartupPacket {
  PacketHdr	hdr;
  char		database[PATH_SIZE];	/* database name */
  char		user[USER_NAMESIZE];	/* user name */
  char		options[ARGV_SIZE];	/* possible additional args */
  char		execFile[ARGV_SIZE];	/*  possible backend to use */
  char		tty[PATH_SIZE];		/*  possible tty for debug output */
} StartupPacket;

/* amount of available data in a packet buffer */
#define MESSAGE_SIZE	(MAX_PACKET_SIZE - sizeof(PacketHdr))

typedef struct MsgPacket {
  PacketHdr	hdr;
  char		message[MESSAGE_SIZE];
} MsgPacket;

/* I/O can be blocking or non-blocking */
#define BLOCKING 	(FALSE)
#define NON_BLOCKING	(TRUE)

/* buffers for incoming packets */
typedef int PacketBufId;

/*
 * Watch out if PacketBufId changes size.  Some compilers
 * will allocate padding.  PacketBuf should be exactly
 * MESSAGE_SIZE big.  I have an assert statement in InitComm
 */
typedef struct PacketBufHdr {
  PacketBufId	id,prev,next;
} PacketBufHdr;

typedef struct PacketBuf {
  PacketBufId	id,prev,next;
  char 		data[MESSAGE_SIZE - sizeof(PacketBufHdr)];
} PacketBuf;


/*
 * socket descriptor port 
 *	we need addresses of both sides to do authentication calls
 */
typedef struct Port {
  int			sock;	/* file descriptor */
  int			mask;	/* select mask */
  int   		nBytes;	/* nBytes read in so far */
  struct sockaddr_in	laddr;	/* local addr (us) */
  struct sockaddr_in	raddr;	/* remote addr (them) */
  PacketBufId		id;	/* id of packet buf currently in use */
  PacketBuf		buf;	/* stream implementation (curr pack buf) */
} Port;

/*
 * Connection to client
 */
typedef struct Connection {
  struct sockaddr_in	addr;	/* addr of client */
  SeqNo			seqno;	/* next expected sequence number */
  ConnId		id,prev,next;      /* connection queue links. */
  PacketBufId		packSend,packRecv; /* send/receive packet bufs */
  long			lastUse;/* last recieve time for Gbg Collection */
  int			uid;	/* unique id (generation number) */
} Connection;

/* invalid socket descriptor */
#define INVALID_SOCK	(-1)

#define INVALID_ID (-1)
#define MAX_CONNECTIONS	10
#define N_PACK_BUFS	20

/* no multi-packet messages yet */
#define MAX_PACKET_BACKLOG	1

#define INITIAL_SEQNO		1
#define	DEFAULT_STRING		""

#endif /* !defined(PqcommIncluded) */
