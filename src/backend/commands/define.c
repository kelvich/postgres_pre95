/* ----------------------------------------------------------------
 * define.c --
 *	POSTGRES "define" utility code.
 *
 * DESCRIPTION:
 *	The "DefineFoo" routines take the parse tree and pick out the
 *	appropriate arguments/flags, passing the results to the
 *	corresponding "FooDefine" routines (in src/catalog) that do
 *	the actual catalog-munging.
 *
 * INTERFACE ROUTINES:
 *	DefineFunction
 *	DefineOperator
 *	DefineAggregate
 *	DefineType
 *
 * NOTES:
 *	These things must be defined and committed in the following order:
 *	"define function":
 *		input/output, recv/send procedures
 *	"define type":
 *		type
 *	"define operator":
 *		operators
 *
 *	Most of the parse-tree manipulation routines are defined in
 *	commands/manip.c.
 *
 * IDENTIFICATION:
 *	$Header$
 *
 * ----------------------------------------------------------------
 */

#include <string.h>
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
#include "commands/manip.h"
#include "nodes/pg_lisp.h"
#include "parse.h"		/* for ARG */
#include "fmgr.h"		/* for fmgr */
#include "utils/builtins.h"	/* prototype for textin() */
#include "utils/log.h"

#include "commands/defrem.h"
#include "planner/xfunc.h"

#include "tcop/dest.h"

#include "catalog/pg_protos.h"	/* prototypes for FooDefine() */

#define	DEFAULT_TYPDELIM	','

/* --------------------------------
 *	DefineFunction
 * --------------------------------
 */
void
DefineFunction(nameargsexe, dest)
    LispValue	nameargsexe;
    CommandDest dest;
{
    LispValue   parameters = CDR(CAR(nameargsexe));
    LispValue	entry;
    String	proname_str = CString(CAR(CAR(nameargsexe)));
    String	probin_str;
    String	prosrc_str;
    NameData	pronameData;
    NameData	prorettypeData;
    Name	languageName;
    bool	canCache;
    bool        trusted;
    LispValue	argList;
    int32       byte_pct, perbyte_cpu, percall_cpu, outin_ratio;
    int         count;
    char        *ptr;
    bool	returnsSet;
    int		i;

    /* ----------------
     * Note:
     * XXX	Checking of "name" validity (16 characters?) is needed.
     * ----------------
     */
    AssertArg(NameIsValid(proname_str));

    (void) namestrcpy(&pronameData, proname_str);

    /* ----------------
     * figure out the language and convert it to lowercase.
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "language");
    languageName = DefineEntryGetName(entry);
    for (i = 0; i < NAMEDATALEN && languageName->data[i]; ++i) {
	languageName->data[i] = tolower(languageName->data[i]);
    }
    
    /* ----------------
     * handle "returntype = X".  The function could return a singleton
     * value or a set of values.  Figure out which.
     * ----------------
     */
    entry = DefineListRemoveRequiredAssignment(&parameters, "returntype");
    if (IsA(CADR(entry),LispList)) {
	(void) namestrcpy(&prorettypeData,
			  (char *) LISPVALUE_STRING(CDR(CADR(entry))));
	returnsSet = true;
    } else {
	(void) namestrcpy(&prorettypeData, CString(CADR(entry)));
	returnsSet = false;
    }

    /* Next attributes only definable for C functions */
    if (!namestrcmp(languageName, "c")) {
	 /* ----------------
	  * handle "[ iscachable ]": figure out if Postquel functions are
	  * cacheable automagically?
	  * ----------------
	  */

	 entry = DefineListRemoveOptionalIndicator(&parameters, "iscachable");
	 canCache = (bool) !null(entry);
	
	 /*
	  * handle trusted/untrusted.  defaults to trusted for now; when
	  * i finish real support it'll default untrusted.  -dpassage
	  */

	 entry = DefineListRemoveOptionalAssignment(&parameters,
						    "trusted");
	 if (null(entry)) {
	     trusted = true;
	 } else {
	     trusted = ((DefineEntryGetString(entry)[0]) == 't');
	 }

	 /*
	  ** handle expensive function parameters
	  */
	 entry = DefineListRemoveOptionalAssignment(&parameters,
						    "byte_pct");
	 if (null(entry)) {
	     byte_pct = BYTE_PCT;
	 } else {
	     byte_pct = DefineEntryGetInteger(entry);
	 }

	 entry = DefineListRemoveOptionalAssignment(&parameters,
						    "perbyte_cpu");
	 if (null(entry)) {
	     perbyte_cpu = PERBYTE_CPU;
	 } else {
	     String perbyte_str = DefineEntryGetString(entry);

	     if (!sscanf(perbyte_str, "%d", &perbyte_cpu)) {
		 for (count = 0, ptr = perbyte_str; *ptr != '\0'; ptr++) {
		     if (*ptr == '!') {
			 count++;
		     }
		 }
		 perbyte_cpu = (int) pow(10.0, (double) count);
	     }
	 }
	 
	 entry = DefineListRemoveOptionalAssignment(&parameters,
						    "percall_cpu");
	 if (null(entry)) {
	     percall_cpu = PERCALL_CPU;
	 } else {
	     String percall_str = DefineEntryGetString(entry);

	     if (!sscanf(percall_str, "%d", &percall_cpu)) {
		 for (count = 0, ptr = percall_str; *ptr != '\0'; ptr++) {
		     if (*ptr == '!') {
			 count++;
		     }
		 }
		 percall_cpu = (int) pow(10.0, (double) count);
	     }
	 }
	 
	 entry = DefineListRemoveOptionalAssignment(&parameters,
						    "outin_ratio");
	 if (null(entry)) {
	     outin_ratio = OUTIN_RATIO;
	 } else {
	     outin_ratio = DefineEntryGetInteger(entry);
	 }
     } else if (!namestrcmp(languageName, "postquel")) {
	 canCache = false;
	 trusted = true;
	 
	 /* query optimizer groks postquel, these are meaningless */
	 perbyte_cpu = percall_cpu = 0;
	 byte_pct = outin_ratio = 100;
     } else {
	 elog(WARN, "DefineFunction: language \"-.*s\" is not supported",
	      NAMEDATALEN, languageName->data);
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

    if (!namestrcmp(languageName, "c")) {
	prosrc_str = "-";
	probin_str = CString(CADR(nameargsexe));
    } else {
	prosrc_str = CString(CADR(nameargsexe));
	probin_str = "-";
    }

    /* C is stored uppercase in pg_language */
    if (!namestrcmp(languageName, "c")) {
	languageName->data[0] = 'C';
    }

    /* ----------------
     *	now have ProcedureDefine do all the work..
     * ----------------
     */
    ProcedureDefine(&pronameData,
		    returnsSet,
		    &prorettypeData,
		    languageName,
		    prosrc_str,		/* converted to text later */
		    probin_str,		/* converted to text later */
		    canCache,
		    trusted,
		    byte_pct,
		    perbyte_cpu,
		    percall_cpu, 
		    outin_ratio,
		    argList,
		    dest);

    /* XXX free palloc'd memory */
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
    Name	name;			/* XXX actually a String */
    LispValue	parameters;
{
    LispValue	entry;
    uint16 	precedence; 		/* operator precedence */
    bool	canHash; 		/* operator hashes */
    bool	isLeftAssociative; 	/* operator is left associative */
    NameData	oprNameData;		/* operator name */
    Name 	functionName;	 	/* function for operator */
    Name 	typeName1;	 	/* first type name */
    Name 	typeName2;	 	/* second type name */
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

    (void) namestrcpy(&oprNameData, (char *) name);

    entry = DefineListRemoveOptionalAssignment(&parameters, "arg1");
    typeName1 = DefineEntryGetName(entry);

    entry = DefineListRemoveOptionalAssignment(&parameters, "arg2");
    typeName2 = DefineEntryGetName(entry);
    
    entry = DefineListRemoveRequiredAssignment(&parameters, "procedure");
    functionName = DefineEntryGetName(entry);

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
	if (!strcmp(string, "right")) {
	    isLeftAssociative = false;
	} else if (strcmp(string, "left") &&
		   strcmp(string, "none") &&
		   strcmp(string, "any")) {
	    elog(WARN, "Define: precedence = what?");
	} else {
	    isLeftAssociative = true;
	}
    }
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "commutator");
    commutatorName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "negator");
    negatorName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "restrict");
    restrictionName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "join");
    joinName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalIndicator(&parameters, "hashes");
    canHash = (bool) !null(entry);
    
    /* ----------------
     * XXX ( ... [ , sort1 = oprname ] [ , sort2 = oprname ] ... )
     * XXX is undocumented in the reference manual source as of 89/8/22.
     * ----------------
     */
    entry = DefineListRemoveOptionalAssignment(&parameters, "sort1");
    sortName1 = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "sort2");
    sortName2 = DefineEntryGetName(entry);

    /* ----------------
     *	there should be nothing more..
     * ----------------
     */
    DefineListAssertEmpty(parameters);

    /* ----------------
     *	now have OperatorDefine do all the work..
     * ----------------
     */
    OperatorDefine(&oprNameData,	/* operator name */
		   typeName1,		/* first type name */
		   typeName2,		/* second type name */
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

    /* XXX free palloc'd memory */
}

/* -------------------
 *  DefineAggregate
 * ------------------
 */
void
DefineAggregate(name, parameters)
    Name name;				/* XXX actually a String */
    LispValue parameters;
{
    NameData		aggNameData;
    Name		stepfunc1Name = (Name) NULL;
    Name		stepfunc2Name = (Name) NULL;
    Name		finalfuncName = (Name) NULL;
    Name		baseType = (Name) NULL;
    Name		stepfunc1Type = (Name) NULL;
    Name		stepfunc2Type = (Name) NULL;
    struct varlena	*primVal = (struct varlena *) NULL;
    struct varlena	*secVal = (struct varlena *) NULL;
    LispValue		entry;
    
    /* sanity checks...name validity */
    
    AssertArg(NameIsValid(name));
    AssertArg(listp(parameters));

    (void) namestrcpy(&aggNameData, (char *) name);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "sfunc1");
    if (entry) {
	stepfunc1Name = DefineEntryGetName(entry);
	entry = DefineListRemoveRequiredAssignment(&parameters, "basetype");
	baseType = DefineEntryGetName(entry);
	entry = DefineListRemoveRequiredAssignment(&parameters, "stype1");
	stepfunc1Type = DefineEntryGetName(entry);
    }
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "sfunc2");
    if (entry) {
	stepfunc2Name = DefineEntryGetName(entry);
	entry = DefineListRemoveRequiredAssignment(&parameters, "stype2");
	stepfunc2Type = DefineEntryGetName(entry);
    }

    entry = DefineListRemoveOptionalAssignment(&parameters, "finalfunc");
    finalfuncName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "initcond1");
    if (entry) {
	primVal = textin(DefineEntryGetString(entry));
    }
    entry = DefineListRemoveOptionalAssignment(&parameters, "initcond2");
    if (entry) {
	secVal = textin(DefineEntryGetString(entry));
    }

    DefineListAssertEmpty(parameters);
    
    /*
     * Most of the argument-checking is done inside of AggregateDefine
     */
    AggregateDefine(&aggNameData, 	/* aggregate name */
		    stepfunc1Name,	/* first step function name */
		    stepfunc2Name,	/* second step function name */
		    finalfuncName,	/* final function name */
		    baseType,		/* type of object being aggregated */
		    stepfunc1Type,	/* return type of first function */
		    stepfunc2Type,	/* return type of second function */
		    primVal,		/* first initial condition */
		    secVal);		/* second initial condition */

    /* XXX free palloc'd memory */
}

/* --------------------------------
 *	DefineType
 * --------------------------------
 */
void
DefineType(name, parameters)
    Name	name;			/* XXX actually a String */
    LispValue	parameters;
{
    LispValue		entry;
    int16		internalLength;		/* int2 */
    int16		externalLength;		/* int2 */
    NameData		typeNameData;
    Name		elemName;
    Name		inputName;
    Name		outputName;
    Name		sendName;
    Name		receiveName;
    char		*defaultValue;		/* Datum */
    bool		byValue;		/* Boolean */
    char		delimiter;
    NameData		shadow_type;
    static NameData	ain = { "array_in" };
    static NameData	aout = { "array_out" };
    
    /* ----------------
     *	sanity checks
     *
     * XXX	Checking of operator "name" validity
     *		(16 characters?) is needed.
     * ----------------
     */
    AssertArg(NameIsValid(name));
    AssertArg(listp(parameters));

    (void) namestrcpy(&typeNameData, (char *) name);

    /*
     * Type names can only be 15 characters long, so that the shadow type
     * can be created using the 16th character as necessary.
     */
    if (NameComputeLength(&typeNameData) >= (NAMEDATALEN - 1)) {
	elog(WARN, "DefineType: type names must be %d characters or less",
	     NAMEDATALEN - 1);
    }
    
    entry = DefineListRemoveRequiredAssignment(&parameters, "internallength");
    internalLength = DefineEntryGetLength(entry);

    entry = DefineListRemoveOptionalAssignment(&parameters, "externallength");
    if (!null(entry)) {
	externalLength = DefineEntryGetLength(entry);
    } else {
	externalLength = 0;		/* FetchDefault? */
    }
    
    entry = DefineListRemoveRequiredAssignment(&parameters, "input");
    inputName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveRequiredAssignment(&parameters, "output");
    outputName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "send");
    sendName = DefineEntryGetName(entry);

    entry = DefineListRemoveOptionalAssignment(&parameters, "delimiter");
    if (!null(entry)) {
	char *p = (char *) DefineEntryGetString(entry);
	delimiter = p[0];
    } else {
	delimiter = DEFAULT_TYPDELIM;	/* FetchDefault? */
    }

    entry = DefineListRemoveOptionalAssignment(&parameters, "receive");
    receiveName = DefineEntryGetName(entry);
    
    entry = DefineListRemoveOptionalAssignment(&parameters, "element");
    elemName = DefineEntryGetName(entry);

    entry = DefineListRemoveOptionalAssignment(&parameters, "default");
    defaultValue = DefineEntryGetString(entry);
    
    entry = DefineListRemoveOptionalIndicator(&parameters, "passedbyvalue");
    byValue = (bool) !null(entry);
    
    /* ----------------
     *	there should be nothing more..
     * ----------------
     */
    DefineListAssertEmpty(parameters);

    /* ----------------
     *	now have TypeDefine do all the real work.
     * ----------------
     */
    (void) TypeDefine(&typeNameData,	/* type name */
		      InvalidObjectId,  /* relation oid (n/a here) */
		      internalLength,	/* internal size */
		      externalLength,	/* external size */
		      'b',		/* type-type (base type) */
		      delimiter,	/* array element delimiter */
		      inputName,	/* input procedure */
		      outputName,	/* output procedure */
		      sendName,		/* send procedure */
		      receiveName,	/* receive procedure */
		      elemName,		/* element type name */
		      defaultValue,	/* default type value */
		      byValue);		/* passed by value */

    /* ----------------
     *  When we create a true type (as opposed to a complex type)
     *  we need to have an shadow array entry for it in pg_type as well.
     * ----------------
     */
    namestrcpy(&shadow_type, "_");
    namestrcat(&shadow_type, (char *) name);

    (void) TypeDefine(&shadow_type,	/* type name */
		      InvalidObjectId,  /* relation oid (n/a here) */
		      -1,		/* internal size */
		      -1,		/* external size */
		      'b',		/* type-type (base type) */
		      DEFAULT_TYPDELIM,	/* array element delimiter */
		      &ain,		/* input procedure */
		      &aout,		/* output procedure */
		      &aout,		/* send procedure */
		      &ain,		/* receive procedure */
		      &typeNameData,	/* element type name */
		      defaultValue,	/* default type value */
		      false);		/* never passed by value */

    /* XXX free palloc'd memory */
}
