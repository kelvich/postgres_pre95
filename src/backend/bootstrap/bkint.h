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

#include "tmp/postgres.h"

#include "access/htup.h"
#include "access/relscan.h"
#include "access/skey.h"
#include "access/tqual.h"
#include "access/xcxt.h"
#include "storage/buf.h"
#include "storage/bufmgr.h"	/* for BufferManagerFlush */
#include "tmp/portal.h"
#include "utils/log.h"
#include "utils/rel.h"

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
	TY_OID8, TY_SMGR
} TYPE;
#define	TY_LAST	TY_SMGR

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
extern          int Int_yylval;

/* ami_sup.c */

extern Relation reldesc;
extern char relname[80];
extern int numattr;          /* number of attributes for the new reln */

extern struct attribute *attrtypes[MAXATTR];

extern Portal BlankPortal;


/* 
 * bootstrap.c - prototypes
 */
void err ARGS((void ));
void BootstrapMain ARGS((int ac , char *av []));
void createrel ARGS((char *name ));
void boot_openrel ARGS((char *name ));
void closerel ARGS((char *name ));
void printrel ARGS((void ));
void randomprintrel ARGS((void ));
void showtup ARGS((HeapTuple tuple , Buffer buffer , Relation relation ));
void showtime ARGS((Time time ));
void DefineAttr ARGS((char *name , char *type , int attnum ));
void InsertOneTuple ARGS((ObjectId objectid ));
void InsertOneValue ARGS((ObjectId objectid , char *value , int i ));
void InsertOneNull ARGS((int i ));
void defineindex ARGS((char *heapName , char *indexName , char *accessMethodName ));
void handletime ARGS((void ));
void cleanup ARGS((void ));
int gettype ARGS((char *type ));
struct attribute *AllocateAttribute ARGS((void ));
unsigned char MapEscape ARGS((char **s ));
int EnterString ARGS((char *str ));
char *LexIDStr ARGS((int ident_num ));
int CompHash ARGS((char *str , int len ));
hashnode *FindStr ARGS((char *str , int length , hashnode *mderef ));
hashnode *AddStr ARGS((char *str , int strlength , int mderef ));
void printhashtable ARGS((void ));
void printstrtable ARGS((void ));
char *emalloc ARGS((unsigned size ));
int LookUpMacro ARGS((char *xmacro ));
void DefineMacro ARGS((int indx1 , int indx2 ));
void printmacros ARGS((void ));
/*
 * after sed runs on the lexer's output these funcs are defined
 */
int Int_yywrap ARGS((void));
int Int_yyerror ARGS((char *str));
