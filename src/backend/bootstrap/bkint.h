static	char ami_h[] = "$Header$";
/**********************************************************************
  ami.h

  include file for ami_parser.y ami_lexer.l ami_code.c

 **********************************************************************/

#include <sys/file.h>
#include <stdio.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>

#include "postgres.h"

#include "bufmgr.h"	/* for BufferManagerFlush */
#include "buf.h"
#include "log.h"
#include "htup.h"
#include "portal.h"
#include "rel.h"
#include "relscan.h"
#include "skey.h"
#include "tim.h"
#include "tqual.h"
#include "xcxt.h"
#include "xid.h"

extern char *calloc();
#define ALLOC(t, c)	(t *)calloc((unsigned)(c), sizeof(t))

#define FIRST_TYPE_OID 16	/* OID of the first type */
#define	MAXATTR 40		/* max. number of attributes in arelation */

typedef	enum {
	Q_OID, Q_LEN, Q_DYN, Q_IN, Q_OUT, Q_EQ, Q_LT
} QUERY;

/* types recognized */
typedef	enum	{
	TY_BOOL, TY_BYTEA, TY_CHAR, TY_CHAR16, TY_DT, TY_INT2, TY_INT28,
	TY_INT4, TY_REGPROC, TY_TEXT, TY_OID, TY_TID, TY_XID, TY_IID,
	TY_OID8
} TYPE;
#define	TY_LAST	TY_OID8

/* ami_lexer.l */
#define STRTABLESIZE	10000
#define HASHTABLESIZE	503
#define	MAXSTRINGSIZE	128

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

char		*strcpy(), *strncpy(), *strcat(), *strncat();
char		*sprintf();
char		*emalloc();
hashnode	*FindStr(), *AddStr();
extern          int Int_yylval;

/* ami_sup.c */

extern Relation reldesc;
extern char relname[80];
extern int numattr;          /* number of attributes for the new reln */

extern struct attribute *attrtypes[MAXATTR];

extern Portal BlankPortal;

extern void createrel	ARGS((char *));
extern void openrel	ARGS((char *));
extern void closerel	ARGS((char *));
extern void printrel	ARGS((void));
extern void randomprintrel ARGS((void));
extern void showtup	ARGS((HeapTuple, Buffer, Relation));

extern void showtime	ARGS((Time));
extern void DefineAttr ARGS((char *, char *, int));
extern void InsertOneTuple ARGS((ObjectId));
extern void InsertOneValue ARGS((ObjectId, int, char *));
extern void handletime ARGS((void));
extern void cleanup	ARGS((void));
extern int gettype	ARGS((char *));
extern struct attribute * AllocateAttribute ARGS((void));
extern void err	ARGS((void));
extern void handle_warn ARGS((void));
extern void die		ARGS((void));
extern void ExcAbort 	ARGS((Exception *, ExcDetail, ExcData, ExcMessage));
extern unsigned char MapEscape ARGS((char **));
extern int  EnterString ARGS((char *));
extern char *LexIDStr	ARGS((int));
extern int  CompHash	ARGS((char *, int));
extern hashnode *FindStr ARGS((char *, int, hashnode *));
extern hashnode *AddStr ARGS((char *, int, int));
extern void printhashtable ARGS((void));
extern void printstrtable ARGS((void));
extern char *emalloc	ARGS((unsigned));
extern int LookUpMacro ARGS((char *));
extern void printmacros ARGS((void));
extern void DefineMacro ARGS((int, int));
extern void AddAttr	ARGS((char *));
extern int  LexIDLen	ARGS((int));
extern char LexIDChar	ARGS((int, int));

