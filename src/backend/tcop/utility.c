/* ----------------------------------------------------------------
 *   FILE
 *	utility.c
 *	
 *   DESCRIPTION
 *     	Contains functions which control the execution of the
 *     	POSTGRES utility commands.  At one time acted as an
 *	interface between the Lisp and C systems.
 *	
 *   INTERFACE ROUTINES
 *	
 *   NOTES
 *	
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tcop.h"

RcsId("$Header$");

/* ----------------
 *	CHECK_IF_ABORTED() is used to avoid doing unnecessary
 *	processing within an aborted transaction block.
 * ----------------
 */
#define CHECK_IF_ABORTED() \
if (IsAbortedTransactionBlockState()) { \
    elog(NOTICE, "(transaction aborted): %s", \
	 "queries ignored until END"); \
    commandTag = "*ABORT STATE*"; \
    break; \
} \
	    
/* ----------------
 *	general utility function invoker
 * ----------------
 */
void
ProcessUtility(command, args, commandString)
    int		command;	/* "tag" */
    LispValue	args;
    char 	*commandString;
{
    String	commandTag = NULL;
    
    switch (command) {
	/*
	 * transactions
	 */
      case BEGIN_TRANS:
	commandTag = "BEGIN";
	CHECK_IF_ABORTED();
	
	BeginTransactionBlock();
	break;
	
      case END_TRANS:
	commandTag = "END";
	EndTransactionBlock();
	break;
	
      case ABORT_TRANS:
	commandTag = "ABORT";
	CHECK_IF_ABORTED();
	
	AbortTransactionBlock();
	break;
	
	/*
	 * portal manipulation
	 */
      case CLOSE:
	commandTag = "CLOSE";
	CHECK_IF_ABORTED();
	
	PerformPortalClose((null(CAR(args))) ? NULL :
			   CString(CAR(args)));
	break;
	
      case FETCH:
	commandTag = "FETCH";
	CHECK_IF_ABORTED();
	
	{
	    String	portalName = NULL;
	    bool	forward;
	    int	count;
	    
	    if (!null(CAR(args))) {
		portalName = CString(CAR(args));
	    }
	    forward = (bool)(CAtom(CADR(args)) == FORWARD);
	    count = 0;
	    if (!lispAtomp(CADDR(args))) {
		count = CInteger(CADDR(args));
		
		if (count < 0) {
		    elog(WARN, "Fetch: specify nonnegative count");
		}
		if (count == 0) {
		    break;
		}
	    }
	    PerformPortalFetch(portalName, forward, count);
	}
	break;
	
      case MOVE:
	commandTag = "MOVE";
	CHECK_IF_ABORTED();
	
	elog(NOTICE, "MOVE: unimplemented in Version 1");
	break;
	
	/*
	 * relation and attribute manipulation
	 */
	
      case CREATE:
	commandTag = "CREATE";
	CHECK_IF_ABORTED();
	
	DefineRelation(CString(CAR(args)),	/*  relation name */
		       CADR(args),		/*  parameters */
		       CDR(CDR(args)));	/*  schema */
	break;
	
      case DESTROY:
	commandTag = "DESTROY";
	CHECK_IF_ABORTED();
	
	{
	    LispValue relationName;
	    foreach (relationName, args) {
		RemoveRelation(CString(CAR(relationName)));
	    }
	}
	break;
	
      case PURGE:
	commandTag = "PURGE";
	CHECK_IF_ABORTED();
	
	{
	    List	tags;
	    String	beforeString = NULL;	/* absolute time string */
	    String	afterString = NULL;	/* relative time string */
	    
	    tags = CADR(args);
	    switch (length(tags)) {
	      case 1:
		break;
	      case 2:
		if (CInteger(CAR(CAR(tags))) == BEFORE) {
		    beforeString = CString(CADR(CAR(tags)));
		} else {
		    afterString = CString(CADR(CAR(tags)));
		}
		break;
	      case 3:
		beforeString = CString(CADR(CAR(tags)));
		afterString = CString(CADR(CADR(tags)));
		break;
	    }
	    RelationPurge(CString(CAR(args)), beforeString, afterString);
	}
	break;
	
      case COPY:
	commandTag = "COPY";
	CHECK_IF_ABORTED();
	
	{
	    String	relationName;
	    String	fileName;
	    String	mapName = NULL;
	    bool	isBinary;
	    bool	noNulls;
	    bool	isFrom;
	    
	    relationName = CString(CAAR(args));
	    isBinary = (bool)!null(CADR(CAR(args)));
	    noNulls = (bool)!null(CADDR(CAR(args)));
	    /*
	     * discard '("relname" [BINARY] [NONULLS])
	     */
	    args = CDR(args);
	    
	    isFrom = (bool)(CAtom(CAAR(args)) == FROM);
	    fileName = CString(CADR(CAR(args)));
	    /*
	     * discard '(FROM/TO "filename")
	     */
	    args = CDR(args);
	    
	    if (!null(CDR(CAR(args)))) {
		mapName = CString(CADR(CAR(args)));
	    }
	    /*
	     * discard '(USING ["mapName"])
	     */
	    args = CDR(args);
	    
	    PerformRelationFilter(relationName, isBinary, noNulls, isFrom,
				  fileName, mapName, args);
	}
	break;
	
      case ADD_ATTR:
	commandTag = "ADD";
	CHECK_IF_ABORTED();
		
	PerformAddAttribute(CString(CAR(args)), CDR(args));
	break;
	
	/*
	 * schema
	 */
      case RENAME:
	commandTag = "RENAME";
	CHECK_IF_ABORTED();
	
	{
	    int	len;
		
	    len = length(args);
	    
	    /*
	     * skip unused "RELATION" or "ATTRIBUTE" tag
		 */
	    args = CDR(args);
	    if (len == 3) {	/* relation */
		renamerel(CString(CAR(args)), CString(CADR(args)));
		} else {	/* attribute */
		    renamerel(CString(CAR(args)), CString(CADR(args)),
			      CString(CADDR(args)));
		}
	    }
	break;
	
	/*
	 * object creation
	 */
      case DEFINE:
	commandTag = "DEFINE";
	CHECK_IF_ABORTED();
		
	switch(LISPVALUE_INTEGER(CAR(args))) {
	  case INDEX:	/* XXX no support for ARCHIVE indices, yet */
	    args = CDR(args);	/* skip "INDEX" token */
	    
	    DefineIndex(CString(CAR(args)), /* relation name */
			CString(CADR(args)),	/* index name */
			CString(CADDR(args)),	/* am name */
			CADDR(CDR(args)),	/* parameters */
			CADDR(CDR(CDR(args))));	/* with */
		break;
	  case OPERATOR:
	    DefineOperator(
			       CString(CADR(args)),	/* operator name */
			   CDR(CDR(args)));	/* rest */
		break;
	  case C_FUNCTION:
	    DefineFunction(
			   CString(CADR(args)),	/* function name */
			   CDR(CDR(args)));	/* rest */
	    break;
	  case P_FUNCTION:
	    DefinePFunction(CString(CADR(args)), /* function name */
			    CString(CADDR(args)), /* relation name */
			    CString(nth(3,args)));  /* query string */
	    break;
	  case RULE:
	    elog(WARN,
	    "Sorry, the old rule system is not supported any more (yet!)");
	    break;
	  case REWRITE:
	    DefineQueryRewrite ( CDR( CDR (args ))) ; 
	    break;
	  case P_TUPLE:
	    prs2DefineTupleRule(args, commandString);
	    break;
	  case P_TYPE:
	    DefineType (CString(CADR(args)),	/* type name */
			CDR(CDR(args)));	/* rest */
	    break;
	  case VIEW:
	    DefineView (CString(CADR(args)),	/* view name */
			CDR(CDR(args)) );	/* retrieve parsetree */
	    break;
	  default:
	    elog(WARN, "unknown object type in define statement");
	} /* switch for define statements */
	break;
	
	/*
	 * object destruction
	 */
      case REMOVE:
	commandTag = "REMOVE";
	CHECK_IF_ABORTED();
		
	switch(CInteger(CAR(args))) {
	  case FUNCTION:
	    RemoveFunction(CString(CADR(args)));
	    break;
	  case INDEX:
	    RemoveIndex(CString(CADR(args)));
	    break;
	  case OPERATOR:
	    {
		String	type2 = NULL;
		
		args = CADR(args);
		if (length(args) == 3) {
		    type2 = CString(CADR(CDR(args)));
		}
		RemoveOperator(CString(CAR(args)), CString(CADR(args)),
			       type2);
	    }
	    break;
	  case RULE:
	    prs2RemoveTupleRule(CString(CADR(args)));
	    break;
	  case P_TYPE:
	    RemoveType(CString(CADR(args)));
	    break;
	}
	break;
	
	/* default */
      default:
	elog(WARN, "ProcessUtility: command #%d unsupport", command);
	break;
    }
    
    EndCommand(commandTag);
}

