/*
 * pqpacket.c -- routines for reading and writing data packets
 *	sent/received by POSTGRES clients and servers
 *
 * $Header$
 *
 * This is the module that understands the lowest-level part
 * of the communication protocol.  All of the trickyness in
 * this module is for making sure that non-blocking IO in
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
 * Memory Allocation:  is not handled in this module
 *
 * IMPORTANT: these routines are called by backends, clients, and
 *	the Postmaster.
 *
 * NOTE: assume htonl() is sufficient for all header structs.  If
 *	header structs become longs, must switch to htonl().
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "tmp/postgres.h"
#include "utils/log.h"
#include "storage/ipci.h"
#include "tmp/pqcomm.h"


/*
 * PacketRecieve -- receive a packet on a port.
 *
 * RETURNS: connection id of the packet sender, if one
 * is available.
 */
PacketReceive(port, buf, nonBlocking, connIdP)
Port		*port;	/* receive port */
PacketBuf	*buf;	/* MAX_PACKET_SIZE-worth of buffer space */
Boolean		nonBlocking; /* NON_BLOCKING or BLOCKING i/o */
ConnId		*connIdP;    /* sender's connection (seqpack) */
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
    packetLen = htonl(hdr->len) + sizeof(PacketHdr);
  } else {
    cc = recvfrom(port->sock, tmp, sizeof(PacketHdr), flag, 
		  &(port->addr), &addrLen);
    if ( cc < sizeof(PacketHdr)) {
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

      /* great. got the header. now get the true length (including
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
		  &(port->addr), &addrLen);
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
 */
PacketSend(port, conn, buf, type, len, nonBlocking)
Port		*port;
Connection	*conn;
PacketBuf	*buf;
MsgType		type;
PacketLen	len;
Boolean		nonBlocking;
{
  int			status;
  PacketLen		totalLen;
  PacketHdr		*hdr = (PacketHdr *) buf;
  int			addrLen = sizeof (struct sockaddr_in);

  Assert( ! nonBlocking );
  Assert( buf );

  totalLen = len;

  hdr->seqno = htonl(conn->seqno++);
  hdr->connId = htonl(conn->id);
  hdr->type = (MsgType)htonl(type);
  hdr->len = htonl(len);

  len = sendto(port->sock, (Addr) buf, totalLen, /* flags */ 0,
	       &(port->addr), addrLen);

  if (len < totalLen) {
    fprintf(stderr,"PacketSend: couldn't send complete packet\n");
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
 */
PacketData(buf, dataP, bufSizeP, msgTypeP, seqnoP)
Addr		buf;
Addr 	*dataP;
int  		*bufSizeP;
MsgType		*msgTypeP;
SeqNo		*seqnoP;
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


/*
 * PacketRetransmit -- resend a buffer.  
 *
 * Assume that the header is already initialized.  We're
 * just going to resend the same message.
 */
PacketRetransmit(buf,port,addr)
Addr		buf;
Port		*port;
Addr		addr;
{
  PacketHdr 	*hdr = (PacketHdr *) buf;
  PacketLen	len,totalLen;
  int		addrLen = sizeof (struct sockaddr_in);

  len = totalLen = htonl(hdr->len);
  len = sendto(port->sock, (Addr) buf, totalLen, /* flags */ 0,
	       (struct sockaddr *)addr, addrLen);

  if (len < totalLen) {
    fprintf(stderr,"PacketWrite: couldn't send complete packet\n");
    return(STATUS_ERROR);
  }

  return(STATUS_OK);
}


/*
 * PacketBuf -- remove the header from a packet buf for
 * 	a client.
 *
 * NOTE: not sure this is necessary.  Some clients are going
 *	to have to know how big the thing is in order to do
 *	memory allocation.
 */
PacketBufInit(bufP, bufSizeP)
Addr 		*bufP;
PacketLen  	*bufSizeP;
{
  *bufP  += sizeof(PacketHdr);
  *bufSizeP -= sizeof(PacketHdr);
}

