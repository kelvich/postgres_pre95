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

#include "tmp/c.h"

RcsId("$Header$");

#include "nodes/pg_lisp.h"
#include "parser/atoms.h"
#include "utils/palloc.h"
#include "utils/log.h"

/*
 * 	Global declaration for LispNil.
 * 	Relies on the definition of "nil" in the LISP image.
 *	XXX Do *NOT* move this below the inclusion of c.h, or this
 *	    will break when used with Franz LISP!
 */

LispValue	LispNil = (LispValue) NULL;
extern bool _equalLispValue();
extern void lispDisplayFp();
extern void lispDisplay();

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

extern char		*malloc();

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
  else {
      elog(WARN,"CString called on non-string\n");
      return(NULL);
  }
}

int 
CAtom(lv)
     LispValue lv;
{
  if (lv->type == PGLISP_ATOM)
    return ((int)(lv->val.name));
  else {
      elog(WARN,"CAtom called on non-atom\n");
      return (NULL);
  }
}

double
CDouble(lval)
     LispValue lval;
{
    if (lval != NULL && lval->type == PGLISP_FLOAT)
      return(lval->val.flonum);
    elog(WARN,"error : bougs float");
    return((double)0);
}
int
CInteger(lval)
     LispValue lval;
{
    if(lval != NULL && 
       ( lval->type == PGLISP_INT ||
	lval->type == PGLISP_ATOM ))
      return(lval->val.fixnum);
    elog (WARN,"error : bogus integer");
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
	int 		keyword;
	ScanKeyword	*search_result;	

	search_result = ScanKeywordLookup(atomName);
	Assert(search_result);

	keyword = search_result->value;

	newobj->type = PGLISP_ATOM;
	newobj->equalFunc = _equalLispValue;
	newobj->printFunc = lispDisplayFp;
	newobj->copyFunc = _copyLispSymbol;
	newobj->val.name = (char *)keyword;
	newobj->cdr = LispNil;
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

	newobj->type = PGLISP_DTPR;
	newobj->equalFunc = _equalLispValue;
	newobj->printFunc = lispDisplayFp;
	newobj->copyFunc = _copyLispList;
	newobj->val.car = LispNil;
	newobj->cdr = LispNil;
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

	newobj->type = PGLISP_FLOAT;
	newobj->equalFunc = _equalLispValue;
	newobj->printFunc = lispDisplayFp;
	newobj->copyFunc = _copyLispFloat;
	newobj->val.flonum = floatValue;
	newobj->cdr = LispNil;
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

	newobj->type = PGLISP_INT;
	newobj->equalFunc = _equalLispValue;
	newobj->printFunc = lispDisplayFp;
	newobj->copyFunc = _copyLispInt;
	newobj->val.fixnum = integerValue;
	newobj->cdr = LispNil;
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

    newobj->type = PGLISP_STR;
    newobj->equalFunc = _equalLispValue;
    newobj->printFunc = lispDisplayFp;
    newobj->copyFunc = _copyLispStr;
    newobj->cdr = LispNil;

    if(string) {
	newstr = palloc(strlen(string)+1);
	newstr = strcpy(newstr,string);
    } else
      newstr = (char *)NULL;

    newobj->val.str = newstr;
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
	
	newobj->type = PGLISP_VECI;
	newobj->equalFunc = _equalLispValue;
	newobj->printFunc = lispDisplayFp;
	newobj->copyFunc = _copyLispVector;
	newobj->val.veci = (struct vectori *)
		palloc((unsigned) (sizeof(struct vectori) + nBytes));
	newobj->val.veci->size = nBytes;
	newobj->cdr = LispNil;
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

/*
 *	lispDisplayFp
 *
 *	Print a PGLISP tree depth-first.
 */
#define length_of(byte_array)	(sizeof(byte_array) / sizeof((byte_array)[0]))

void 
lispDisplayFp(fp, lispObject,iscdr)
	FILE 		*fp;
	LispValue	lispObject;
        int             iscdr;
{
	register	i;

	if (lispObject == LispNil) {
		fprintf(fp, "nil ");
		return;
	}
	switch(lispObject->type) {
	case PGLISP_ATOM:
	  for (i=0; i < 85  ; i++ )
	    if (ScanKeywords[i].value == (int)(lispObject->val.name) )
	      fprintf (fp, "%s ",ScanKeywords[i].name);
		break;
	case PGLISP_DTPR:
		if(!iscdr)
			fprintf(fp, "(");
		lispDisplayFp(fp, CAR(lispObject),0);
		if(CDR(lispObject)!=LispNil) {
			if(CDR(lispObject)->type != PGLISP_DTPR)
				fprintf(fp, " . ");
			lispDisplayFp(fp, CDR(lispObject),1);
		}
		if(!iscdr)
			fprintf(fp, ")");
		break;
	case PGLISP_FLOAT:
		fprintf(fp, "%g ", lispObject->val.flonum);
		break;
	case PGLISP_INT:
		fprintf(fp, "%d ", lispObject->val.fixnum);
		break;
	case PGLISP_STR:
		fprintf(fp, "\"%s\" ", lispObject->val.str);
		break;
	case PGLISP_VECI:
		fprintf(fp, "#<%d:", lispObject->val.veci->size);
		for (i = 0; i < lispObject->val.veci->size; ++i)
			fprintf(fp, " %d", lispObject->val.veci->data[i]);
		fprintf(fp, " >");
		break;
	default:
		(* ((Node)lispObject)->printFunc)(fp, lispObject);
		/*fprintf(fp, "\nUnknown LISP type : internal error\n");*/
		break;
	}
}

/*
 *	lispDisplay
 *
 * 	Print a PGLISP tree depth-first in stdout
 */
void
lispDisplay(lispObject,iscdr)
	LispValue	lispObject;
        int             iscdr;
{
    lispDisplayFp(stdout, lispObject, iscdr);
    fflush(stdout);
}

int
crap(lispObject)
	LispValue	lispObject;
{
    lispDisplayFp(stdout, lispObject, 0);
    fflush(stdout);
	return(0);
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

    for (p = list; p != LispNil; p = CDR(p)) {
	retval = nappend1(retval,CAR(p));
    }

    for (p = retval; CDR(p) != LispNil; p = CDR(p))
      ;

    CDR(p) = lispList();
    CAR(CDR(p)) = lispObject;
    
    return(retval);
}

LispValue
car(dottedPair)
	LispValue	dottedPair;
{
	if (null(dottedPair)) {
		return (LispNil);
	}
	AssertArg(listp(dottedPair));
	return (CAR(dottedPair));
}

LispValue
cdr(dottedPair)
	LispValue	dottedPair;
{
	if (null(dottedPair)) {
		return (LispNil);
	}
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



/*********************************************************************

  consp
  - p74 of CLtL

 *********************************************************************/
 
bool
consp(foo)
     LispValue foo;
{
    if (foo)
      return((bool)(foo->type == PGLISP_DTPR));
    else
      return(false);
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

     Assert(listp (list));
     if (list == LispNil)
        return (lispCopy(lispObject));
     
     newlist = lispCopy(list);
     newlispObject = lispCopy(lispObject);

     for (p = newlist; CDR(p) != LispNil; p = CDR(p))
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
length (list)
     LispValue list;
{
     LispValue temp;
     int count = 0;
     for (temp = list; temp != LispNil; temp = CDR(temp))
       count += 1;

     return(count);
}


LispValue
nthCdr (index, list)
     LispValue list;
     int index;
{
    int i;
    LispValue temp = list;
    for (i= 1; i <= index; i++) {
	if (temp != LispNil)
	  temp = CDR (temp);
	else 
	  return(LispNil);
    }
    return(temp);
}

bool
null (lispObj)
     LispValue lispObj;
{
     if (lispObj == LispNil)
       return(true);
     else
       return(false);
}

LispValue
nconc (list1, list2)
     LispValue list1,list2;
{
     LispValue temp;

     if (list1 == LispNil)
       return(list2);
     if (list2 == LispNil)
       return(list1);
     if (list1 == list2)
	elog(WARN,"trying to nconc a list to itself");

     for (temp = list1;  CDR(temp) != LispNil; temp = CDR(temp))
       ;

     CDR(temp) = list2;
     return(list1);      /* list1 is now list1[]list2  */
}



LispValue
nreverse (list)
     LispValue list;
{
     LispValue temp =LispNil;
     LispValue rlist = LispNil;
     LispValue p = LispNil;
     bool last = true;

     if(null(list))
       return(LispNil);

     Assert(IsA(list,LispList));

     if (length (list) == 1)
       return(list);

     for (p = list; p != LispNil; p = CDR(p)) {
            rlist = lispCons (CAR(p),rlist);
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

    if(length(foo) == 1)
      return(foo);
    foreach (i,foo) {
	if (listp (i) && !null(i)) {
	    foreach (j,CDR(i)) {
		if ( (* test)(CAR(i),CAR(j)) )
		  there_exists_duplicate = true;
	    }
	    if ( ! there_exists_duplicate ) 
	      result = nappend1 (result, CAR(i) );

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
	if ( equal(CAR(i),foo))
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

bool
atom(foo)
     LispValue foo;
{
    return ((bool)(foo->type == PGLISP_ATOM));
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

LispValue
push(foo,bar)
     LispValue foo;
     List bar;
{
	LispValue tmp = lispList();

	if (bar == LispNil) {
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
    for (bar = foo; CDR(bar) != LispNil ; bar = CDR(bar))
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
    if (null (bar))
      return(foo); /* XXX - should be copy of foo */
    
    Assert (IsA(foo,LispList));
    Assert (IsA(bar,LispList));

    foreach (i,foo) {
	foreach (j,bar) {
	    if (! equal (CAR(i),CAR(j))) {
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

LispValue
LispRemove (foo,bar)
     LispValue foo;
     List bar;
{
    LispValue temp = LispNil;
    LispValue result = LispNil;

    for (temp = bar; temp != LispNil ; temp = CDR(temp))
      if (! equal(foo,CAR(temp)) ) {
	  result = append1(result,CAR(temp));
      }
	  
    return(result);
}

bool
listp(foo)
     LispValue foo;
{
    if (foo)
      return((bool)(foo->type == PGLISP_DTPR));
    else
      return(true);
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
integerp(foo)
     LispValue foo;
{
  if (foo)
    return((bool)(foo->type==PGLISP_INT));
  else
    return(false);
}

bool
zerop(foo)
     LispValue foo;
{
    if (integerp(foo))
	return((bool)(CInteger(foo) == 0));
    else
	{
	    elog(WARN,"zerop called on noninteger");
	    return ((bool) 1); /* non-integer is always zero */
	}
}

bool
eq(foo,bar)
    LispValue foo,bar;
{
    return ((bool)(foo == bar));
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
