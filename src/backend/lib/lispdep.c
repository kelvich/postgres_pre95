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

#include <strings.h>
#include <stdio.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"
#include "parser/atoms.h"
#include "utils/palloc.h"
#include "utils/log.h"
#include "tmp/stringinfo.h"

extern bool _equalLispValue();

/* ----------------
 *	_copy function extern declarations
 * ----------------
 */
extern bool	_copyLispValue();
extern bool	_copyLispSymbol();
extern bool	_copyLispList();
extern bool	_copyLispInt();
extern bool	_copyLispFloat();
extern bool	_copyLispVector();
extern bool	_copyLispStr();

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

/* ===================== PGLISP ==================== */


char *
CString(lstr)
     LispValue lstr;
{
    if (stringp(lstr))
	return(LISPVALUE_STRING(lstr));
    else {
	elog(WARN,"CString called on non-string\n");
	return(NULL);
    }
}

int 
CAtom(lv)
     LispValue lv;
{
    if (atom(lv))
	return ((int)(LISPVALUE_SYMBOL(lv)));
    else {
	elog(WARN,"CAtom called on non-atom\n");
	return (NULL);
    }
}

double
CDouble(lval)
     LispValue lval;
{
    if (floatp(lval))
	return(LISPVALUE_DOUBLE(lval));
    
    elog(WARN,"error : bougs float");
    return((double)0);
}
int
CInteger(lval)
     LispValue lval;
{
    if (integerp(lval) || atom(lval))
	return(LISPVALUE_INTEGER(lval));
    
    elog(WARN,"error : bogus integer");
    return(0);
}

/* ----------------
 *	lispAtom actually returns a node with type T_LispSymbol
 * ----------------
 */
LispValue
lispAtom(atomName)
    char *atomName; 
{
    LispValue	newobj = lispAlloc();
    int 	keyword;
    ScanKeyword	*search_result;	

    search_result = ScanKeywordLookup(atomName);
    Assert(search_result);

    keyword = search_result->value;

    LISP_TYPE(newobj) = 		PGLISP_ATOM;
    newobj->equalFunc = 	_equalLispValue;
    newobj->outFunc = 		_outLispValue;
    newobj->copyFunc = 		_copyLispSymbol;
    LISPVALUE_SYMBOL(newobj) = 	(char *) keyword;
    CDR(newobj) = 		LispNil;
    
    return(newobj);
}

/* ----------------
 *	lispDottedPair actually returns a node of type T_LispList
 * ----------------
 */
LispValue
lispDottedPair()
{
    LispValue	newobj = lispAlloc();
    
    LISP_TYPE(newobj) = 		PGLISP_DTPR;
    newobj->equalFunc = 	_equalLispValue;
    newobj->outFunc = 		_outLispValue;
    newobj->copyFunc = 		_copyLispList;
    CAR(newobj) = 		LispNil;
    CDR(newobj) = 		LispNil;

    return(newobj);
}

/* ----------------
 *	lispFloat returns a node of type T_LispFloat
 * ----------------
 */
LispValue
lispFloat(floatValue)
    double	floatValue;
{
    LispValue	newobj = lispAlloc();

    LISP_TYPE(newobj) = 		PGLISP_FLOAT;
    newobj->equalFunc = 	_equalLispValue;
    newobj->outFunc = 		_outLispValue;
    newobj->copyFunc =		_copyLispFloat;
    LISPVALUE_DOUBLE(newobj) = 	floatValue;
    CDR(newobj) = 		LispNil;

    return(newobj);
}

/* ----------------
 *	lispInteger returns a node of type T_LispInt
 * ----------------
 */
LispValue
lispInteger(integerValue)
    int	integerValue;
{
    LispValue	newobj = lispAlloc();

    LISP_TYPE(newobj) = 	PGLISP_INT;
    newobj->equalFunc = 	_equalLispValue;
    newobj->outFunc = 		_outLispValue;
    newobj->copyFunc = 		_copyLispInt;
    LISPVALUE_INTEGER(newobj) = integerValue;
    CDR(newobj) = 		LispNil;

    return(newobj);
}

/* ----------------
 *	lispName returns a node of type T_LispStr
 *	but gurantees that the string is char16 aligned and filled
 * ----------------
 */
LispValue
lispName(string)
	char	*string;
{
    LispValue	newobj = lispAlloc();
    char		*newstr;

    LISP_TYPE(newobj) = PGLISP_STR;
    newobj->equalFunc = _equalLispValue;
    newobj->outFunc = _outLispValue;
    newobj->copyFunc = _copyLispStr;
    CDR(newobj) = LispNil;

    if (string) {
	if (strlen(string) > sizeof(NameData)) {
	    elog(WARN,"Name %s was longer than %d",string,sizeof(NameData));
	    /* NOTREACHED */
	}
	newstr = (char *) palloc(sizeof(NameData)+1);
	newstr = strcpy(newstr,string);
    } else
      newstr = (char *)NULL;

    LISPVALUE_STRING(newobj) = newstr;
    
    return(newobj);
}


/* ----------------
 *	lispString returns a node of type T_LispStr
 * ----------------
 */
LispValue
lispString(string)
	char	*string;
{
    LispValue	newobj = lispAlloc();
    char		*newstr;

    LISP_TYPE(newobj) = PGLISP_STR;
    newobj->equalFunc = _equalLispValue;
    newobj->outFunc = 	_outLispValue;
    newobj->copyFunc = 	_copyLispStr;
    CDR(newobj) =	 LispNil;

    if (string) {
	newstr = (char *) palloc(strlen(string)+1);
	newstr = strcpy(newstr,string);
    } else
      newstr = (char *)NULL;

    LISPVALUE_STRING(newobj) = newstr;
    return(newobj);
}

/* ----------------
 *	lispVectori returns a node of type T_LispVector
 * ----------------
 */
LispValue
lispVectori(nBytes)
    int	nBytes;
{
    LispValue	newobj = lispAlloc();
	
    LISP_TYPE(newobj) = PGLISP_VECI;
    newobj->equalFunc = _equalLispValue;
    newobj->outFunc = 	_outLispValue;
    newobj->copyFunc = 	_copyLispVector;
    
    LISPVALUE_VECI(newobj) = (struct vectori *)
	palloc((unsigned) (sizeof(struct vectori) + nBytes));
    LISPVALUE_VECTORSIZE(newobj) = nBytes;
    CDR(newobj) = LispNil;

    return(newobj);
}

LispValue
evalList(list)
	LispValue	list;
{
    elog(WARN,"trying to evaluate a list, unsupported function");
    return(list);
}

LispValue
quote(lispObject)
	LispValue	lispObject;
{
    elog(WARN,"calling quote which is being phased out");
    return(lispObject);
}

/* ===================== LISP INDEPENDENT ==================== */

/*
 *	Manipulation routines.
 *
 *	lispList - allocate a new cons cell, initialized to (nil . nil)
 *	lispCons - allocate a new cons cell, initialized to the arguments
 *	nappend1 - destructive append of "object" to the end of "list"
 *	car, cdr - same as LISP
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
	
	if (null(list)) {
	    list = lispList();
	    CAR(list) = lispObject;
	    CDR(list) = LispNil;
	    return(list);
	}
	
	for (p = list; !null(CDR(p)); p = CDR(p))
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

    if (null(list)) {
	retval = lispList();
	CAR(retval) = lispObject;
	CDR(retval) = LispNil;
	return(retval);
    }

    for (p = list; !null(p); p = CDR(p)) {
	retval = nappend1(retval,CAR(p));
    }

    for (p = retval; !null(CDR(p)); p = CDR(p))
      ;

    CDR(p) = lispList();
    CAR(CDR(p)) = lispObject;
    
    return(retval);
}

LispValue
car(dottedPair)
	LispValue	dottedPair;
{
	if (null(dottedPair))
	    return (LispNil);
	
	AssertArg(listp(dottedPair));
	return (CAR(dottedPair));
}

LispValue
cdr(dottedPair)
	LispValue	dottedPair;
{
	if (null(dottedPair))
	    return (LispNil);
	
	AssertArg(listp(dottedPair));
	return (CDR(dottedPair));
}

LispValue
rplaca(dottedPair, newValue)
	LispValue	dottedPair, newValue;
{
	CAR(dottedPair) = newValue;
	return(dottedPair);
}

LispValue
rplacd(dottedPair, newValue)
	LispValue	dottedPair, newValue;
{
	CDR(dottedPair) = newValue;
	return(dottedPair);
}

/* XXX fix lispList and get rid of this ... */
init_list(list, newValue)
	LispValue	list, newValue;
{
	CAR(list) = newValue;
	CDR(list) = LispNil;
}


/*
 *      More Manipulation routines
 *
 *      append   - appends lisp obj to the end of the lisp.
 *                 non-destructive. XXX needs to be extended to
 *                 manipulate lists
 *      length   - returns the length of a list, as an int.
 *      nthCdr   - returns a list where the car of the list
 *                 is the indexed element.  Used to implement the
 *                 nth function.
 *      nconc    - returns the concatenation of *2* lists.
 *                 destructive modification.
 */


LispValue
append(list,lispObject)
     LispValue list, lispObject;
{
     LispValue  p;
     LispValue newlist, newlispObject;
     LispValue lispCopy();

     if (null(list))
        return lispCopy(lispObject);
     
     Assert(listp(list));
     
     newlist = lispCopy(list);
     newlispObject = lispCopy(lispObject);

     for (p = newlist; !null(CDR(p)); p = CDR(p))
       ;
     CDR(p) = newlispObject;
     return(newlist);

}

/* ----------------
 *	lispCopy has been moved to copyfuncs.c with the other
 *	copy functions.
 * ----------------
 */

int
length(list)
     LispValue list;
{
     LispValue temp;
     int count = 0;
     for (temp = list; !null(temp); temp = CDR(temp))
       count += 1;

     return(count);
}


LispValue
nthCdr(index, list)
     LispValue list;
     int index;
{
    int i;
    LispValue temp = list;
    for (i= 1; i <= index; i++) {
	if (! null(temp))
	    temp = CDR(temp);
	else 
	    return(LispNil);
    }
    return(temp);
}

LispValue
nconc(list1, list2)
     LispValue list1,list2;
{
     LispValue temp;

     if (list1 == LispNil)
       return(list2);
     if (list2 == LispNil)
       return(list1);
     if (list1 == list2)
	elog(WARN,"trying to nconc a list to itself");

     for (temp = list1; !null(CDR(temp)); temp = CDR(temp))
       ;

     CDR(temp) = list2;
     return(list1);      /* list1 is now list1[]list2  */
}



LispValue
nreverse(list)
     LispValue list;
{
     LispValue temp =LispNil;
     LispValue rlist = LispNil;
     LispValue p = LispNil;
     bool last = true;

     if(null(list))
       return(LispNil);

     Assert(IsA(list,LispList));

     if (length(list) == 1)
       return(list);

     for (p = list; !null(p); p = CDR(p)) {
            rlist = lispCons(CAR(p),rlist);
     }

     CAR(list) = CAR(rlist);
     CDR(list) = CDR(rlist);
     return(list);
}

int
position(foo,bar)
     LispValue foo;
     List bar;
{
    return(0);
}

/*
 * member()
 * - nondestructive, returns t iff foo is a member of the list
 *   bar
 */
bool
member(foo,bar)
     LispValue foo;
     List bar;
{
    LispValue i;
    foreach (i,bar)
      if (equal(CAR(i),foo))
	return(true);
    return(false);
}

LispValue
remove_duplicates(foo,test)
     List foo;
     bool (* test)();
{
    LispValue i;
    LispValue j;
    LispValue result = LispNil;
    bool there_exists_duplicate = false;
    int times = 0;

    if (length(foo) == 1)
	return(foo);
    
    foreach (i,foo) {
	if (listp(i) && !null(i)) {
	    foreach (j,CDR(i)) {
		if ( (* test)(CAR(i),CAR(j)) )
		  there_exists_duplicate = true;
	    }
	    if (! there_exists_duplicate) 
	      result = nappend1(result, CAR(i) );

	    there_exists_duplicate = false;
	}
    }
    /* XXX assumes that the last element in the list is never
     *  deleted.
     */
/*    result = nappend1(result,last_element(foo));  */
    return(result);
}

/* Returns the leftmost element in the list which 
 * does not satisfy the predicate.  Returns LispNil 
 * if no elements satisfiy the pred.
 */

LispValue
find_if_not(pred,bar)
     LispValue bar;
     bool (*pred)();
{
    LispValue temp;
    
    foreach(temp,bar)
      if (!(*pred)(CAR(temp)))
	return(CAR(temp));
    return(LispNil);
}

LispValue
LispDelete(foo,bar)
     LispValue foo;
     List bar;
{
    LispValue i = LispNil;
    LispValue j = LispNil;

    foreach (i,bar) {
	if (equal(CAR(i),foo))
	  if (i == bar ) {
	      /* first element */
	      CAR(bar) = CAR(CDR(bar));
	      CDR(bar) = CDR(CDR(bar));
	  } else {
	      CDR(j) = CDR(i);
	  }
	j = i;
    }
}

LispValue
setf(foo,bar)
     LispValue foo,bar;
{
    elog(WARN,"unsupported function, 'setf' being called");
    return(bar);
}

LispValue
LispRemove(foo,bar)
     LispValue foo;
     List bar;
{
    LispValue temp = LispNil;
    LispValue result = LispNil;

    for (temp = bar; !null(temp); temp = CDR(temp))
      if (! equal(foo,CAR(temp)) ) {
	  result = append1(result,CAR(temp));
      }
	  
    return(result);
}

List
nLispRemove(foo, bar)
List foo;
LispValue bar;
{
    LispValue x;
    List result = LispNil;

    foreach (x, foo)
	if (bar != CAR(x))
	    result = nappend1(result, CAR(x));
    return result;
}


LispValue
set_difference(foo,bar)
     LispValue foo,bar;
{
    LispValue temp1 = LispNil;
    LispValue result = LispNil;

    if(null(bar))
	return(foo); 
    
    foreach (temp1,foo) {
	if (! member(CAR(temp1),bar))
	  result = nappend1(result,CAR(temp1));
    }
    return(result);
}

List
nset_difference(foo, bar)
List foo, bar;
{
    LispValue x;
    List result;

    result = foo;
    foreach (x, bar) {
	result = nLispRemove(result, CAR(x));
      }
    return result;
}

LispValue
push(foo,bar)
     LispValue foo;
     List bar;
{
	LispValue tmp = lispList();

	if (null(bar)) {
		CAR(tmp) = foo;
		CDR(tmp) = LispNil;
		return(tmp);
	}

	CAR(tmp) = CAR(bar);
	CDR(tmp) = CDR(bar);

	CAR(bar) = foo;
	CDR(bar) = tmp;

	return(bar);
}

LispValue 
last(foo)
     LispValue foo;
{
    LispValue bar;
    for (bar = foo; !null(CDR(bar)); bar = CDR(bar))
      ;
    return(bar);
}

LispValue
LispUnion(foo,bar)
     LispValue foo,bar;
{
    LispValue retval = LispNil;
    LispValue i = LispNil;
    LispValue j = LispNil;
    
    if (null(foo))
      return(bar); /* XXX - should be copy of bar */
    
    if (null(bar))
      return(foo); /* XXX - should be copy of foo */
    
    Assert(IsA(foo,LispList));
    Assert(IsA(bar,LispList));

    foreach (i,foo) {
	foreach (j,bar) {
	    if (! equal(CAR(i),CAR(j))) {
	      retval = nappend1(retval,CAR(i));
	      break;
	    }
	}
    }
    foreach(i,bar) {
      retval = nappend1(retval,CAR(i));
    }

    return(retval);
}

/* temporary functions */

LispValue 
mapcar(foo,bar)
     void (*foo)();
     LispValue bar;
{
    elog(WARN, "unsupported function 'mapcar'");
    return(bar);
}

bool
zerop(foo)
     LispValue foo;
{
    if (integerp(foo))
	return((bool)(CInteger(foo) == 0));
    else {
	elog(WARN,"zerop called on noninteger");
	return ((bool) 1); /* non-integer is always zero */
    }
}

LispValue
lispArray(foo)
     int foo;
{
    elog(WARN,"bogus function : lispArray");
    return(lispInteger(foo));
}

/*    
 *    	number-list
 *    
 *    	Returns a list with 'n' fixnums from 'start' in order.
 *    
 */

List
number_list(start,n)
     int start,n;
{
    LispValue return_list = LispNil;
    int i,j;

    for(i = start, j = 0; j < n ; j++,i++ )
      return_list = nappend1(return_list,lispInteger(i));

    return(return_list);
}


LispValue
apply(foo,bar)
     LispValue (*foo)();
     LispValue bar;
{
    elog(WARN,"unsupported function 'apply' being called");
    return(bar);
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
     bool(*foo)();
     LispValue bar;
{
    LispValue i = LispNil;
    LispValue temp = LispNil;

    foreach(i,bar) {
	temp =CAR(i);
	if ((*foo)(temp))
	  return(temp);
    }
    return(LispNil);
    
}

LispValue
sort(foo)
     LispValue foo;
{
    if (length(foo) == 1)
      return(foo);
    else {
     elog(WARN,"unsupported function");
	 return(foo);
	}
}

double
expt(foo)
     double foo;
{
    elog(WARN,"unsupported function");
    return foo;
}

bool
same(foo,bar)
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

/*---------------------------------------------------------------------
 *	lispDisplayFp
 *
 *	Print a PGLISP tree depth-first.
 *---------------------------------------------------------------------
 */

void 
lispDisplayFp(fp, lispObject)
FILE 		*fp;
LispValue	lispObject;
{
    char *s;

    s = lispOut(lispObject);
    fprintf(fp, "%s", s);
    pfree(s);
}

/*------------------------------------------------------------------
 *	lispDisplay
 *
 * 	Print a PGLISP tree depth-first in stdout
 *------------------------------------------------------------------
 */
void
lispDisplay(lispObject)
LispValue	lispObject;
{
    lispDisplayFp(stdout, lispObject);
    fflush(stdout);
}

/*--------------------------------------------------------------------------
 * lispOut
 *
 * Given a lisp structure, create a string with its visula representation.
 *
 * Keep Postgres memory clean! Don't forget to 'pfree' the string when
 * you are done with it!
 *--------------------------------------------------------------------------
 */
char *
lispOut(lispObject)
    LispValue	lispObject;
{
    StringInfo str;
    char *s;

    str = makeStringInfo();
    _outLispValue(str, lispObject);
    s = str->data;

    /*
     * free the StringInfoData, but not the string itself...
     */
    pfree(str);

    return(s);
}

/*--------------------------------------------------------------------------
 * _outLispValue
 *
 * Given a lisp structure, create a string with its visual representation.
 * In fact, we do not create a string, but a 'StringInfo'.
 * If you want to get back just the string, use 'lispOut'.
 *
 *--------------------------------------------------------------------------
 */

void
_outLispValue(str, lispObject)
StringInfo str;
LispValue	lispObject;
{

    register int i;
    register LispValue t;
    register int done;
    char buf[500];
    char *buf2;


    if (null(lispObject)) {
	appendStringInfo(str, "nil ");
	return;
    }

    switch(lispObject->type) {
	case PGLISP_ATOM:
	    buf2 = AtomValueGetString((int)(LISPVALUE_SYMBOL(lispObject)));
	    sprintf(buf, "%s ", buf2);
	    appendStringInfo(str, buf);
	    break;
	case PGLISP_DTPR:
	    appendStringInfo(str, "(");
	    /*
	     * This object can be the beginning of a list of objects,
	     * or a single dotted pair (e.g. "(1 . 2)").
	     * In the first case its CDR will be either NIL or
	     * another dotted pair.
	     * In the second case it won't!
	     */
	    done = 0;
	    t = lispObject;
	    while (!done) {
		/*
		 * first, print the CAR
		 */
		_outLispValue(str, CAR(t));
		/*
		 * Now check the CDR & decide what to do...
		 */
		if (null(CDR(t))) {
		    /*
		     * End of list
		     */
		    done = 1;
		} else if (consp(CDR(t))) {
		    /*
		     * there is at least another element in the list
		     */
		    t = CDR(t);
		} else {
		    /*
		     * this is a dotted pair
		     * print its CDR too and get out of here!
		     */
		    appendStringInfo(str, " . ");
		    _outLispValue(str, CDR(t));
		    done = 1;
		}
	    } /*while*/
	    appendStringInfo(str, ")");
	    break;
	case PGLISP_FLOAT:
	    sprintf(buf, "%g ", LISPVALUE_DOUBLE(lispObject));
	    appendStringInfo(str, buf);
	    break;
	case PGLISP_INT:
	    sprintf(buf, "%d ", LISPVALUE_INTEGER(lispObject));
	    appendStringInfo(str, buf);
	    break;
	case PGLISP_STR:
	    sprintf(buf, "\"%s\" ", LISPVALUE_STRING(lispObject));
	    appendStringInfo(str, buf);
	    break;
	case PGLISP_VECI:
	    sprintf(buf, "#<%d:", LISPVALUE_VECTORSIZE(lispObject));
	    appendStringInfo(str, buf);
	    for (i = 0; i < LISPVALUE_VECTORSIZE(lispObject); ++i) {
		sprintf(buf, " %d", LISPVALUE_BYTEVECTOR(lispObject)[i]);
		appendStringInfo(str, buf);
	    }
	    sprintf(buf, " >");
	    appendStringInfo(str, buf);
	    break;
	default:
	    (* ((Node)lispObject)->outFunc)(str, lispObject);
	    break;
    }

}
