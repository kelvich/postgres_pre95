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

#include "command.h"
#include "creatinh.h"
#include "defrem.h"
#include "pg_lisp.h"		/* lisp-compat package */
#include "xcxt.h"		/* for {Start,Commit,Abort}TransactionBlock */

extern int PG_INTERACTIVE;

#include "parse.h"		/* y.tab.h, created by yacc'ing gram.y */
#include "log.h"		/* error logging */

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
#ifndef	PERFECTPARSER
		AssertArg(consp(args) && null(CDR(args)));
#endif
		StartTransactionBlock();
		break;
	case END_TRANS:
		commandTag = "END";
#ifndef	PERFECTPARSER
		AssertArg(consp(args) && null(CDR(args)));
#endif
		CommitTransactionBlock();
		break;
	case ABORT_TRANS:
		commandTag = "ABORT";
#ifndef	PERFECTPARSER
		AssertArg(consp(args) && null(CDR(args)));
#endif
		AbortTransactionBlock();
		break;

		/*
		 * portal manipulation
		 */
	case CLOSE:
		commandTag = "CLOSE";
#ifndef	PERFECTPARSER
		AssertArg(consp(args));
		AssertArg(null(CDR(args)));
		AssertArg(null(CAR(args)) || lispStringp(CAR(args)));
#endif
		PerformPortalClose((null(CAR(args))) ? NULL :
			CString(CAR(args)));
		break;
	case FETCH:
		commandTag = "FETCH";
	{
		String	portalName = NULL;
		bool	forward;
		int	count;
#ifndef	PERFECTPARSER
		AssertArg(consp(args));
		AssertArg(consp(CDR(args)));
		AssertArg(lispAtomp(CADR(args)));
		AssertArg(CAtom(CADR(args)) == FORWARD || CAtom(CADR(args)) == BACKWARD);
		AssertArg(consp(CDR(CDR(args))));
		AssertArg(lispIntegerp(CADDR(args)) || lispAtomp(CADDR(args)));
		AssertArg(null(CDR(CDR(CDR(args)))));
#endif
		if (!null(CAR(args))) {
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CAR(args)));
#endif
			portalName = CString(CAR(args));
		}
		forward = (bool)(CAtom(CADR(args)) == FORWARD);
		count = 0;
		if (lispAtomp(CADDR(args))) {
#ifndef	PERFECTPARSER
			AssertArg(CAtom(CADDR(args)) == ALL);
#endif
		} else {
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
		DefineRelation(CAR(args),	/*  relation name */
			CADR(args),		/*  parameters */
			CDR(CDR(args)));	/*  schema */
		break;

	case DESTROY:
		commandTag = "DESTROY";
	{
		LispValue relationName;
		foreach (relationName, args) {
#ifndef	PERFECTPARSER
			AssertArg(listp(relationName));
			AssertArg(lispStringp(CAR(relationName)));
#endif
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
#ifndef	PERFECTPARSER
		AssertArg(listp(args));
		AssertArg(length(args) == 2);
#endif
		tags = CADR(args);
		switch (length(tags)) {
		case 0:
			break;
		case 1:
#ifndef	PERFECTPARSER
			AssertArg(lispIntegerp(CAR(CAR(tags))));
			AssertArg(lispStringp(CADR(CAR(tags))));
#endif
			if (CInteger(CAR(CAR(tags))) == BEFORE) {
				beforeString = CString(CADR(CAR(tags)));
			} else {
				afterString = CString(CADR(CAR(tags)));
#ifndef	PERFECTPARSER
				AssertArg(CInteger(CAR(CAR(tags))) == AFTER);
#endif
			}
			break;
		case 2:
#ifndef	PERFECTPARSER
			AssertArg(lispIntegerp(CAR(CAR(tags))));
			AssertArg(CInteger(CAR(CAR(tags))) == BEFORE);
			AssertArg(lispStringp(CADR(CAR(tags))));
			AssertArg(lispIntegerp(CAR(CADR(tags))));
			AssertArg(CInteger(CAR(CADR(tags))) == AFTER);
			AssertArg(lispStringp(CADR(CADR(tags))));
#endif
			beforeString = CString(CADR(CAR(tags)));
			afterString = CString(CADR(CADR(tags)));
			break;
		default:
#ifndef	PERFECTPARSER
			AssertArg(false);
#endif
		}
		RelationPurge(CString(CAR(args)), beforeString, afterString);
	}
		break;

		/* tags/date alist */
	case COPY:
		commandTag = "COPY";
	{
		String	relationName;
		String	fileName;
		String	mapName = NULL;
		bool	isBinary = false;
		bool	noNulls = false;
		bool	isFrom;

#ifndef	PERFECTPARSER
		AssertArg(length(args) >= 3);
		AssertArg(length(CAR(args)) == 3);
		AssertArg(lispStringp(CAAR(args)));
#endif
		relationName = CString(CAAR(args));
		if (!null(CADR(CAR(args)))) {
#ifndef	PERFECTPARSER
			AssertArg(lispIntegerp(CADR(CAR(args))));
			AssertArg(CInteger(CADR(CAR(args))) == BINARY);
#endif
			isBinary = true;
		}
		if (!null(CADDR(CAR(args)))) {
#ifndef	PERFECTPARSER
			AssertArg(lispIntegerp(CADDR(CAR(args))));
			AssertArg(CInteger(CADDR(CAR(args))) == NONULLS);
#endif
			noNulls = true;
		}
		/*
		 * discard '("relname" [BINARY] [NONULLS])
		 */
		args = CDR(args);

#ifndef	PERFECTPARSER
		AssertArg(length(CAR(args)) == 2);
		AssertArg(lispAtomp(CAAR(args)));
		AssertArg(lispStringp(CADR(CAR(args))));
#endif
		if (CAtom(CAAR(args)) == FROM) {
			isFrom = true;
		} else {
#ifndef	PERFECTPARSER
			AssertArg(CAtom(CAAR(args)) == TO);
#endif
			isFrom = false;
		}
		fileName = CString(CADR(CAR(args)));
		/*
		 * discard '(FROM/TO "filename")
		 */
		args = CDR(args);

#ifndef	PERFECTPARSER
		AssertArg(length(CAR(args)) >= 1);
		/*
		 * Note:
		 *	CAAR(args) is not checked to verify it is a USING int.
		 */
#endif
		if (!null(CDR(CAR(args)))) {
#ifndef	PERFECTPARSER
			AssertArg(length(CAR(args)) == 2);
			AssertArg(lispStringp(CADR(CAR(args))));
#endif
			mapName = CString(CADR(CAR(args)));
		}
		/*
		 * discard '(USING ["mapName"])
		 */
		args = CDR(args);
#ifndef	PERFECTPARSER
		/*
		 * Check the format of args (the domain list) here
		 */
#endif
		PerformRelationFilter(relationName, isBinary, noNulls, isFrom,
			fileName, mapName, args);
	}
		break;

		/* domain list */
#if 0
	case ADD_ATTR:
		commandTag = "ADD";
#ifndef	PERFECTPARSER
		AssertArg(consp(args) && lispStringp(CAR(args)));
		AssertArg(consp(CDR(args)) && lispStringp(CADR(args)));
		AssertArg(null(CDR(CDR(args))));
#endif
		addattribute(CString(CAR(args)),
		relation_add_attribute (CAR (args),	/* relation name */
			CDR (args));
		break;
#endif
		/*
		 * schema
		 */
	case RENAME:
		commandTag = "RENAME";
	{
		int	len;

		len = length(args);
#ifndef	PERFECTPARSER
		AssertArg(len == 3 || len == 4);
		AssertArg(lispStringp(CADR(args)));
		AssertArg(lispStringp(CADDR(args)));
		if (len == 4) {
			AssertArg(lispStringp(CADDR(CDR(args))));
		}
#endif
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
		/* object creation */
	case DEFINE:
		commandTag = "DEFINE";
#ifndef	PERFECTPARSER
		/*
		 * Index is an integer; Type, etc. are atoms?!?
		 */
		AssertArg(atom(CAR(args)) || integerp(CAR(args)));
#endif
		switch(LISPVALUE_INTEGER(CAR(args))) {
		case INDEX:	/* XXX no support for ARCHIVE indices, yet */
			args = CDR(args);	/* skip "INDEX" token */
#ifndef	PERFECTPARSER
			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
			AssertArg(lispStringp(CADR(args)));
			AssertArg(listp(CDR(CDR(args))));
			AssertArg(lispStringp(CADDR(args)));
			AssertArg(listp(CDR(CDR(CDR(args)))));
			AssertArg(listp(CDR(CDR(CDR(CDR(args))))));
#endif
			DefineIndex(CString(CAR(args)), /* relation name */
				CString(CADR(args)),	/* index name */
				CString(CADDR(args)),	/* am name */
				CADDR(CDR(args)),	/* parameters */
				CADDR(CDR(CDR(args))));	/* with */
			break;
		case OPERATOR:
			args = CDR(args);	/* skip "OPERATOR" token */
#ifndef	PERFECTPARSER
			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
#endif
			DefineOperator(
				CString(CAR(args)),	/* operator name */
				CDR(args));		/* rest */
			break;
		case FUNCTION:
			args = CDR(args);	/* skip "FUNCTION" token */
#ifndef	PERFECTPARSER
			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
#endif
			DefineFunction(
				CString(CAR(args)),	/* function name */
				CDR(args));		/* rest */
			break;

		case RULE:
			elog(WARN,
				"InvokeUtility: DEFINE RULE now unsupported");
#if 0
			rule_define(CAR(args),		/* rule name */
				CADR(args));		/* parsed rule query */
#endif
			break;

		case P_TYPE:
			args = CDR(args);	/* skip "TYPE" token */
#ifndef	PERFECTPARSER
			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
#endif
			DefineType(
				CString(CAR(args)),	/* type name */
				CDR(args));		/* rest */
			break;
		default:
			elog(WARN, "unknown DEFINE parse type %d",
				CInteger(CAR(args)));
			break;
		}
		break;

		/*
		 * object destruction
		 */
	case REMOVE:
		commandTag = "REMOVE";
#ifndef	PERFECTPARSER
		AssertArg(consp(args));
#endif
		switch(CInteger(CAR(args))) {
		case FUNCTION:
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CADR(args)));
#endif
			RemoveFunction(CString(CADR(args)));
			break;
		case INDEX:
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CADR(args)));
#endif
			RemoveIndex(CString(CADR(args)));
			break;
		case OPERATOR:
		{
			int	argCount;
			String	type2;
#ifndef	PERFECTPARSER
			AssertArg(consp(CADR(args)));
			AssertArg(null(CDR(CDR(args))));
#endif
			args = CADR(args);
			argCount = length(args);
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CAR(args)));
			AssertArg(2 <= argCount && argCount <= 3);
			AssertArg(lispStringp(CADR(args)));
			if (argCount == 3) {
				AssertArg(lispStringp(CADR(CDR(args))));
			}
#endif
			type2 = NULL;
			if (argCount == 3) {
				type2 = CString(CADR(CDR(args)));
			}
			RemoveOperator(CString(CAR(args)), CString(CADR(args)),
				type2);
		}
			break;
		case RULE:
#ifndef	PERFECTPARSER
			AssertArg(length(args) == 2);
			AssertArg(lispStringp(CADR(args)));
#endif
			RemoveRule(CString(CADR(args)));
			break;
		case P_TYPE:
#ifndef	PERFECTPARSER
			AssertArg(length(args) == 2);
			AssertArg(lispStringp(CADR(args)));
#endif
			RemoveType(CString(CADR(args)));
			break;
		default:
			elog(WARN, "unknown REMOVE parse type %d",
				CInteger(CAR(args)));
		}
			
		/* default */
	default:
		elog(WARN, "ProcessUtility: command #%d unsupport", command);
		break;
	}

	EndCommand(commandTag);
}
