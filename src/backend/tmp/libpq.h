/* 
 * POSTGRES Data Base Management System
 * 
 * Copyright (c) 1989 Regents of the University of California
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for educational, research, and non-profit purposes and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of the University of California not be used in advertising
 * or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Permission to incorporate this
 * software into commercial products can be obtained from the Campus
 * Software Office, 295 Evans Hall, University of California, Berkeley,
 * Ca., 94720.  The University of California makes no representations
 * about the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */ 
/*
 * libpq.h -- 
 *	POSTGRES frontend LIBPQ buffer structure definitions.
 *
 * Identification:
 *	$Header$
 */

#ifndef LibpqIncluded 	/* include this file only once. */
#define LibpqIncluded 	1

#include <stdio.h>
#include <string.h>
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
extern PortalBuffer *addPortal ARGS((void));
extern GroupBuffer *addGroup ARGS((PortalBuffer *portal));
extern TypeBlock *addTypes ARGS((int n));
extern TupleBlock *addTuples ARGS((void));
extern char **addTuple ARGS((int n));
extern char *addValues ARGS((int n));
extern PortalEntry *addPortalEntry ARGS((void));
extern void freePortalEntry ARGS((int i));
extern void freePortal ARGS((PortalBuffer *portal));
extern PortalBuffer *portal_setup ARGS((char *pname));
extern void portal_close ARGS((char *pname));
extern int PQnportals ARGS((int rule_p));
extern void PQpnames ARGS((char **pnames, int rule_p));
extern PortalBuffer *PQparray ARGS((char *pname));
extern int PQrulep ARGS((PortalBuffer *portal));
extern int PQntuples ARGS((PortalBuffer *portal));
extern int PQngroups ARGS((PortalBuffer *portal));
extern int PQntuplesGroup ARGS((PortalBuffer *portal, int group_index));
extern int PQnfieldsGroup ARGS((PortalBuffer *portal, int group_index));
extern int PQfnumberGroup ARGS((PortalBuffer *portal, int group_index, char *field_name));
extern char *PQfnameGroup ARGS((PortalBuffer *portal, int group_index, int field_number));
extern int PQftypeGroup ();
extern int PQnfields ARGS((PortalBuffer *portal, int tuple_index));
extern int PQfnumber ARGS((PortalBuffer *portal, int tuple_index, char *field_name));
extern char *PQfname ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern int PQftype ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern int PQsametype ARGS((PortalBuffer *portal, int tuple_index1, int tuple_index2));
extern int PQgetgroup ARGS((PortalBuffer *portal, int tuple_index));
extern char *PQgetvalue ARGS((PortalBuffer *portal, int tuple_index, int field_number));
extern void PQerror ();

extern char *PQdb ARGS((void));
extern void PQsetdb ARGS((char *dbname));
extern void PQreset ARGS((void));
extern void PQfinish ARGS((void));
extern void PQtrace ARGS((void));
extern void PQuntrace ARGS((void));
extern void pqdebug ARGS((char *target, char *msg));
extern void pqdebug2 ARGS((char *target, char *msg1, char *msg2));
extern void read_initstr ARGS((void));
extern char *process_portal ARGS((int rule_p));
extern void read_remark ARGS((char id[]));
extern char *PQfn ARGS((int fnid, void *result_buf, int result_len, int result_is_int, PQArgBlock *args, int nargs));
extern char *PQexec ARGS((char *query));
extern void InitVacuumDemon ARGS((String host, String database, String terminal, String option, String port, String vacuum));


/*
 *The following internal variables of libqp can be accessed by the programmer:
 */
extern
char    *PQhost;                /* the machine on which POSTGRES backend
                                   is running */
extern
char    *PQport;         	/* the communication port with the
                                   POSTGRES backend */
extern
char    *PQtty;                 /* the tty on PQhost backend message
                                   is displayed */
extern
char    *PQoption;              /* the optional arguments to POSTGRES
                                   backend */
extern
char    *PQdatabase;            /* the POSTGRES database to access */

extern
int     PQportset;          	/* 1 if the communication with backend
                                   is set */
extern
int     PQxactid ;           	/* the transaction id of the current
                                   transaction */
extern
char    *PQinitstr;      	/* the initialization string passed
                                   to backend */
extern
int     PQtracep;           	/* 1 to print out debugging message */


#endif	/* ifndef LibpqIncluded */
