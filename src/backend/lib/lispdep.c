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

#include "pg_lisp.h"
#include "log.h"

/*
 * 	Global declaration for LispNil.
 * 	Relies on the definition of "nil" in the LISP image.
 *	XXX Do *NOT* move this below the inclusion of c.h, or this
 *	    will break when used with Franz LISP!
 */

LispValue	LispNil = (LispValue) NULL;

#include "c.h"
#include "nodes.h"
extern char *malloc();

#define lispAlloc() palloc(sizeof(struct _LispValue))

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

/* ===================== PGLISP ==================== */
#include <strings.h>

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

int
CInteger(lval)
     LispValue lval;
{
  if(lval != NULL && lval->type == PGLISP_INT)
      return(lval->val.fixnum);
  elog (WARN,"error : bogus integer");
  return(0);
}

typedef struct ScanKeyword {
	char	*name;
	int	value;
} ScanKeyword;

extern ScanKeyword *ScanKeywordLookup();

LispValue
lispAtom(atomName)
	char *atomName; 
{
	LispValue	newobj = lispAlloc();
	int keyword;

	keyword = (ScanKeywordLookup(atomName))->value;

	newobj->type = PGLISP_ATOM;
	newobj->val.name = (char *)keyword;
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
    newobj->cdr = LispNil;

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
	LispValue	newobj = lispAlloc();
	
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

/*
 *	lispDisplay
 *
 *	Print a PGLISP tree depth-first.
 */
#define length_of(byte_array)	(sizeof(byte_array) / sizeof((byte_array)[0]))

	extern ScanKeyword ScanKeywords[];
void 
lispDisplay(lispObject,iscdr)
	LispValue	lispObject;
        int             iscdr;
{
	register	i;

	if (lispObject == LispNil) {
		printf("nil ");
		return;
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
		((Node)lispObject)->printFunc(lispObject);
		for (i = 0; i < lispObject->val.veci->size; ++i)
			printf(" %d", lispObject->val.veci->data[i]);
		printf(" >");
		break;
	default:
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

bool
equal(foo,bar)
     LispValue foo,bar;
{
  return(true);
}
    for (p = list; p != LispNil; p = CDR(p)) {
    }

    for (p = retval; CDR(p) != LispNil; p = CDR(p))
      ;

    CDR(p) = lispList();
    CAR(CDR(p)) = lispObject;
    
    return(retval);
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
          newlist = lispList();
          CAR(newlist) = lispObject;
          CDR(newlist) = LispNil;
          return(newlist);
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
     for (temp = list; CDR(temp) != LispNil; temp = CDR(temp))
 *      nthCdr   - returns a list where the car of the list
 *                 is the indexed element.  Used to implement the
 *                 nth function.
 *      nconc    - returns the concatenation of *2* lists.
 *                 destructive modification.
 */


LispValue
append(list,lispObject)
     LispValue list, lispObject;
     int i;

     if (index > length(list))
       return (LispNil);

     for (i= 1; i <= index; i++)
       list = CDR (list);

     return(list);
	     return (lispObject);  /* XXX should copy  */
	 }
     }
     for (p = newlist; CDR(p) != LispNil; p = CDR(p))
       ;
     CDR(p) = lispList();
     CAR(CDR(p)) = lispObject;
     return(newlist);

}

int
length (list)
     LispValue list;
{
     LispValue temp;
     int count = 0;
     for (temp = list; temp != LispNil; temp = CDR(temp))
     for (temp = list1; CDR(temp) != LispNil; temp = CDR(temp))


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
       return(false);
          if (last) {
               last = false;
               rlist = lispCons(CAR(p),LispNil);
          }
          else
}

LispValue
     list = rlist;
{
     LispValue temp;

     if (list1 == LispNil)
       return(list2);
     if (list1 == list2)
	elog(WARN,"trying to nconc a list to itself");

     for (temp = list1;  CDR(temp) != LispNil; temp = CDR(temp))
       ;


LispValue
nreverse (list)
     LispValue list;
{
    return(true);
     if(null(list))
       return(LispNil);

remove_duplicates(foo)

       return(list);
    return(foo);
 * member()
 * - nondestructive, returns t iff foo is a member of the list
     List bar;
find_if_not(foo,bar,baz)
     LispValue foo,bar;
     bool (*baz)();
      if (equal(CAR(i),foo))
    return(bar);
     List foo;
     bool (* test)();
{
    LispValue i;
    LispValue j;
    LispValue result = LispNil;
    bool there_exists_duplicate = false;

    if(length(foo) == 1)
      return(foo);
    foreach (i,foo) {
	if (listp (i) && !null(i)) {
	    foreach (j,CDR(i)) {
		if ( (* test)(CAR(i),CAR(j)) )
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
	j = i;
    }
}

LispValue
setf(foo,bar)
     LispValue foo,bar;
{
    elog(WARN,"unsupported function, 'setf' being called");
}

      if (equal(foo,temp)) {
	/* XXX - remove foo from temp */
	  break;
     LispValue foo;
    return(bar);
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
     LispValue foo;
     List bar;
{
    elog(WARN,"unsupported function 'push' called");
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
/*
LispValue
lispEvery(foo,bar)
     LispValue foo,bar;
{
    return(bar);
}
*/
    LispValue i = LispNil;
    LispValue j = LispNil;
    
    if (null(foo))
      return(bar); /* XXX - should be copy of bar */
    if (null (bar))
/*
LispValue
list(foo)
     LispValue foo;
{
    return(foo);
}
*/
      return(foo); /* XXX - should be copy of foo */
    
    Assert (IsA(foo,LispList));
    Assert (IsA(bar,LispList));

    foreach (i,foo) {
	foreach (j,bar) {
	    if (! equal (CAR(i),CAR(j))) {
	      retval = nappend1(retval,CAR(i));
Node
Contents(foo)
     LispValue foo;
{
    return((Node)(foo->val.veci));
}

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
remove (foo,bar)
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
    return(CAR(bar));
     void (*foo)();
     LispValue bar;
{
find(pred,bar)
     bool (*pred)();
}
     LispValue foo;
    return(CAR(bar));
     LispValue foo;
{
    if (integerp(foo))
      return((bool)(CInteger(foo) == 0));
}

eq(foo,bar)
     LispValue foo,bar;
{
    return ((bool)(foo == bar));
}

LispValue
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
