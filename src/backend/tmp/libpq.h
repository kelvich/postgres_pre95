/* ----------------------------------------------------------------
 *   FILE
 *	libpq.h
 *
 *   DESCRIPTION
 *	POSTGRES LIBPQ buffer structure definitions.
 *
 *   NOTES
 *	This file contains definitions for structures and
 *	externs for functions used by both frontend applications
 *	and the POSTGRES backend.  See the files libpq-fe.h and
 *	libpq-be.h for frontend/backend specific information
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef LibpqIncluded 	/* include this file only once. */
#define LibpqIncluded 	1

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "tmp/simplelists.h"
#include "utils/exc.h"
#include "tmp/postgres.h"

#include "tmp/pqcomm.h"

/* ----------------
 * PQArgBlock --
 *	Information (pointer to array of this structure) required
 *	for the PQfn() call.
 * ----------------
 */
typedef struct {
    int len;
    int isint;
    union {
        int *ptr;	/* can't use void (dec compiler barfs)	*/
	int integer;
    } u;
} PQArgBlock;

/* ----------------
 * TypeBlock --
 * 	Information about an attribute.
 * ----------------
 */
#define NameLength 16

typedef struct TypeBlock {
    char name[NameLength];	/* name of the attribute */
    int adtid;			/* adtid of the type */
    int adtsize;		/* adtsize of the type */
} TypeBlock;

/* ----------------
 * TupleBlock --
 *	Data of a tuple.
 * ----------------
 */
#define TupleBlockSize 100

typedef struct TupleBlock {
    char **values[TupleBlockSize];	/* an array of tuples */
    int *lengths[TupleBlockSize];       /* an array of length vec. foreach
					   tuple */
    struct TupleBlock *next;		/* next tuple block */
    int    tuple_index;			/* current tuple index */
} TupleBlock;

/* ----------------
 * GroupBuffer --
 * 	A group of tuples with the same attributes.
 * ----------------
 */
typedef struct GroupBuffer {
    int no_tuples;		/* number of tuples in this group */
    int no_fields;		/* number of attributes */
    TypeBlock *types;  		/* types of the attributes */
    TupleBlock *tuples;		/* tuples in this group */
    struct GroupBuffer *next;	/* next group */
} GroupBuffer;

/* ----------------
 * PortalBuffer --
 *	Data structure of a portal buffer.  
 * ----------------
 */
typedef struct PortalBuffer {
    int rule_p;			/* 1 if this is an asynchronized portal. */
    int no_tuples;		/* number of tuples in this portal buffer */
    int no_groups;		/* number of tuple groups */
    GroupBuffer *groups;	/* linked list of tuple groups */
} PortalBuffer;

/* ----------------
 * PortalEntry --
 *	an entry in the global portal table
 *
 * Note: the portalcxt is only meaningful for PQcalls made from
 *       within a postgres backend.  frontend apps should ignore it.
 * ----------------
 */
#define PortalNameLength 32

typedef struct PortalEntry {
    char 	  name[PortalNameLength]; /* name of this portal */
    PortalBuffer  *portal;	          /* tuples contained in this portal */
    Pointer	  portalcxt;	          /* memory context (for backend) */
    Pointer	  result;	          /* result for PQexec */
} PortalEntry;

/* #define MAXPORTALS 10 */
/* the portals[] array should not be statically sized to MAXPORTALS,         */
/* now portals[] starts off at PORTALS_INITIAL_SIZE, and grows dynamically  */
   
#define PORTALS_INITIAL_SIZE 32
#define PORTALS_GROW_BY      32

extern PortalEntry** portals;
extern size_t portals_array_size;

/*
 *  Asynchronous notification
 */
typedef struct PQNotifyList {
    char relname[NAMEDATALEN];	/* name of relation containing data */
    int be_pid;			/* process id of backend */
    int valid;			/* has this already been handled by user. */
    SLNode Node;
} PQNotifyList;

/*
 * Exceptions.
 */

#define libpq_raise(X, Y) ExcRaise((Exception *)(X), (ExcDetail) (Y),\
				   (ExcData)0, (ExcMessage) 0)

extern Exception MemoryError, PortalError, PostquelError, ProtocolError;

/* 
 * POSTGRES backend dependent Constants. 
 */

#define initstr_length 256
#define error_msg_length 4096
#define command_length 20
#define remark_length 80
#define portal_name_length 16

extern    char PQerrormsg[error_msg_length];
/*
 * External functions.
 */
/* portal.c */
extern void pqdebug ARGS((char *target, char *msg));
extern void pqdebug2 ARGS((char *target, char *msg1, char *msg2));
extern void PQtrace ARGS((void));
extern void PQuntrace ARGS((void));
extern int PQnportals ARGS((int rule_p));
extern void PQpnames ARGS((char **pnames, int rule_p));
extern PortalBuffer *PQparray ARGS((char *pname));
extern int PQrulep ARGS((PortalBuffer *portal));
extern int PQntuples ARGS((PortalBuffer *portal));
extern int PQninstances ARGS((PortalBuffer *portal));
extern int PQngroups ARGS((PortalBuffer *portal));
extern int PQntuplesGroup ARGS((PortalBuffer *portal, int group_index));
extern int PQninstancesGroup ARGS((PortalBuffer *portal, int group_index));
extern int PQnfieldsGroup ARGS((PortalBuffer *portal, int group_index));
extern int PQfnumberGroup ARGS((PortalBuffer *portal, int group_index, char *field_name));
extern char *PQfnameGroup ARGS((PortalBuffer *portal, int group_index, int field_number));
extern GroupBuffer *PQgroup ARGS((PortalBuffer *portal, int tuple_index));
extern int PQgetgroup ARGS((PortalBuffer *portal, int tuple_index));
extern int PQnfields ARGS((PortalBuffer *portal, int tuple_index));
extern int PQfnumber ARGS((PortalBuffer *portal, int tuple_index, char *field_name));
extern char *PQfname ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern int PQftype ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern int PQsametype ARGS((PortalBuffer *portal, int tuple_index1, int tuple_index2));
extern char *PQgetvalue ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern int PQgetlength ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern void PQclear ARGS((char *pname));
extern void PQcleanNotify ARGS((void));
extern void PQnotifies_init ARGS((void));
extern PQNotifyList *PQnotifies ARGS((void));
extern void PQremoveNotify ARGS((PQNotifyList *nPtr));
extern void PQappendNotify ARGS((char *relname, int pid));

/* portalbuf.c */
extern caddr_t pbuf_alloc ARGS((size_t size));
extern void pbuf_free ARGS((caddr_t pointer));
extern PortalBuffer *pbuf_addPortal ARGS((void));
extern GroupBuffer *pbuf_addGroup ARGS((PortalBuffer *portal));
extern TypeBlock *pbuf_addTypes ARGS((int n));
extern TupleBlock *pbuf_addTuples ARGS((void));
extern char **pbuf_addTuple ARGS((int n));
extern int *pbuf_addTupleValueLengths ARGS((int n));
extern char *pbuf_addValues ARGS((int n));
extern PortalEntry *pbuf_addEntry ARGS((void));
extern void pbuf_freeEntry ARGS((int i));
extern void pbuf_freeTypes ARGS((TypeBlock *types));
extern void pbuf_freeTuples ARGS((TupleBlock *tuples, int no_tuples, int no_fields));
extern void pbuf_freeGroup ARGS((GroupBuffer *group));
extern void pbuf_freePortal ARGS((PortalBuffer *portal));
extern int pbuf_getIndex ARGS((char *pname));
extern void pbuf_setportalinfo ARGS((PortalEntry *entry, char *pname));
extern PortalEntry *pbuf_setup ARGS((char *pname));
extern void pbuf_close ARGS((char *pname));
extern GroupBuffer *pbuf_findGroup ARGS((PortalBuffer *portal, int group_index));
extern int pbuf_findFnumber ARGS((GroupBuffer *group, char *field_name));
extern void pbuf_checkFnumber ARGS((GroupBuffer *group, int field_number));
extern char *pbuf_findFname ARGS((GroupBuffer *group, int field_number));

/* pqcomm.c */
extern void pq_init ARGS((int fd));
extern void pq_gettty ARGS((char *tp));
extern int pq_getport ARGS((void));
extern void pq_close ARGS((void));
extern void pq_flush ARGS((void));
extern int pq_getstr ARGS((char *s, int maxlen));
extern int PQgetline ARGS((char *s, int maxlen));
extern int PQputline ARGS((char *s));
extern int pq_getnchar ARGS((char *s, int off, int maxlen));
extern int pq_getint ARGS((int b));
extern void pq_putstr ARGS((char *s));
extern void pq_putnchar ARGS((char *s, int n));
extern void pq_putint ARGS((int i, int b));
extern int pq_sendoob ARGS((char *msg, int len));
extern int pq_recvoob ARGS((char *msgPtr, int *lenPtr));
extern int pq_getinaddr ARGS((struct sockaddr_in *sin, char *host, int port));
extern int pq_getinserv ARGS((struct sockaddr_in *sin, char *host, char *serv));
extern int pq_connect ARGS((char *dbname, char *user, char *args, char *hostName, char *debugTty, char *execFile, int portName));
extern void pq_regoob ARGS((void (*fptr)()));
extern void pq_unregoob ARGS((void));
extern void pq_async_notify ARGS((void));
extern int StreamServerPort ARGS((char *hostName, int portName, int *fdP));
extern int StreamConnection ARGS((int server_fd, Port *port));
extern int StreamClose ARGS((int sock));
extern int StreamOpen ARGS((char *hostName, int portName, Port *port));

#endif /* LibpqIncluded */
