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

#include "exception.h"

/*
 * PQArgBlock --
 *	Information (pointer to array of this structure) required
 *	for the PQfn() call.
 */

typedef struct {
    int len;
    int isint;
    union {
        void *ptr;
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

#define libpq_raise(X, Y)	raise4((X), 0, NULL, (Y))

extern Exception MemoryError, PortalError, PostquelError, ProtocolError;

/* 
 * POSTGRES backend dependent Constants. 
 */

#define initstr_length 256
#define error_msg_length 80
#define portal_name_length 16
#define command_length 20
#define remark_length 80
#define portal_name_length 16

/*
 * External functions.
 */
extern PortalBuffer *addPortal ();
extern GroupBuffer *addGroup ();
extern TypeBlock *addTypes ();
extern TupleBlock *addTuples ();
extern char **addTuple ();
extern char *addValues ();
extern PortalEntry *addPortalEntry ();
extern void freePortalEntry ();
extern void freePortal ();
extern PortalBuffer *portal_setup ();
extern void portal_close ();
extern int PQnportals ();
extern void PQpnames ();
extern PortalBuffer *PQparray ();
extern int PQrulep ();
extern int PQntuples ();
extern int PQngroups ();
extern int PQntuplesGroup ();
extern int PQnfieldsGroup ();
extern int PQfnumberGroup ();
extern char *PQfnameGroup ();
extern int PQftypeGroup ();
extern int PQnfields ();
extern int PQfnumber ();
extern char *PQfname ();
extern int PQftype ();
extern int PQsametype ();
extern int PQgetgroup ();
extern char *PQgetvalue ();
extern char *PQdb ();
extern void PQsetdb ();
extern void PQreset ();
extern void PQfinish ();
extern void PQtrace ();
extern void PQuntrace ();
extern void PQerror ();
extern char *PQexec ();

#endif	/* ifndef LibpqIncluded */
