/*
 * lispdep.c --
 *	LISP-dependent C code.
 *
 *	This is where we really get our hands dirty with LISP guts.
 *
 *	Used in:
 *		parser/*.c
 *		fmgr/{fmgr,ppreserve}.c
 *		util/{plancat,rlockutils,syscache}.c
 */

#include "lispdep.h"

/*
 * 	Global declaration for LispNil.
 * 	Relies on the definition of "nil" in the LISP image.
 *	XXX Do *NOT* move this below the inclusion of c.h, or this
 *	    will break when used with Franz LISP!
 */
#ifdef FRANZ43
LispValue	LispNil = (LispValue) nil;
#endif /* FRANZ43 */

#ifdef KCL
LispValue	LispNil = (LispValue) Cnil;
#endif /* KCL */

#ifdef ALLEGRO
/* LispNil is defined in the Include file as :

#define LispNil NULL
 */
#endif /* ALLEGRO */

#ifdef PGLISP
LispValue	LispNil = (LispValue) NULL;
#endif /* PGLISP */

#include "c.h"

RcsId("$Header$");

/*
 *	Allocation and manipulation routines.
 *
 *	XXX It may be assumed that garbage collection has been turned off
 *	    before these routines are called.
 *
 *	lispAtom - new atom with print name set to the argument
 *	lispDottedPair - new uninitialized cons cell
 *	lispFloat - new floating point number (>= 32 bit)
 *	lispInteger - new integer (>= 32-bit)
 *	lispString - new string (null-delimited)
 *	lispVectori - new byte vector (size in bytes >= argument)
 *	evalList - eval the argument
 *	quote - quote the argument symbol (only really necessary for Franz?)
 */

#ifdef FRANZ43
/* ===================== FRANZ43 ==================== */

void
lispDisplay(lispObject,iscdr)
     LispValue lispObject;
     int iscdr;
{
	LispValue l = quote( lispObject);
	evalList ( l );
}
     

char *
CString ( lstr )
     LispValue lstr;
{
	return ((char *)lstr);
}

int
CInteger (l_int )
     LispValue l_int;
{
	return ((int)l_int->i);
}

lispNullp ( lval)
     LispValue lval;
{
	return ( lval == LispNil );
}
LispValue
lispAtom(atomName)
	char	*atomName;
{
/*	MakeUpper(atomName); /* makes atomName all caps*/
	return(matom(atomName));
}

LispValue
lispDottedPair()
{
	return(newdot());
}

LispValue
lispFloat(floatValue)
	double floatValue;
{
	LispValue	p = newdoub();
	LISPVALUE_DOUBLE(p) = floatValue;
	return(p);
}

LispValue
lispInteger(integerValue)
	int32	integerValue;
{
	return(inewint((int) integerValue));
}

LispValue
lispString(string)
	char	*string;
{
	return(mstr(string));
}

LispValue
lispVectori(nBytes)
	int	nBytes;
{
	return(nveci(nBytes));
}

LispValue
evalList(list)			/* XXX plain "eval" conflicts with Franz */
	LispValue	list;
{
	return(ftolsp(2, lispAtom("eval"), list));
}

LispValue
quote(lispObject)
	LispValue	lispObject;
{
	LispValue p = lispDottedPair();

	CAR(p) = lispAtom("quote");
	CDR(p) = lispObject;
	return(p);
}

#endif /* FRANZ43 */

#ifdef KCL
/* ===================== KCL ==================== */

LispValue
lispAtom(atomName)
	char	*atomName;
{
	return(make_ordinary(atomName));
}

LispValue
lispDottedPair()
{
	return(make_cons(LispNil, LispNil));
}

LispValue
lispFloat(floatValue)
	double floatValue;
{
	return(make_longfloat(floatValue));
}

LispValue
lispInteger(integerValue)
	int32	integerValue;
{
	return(make_fixnum((int) integerValue));
}

LispValue
lispString(string)
	char	*string;
{
	return(make_simple_string(string));
}

LispValue
lispVectori(nBytes)
	int	nBytes;
{
	extern LispValue	Sfixnum;

	nBytes = (nBytes + 3) / 4;
	vs_base = vs_top;
	vs_push(Sfixnum);		/* element type */
	vs_push(lispInteger(nBytes));	/* array dimension */
	vs_push(LispNil);		/* adjustable */
	vs_push(LispNil);		/* fill pointer */
	vs_push(LispNil);		/* displaced-to */
	vs_push(LispNil);		/* displace-offset */
	vs_push(LispNil);		/* static */
	siLmake_vector();
	return(vs_base[0]);
}

LispValue
evalList(list)
	LispValue	list;
{
	vs_mark;
	eval(list);
	vs_reset;
	return(vs_base[0]);
}

LispValue
quote(lispObject)
	LispValue	lispObject;
{
	return(lispObject);
}

#endif /* KCL */

#ifdef ALLEGRO
/* ===================== ALLEGRO ==================== */
static int  MAKE_DOTTED_PAIR,MAKE_FLOAT,MAKE_STRING,MAKE_INTEGER,
MAKE_VECTORI,EVAL,MAKE_QUOTED_OBJECT;

void *
makelispfunctions(a,b,c,d,e,f)
     int a,b,c,d,e,f;
{
	MAKE_DOTTED_PAIR = a;
	MAKE_INTEGER = b;
	MAKE_FLOAT = c;
	MAKE_STRING = d;
	MAKE_VECTORI = e;
	MAKE_QUOTED_OBJECT = f;
}

LispValue
lispMatom(atomNum)
	int atomNum;
{
	extern long lisp_value();
	return ((char *)lisp_value(atomNum-256));
}

typedef struct ScanKeyword {
	char *name;
	int value;
} ScanKeyword;

#define endof(byte_array)	(&byte_array[lengthof(byte_array)])

LispValue
lispAtom(atomName)
     char *atomName;
{
        extern ScanKeyword ScanKeywords[];
	extern ScanKeyword *ScanKeywordLookup();
	extern long lisp_value();
	ScanKeyword *lookup;
	ScanKeyword	*low	= &ScanKeywords[0];
	ScanKeyword	*high	= &ScanKeywords[76];
	ScanKeyword	*middle;
	int		difference;

/*	printf("now looking for : %s\n",atomName);*/
	MakeLower(atomName);
	fflush(stdout);

	while (low <= high) {
		middle = low + (high - low) / 2;
		difference = strcmp(middle->name, atomName);
		if (difference == 0)
		  break;
		else if (difference < 0)
		  low = middle + 1;
		else
		  high = middle - 1;
	}
/*	printf("found at %d",(middle-&ScanKeywords[0]));*/
	fflush(stdout);
	if (difference == 0 )
	  return ((char *)lisp_value((middle-&ScanKeywords[0])+1));
	else
	  return ((char *)lisp_value(1));
}

LispValue
lispDottedPair()
{
    char * x;
    x = lisp_call(MAKE_DOTTED_PAIR,nilval,nilval);

    return(x);
}

LispValue
lispFloat(floatValue)
	double floatValue;
{
	return(lisp_call(MAKE_FLOAT,floatValue));
}

LispValue
lispInteger(integerValue)
	int32	integerValue;
{
	return(lisp_call(MAKE_INTEGER,integerValue));
}

LispValue
lispString(string)
	char	*string;
{
	return(lisp_call(MAKE_STRING,string));
}

LispValue
lispVectori(nBytes)
	int	nBytes;
{
	return(lisp_call(MAKE_VECTORI,nBytes));
}

LispValue
evalList(list)			/* XXX plain "eval" conflicts with Franz */
	LispValue	list;
{
	return(lisp_call(EVAL,list));
}

LispValue
quote(lispObject)
	LispValue	lispObject;
{
	return(lisp_call(MAKE_QUOTED_OBJECT,lispObject));
}

char *
CString(lispObject)
     LispValue lispObject;
{
	  return( (char *)(V_char ( lispObject )) );
}

lispNullp ( lispObject)
     LispValue lispObject;
{
	return ( Null (lispObject ));
}

int 
CInteger(lispObject)
     LispValue lispObject;
{
	return ( (int )(FixnumToInt((int)lispObject)) ); /* XXX - should check */
}
#endif /* ALLEGRO */

#ifdef PGLISP
/* ===================== PGLISP ==================== */
#include <strings.h>

extern char		*malloc();
#define	lispAlloc()	((LispValue) malloc(sizeof(struct lisp_atom)))

#define	PGLISP_ATOM	0
#define	PGLISP_DTPR	1
#define	PGLISP_FLOAT	2
#define	PGLISP_INT	3
#define	PGLISP_STR	4
#define	PGLISP_VECI	5

lispNullp ( lval)
     LispValue lval;
{
	return ( lval == LispNil );
}

char *
CString(lstr)
     LispValue lstr;
{
	if(lstr->type == PGLISP_STR)
	  return(lstr->val.str);
	else
	  return(NULL);
}

int
CInteger(lval)
     LispValue lval;
{
	if(lval != NULL)
	  if(lval->type == PGLISP_INT)
	    return(lval->val.fixnum);
	  else
	    return(0);
	else
	  return(0);
}
LispValue
lispAtom(atomName)
	char	*atomName;
{
	LispValue	newobj = lispAlloc();
	char		*newAtomName = malloc(strlen(atomName)+1);

	newobj->type = PGLISP_ATOM;
	newobj->val.name = strcpy(newAtomName, atomName);
	newobj->cdr = LispNil;
	return(newobj);
}

LispValue
lispDottedPair()
{
	LispValue	newobj = lispAlloc();

	newobj->type = PGLISP_DTPR;
	newobj->val.car = LispNil;
	newobj->cdr = LispNil;
	return(newobj);
}

LispValue
lispFloat(floatValue)
	double	floatValue;
{
	LispValue	newobj = lispAlloc();

	newobj->type = PGLISP_FLOAT;
	newobj->val.flonum = floatValue;
	newobj->cdr = LispNil;
	return(newobj);
}

LispValue
lispInteger(integerValue)
	int	integerValue;
{
	LispValue	newobj = lispAlloc();

	newobj->type = PGLISP_INT;
	newobj->val.fixnum = integerValue;
	newobj->cdr = LispNil;
	return(newobj);
}

LispValue
lispString(string)
	char	*string;
{
    LispValue	newobj = lispAlloc();
    char		*newstr;

    newobj->type = PGLISP_STR;
	newobj->val.str = strcpy(newstr,string);
    if(string) {
	newstr = malloc(strlen(string)+1);
	newstr = strcpy(newstr,string);
    } else
      newstr = (char *)NULL;

    newobj->val.str = newstr;
    return(newobj);
}

LispValue
{
		malloc((unsigned) (sizeof(struct vectori) + nBytes));
	
	newobj->type = PGLISP_VECI;
	newobj->equalFunc = _equalLispValue;
	newobj->printFunc = lispDisplay;
	newobj->val.veci = (struct vectori *)
		palloc((unsigned) (sizeof(struct vectori) + nBytes));
	newobj->val.veci->size = nBytes;
	newobj->cdr = LispNil;
	return(newobj);
	return(list);
LispValue
evalList(list)
	LispValue	list;
{
    elog(WARN,"trying to evaluate a list, unsupported function");
    return(list);
	return(lispObject);
LispValue
quote(lispObject)
	LispValue	lispObject;
{
    elog(WARN,"calling quote which is being phased out");
    return(lispObject);
}
 *	lispDisplay
 *
 *	Print a PGLISP tree depth-first.
 */
#define length_of(byte_array)	(sizeof(byte_array) / sizeof((byte_array)[0]))

void 
lispDisplay(lispObject,iscdr)
	LispValue	lispObject;
        int             iscdr;
{
	register	i;

		printf("%s ", lispObject->val.name);
	}
	switch(lispObject->type) {
	case PGLISP_ATOM:
	  for (i=0; i < 85  ; i++ )
	    if (ScanKeywords[i].value == (int)(lispObject->val.name) )
	      printf ("%s ",ScanKeywords[i].name);
		break;
	case PGLISP_DTPR:
		if(!iscdr)
			printf("(");
		lispDisplay(CAR(lispObject),0);
		if(CDR(lispObject)!=LispNil) {
			if(CDR(lispObject)->type != PGLISP_DTPR)
				printf(".");
			lispDisplay(CDR(lispObject),1);
		}
		if(!iscdr)
			printf(")");
		break;
	case PGLISP_FLOAT:
		printf("%g ", lispObject->val.flonum);
		break;
	case PGLISP_INT:
		printf("%d ", lispObject->val.fixnum);
		break;
	case PGLISP_STR:
		printf("\"%s\" ", lispObject->val.str);
		break;
	case PGLISP_VECI:
		printf("\nUnknown LISP type : internal error\n");
			printf(" %d", lispObject->val.veci->data[i]);
		printf(" >");
		break;
	default:
#endif /* PGLISP */
		(* ((Node)lispObject)->printFunc)(lispObject);

		/*printf("\nUnknown LISP type : internal error\n");*/
		break;
	}
}


/* ===================== LISP INDEPENDENT ==================== */

/*
 *	Manipulation routines.
 *
 *	lispList - allocate a new cons cell, initialized to (nil . nil)
 *	lispCons - allocate a new cons cell, initialized to the arguments
 *	nappend1 - destructive append of "object" to the end of "list"
 *	rplaca, rplacd - same as LISP
 *	init_list - set the car of a list
 */

LispValue
lispList()
{
	LispValue p;

	p = lispDottedPair();
	CAR(p) = LispNil;
	CDR(p) = LispNil;
	return(p);
}

LispValue
lispCons(lispObject1, lispObject2)
	LispValue lispObject1, lispObject2;
{
	LispValue p = lispDottedPair();

	CAR(p) = lispObject1;
	CDR(p) = lispObject2;
	return(p);
}


LispValue
nappend1(list, lispObject)
	LispValue	list, lispObject;
{
	LispValue	p;
	
	if (list == LispNil) {
		list = lispList();
		CAR(list) = lispObject;
		CDR(list) = LispNil;
		return(list);
	}
	for (p = list; CDR(p) != LispNil; p = CDR(p))
		;
	CDR(p) = lispList();
	CAR(CDR(p)) = lispObject;
	return(list);
}


/* XXX - this will only work for single level lists */

LispValue
append1(list, lispObject)
	LispValue	list, lispObject;
{
    LispValue	p = LispNil;
    LispValue 	retval = LispNil;

    if (list == LispNil) {
	retval = lispList();
	CAR(retval) = lispObject;
	CDR(retval) = LispNil;
	return(retval);
    }

LispValue
find_if(pred,bar)
     bool (*pred )();
     LispValue bar;
{
    LispValue temp;

    foreach (temp,bar) 
      if ((*pred)(CAR(temp)))
	return(CAR(temp));
    return(LispNil);
}

LispValue
find(foo,bar,test, key)
     LispValue foo;
     LispValue bar;
     bool (*test)();
     Node (*key)();

{
    LispValue temp;

    elog(WARN,"unsupported function");

    foreach(temp,bar) {
	if ((*test)(foo,(*key)(CAR(temp))))
	  return(CAR(temp));
    }
    return(LispNil);
}

LispValue
some(foo,bar)
     LispValue foo,bar;
{
    elog(WARN,"unsupported function");
    return(foo);
}

LispValue
sort(foo)
     LispValue foo;
{
    if (length(foo) == 1)
      return(foo);
    else
     elog(WARN,"unsupported function");
}

bool
stringp(foo)
     LispValue foo;
{
    return((bool)(foo->type == PGLISP_STR));
}

double
expt(foo)
     double foo;
{
    elog(WARN,"unsupported function");
    return foo;
}

bool
floatp(foo)
     LispValue foo;
{
    if (foo)
      return((bool)(foo->type == PGLISP_FLOAT));
    else
      return(false);
}
bool
numberp(foo)
     LispValue foo;
{
    return((bool)(integerp (foo) || 
		  floatp (foo)));
}

bool
same (foo,bar)
     LispValue foo;
     LispValue bar;
{
  LispValue temp = LispNil;

  if (null(foo))
    return(null(bar));
  if (null(bar))
    return(null(foo));
  if (length(foo) == length(bar)) {
    foreach (temp,foo) {
      if (!member(CAR(temp),bar))
	return(false);
    }
    return(true);
  }
  return(false);
	
}	  
