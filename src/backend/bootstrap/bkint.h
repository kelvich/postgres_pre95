/* ----------------------------------------------------------------
 *   FILE
 *	bkint.h
 *
 *   DESCRIPTION
 *	include file for the bootstrapping code
 *
 *   NOTES
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef bootstrapIncluded		/* include this file only once */
#define bootstrapIncluded	1

#include <sys/file.h>
#include <stdio.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>

#include "tmp/postgres.h"

#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"	/* for BufferManagerFlush */
#include "tmp/portal.h"
#include "utils/log.h"
#include "utils/rel.h"

#define ALLOC(t, c)	(t *)calloc((unsigned)(c), sizeof(t))

#define FIRST_TYPE_OID 16	/* OID of the first type */
#define	MAXATTR 40		/* max. number of attributes in a relation */

/* ami_lexer.l */
#define STRTABLESIZE	10000
#define HASHTABLESIZE	503

/* Hash function numbers */
#define NUM	23
#define	NUMSQR	529
#define	NUMCUBE	12167

typedef struct hashnode {
	int		strnum;		/* Index into string table */
	struct hashnode	*next;
} hashnode;

typedef struct mcro {
  char *s;
  int  mderef;
} macro;

#define EMITPROMPT printf("> ")

/* ami_sup.c */

extern Relation reldesc;
extern char relname[80];
extern int numattr;          /* number of attributes for the new reln */

extern struct attribute *attrtypes[MAXATTR];

extern Portal BlankPortal;


/*
 *	prototypes for bootstrap.c.
 *	Automatically generated using mkproto
 */
extern void err ARGS((void));
extern void BootstrapMain ARGS((int ac, char *av[]));
extern void createrel ARGS((char *name));
extern void boot_openrel ARGS((char *name));
extern void closerel ARGS((char *name));
extern void printrel ARGS((void));
extern void randomprintrel ARGS((void));
extern void showtup ARGS((HeapTuple tuple, Buffer buffer, Relation relation));
extern void showtime ARGS((AbsoluteTime time));
extern void DefineAttr ARGS((char *name, char *type, int attnum));
extern void InsertOneTuple ARGS((ObjectId objectid));
extern void InsertOneValue ARGS((ObjectId objectid, char *value, int i));
extern void InsertOneNull ARGS((int i));
extern void defineindex ARGS((char *heapName, char *indexName, char *accessMethodName, List attributeList));
extern bool BootstrapAlreadySeen ARGS((ObjectId id));
extern void cleanup ARGS((void));
extern int gettype ARGS((char *type));
extern struct attribute *AllocateAttribute ARGS((void));
extern unsigned char MapEscape ARGS((char **s));
extern int EnterString ARGS((char *str));
extern char *LexIDStr ARGS((int ident_num));
extern int CompHash ARGS((char *str, int len));
extern hashnode *FindStr ARGS((char *str, int length, hashnode *mderef));
extern hashnode *AddStr ARGS((char *str, int strlength, int mderef));
extern void printhashtable ARGS((void));
extern void printstrtable ARGS((void));
extern char *emalloc ARGS((unsigned size));
extern int LookUpMacro ARGS((char *xmacro));
extern void DefineMacro ARGS((int indx1, int indx2));
extern void printmacros ARGS((void));
extern int index_register ARGS((Name heap, Name ind, AttributeNumber natts, AttributeNumber *attnos, uint16 nparams, Datum *params, FuncIndexInfo *finfo, LispValue predInfo));
extern int build_indices ARGS((void));

#endif /* bootstrapIncluded */
