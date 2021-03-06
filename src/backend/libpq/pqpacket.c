/*
 * pqpacket.c -- routines for reading and writing data packets
 *	sent/received by POSTGRES clients and servers
 *
 * $Header$
 *
 * This is the module that understands the lowest-level part
 * of the communication protocol.  All of the trickiness in
 * this module is for making sure that non-blocking I/O in
 * the Postmaster works correctly.   Check the notes in PacketRecv
 * on non-blocking I/O.
 *
 * Data Structures:
 *	Port has two important functions. (1) It records the 
 *	sock/addr used in communication. (2) It holds partially
 *	read in messages.  This is especially important when
 *	we haven't seen enough to construct a complete packet 
 *	header.
 *
 * PacketBuf -- None of the clients of this module should know
 *	what goes into a packet hdr (although they know how big
 *	it is).  This routine is in charge of host to net order
 *	conversion for headers.  Data conversion is someone elses
 *	responsibility.
 *
 * IMPORTANT: these routines are called by backends, clients, and
 *	the Postmaster.
 *
 * NOTE: assume htonl() is sufficient for all header structs.  If
 *	header structs become longs, must switch to htonl().
 * XXX say what??
 *
 * XXX this fake-o UDP cruft has got to go.  none of these routines are
 * called more than once, anywhere.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#include "tmp/postgres.h"
#include "utils/log.h"
#include "storage/ipci.h"
#include "tmp/pqcomm.h"
#include "tmp/libpq.h"

/*
 * PacketReceive -- receive a packet on a port.
 *
 * RETURNS: connection id of the packet sender, if one
 * is available.
 *
 * XXX this code is called in exactly one place
 */
PacketReceive(port, buf, nonBlocking, connIdP)
    Port	*port;	/* receive port */
    PacketBuf	*buf;	/* MAX_PACKET_SIZE-worth of buffer space */
    Boolean	nonBlocking; /* NON_BLOCKING or BLOCKING i/o */
    ConnId	*connIdP;    /* sender's connection (seqpack) */
{
    int		status;
    PacketLen	cc;	    	/* character count -- bytes recvd */
    PacketLen	packetLen; 	/* remaining packet chars to read */
    Addr	tmp;	   	/* curr recv buf pointer */
    PacketHdr	*hdr = (PacketHdr *) buf;  /* incoming hdr information */
    int		addrLen = sizeof(struct sockaddr_in);
    int		flag;
    int		decr;

    if (nonBlocking) {
	flag = MSG_PEEK;
	decr = 0;
    } else {
	flag = 0;
	decr = sizeof(PacketHdr);
    }
    /*
     * Assume port->nBytes is zero unless we were interrupted during
     * non-blocking I/O.  This first recvfrom() is to get the hdr
     * information so we know how many bytes to read.  Life would
     * be very complicated if we read too much data (buffering).
     */
    tmp = ((Addr)buf) + port->nBytes;
    if (port->nBytes >= sizeof(PacketHdr)) {
#if 0
	packetLen = htonl(hdr->len) + sizeof(PacketHdr);
#endif
	packetLen = htonl(hdr->len) - port->nBytes;
    } else {
	cc = recvfrom(port->sock, tmp, sizeof(PacketHdr), flag, 
		      &(port->raddr), &addrLen);
	if (cc < sizeof(PacketHdr)) {
	    /* if cc is negative, the system call failed */
	    if (cc < 0) {
		return(STATUS_ERROR);
	    }
	    /* 
	     * cc == 0 means the connection was broken at the 
	     * other end.
	     */
	    else if (! cc) {
		return(STATUS_INVALID);

	    } else {
		/*
		 * Worst case.  We didn't even read in enough data to
		 * get the header.  IN UDP (seq-pack) THIS IS IMPOSSIBLE.
		 * Messages are delivered in full packets.  In the stream
		 * protocol, it probably won't happen unless the client
		 * is malicious (so its even harder to test).
		 *
		 * Don't save the number of bytes we've read so far.
		 * Since we only peeked at the incoming message, the
		 * kernel is going to keep it for us.
		 */
		*connIdP = INVALID_ID;
		return(STATUS_NOT_DONE);
	    }
	} else {
	    /*
	     * great. got the header. now get the true length (including
	     * header size).
	     */
	    packetLen = htonl(hdr->len);
	    packetLen -= decr;
	    tmp += decr - port->nBytes;
	}
    }

    /*
     * Now that we know how big it is, read the packet.  We read
     * the entire packet, since the last call was just a peek.
     */
    while (packetLen) { 
	cc = recvfrom(port->sock, tmp, packetLen, NULL, 
		      &(port->raddr), &addrLen);
	if (cc < 0)
	    return(STATUS_ERROR);
	/* 
	 * cc == 0 means the connection was broken at the 
	 * other end.
	 */
	else if (! cc) 
	    return(STATUS_INVALID);

	tmp += cc;
	packetLen -= cc;


	/* if non-blocking, we're done. */
	if (nonBlocking && packetLen) {
	    port->nBytes += cc;
	    return(STATUS_NOT_DONE);
	}
    }

    *connIdP = htonl(hdr->connId);
    port->nBytes = 0;
    return(STATUS_OK);
}

/*
 * PacketSend -- send a single-packet message.
 *
 * RETURNS: STATUS_ERROR if the write fails, STATUS_OK otherwise.
 * SIDE_EFFECTS: may block.
 * NOTES: Non-blocking writes would significantly complicate 
 *	buffer management.  For now, we're not going to do it.
 *
 * XXX this code is called in exactly one place
 */
PacketSend(port, conn, buf, type, len, nonBlocking)
    Port	*port;
    Connection	*conn;
    PacketBuf	*buf;
    MsgType	type;
    PacketLen	len;
    Boolean	nonBlocking;
{
    int		status;
    PacketLen	totalLen;
    PacketHdr	*hdr = (PacketHdr *) buf;
    int		addrLen = sizeof(struct sockaddr_in);

    Assert(!nonBlocking);
    Assert(buf);

    totalLen = len;

    hdr->seqno = htonl(conn->seqno++);
    hdr->connId = htonl(conn->id);
    hdr->type = (MsgType)htonl(type);
    hdr->len = htonl(len);

    len = sendto(port->sock, (Addr) buf, totalLen, /* flags */ 0,
		 &(port->raddr), addrLen);

    if (len < totalLen) {
	(void) sprintf(PQerrormsg,
		       "FATAL: PacketSend: couldn't send complete packet: errno=%d\n",
		       errno);
	fputs(PQerrormsg, stderr);
	return(STATUS_ERROR);
    }

    return(STATUS_OK);
}

/*
 * PacketData -- return components of msg hdr.
 *
 * Clients of this module don't know what the headers look
 * like.  
 *
 * NOTE: seqnoP is an IN/OUT parameter.  The others are out only.
 *
 * XXX this code is called in exactly one place
 */
PacketData(buf, dataP, bufSizeP, msgTypeP, seqnoP)
    Addr	buf;
    Addr 	*dataP;
    int  	*bufSizeP;
    MsgType	*msgTypeP;
    SeqNo	*seqnoP;
{
    PacketHdr	*hdr = (PacketHdr *) buf;

    /* seq pack protocol only */
    if (htonl(hdr->seqno) != *seqnoP)
	*msgTypeP = DUPLICATE_MSG;
    else
	*seqnoP++;

    *msgTypeP = (MsgType)htonl(hdr->type);
    *bufSizeP = htonl(hdr->len);

    /* data part starts right after the header */
    *dataP = (Addr) (hdr+1);
}
