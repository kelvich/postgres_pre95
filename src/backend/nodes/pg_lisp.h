/* ----------------------------------------------------------------
 *   FILE
 *	pg_lisp.h
 *	
 *   DESCRIPTION
 *	C definitions to simulate LISP structures
 *
 *   NOTES
 *	Prior to version 1, this file was only used by the C portions
 *	of the postgres code (esp. the parser) to allow that code to
 *	function independently of allegro common lisp.  When V1 was
 *	ported to C, this file became the place where portions of
 *	the allegro lisp functionality was moved.  More specifically,
 *	the lisp list structure simulation stuff is here.
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#ifndef	LispDepIncluded
#define	LispDepIncluded

#include <stdio.h>
#include "tmp/c.h"
#include "nodes/nodes.h"
#include "tmp/stringinfo.h"

/* ----------------------------------------------------------------
 *	Node Function Declarations
 *
 *  All of these #defines indicate that we have written print/equal/copy
 *  support for the classes named.  The print routines are in
 *  lib/C/printfuncs.c, the equal functions are in lib/C/equalfincs.c and
 *  the copy functions can be found in lib/C/copyfuncs.c
 *
 *  An interface routine is generated automatically by Gen_creator.sh for
 *  each node type.  This routine will call either do nothing or call
 *  an _print, _equal or _copy function defined in one of the above
 *  files, depending on whether or not the appropriate #define is specified.
 *
 *  Thus, when adding a new node type, you have to add a set of
 *  _print, _equal and _copy functions to the above files and then
 *  add some #defines below.
 *
 *  This is pretty complicated, and a better-designed system needs to be
 *  implemented.
 * ----------------------------------------------------------------
 */

/* ----------------
 *	Node Copy Function declarations
 * ----------------
 */
#define	CopyLispValueExists
#define	CopyLispSymbolExists
#define	CopyLispListExists
#define	CopyLispIntExists
#define	CopyLispFloatExists
#define	CopyLispVectorExists
#define	CopyLispStrExists

extern bool	CopyLispValue();
extern bool	CopyLispSymbol();
extern bool	CopyLispList();
extern bool	CopyLispInt();
extern bool	CopyLispFloat();
extern bool	CopyLispVector();
extern bool	CopyLispStr();

/* ----------------------------------------------------------------
 *			node definitions
 * ----------------------------------------------------------------
 */

/* ----------------
 *	vectori definition used in LispValue
 * ----------------
 */
struct vectori {
	int	size;
	char	data[1];		/* variable length array */
};

/* ----------------
 *	LispValue definition -- class used to support lisp structures
 *	in C.  This is here because we did not want to totally rewrite
 *	planner and executor code which depended on lisp structures when
 *	we ported postgres V1 from lisp to C. -cim 4/23/90
 * ----------------
 */
typedef union { 
    char			*name;	/* symbol */
    char   			*str;	/* string */ 
    int    			fixnum; 
    double 			flonum; 
    struct _LispValue		*car;	/* dotted pair */ 
    struct vectori		*veci; 
} LispValueUnion;
    
class (LispValue) public (Node) {
#define LispValueDefs \
    inherits0(Node); \
    LispValueUnion	val; \
    struct _LispValue	*cdr 
/* private: */
    LispValueDefs;	
/* public: */
};

/* ----------------
 *	"List" abbreviation for LispValue -- basically used anywhere
 *	we expect to deal with list data.
 * ----------------
 */
#define List LispValue

/* ----------------
 *	arrays of LispValues are needed in execnodes.h
 *	-cim 8/29/89
 * ----------------
 */
typedef LispValue *LispValuePtr;
#define	ListPtr	 LispValuePtr

/*
 * 	Global declaration for LispNil.
 */
#define LispNil 	((LispValue) NULL)

/*
 *	Global declaration for LispTrue.
 */
#define LispTrue 	((LispValue) 1)

/* ----------------
 *	dummy classes
 *
 *	these next node classes are actually only used for
 *	the tags generated.. the classes themselves are bogus.
 * ----------------
 */
class (LispSymbol) public (Node) {
    inherits0(Node);
};

class (LispList) public (Node) {
    inherits0(Node);
};

class (LispInt) public (Node) {
    inherits0(Node);
};

class (LispFloat) public (Node) {
    inherits0(Node);
};

class (LispVector) public (Node) {
    inherits0(Node);
};

class (LispStr) public (Node) {
    inherits0(Node);
};

/* ----------------
 *	macros and defines which depend on dummy class tags
 * ----------------
 */
#define	PGLISP_ATOM	classTag(LispSymbol)
#define	PGLISP_DTPR	classTag(LispList)
#define	PGLISP_FLOAT	classTag(LispFloat)
#define	PGLISP_INT	classTag(LispInt)
#define	PGLISP_STR	classTag(LispStr)
#define	PGLISP_VECI	classTag(LispVector)

#define	LISP_BYTEVECTOR	classTag(LispVector)
#define	LISP_DOUBLE	classTag(LispFloat)
#define	LISP_INTEGER	classTag(LispInt)
#define	LISP_STRING	classTag(LispStr)

/* ----------------
 *	lisp value accessor macros
 * ----------------
 */
#define	CAR(LISPVALUE)			((LISPVALUE)->val.car)
#define	CDR(LISPVALUE)			((LISPVALUE)->cdr)
#define CAAR(lv)                	(CAR(CAR(lv)))
#define CADR(lv)			(CAR(CDR(lv)))
#define CADDR(lv)			(CAR(CDR(CDR(lv))))
#define	LISPVALUE_DOUBLE(LISPVALUE)	((LISPVALUE)->val.flonum)
#define	LISPVALUE_INTEGER(LISPVALUE)	((LISPVALUE)->val.fixnum)
#define	LISPVALUE_STRING(LISPVALUE)	((LISPVALUE)->val.str)
#define	LISPVALUE_SYMBOL(LISPVALUE)	((LISPVALUE)->val.name)
#define	LISPVALUE_VECI(LISPVALUE)	((LISPVALUE)->val.veci)
#define	LISPVALUE_BYTEVECTOR(LISPVALUE)	((LISPVALUE)->val.veci->data)
#define	LISPVALUE_VECTORSIZE(LISPVALUE)	((LISPVALUE)->val.veci->size)

#define	LISP_TYPE(LISPVALUE)		((LISPVALUE)->type)

/* ----------------
 *	predicates
 * ----------------
 */
#define lispNullp(p) ((List)(p) == LispNil)

#define null(p) \
    ((bool) lispNullp(p))

#define consp(x) \
    ((x) ? (bool)(LISP_TYPE(x) == PGLISP_DTPR) : false)

#define listp(x) (lispNullp(x) || consp(x))

#define lispStringp(x) \
    ((x) ? (bool)(LISP_TYPE(x) == PGLISP_STR) : false)

#define stringp(foo) lispStringp(foo)

#define lispIntegerp(x) \
    ((x) ? (bool)(LISP_TYPE(x) == PGLISP_INT) : false)

#define integerp(foo) lispIntegerp(foo)

#define lispAtomp(x) \
    ((x) ? (bool)(LISP_TYPE(x) == PGLISP_ATOM) : false)

#define atom(foo) lispAtomp(foo)

#define floatp(x) \
    ((x) ? (bool)(LISP_TYPE(x) == PGLISP_FLOAT) : false)

#define numberp(foo) \
   ((bool) (integerp(foo) || floatp(foo)))

/* ----------------
 *	lispAlloc
 * ----------------
 */
#define lispAlloc() (LispValue) palloc(sizeof(classObj(LispValue)))

/* ----------------
 *	lisp support macros
 * ----------------
 */
#define eq(foo,bar) ((bool)((foo) == (bar)))

#define nth(index,list)         CAR(nthCdr(index,list))

#define foreach(_elt_,_list_)	for(_elt_=(LispValue)_list_; \
_elt_!=LispNil;_elt_=CDR(_elt_))

/* sigh, used so often, I'm lazy to do global search & replace -jeff*/
#define cons(x,y) lispCons(x,y)

/* ----------------
 *	obsolete garbage.  this should go away -cim 4/23/90
 * ----------------
 */
#define	LISP_GC_OFF		/* yow! */
#define	LISP_GC_ON		/* yow! */
#define	LISP_GC_PROTECT(X)	/* yow! */

/* ----------------
 *	extern definitions
 * ----------------
 */
extern LispValue lispAtom ARGS((char *atomName ));
extern LispValue lispDottedPair ARGS((void ));
extern LispValue lispFloat ARGS((double floatValue ));
extern LispValue lispInteger ARGS((int integerValue ));
extern LispValue lispName ARGS((char *string ));
extern LispValue lispString ARGS((char *string ));
extern LispValue lispVectori ARGS((int nBytes ));
extern LispValue evalList ARGS((LispValue list ));
extern LispValue quote ARGS((LispValue lispObject ));

extern LispValue lispList ARGS((void ));
extern LispValue lispCons ARGS((LispValue lispObject1 , LispValue lispObject2 ));
extern LispValue nappend1 ARGS((LispValue list , LispValue lispObject ));
extern LispValue append1 ARGS((LispValue list , LispValue lispObject ));
extern LispValue car ARGS((LispValue dottedPair ));
extern LispValue cdr ARGS((LispValue dottedPair ));
extern LispValue rplaca ARGS((LispValue dottedPair , LispValue newValue ));
extern LispValue rplacd ARGS((LispValue dottedPair , LispValue newValue ));

extern int init_list ARGS((LispValue list , LispValue newValue ));
extern LispValue append ARGS((LispValue list , LispValue lispObject ));
extern int length ARGS((LispValue list ));
extern LispValue nthCdr ARGS((int index , LispValue list ));
extern LispValue nconc ARGS((LispValue list1 , LispValue list2 ));
extern LispValue nreverse ARGS((LispValue list ));
extern int position ARGS((LispValue foo , List bar ));
extern bool member ARGS((LispValue foo , List bar ));
extern LispValue remove_duplicates ARGS((List foo , bool (*test )()));
extern LispValue find_if_not ARGS((bool (*pred )(), LispValue bar ));
extern LispValue LispDelete ARGS((LispValue foo , List bar ));
extern LispValue setf ARGS((LispValue foo , LispValue bar ));
extern LispValue LispRemove ARGS((LispValue foo , List bar ));
extern List nLispRemove ARGS((List foo , LispValue bar ));
extern LispValue set_difference ARGS((LispValue foo , LispValue bar ));
extern List nset_difference ARGS((List foo , List bar ));
extern LispValue push ARGS((LispValue foo , List bar ));
extern LispValue last ARGS((LispValue foo ));
extern LispValue LispUnion ARGS((LispValue foo , LispValue bar ));
extern LispValue mapcar ARGS((void (*foo )(), LispValue bar ));
extern bool zerop ARGS((LispValue foo ));
extern LispValue lispArray ARGS((int foo ));
extern List number_list ARGS((int start , int n ));
extern LispValue apply ARGS((LispValue (*foo )(), LispValue bar ));
extern LispValue find_if ARGS((bool (*pred )(), LispValue bar ));
extern LispValue find ARGS((LispValue foo , LispValue bar , bool (*test )(), Node (*key )()));
extern LispValue some ARGS((bool (*foo )(), LispValue bar ));
extern LispValue sort ARGS((LispValue foo ));
extern double expt ARGS((double foo ));
extern bool same ARGS((LispValue foo , LispValue bar ));

extern bool equal ARGS((Node a , Node b ));

extern LispValue collect ARGS((bool (*pred )(), LispValue list ));
extern LispValue last_element ARGS((LispValue list ));

extern char *CString ARGS((LispValue lstr ));
extern int CAtom ARGS((LispValue lv ));
extern double CDouble ARGS((LispValue lval ));
extern int CInteger ARGS((LispValue lval ));

extern LispValue ppreserve ARGS((char *pallocObject ));
extern LispValue lppreserve ARGS((LispValue pallocObject ));
extern char *prestore ARGS((char *ppreservedObject ));
extern LispValue lprestore ARGS((LispValue ppreservedObject ));

/*===============================
 * in/out print/read functions...
 *===============================
 */

/*---------------------------
 * lispDisplayFp
 * print a lisp structure in the given file.
 */
extern 
void
lispDisplayFp ARGS((
	FILE		*fp,
	LispValue	lispObject
));

/*---------------------------
 * lispDisplay
 * print a lisp structure in stdout, & flush it!
 */
extern
void
lispDisplay ARGS((
	LispValue	lispObject
));

/*---------------------------
 * lispOut
 * given a lisp structure create & return a string holding
 * its ascii representation
 */
extern
char *
lispOut ARGS((
	LispValue	lispObject
));

/*---------------------------
 * lispToStringInfo
 * given a lisp structure, fill the given 'StringInfo' with its
 * ascii representation.
 */
extern
void
_outLispValue  ARGS((
	StringInfo	str,
	LispValue	lispObject
));

#endif /* !LispDepIncluded */
