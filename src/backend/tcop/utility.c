/*     
 * utility.c --
 *     
 *      DESCRIPTION
 *     	Contains functions which control the execution of the
 *     	POSTGRES utility commands.  Acts as an interface between
 *     	the Lisp and C systems.
 *     
 */

#include "c.h"

RcsId("$Header$");

#include "creatinh.h"
#include "defrem.h"
#include "pg_lisp.h"		/* lisp-compat package */
#include "xcxt.h"		/* for {Start,Commit,Abort}TransactionBlock */

#include "parse.h"		/* y.tab.h, created by yacc'ing gram.y */
#include "log.h"		/* error logging */

#include "command.h"

/*    
 *     general utility function invoker
 *    
 */
void
ProcessUtility(command, args)
	int		command;	/* "tag" */
	LispValue	args;
{
    String	commandTag = NULL;
    
    switch (command) {
	/*
	 * transactions
	 */
      case BEGIN_TRANS:
	commandTag = "BEGIN";
	StartTransactionBlock();
	break;
      case END_TRANS:
	commandTag = "END";
	CommitTransactionBlock();
	break;
      case ABORT_TRANS:
	commandTag = "ABORT";
	AbortTransactionBlock();
	break;
	
	/*
	 * portal manipulation
	 */
      case CLOSE:
	commandTag = "CLOSE";
	PerformPortalClose((null(CAR(args))) ? NULL :
			   CString(CAR(args)));
	break;
      case FETCH:
	commandTag = "FETCH";
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
	elog(WARN, "MOVE: unimplemented in Version 1");
	
	/*
	 * relation and attribute manipulation
	 */
      case CREATE:
	commandTag = "CREATE";
	DefineRelation(CString(CAR(args)),	/*  relation name */
		       CADR(args),		/*  parameters */
		       CDR(CDR(args)));	/*  schema */
	break;
	
      case DESTROY:
	commandTag = "DESTROY";
	{
	    LispValue relationName;
	    foreach (relationName, args) {
		RemoveRelation(CString(CAR(relationName)));
	    }
	}
	break;
	
      case PURGE:
	commandTag = "PURGE";
	{
	    List	tags;
	    String	beforeString = NULL;	/* absolute time string */
	    String	afterString = NULL;	/* relative time string */
	    
	    tags = CADR(args);
	    switch (length(tags)) {
	      case 0:
		break;
	      case 1:
		if (CInteger(CAR(CAR(tags))) == BEFORE) {
		    beforeString = CString(CADR(CAR(tags)));
		} else {
		    afterString = CString(CADR(CAR(tags)));
		}
		break;
	      case 2:
		beforeString = CString(CADR(CAR(tags)));
		afterString = CString(CADR(CADR(tags)));
		break;
	    }
	    RelationPurge(CString(CAR(args)), beforeString, afterString);
	}
	break;
	
      case COPY:
	commandTag = "COPY";
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
	
	PerformAddAttribute(CString(CAR(args)), CDR(args));
	break;
	
	/*
	 * schema
	 */
      case RENAME:
	commandTag = "RENAME";
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
	  case FUNCTION:
	    DefineFunction(
			   CString(CADR(args)),	/* function name */
			   CDR(CDR(args)));	/* rest */
	    break;
	  case RULE:
	    DefineRule(CString(CADR(args)),	/* rule name */
		       CADDR(args));		/* parsed rule query */
	    break;
	  case REWRITE:
	    DefineQueryRewrite ( CDR (args )) ; 
	    break;
	  case TUPLE:
	    /* XXX - Spyros, can you add your stuff here */
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
	    RemoveRule(CString(CADR(args)));
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

