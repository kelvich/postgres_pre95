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
#include "utils/exc.h"

/*
 * PQArgBlock --
 *	Information (pointer to array of this structure) required
 *	for the PQfn() call.
 */

typedef struct {
    int len;
    int isint;
    union {
        int *ptr;	/* can't use void (dec compiler barfs)	*/
	int integer;
    } u;
} PQArgBlock;

/* 
 * TypeBlock --
 * 	Information about an attribute.
 */
#define NameLength 16

typedef struct TypeBlock {
    char name[NameLength];	/* name of the attribute */
    int adtid;			/* adtid of the type */
    int adtsize;		/* adtsize of the type */
} TypeBlock;

/*
 * TupleBlock --
 *	Data of a tuple.
 */
#define TupleBlockSize 100

typedef struct TupleBlock {
    char **values[TupleBlockSize];	/* an array of tuples */
    struct TupleBlock *next;		/* next tuple block */
    int    tuple_index;			/* current tuple index */
} TupleBlock;

/*
 * GroupBuffer --
 * 	A group of tuples with the same attributes.
 */
typedef struct GroupBuffer {
    int no_tuples;		/* number of tuples in this group */
    int no_fields;		/* number of attributes */
    TypeBlock *types;  		/* types of the attributes */
    TupleBlock *tuples;		/* tuples in this group */
    struct GroupBuffer *next;	/* next group */
} GroupBuffer;

/* 
 * PortalBuffer --
 *	Data structure of a portal buffer.  
 */
typedef struct PortalBuffer {
    int rule_p;			/* 1 if this is an asynchronized portal. */
    int no_tuples;		/* number of tuples in this portal buffer */
    int no_groups;		/* number of tuple groups */
    GroupBuffer *groups;	/* tuple groups */
} PortalBuffer;

/*
 * Define global variables used by LIBPQ.
 */

typedef struct PortalEntry {
    char name[NameLength];
    PortalBuffer *portal;
} PortalEntry;

#define MAXPORTALS 10

extern PortalEntry *portals[];

/*
 * Exceptions.
 */

#define libpq_raise(X, Y) ExcRaise((X), (Y))

extern Exception MemoryError, PortalError, PostquelError, ProtocolError;

/* 
 * POSTGRES backend dependent Constants. 
 */

#define initstr_length 256
#define error_msg_length 80
#define command_length 20
#define remark_length 80
#define portal_name_length 16

/*
 * External functions.
 */
extern void pqdebug ARGS((char *target, char *msg));
extern void pqdebug2 ARGS((char *target, char *msg1, char *msg2));
extern void PQtrace ARGS(());
extern void PQuntrace ARGS(());
extern int PQnportals ARGS((int rule_p));
extern void PQpnames ARGS((int pnames, int rule_p));
extern PortalBuffer *PQparray ARGS((char *pname));
extern int PQrulep ARGS((PortalBuffer *portal));
extern int PQntuples ARGS((PortalBuffer *portal));
extern int PQngroups ARGS((PortalBuffer *portal));
extern int PQntuplesGroup ARGS((PortalBuffer *portal, int group_index));
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
extern void PQclear ARGS((char *pname));
extern caddr_t pbuf_alloc ARGS((size_t size));
extern void pbuf_free ARGS((caddr_t pointer));
extern PortalBuffer *pbuf_addPortal ARGS(());
extern GroupBuffer *pbuf_addGroup ARGS((PortalBuffer *portal));
extern TypeBlock *pbuf_addTypes ARGS((int n));
extern TupleBlock *pbuf_addTuples ARGS(());
extern char **pbuf_addTuple ARGS((int n));
extern char *pbuf_addValues ARGS((int n));
extern PortalEntry *pbuf_addEntry ARGS(());
extern void pbuf_freeEntry ARGS((int i));
extern void pbuf_freeTypes ARGS((TypeBlock *types));
extern void pbuf_freeTuples ARGS((int no_ tuples, int no_tuples, int no_fields));
extern void pbuf_freeGroup ARGS((GroupBuffer *group));
extern void pbuf_freePortal ARGS((PortalBuffer *portal));
extern int pbuf_getIndex ARGS((char *pname));
extern PortalEntry *pbuf_setup ARGS((char *pname));
extern void pbuf_close ARGS((char *pname));
extern GroupBuffer *pbuf_findGroup ARGS((PortalBuffer *portal, int group_index));
extern pbuf_findFnumber ARGS((GroupBuffer *group, char *field_name));
extern void pbuf_checkFnumber ARGS((GroupBuffer *group, int field_number));
extern char *pbuf_findFname ARGS((GroupBuffer *group, int field_number));
extern void pq_init ARGS((int fd));
extern void pq_gettty ARGS((int tp));
extern int pq_getport ARGS(());
extern void pq_close ARGS(());
extern void pq_flush ARGS(());
extern void pq_getstr ARGS((char *s, int maxlen));
extern void pq_getnchar ARGS((char *s, int off, int maxlen));
extern int pq_getint ARGS((int b));
extern void pq_putstr ARGS((char *s));
extern void pq_putnchar ARGS((char *s, int n));
extern void pq_putint ARGS((int i, int b));
extern int pq_getinaddr ARGS((struct sockaddr_in *sin, char *host, int port));
extern int pq_getinserv ARGS((struct sockaddr_in *sin, char *host, char *serv));
extern int pq_connect ARGS((char *host, char *port));
extern int pq_accept ARGS(());
    
#endif LibpqIncluded
