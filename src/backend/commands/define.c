/* ----------------------------------------------------------------
 * define.c --
 *	POSTGRES define (function | type | operator) utility code.
 *
 * NOTES:
 *	These things must be defined and committed in the following order:
 *		input/output, recv/send procedures
 *		type
 *		operators
 * ----------------------------------------------------------------
 */

#include <strings.h>	/* XXX style */
#include <ctype.h>
#include <math.h>

#include "tmp/postgres.h"

RcsId("$Header$");

#include "access/ftup.h"
#include "access/heapam.h"
#include "access/htup.h"
#include "access/tqual.h"
#include "catalog/catname.h"
#include "catalog/syscache.h"
#include "manip.h"
#include "nodes/pg_lisp.h"
#include "parser/parse.h"	/* for ARG */
#include "utils/fmgr.h"		/* for fmgr */
#include "utils/builtins.h"	/* prototype for textin() */
#include "utils/log.h"

#include "commands/defrem.h"
#include "planner/xfunc.h"

#include "tcop/dest.h"

/* ----------------
 *	external functions
 * ----------------
 */
extern	ObjectId TypeDefine();
extern	void	 ProcedureDefine();
extern	void	 OperatorDefine();
extern  void	 AggregateDefine();

/* ----------------
 *	this is used by the DefineXXX functions below.
 * ----------------
 */
extern
String		/* XXX Datum */
FetchDefault ARGS((
	String	string,
	String	standard
));

/* ----------------------------------------------------------------
 *		Define Function / Operator / Type
 * ----------------------------------------------------------------
 */

/* --------------------------------
 *	DefineFunction
 * --------------------------------
 */
void
DefineFunction(nameargsexe, dest)
    LispValue	nameargsexe;
    CommandDest dest;
{
    Name	name = (Name) CString(CAR(CAR(nameargsexe)));
    LispValue   parameters = CDR(CAR(nameargsexe));
    LispValue	entry;
    String	languageName, fileName, sourceCode;
    char        *c;
    String	returnTypeName;
    char        blankstring[2];
    bool	canCache;
    LispValue	argList;
    int32       byte_pct, perbyte_cpu, percall_cpu, outin_ratio;
    String      perbyte_str, percall_str;
    int         count;
    char        *ptr;
    bool	returnsSet;

    /* ----------------
     * Note:
     * XXX	Checking of "name" validity (16 characters?) is needed.
     * ----------------
     */
    AssertArg(NameIsValid(name));

    /* figure out the language */
    entry = DefineListRemoveRequiredAssignment(&parameters, "language");
    languageName = DefineEntryGetString(entry);
    /* lowercase-ise the language */
    for (c = languageName; *c != '\0'; c++)
      *c = tolower(*c);

    if (!strcmp(languageName, "postquel") && !strcmp(languageName, "c"))
      elog(WARN, "DefineFunction: Specified language not supported");	

    /* ----------------
     * handle "returntype = X".  The function could return a singleton
     * value or a set of values.  Figure out which.
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "returntype");
    if (IsA(CDR(entry),LispList)) {
	returnTypeName = LISPVALUE_STRING(CDR(CADR(entry)));
	returnsSet = true;
    } else {
	returnTypeName = DefineEntryGetString(entry);
	returnsSet = false;
    }

    /* Next attributes only definable for C functions */
    if (!strcmp(languageName, "c"))
     {
	 /* ----------------
	  * handle "[ iscachable ]": figure out if Postquel functions are
	  * cacheable automagically?
	  * ----------------
	  */

	 entry = DefineListRemoveOptionalIndicator(&parameters, "iscachable");
	 canCache = (bool)!null(entry);

	 /*
	  ** handle expensive function parameters
	  */
	 entry = DefineListRemoveOptionalAssignment(&parameters, "byte_pct");
	 if (null(entry)) byte_pct = BYTE_PCT;
	 else byte_pct = DefineEntryGetInteger(entry);

	 entry = DefineListRemoveOptionalAssignment(&parameters, "perbyte_cpu");
	 if (null(entry)) perbyte_cpu = PERBYTE_CPU;
	 else 
	  {
	      perbyte_str = DefineEntryGetString(entry);
	      if (!sscanf(perbyte_str, "%d", &perbyte_cpu))
	       {
		   for (count = 0, ptr = perbyte_str; *ptr != '\0'; ptr++)
		     if (*ptr == '!') count++;
		   perbyte_cpu = (int) pow(10.0, (double)count);
	       }
	  }

	 entry = DefineListRemoveOptionalAssignment(&parameters, "percall_cpu");
	 if (null(entry)) percall_cpu = PERCALL_CPU;
	 else 
	  {
	      percall_str = DefineEntryGetString(entry);
	      if (!sscanf(percall_str, "%d", &percall_cpu))
	       {
		   for (count = 0, ptr = percall_str; *ptr != '\0'; ptr++)
		     if (*ptr == '!') count++;
		   percall_cpu = (int) pow(10.0, (double)count);
	       }
	  }


	 entry = DefineListRemoveOptionalAssignment(&parameters, "outin_ratio");
	 if (null(entry)) outin_ratio = OUTIN_RATIO;
	 else outin_ratio = DefineEntryGetInteger(entry);

     }

    /* ----------------
     * handle "[ arg is (...) ]"
     * XXX fix optional arg handling below
     * ----------------
     */
    argList = LispRemoveMatchingSymbol(&parameters, ARG);
	 
    if (!null(argList)) {
	LispValue	rest;
	     
	/*
	 * first discard symbol 'arg from list
	 */
	argList = CDR(argList);
	AssertArg(length(argList) > 0);
	     
	foreach (rest, argList) {
	    if (!lispStringp(CAR(rest))) {
		elog(WARN, "DefineFunction: arg type = ?");
	    }
	}
    }
	 
    /* ----------------
     *	there should be nothing more
     * ----------------
     */
    DefineListAssertEmpty(parameters);
	 

    /* set up the sourcecode and filename strings */
    blankstring[0] = '-';
    blankstring[1] = '\0';
    if (!strcmp(languageName, "c"))
     {
	 sourceCode = blankstring;
	 fileName = CString(CADR(nameargsexe));
     }
    else
     {
	 sourceCode = CString(CADR(nameargsexe));
	 fileName = blankstring;
     }

    /* C is stored uppercase in pg_language */
    if (!strcmp(languageName, "c"))
      *languageName = 'C';

    /* ----------------
     *	now have ProcedureDefine do all the work..
     * ----------------
     */
    ProcedureDefine(name,
		    returnsSet,
		    returnTypeName,
		    languageName,
		    sourceCode,
		    fileName,
		    canCache,
		    byte_pct, perbyte_cpu, percall_cpu, 
		    outin_ratio,
		    argList,
		    dest);
}

/*
 *  Utility to handle definition of postquel procedures.
 *   NO LONGER USED.  -- JMH, 6/25/92
 */

void
DefinePFunction(pname,parameters, query_tree)
     char *pname;
     List parameters;
     List query_tree;
{
  static char query_buf[1024];
  static char tbuf[32];
  extern void eval_as_new_xact();
  List entry; 
  List p = LispNil;
  List argList;
  char *relname;
  List reln;
  String  returnTypeName;
  /*
   *  First we have to add a column to the relation.
   */

  /* XXX Fix this after catalogs fix to get relation type. */

  entry = DefineListRemoveRequiredAssignment(&parameters, "returntype");
  returnTypeName = DefineEntryGetString(entry);

  argList = LispRemoveMatchingSymbol(&parameters, ARG);
	 
  argList = CDR(argList);
  AssertArg(length(argList) > 0);
	     
  if (length(argList) >1)
      elog(WARN, "POSTQUEL functions with base type arguments aren't implemented yet");
  reln = CAR(argList);
  if (!lispStringp(reln))
      elog(WARN, "DefineFunction: arg type = ?");
   relname = CString(reln);
  sprintf(query_buf, "addattr (%s = SET ) to %s", pname, relname); 

  pg_eval(query_buf, (char *) NULL, (ObjectId *) NULL, 0); 

  /*
   * Now we have to define the appropriate rule for the Postquel
   * function(procedure).
   */
  CommandCounterIncrement();
  strcpy(tbuf, pname);
  strcat(tbuf, "_rule");
  p = nappend1(p, lispString(tbuf));	/* rulename */
  p = nappend1(p, lispAtom("retrieve")); /* event_type */
  p = nappend1(p, lispCons(lispString(relname),
			   lispCons(lispString(pname), LispNil)));
	       /* event_obj */
  p = nappend1(p, LispNil);	/* event_qual */
  p = nappend1(p, lispInteger(true)); /* is instead */
  p = nappend1(p, lispCons(query_tree,LispNil));	/* action list*/
  DefineQueryRewrite(p);


#ifdef notdef
  sprintf(query_buf, "define rewrite rule %s_rule is on retrieve to %s.%s do instead %s", pname, relname, pname, qstring);

  /*  printf("Rule defined is: %s\n", query_buf); */

  StartTransaction();*/
  eval_as_new_xact(query_buf);
#endif
}


/* --------------------------------
 *	DefineOperator
 *
 *	this function extracts all the information from the
 *	parameter list generated by the parser and then has
 *	OperatorDefine() do all the actual work.
 * --------------------------------
 */
void
DefineOperator(name, parameters)
    Name	name;
    LispValue	parameters;
{
    LispValue	entry;
    Name 	functionName;	 	/* function for operator */
    Name 	typeName1;	 	/* first type name */
    Name 	typeName2;	 	/* optional second type name */
    uint16 	precedence; 		/* operator precedence */
    bool	canHash; 		/* operator hashes */
    bool	isLeftAssociative; 	/* operator is left associative */
    Name 	commutatorName;	 	/* optional commutator operator name */
    Name 	negatorName;	 	/* optional negator operator name */
    Name 	restrictionName;	/* optional restrict. sel. procedure */
    Name 	joinName;	 	/* optional join sel. procedure name */
    Name 	sortName1;	 	/* optional first sort operator */
    Name 	sortName2;	 	/* optional second sort operator */
    
    /* ----------------
     *	sanity checks
     *
     * XXX	Checking of operator "name" validity
     *		(16 characters?) is needed.
     * ----------------
     */
    AssertArg(NameIsValid(name));
    AssertArg(listp(parameters));

    /* ----------------
     * handle "arg1 = typname"
     *	
     * XXX ( ... arg1 = typname [ , arg2 = typname ] ... )
     * XXX is undocumented in the reference manual source as of 89/8/22.
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "arg1");
    typeName1 = DefineEntryGetName(entry);
    
    /* ----------------
     * handle "[ arg2 = typname ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "arg2");
    typeName2 = NULL;
    if (!null(entry)) {
	typeName2 = DefineEntryGetName(entry);
    }
    
    /* ----------------
     * handle "procedure = proname"
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "procedure");
    functionName = DefineEntryGetName(entry);
    
    /* ----------------
     * handle "[ precedence = number ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "precedence");
    if (null(entry)) {
	precedence = 0;		/* FetchDefault? */
    } else {
	precedence = DefineEntryGetInteger(entry);
    }
    
    /* ----------------
     * handle "[ associativity = (left|right|none|any) ]"
     *	
     * XXX Associativity code below must be fixed when the catalogs and
     * XXX the planner/executor support proper associativity semantics.
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "associativity");
    if (null(entry)) {
	isLeftAssociative = true;	/* XXX FetchDefault */
    } else {
	String	string;
	
	string = DefineEntryGetString(entry);
	if (StringEquals(string, "right")) {
	    isLeftAssociative = false;
	} else if (!StringEquals(string, "left") &&
		   !StringEquals(string, "none") &&
		   !StringEquals(string, "any")) {
	    elog(WARN, "Define: precedence = what?");
	} else {
	    isLeftAssociative = true;
	}
    }
    
    /* ----------------
     * handle "[ commutator = oprname ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "commutator");
    commutatorName = NULL;
    if (!null(entry)) {
	commutatorName = DefineEntryGetName(entry);
    }
    
    /* ----------------
     * handle "[ negator = oprname ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "negator");
    negatorName = NULL;
    if (!null(entry)) {
	negatorName = DefineEntryGetName(entry);
    }
    
    /* ----------------
     * handle "[ restrict = proname ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "restrict");
    restrictionName = NULL;
    if (!null(entry)) {
	restrictionName = DefineEntryGetName(entry);
    }
    
    /* ----------------
     * handle "[ join = proname ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "join");
    joinName = NULL;
    if (!null(entry)) {
	joinName = DefineEntryGetName(entry);
    }
    
    /* ----------------
     * handle "[ hashes ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalIndicator(&parameters, "hashes");
    canHash = (bool)!null(entry);
    
    /* ----------------
     * handle "[ sort1 = oprname ]"
     *	
     * XXX ( ... [ , sort1 = oprname ] [ , sort2 = oprname ] ... )
     * XXX is undocumented in the reference manual source as of 89/8/22.
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "sort1");
    sortName1 = NULL;
    if (!null(entry)) {
	sortName1 = DefineEntryGetName(entry);
    }
    
    /* ----------------
     * handle "[ sort2 = oprname ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "sort2");
    sortName2 = NULL;
    if (!null(entry)) {
	sortName2 = DefineEntryGetName(entry);
    }

    /* ----------------
     *	there should be nothing more..
     * ----------------
     */
    DefineListAssertEmpty(parameters);

    /* ----------------
     *	now have OperatorDefine do all the work..
     * ----------------
     */
    OperatorDefine(name,		/* operator name */
		   typeName1,		/* first type name */
		   typeName2,		/* optional second type name */
		   functionName,	/* function for operator */
		   precedence,		/* operator precedence */
		   isLeftAssociative,	/* operator is left associative */
		   commutatorName,	/* optional commutator operator name */
		   negatorName,		/* optional negator operator name */
		   restrictionName,	/* optional restrict. sel. procedure */
		   joinName,		/* optional join sel. procedure name */
		   canHash,		/* operator hashes */
		   sortName1,		/* optional first sort operator */
		   sortName2);		/* optional second sort operator */
}

/* -------------------
 *  DefineAggregate
 * ------------------
 */
void
DefineAggregate(name, parameters)
    Name name;
    LispValue parameters;
{
    Name   stepfunc1Name, stepfunc2Name, finalfuncName;
    String primStr, secStr;
    struct varlena *primVal, *secVal;
    LispValue entry;

  /* sanity checks...name validity */

  AssertArg(NameIsValid(name));
  AssertArg(listp(parameters));

  /* handle "stepfunc = proname" */
  entry = DefineListRemoveRequiredAssignment(&parameters, "sfunc1");
  stepfunc1Name = DefineEntryGetName(entry);
   
   /* handle "countfunc = proname " */
  entry = DefineListRemoveRequiredAssignment(&parameters, "sfunc2");
  stepfunc2Name = DefineEntryGetName(entry);

  /* handle "finalfunc = proname */
  entry = DefineListRemoveRequiredAssignment(&parameters, "finalfunc");
  finalfuncName = DefineEntryGetName(entry);

  /* -----------------
   * handle first initial condition
   */
  entry = DefineListRemoveRequiredAssignment(&parameters, "initcond1");
  primStr = DefineEntryGetString(entry);
  primVal = textin(primStr);

  /* -------------------
   * handle second initial condition
   */
  entry = DefineListRemoveRequiredAssignment(&parameters, "initcond2");
  secStr = DefineEntryGetString(entry);
  secVal = textin(secStr);

  DefineListAssertEmpty(parameters);

  AggregateDefine(name,  		/* aggregate name */
		  stepfunc1Name,	/* first step function name */
		  stepfunc2Name,	/* second step function name */
		  finalfuncName,	/* final function name */
		  primVal,	/* first initial condition */
		  secVal);	/* second initial condition */
}

/* --------------------------------
 *	DefineType
 * --------------------------------
 */
void
DefineType(name, parameters)
    Name	name;
    LispValue	parameters;
{
    LispValue	entry;
    int16	internalLength;		/* int2 */
    int16	externalLength;		/* int2 */
	Name	elemName;
    Name	inputName;
    Name	outputName;
    Name	sendName;
    Name	receiveName;
    char*	defaultValue;		/* Datum */
    bool	byValue;		/* Boolean */
    char	delimiter;
    char	shadow_type[16];
    
    /* ----------------
     *	sanity checks
     *
     * XXX	Checking of operator "name" validity
     *		(16 characters?) is needed.
     * ----------------
     */
    AssertArg(NameIsValid(name));
    AssertArg(listp(parameters));

    /*
     * Type names can only be 15 characters long, so that the shadow type
     * can be created using the 16th character as necessary.
     */

    if (strlen(name) > 15)
    {
	elog(WARN, "DefineType: Names can only be 15 characters long or less");
    }
    
    /* ----------------
     * handle "internallength = (number | variable)"
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters,
					       "internallength");
    internalLength = DefineEntryGetLength(entry);

    /* ----------------
     * handle "[ externallength = (number | variable) ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters,
					       "externallength");
    externalLength = 0;		/* FetchDefault? */
    if (!null(entry)) {
	externalLength = DefineEntryGetLength(entry);
    }
    
    /* ----------------
     * handle "input = procedure"
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "input");
    inputName = DefineEntryGetName(entry);
    
    /* ----------------
     * handle "output = procedure"
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "output");
    outputName = DefineEntryGetName(entry);
    
    /* ----------------
     * handle "[ send = procedure ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "send");
    sendName = NULL;
    if (!null(entry)) {
	sendName = DefineEntryGetName(entry);
    }
    

	/*
	 * ----------------
	 * handle "[ delimiter = delim]"
	 * ----------------
	 */

    entry = DefineListRemoveOptionalAssignment(&parameters, "delimiter");
    delimiter = ',';
    if (!null(entry)) {
	char *p = (char *) DefineEntryGetName(entry);
	delimiter = p[0];
    }

    /* ----------------
     * handle "[ receive = procedure ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "receive");
    receiveName = NULL;
    if (!null(entry)) {
	receiveName = DefineEntryGetName(entry);
    }
    
	/*
	 * ----------------
	 * handle "[ element = type]"
	 * ----------------
	 */

    entry = DefineListRemoveOptionalAssignment(&parameters, "element");
    elemName = NULL;
    if (!null(entry)) {
	elemName = DefineEntryGetName(entry);
    }

    /* ----------------
     * handle "[ default = `...' ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "default");
    defaultValue = NULL;
    if (!null(entry)) {
	defaultValue = DefineEntryGetString(entry);
    }
    
    /* ----------------
     * handle "[ passedbyvalue ]"
     * ----------------
     */
    entry = DefineListRemoveOptionalIndicator(&parameters, "passedbyvalue");
    byValue = (bool)!null(entry);
    
    /* ----------------
     *	there should be nothing more..
     * ----------------
     */
    DefineListAssertEmpty(parameters);

    /* ----------------
     *	now have TypeDefine do all the real work.
     * ----------------
     */
    (void) TypeDefine(name,		/* type name */
		      InvalidObjectId,  /* relation oid (n/a here) */
		      internalLength,	/* internal size */
		      externalLength,	/* external size */
		      'b',		/* type-type (base type) */
			  delimiter,	/* array element delimiter */
		      inputName,	/* input procedure */
		      outputName,	/* output procedure */
		      sendName,		/* send procedure */
		      receiveName,	/* recieve procedure */
			  elemName,		/* element type name */
		      defaultValue,	/* default type value */
		      byValue);		/* passed by value */
	/*
	 * ----------------
	 *  When we create a true type (as opposed to a complex type)
	 *  we need to have an shadow array entry for it in pg type as well.
	 * ----------------
	 */
    sprintf(shadow_type, "_%s", name);

    (void) TypeDefine(shadow_type,		/* type name */
		      InvalidObjectId,  /* relation oid (n/a here) */
		      -1,		/* internal size */
		      -1,		/* external size */
		      'b',		/* type-type (base type) */
		      ',',		/* array element delimiter */
		      "array_in",	/* input procedure */
		      "array_out",	/* output procedure */
		      "array_out",	/* send procedure */
		      "array_in",	/* recieve procedure */
		      name, 		/* element type name */
		      defaultValue,	/* default type value */
		      false);		/* never passed by value */
}
