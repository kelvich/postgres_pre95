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

#define	PGLISP_ATOM	255
#define	PGLISP_DTPR	254
#define	PGLISP_FLOAT	253
#define	PGLISP_INT	252
#define	PGLISP_STR	251
#define	PGLISP_VECI	250

#define	LISP_BYTEVECTOR			250
#define	LISP_DOUBLE			253
#define	LISP_INTEGER			252
#define	LISP_STRING			251

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

extern bool listp();
extern int CAtom();

/* temporary functions */

extern LispValue mapcar();

#endif /* !LispDepIncluded */
