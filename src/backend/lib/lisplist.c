/* $Header$ */

#include <varargs.h>
#include "pg_lisp.h"

/*
 * lispList will take a list of "valid" lispValues
 * and produce a pg_lisp list
 * NOTE: terminates on first "-1" it finds.
 * e.g. lispList ( lispAtom("foo"), lispString("bar"), -1 , x )
 *      will produce ( foo "bar" )
 */

LispValue
MakeList ( va_alist )
va_dcl
{
    va_list args;
    List retval = LispNil;
    List temp = LispNil;
    List tempcons = LispNil;

    va_start(args);

    while ( (int)(temp =  va_arg(args, LispValue) ) != -1) {
	temp = lispCons ( temp, LispNil );
	if ( tempcons == LispNil ) {
	    tempcons  = temp;
	    retval = temp;
	} else
	    CDR(tempcons) = temp;
	    tempcons = temp;
    }

    va_end(args);

    return ( retval );
}
