/*
 * lispdep.h --
 *	LISP-dependent declarations.
 *
 * Identification:
 *	$Header$
 */

#ifndef	LispDepIncluded
#define	LispDepIncluded

#include "nodes.h"
#include <stdio.h>
#include "tags.h"
#include "c.h"

/* ==========================================================================
 *	POSTGRES "LISP"
 *		A hack to let the parser run standalone.
 */
struct vectori {
	int	size;
	char	data[1];		/* variable length array */
};

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

#define List LispValue

/* ----------------
 *	arrays of LispValues are needed in execnodes.h
 *	-cim 8/29/89
 * ----------------
 */
typedef LispValue *LispValuePtr;
#define	ListPtr	 LispValuePtr

/*
struct lisp_atom {
	int 			type;
	union {
		char			*name;
		char   			*str;
		int    			fixnum;
		double 			flonum;
		struct lisp_atom	*car;
		struct vectori		*veci;
	} 			val;
	struct lisp_atom	*cdr;
};

typedef struct lisp_atom	*LispValue;
*/

#define	LISP_GC_OFF		/* yow! */
#define	LISP_GC_ON		/* yow! */
#define	LISP_GC_PROTECT(X)	/* yow! */

#define	CAR(LISPVALUE)			((LISPVALUE)->val.car)
#define	CDR(LISPVALUE)			((LISPVALUE)->cdr)
#define CAAR(lv)                (CAR(CAR(lv)))
#define CADR(lv)			(CAR(CDR(lv)))
#define CADDR(lv)			(CAR(CDR(CDR(lv))))
#define	LISPVALUE_DOUBLE(LISPVALUE)	((LISPVALUE)->val.flonum)
#define	LISPVALUE_INTEGER(LISPVALUE)	((LISPVALUE)->val.fixnum)
#define	LISPVALUE_BYTEVECTOR(LISPVALUE)	((LISPVALUE)->val.veci->data)

#define	LISP_TYPE(LISPVALUE)		((LISPVALUE)->type)

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

/*
 *	Defined in lisplib/lispdep.c
 */
#define LispTrue 	((LispValue)1)

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
extern LispValue 	rplaca();
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

#define nth(index,list)         CAR(nthCdr(index,list))

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
#define foreach(_elt_,_list_)	for(_elt_=(LispValue)_list_; \
_elt_!=LispNil;_elt_=CDR(_elt_))

/* sigh, used so often, I'm lazy to do global search & replace */
#define cons(x,y) lispCons(x,y)


#endif /* !LispDepIncluded */
