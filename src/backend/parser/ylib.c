static char *ylib_c = "$Header$";

#define MAXPATHLEN 1024

#include <strings.h>
#include <stdio.h>
#include <pwd.h>
#include "log.h"
#include "catalog_utils.h"
#include "pg_lisp.h"
#include "exc.h"
#include "excid.h"
#include "io.h"
#include "palloc.h"
#include "parse_query.h"
#include "primnodes.h"
#include "primnodes.a.h"
#include "parse.h"

LispValue parsetree;

#define DB_PARSE(foo) 

parser(str, l)
     char *str;
     LispValue l;
{
    LispValue yyl;
    int yyresult;


    init_io();
/*
 * Parser must now return a parse tree in C space.  Thus, it cannot
 * start/end at this high a granularity.
 *
    startmmgr(0);
 */

    /* Set things up to read from the string, if there is one */
    /* DB_PARSE("parser got : %s\n",str);  
    fflush(stdout); */
    if (strlen(str) != 0) {
	StringInput = 1;
	TheString = palloc(strlen(str) + 1);
	bcopy(str,TheString,strlen(str)+1);
    }

    /* ExcBegin(); */

    {
      parser_init();
      yyresult = yyparse();
      
      /* DB_PARSE("parse_string is %s\n",str); */
      
      fflush(stdout);
      clearerr(stdin);
      
      if (yyresult) {	/* error */
	/* DB_PARSE("parser error\n");
	   printf("parse_string is %s\n",str); */
	fflush(stdout);
	CAR(l) = LispNil;
	CDR(l) = LispNil;
	return;
      }
      
      CAR(l) = CAR(parsetree);
      CDR(l) = CDR(parsetree);
    }

/*    ExcExcept(); {
	if (exception.id == &SystemError) {
	    elog(FATAL, exception.message);
	} else if (exception.id == &InternalError) {
	    elog(FATAL, exception.message);
	} else if (exception.id == &CatalogFailure) {
	    elog(WARN, exception.message);
	} else if (exception.id == &SemanticError) {
	    elog(WARN, exception.message);
	} else {
	    reraise();
	}
    }
    ExcEnd();
    /* XXX close_rt(); */
/*
 * Parser must now return a parse tree in C space.  Thus, it cannot
 * start/end at this high a granularity.
 *
    endmmgr(NULL);
 */
    if (yyl == NULL) {
	return(-1);
    } else {
	return(0);
    }
}

char *
expand_file_name(file)
char *file;
{
    char *str;
    int ind;

    str = (char *) palloc(MAXPATHLEN * sizeof(*str));
    str[0] = '\0';
    if (file[0] == '~') {
	if (file[1] == '\0' || file[1] == '/') {
	    /* Home directory */
	    strcpy(str, getenv("HOME"));
	    ind = 1;
	} else {
	    /* Someone else's directory */
	    char name[16], *p;
	    struct passwd *pw;
	    int len;

	    if ((p = (char *) index(file, '/')) == NULL) {
		strcpy(name, file+1);
		len = strlen(name);
	    } else {
		len = (p - file) - 1;
		strncpy(name, file+1, len);
		name[len] = '\0';
	    }
	    /*printf("name: %s\n");*/
	    if ((pw = getpwnam(name)) == NULL) {
		elog(WARN, "No such user: %s\n", name);
		ind = 0;
	    } else {
		strcpy(str, pw->pw_dir);
		ind = len + 1;
	    }
	}
    } else {
	ind = 0;
    }
    strcat(str, file+ind);
    return(str);
}

LispValue
new_filestr ( filename )
     LispValue filename;
{
  return (lispString (expand_file_name (CString(filename))));
}

int
lispAssoc ( element, list)
     LispValue element, list;
{
    LispValue temp = list;
    int i = 0;
    if (list == LispNil) 
      return -1; 
    /* printf("Looking for %d", CInteger(element));*/

    while (temp != LispNil ) {
	if(CInteger(CAR(temp)) == CInteger(element))
	  return i;
	temp = CDR(temp);
	i ++;
    }
	   
    return -1;
}

LispValue
parser_typecast ( expr, typename )
     LispValue expr;
     LispValue typename;
{
    /* check for passing non-ints */
    Const adt;
    Datum lcp;
    Type tp = type(CString(typename));
    int32 len = tlen(tp);
    char *cp = NULL;

    char *const_string; 
	bool string_palloced = false;
    
    switch ( CInteger(CAR(expr)) ) {
      case 23: /* int4 */
	  const_string = palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%d",
		get_constvalue(CDR(expr)));
	break;
      case 19: /* char16 */
	  const_string = palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%s",
		get_constvalue(CDR(expr)));
	break;
      case 18: /* char */
	  const_string = palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%c",
		get_constvalue(CDR(expr)));
	break;
      case 701:/* float8 */
	  const_string = palloc(256);
	  string_palloced = true;
	sprintf(const_string,"%f",
		get_constvalue(CDR(expr)));
	break;
      case 25: /* text */
	const_string = 
	  DatumGetPointer(
			  get_constvalue(CDR(expr)) );
	  const_string = (char *) textout(const_string);
	break;
      default:
	elog(WARN,"unknown type%d ",
	     CInteger(CAR(expr)) );
    }
    
    cp = instr2 (tp, const_string);
    
    
    if (!tbyvalue(tp)) {
	if (len >= 0 && len != PSIZE(cp)) {
	    char *pp;
	    pp = palloc(len);
	    bcopy(cp, pp, len);
	    cp = pp;
	}
	lcp = PointerGetDatum(cp);
    } else {
	switch(len) {
	  case 1:
	    lcp = Int8GetDatum(cp);
	    break;
	  case 2:
	    lcp = Int16GetDatum(cp);
	    break;
	  case 4:
	    lcp = Int32GetDatum(cp);
	    break;
	  default:
	    lcp = PointerGetDatum(cp);
	    break;
	}
    }
    
    adt = MakeConst ( typeid(tp), len, lcp , 0 );
    /*
      printf("adt %s : %d %d %d\n",CString(expr),typeid(tp) ,
      len,cp);
      */
	if (string_palloced) pfree(const_string);
    return (lispCons  ( lispInteger (typeid(tp)) , adt ));
}

char *
after_first_white_space( input )
     char *input;
{
    char *temp = input;
    while ( *temp != ' ' && *temp != 0 ) 
      temp = temp + 1;

    return (temp+1);
}

int 
*int4varin( input_string )
     char *input_string;
{
    int *foo = (int *)malloc(1024*sizeof(int));
    register int i = 0;

    char *temp = input_string;
    
    while ( sscanf(temp,"%ld", &foo[i+1]) == 1 
	   && i < 1022 ) {
	i = i+1;
	temp = after_first_white_space(temp);
    }
    foo[0] = i;
    return(foo);
}

char *
int4varout ( an_array )
     int *an_array;
{
    int temp = an_array[0];
    char *output_string = NULL;
    extern int itoa();
    int i;

    if ( temp > 0 ) {
	char *walk;
	output_string = (char *)malloc(16*temp); /* assume 15 digits + sign */
	walk = output_string;
	for ( i = 0 ; i < temp ; i++ ) {
	    itoa(an_array[i+1],walk);
	    printf ( "%s\n", walk );
	    while (*++walk != '\0')
	      ;
	    *walk++ = ' ';
	}
	*--walk = '\0';
    }
    return(output_string);
}

#define ADD_TO_RT(rt_entry)     p_rtable = nappend1(p_rtable,rt_entry) 
List
ParseFunc ( funcname , fargs )
     char *funcname;
     List fargs;
{
    extern Func MakeFunc();
    extern OID funcname_get_rettype();
    extern OID funcname_get_funcid();
    OID rettype = (OID)0;
    OID funcid = (OID)0;
    Func funcnode = (Func)NULL;
    LispValue i = LispNil;
    List first_arg_type = NULL;
    char *relname = NULL;
    extern List p_rtable;

    first_arg_type = CAR(CAR(fargs));

    if ( CAtom(first_arg_type) == RELATION ) {
	Var newvar = NULL;

	relname = CString(CDR(CAR(fargs)));
	/* this is really a method */

	if( RangeTablePosn ( relname ,LispNil ) == 0 )
	  ADD_TO_RT( MakeRangeTableEntry (relname,
					  LispNil, 
					  relname ));

	funcid = funcname_get_funcid ( funcname );
	rettype = funcname_get_rettype ( funcname );
	
	if ( funcid != (OID)0 && rettype != (OID)0 ) {
	    funcnode = MakeFunc ( funcid , rettype , false );
	} else
	  elog (WARN,"function %s does not exist",funcname);

	CDR(CAR(fargs)) = make_var ( relname , funcname );

	foreach ( i , fargs ) {
	    CAR(i) = CDR(CAR(i));
	}

    } else {
	/* is really a function */

	funcid = funcname_get_funcid ( funcname );
	rettype = funcname_get_rettype ( funcname );
	
	if ( funcid != (OID)0 && rettype != (OID)0 ) {
	    funcnode = MakeFunc ( funcid , rettype , false );
	} else
	  elog (WARN,"function %s does not exist",funcname);
	
	foreach ( i , fargs ) {
	    CAR(i) = CDR(CAR(i));
	}

	return ( lispCons (lispInteger(rettype) ,
			   lispCons ( funcnode , fargs )));
    } /* was a function */
	    
}

/*
 * RemoveUnusedRangeTableEntries
 * - removes relations from the rangetable that are no longer
 *   useful.  This helps in preventing the planner from generating
 *   extra joins that are not needed.
 */
List
RemoveUnusedRangeTableEntries ( parsetree )
     List parsetree;
{

}

/* 
 * FlattenRelationList
 *
 * at this point, time-qualified relation/relation-lists
 * have a lispInteger in the front,
 * inheritance relations have a lispString("*")
 * in front
 */ 
List
FlattenRelationList ( rel_list )
     List rel_list;
{
    List temp = NULL;
    List possible_rtentry = NULL;

    foreach ( temp, rel_list ) {
	possible_rtentry = CAR(temp);
	
	if ( consp ( possible_rtentry ) ) {

	    /* can be any one of union, inheritance or timerange queries */
	    
	} else {
	    /* normal entry */
	    
	}
    }
}
List
MakeFromClause ( from_list, base_rel )
     List from_list;
     LispValue base_rel;
{
    List i = NULL;
    List j = NULL;
    List flags = lispCons(lispInteger(0),LispNil);
    /* from_list will be a list of strings */
    /* base_rel will be a wierd assortment of things, 
       including timerange, inheritance and union relations */

    elog(NOTICE, "the pseudo relations are:");
    lispDisplay ( from_list , 0 );
    elog(NOTICE, "the real relations are:");
    lispDisplay ( base_rel , 0 );

    foreach ( i , from_list ) {
        List temp = CAR(i);
    	if ( base_rel->type == PGLISP_STR ) {
	    /* Just a normal relation */
	    ADD_TO_RT(MakeRangeTableEntry( CString(base_rel), LispNil, 
					   CString(temp)));
    	} else if ( base_rel->type == PGLISP_DTPR ) {
	        List first = CAR(base_rel);
	        if ( first->type == PGLISP_STR ) {
		    if ( !strcmp(CString(first),"*") ) {
		        /* inheritance query */
		        flags = nappend1(flags, lispAtom("inherits"));
		    } else {
			elog(WARN,"unioned relations not allowed in from");
		    }
	        } 
                if ( first->type == PGLISP_INT ) {
		    /* timerange query */
		    CAR(flags) = first;
	        } 
 	    ADD_TO_RT(MakeRangeTableEntry( CString(CDR(base_rel)),flags,
					   CString(temp) ));
    	}
    }
}

