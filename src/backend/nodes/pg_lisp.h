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
#include "nodes.h"

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
class (LispValue) public (Node) {
    /* private: */
    inherits(Node);
    union {
	char			*name;	/* symbol */
	char   			*str;	/* string */
	int    			fixnum;
	double 			flonum;
	struct _LispValue	*car;	/* dotted pair */
	struct vectori		*veci;
    } 			val;
    struct _LispValue	*cdr;
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

/* ----------------
 *	dummy classes
 *
 *	these next node classes are actually only used for
 *	the tags generated.. the classes themselves are bogus.
 * ----------------
 */
class (LispSymbol) public (Node) {
    inherits(Node);
};

class (LispList) public (Node) {
    inherits(Node);
};

class (LispInt) public (Node) {
    inherits(Node);
};

class (LispFloat) public (Node) {
    inherits(Node);
};

class (LispVector) public (Node) {
    inherits(Node);
};

class (LispStr) public (Node) {
    inherits(Node);
};

/* ----------------
 *	macros and defines which depend on dummy class tags
 * ----------------
 */
#define	PGLISP_ATOM	T_LispSymbol
#define	PGLISP_DTPR	T_LispList
#define	PGLISP_FLOAT	T_LispFloat
#define	PGLISP_INT	T_LispInt
#define	PGLISP_STR	T_LispStr
#define	PGLISP_VECI	T_LispVector

#define	LISP_BYTEVECTOR			T_LispVector
#define	LISP_DOUBLE			T_LispFloat
#define	LISP_INTEGER			T_LispInt
#define	LISP_STRING			T_LispStr

#define lispStringp(x) ((bool)(LISP_TYPE(x)==PGLISP_STR))
#define lispIntegerp(x) ((bool)(LISP_TYPE(x)==PGLISP_INT))
#define lispAtomp(x) ((bool)(LISP_TYPE(x)==PGLISP_ATOM))

/* ----------------
 *	lispAlloc
 * ----------------
 */
#define lispAlloc() (LispValue) palloc(sizeof(classObj(LispValue)))

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
#define	LISPVALUE_BYTEVECTOR(LISPVALUE)	((LISPVALUE)->val.veci->data)

#define	LISP_TYPE(LISPVALUE)		((LISPVALUE)->type)

/*
 *	Defined in lisplib/lispdep.c
 */
#define LispTrue 	((LispValue)1)

/* ----------------
 *	lisp support macros
 * ----------------
 */
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
extern LispValue 	LispNil;
extern LispValue	lispAtom();
extern LispValue	lispDottedPair();
extern LispValue	lispFloat();
extern LispValue	lispInteger();
extern LispValue	lispString();
extern LispValue	lispVectori();
extern LispValue	evalList();
extern LispValue	quote();

extern LispValue	lispList();
extern LispValue        lispCons();
extern LispValue	nappend1();
extern LispValue 	car();
extern LispValue 	cdr();
extern LispValue 	rplaca();
extern LispValue 	rplacd();
extern LispValue 	find_if_not();
extern LispValue 	find_if();
extern LispValue 	rplacd();
extern			init_list();

/*
 *	Defined in fmgr/ppreserve.c
 */
extern LispValue	ppreserve();
extern LispValue	lppreserve();
extern char		*prestore();
extern LispValue	lprestore();

/* 
 * as yet undefined, but should be defined soon
 */
extern LispValue nthCdr();
extern LispValue lispArray();
/* extern LispValue list(); /* XXX - varargs ??? */
extern LispValue setf();
extern LispValue find();
extern LispValue nconc();
extern LispValue nreverse();

extern int length();
extern LispValue remove();
extern LispValue remove_duplicates();
extern LispValue setf();
extern bool equal();

extern LispValue  LispUnion();
extern LispValue set_difference();
extern LispValue append();
extern LispValue LispDelete();
extern LispValue push();
extern bool null();
extern LispValue collect();
extern LispValue last_element();
extern LispValue last();
extern bool same();

extern bool listp();
extern int CAtom();
extern double CDouble();
extern char *CString();
extern int CInteger();

/* temporary functions */

extern LispValue mapcar();

#endif /* !LispDepIncluded */
