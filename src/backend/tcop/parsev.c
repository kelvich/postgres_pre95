/* ----------------------------------------------------------------
 *   FILE
 *	parsev.c
 *	
 *   DESCRIPTION
 *	Parser validity checking code.
 *
 *   INTERFACE ROUTINES
 *	ValidateParse
 *
 *   NOTES
 *	Not sure why this is in the tcop directory
 *
 *   IDENTIFICATION
 *	$Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

 RcsId("$Header$");

/* ----------------
 *	FILE INCLUDE ORDER GUIDELINES
 *
 *	1) tcopdebug.h
 *	2) various support files ("everything else")
 *	3) node files
 *	4) catalog/ files
 *	5) execdefs.h and execmisc.h, if necessary.
 *	6) extern files come last.
 * ----------------
 */
#include "tcop/tcopdebug.h"

#include "parser/parse.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"

/* ----------------
 *	ValidateParsedQuery
 * ----------------
 */
static void
ValidateParsedQuery(parse)
    List	parse;
{
    AssertArg(consp(parse) && !lispIntegerp(CAR(parse)));

    /*
     * XXX code here
     */
}

/* ----------------
 *	ValidateUtility
 * ----------------
 */
static void
ValidateUtility(command, args)
	int		command;	/* "tag" */
	LispValue	args;
{
	switch (command) {
		/*
		 * transactions
		 */
	case BEGIN_TRANS:
	case END_TRANS:
	case ABORT_TRANS:
		AssertArg(null(args));
		break;

		/*
		 * portal manipulation
		 */
	case CLOSE:
		AssertArg(consp(args));
		AssertArg(null(CDR(args)));
		AssertArg(null(CAR(args)) || lispStringp(CAR(args)));
		break;
	case FETCH:
		AssertArg(consp(args));
		AssertArg(consp(CDR(args)));
		AssertArg(lispAtomp(CADR(args)));
		AssertArg(CAtom(CADR(args)) == FORWARD || CAtom(CADR(args)) == BACKWARD);
		AssertArg(consp(CDR(CDR(args))));
		AssertArg(lispIntegerp(CADDR(args)) || lispAtomp(CADDR(args)));
		AssertArg(null(CDR(CDR(CDR(args)))));

		if (!null(CAR(args))) {
			AssertArg(lispStringp(CAR(args)));
		}
		if (lispAtomp(CADDR(args))) {
			AssertArg(CAtom(CADDR(args)) == ALL);
		}
		break;
	case MOVE:
		/*
		 * XXX code here
		 */
		break;

		/*
		 * relation and attribute manipulation
		 */
	case CREATE:
	{
		List	l;
		List	schema;
		

		AssertArg(length(args) >= 2);
		AssertArg(lispStringp(CAR(args)));
		/*
		 * validate parameters
		 */
		l = CADR(args);
		AssertArg(listp(l) && length(l) == 4);
		if (!lispNullp(CAR(l))) {
			AssertArg(lispIntegerp(CAR(CAR(l))));
			AssertArg(CInteger(CAR(CAR(l))) == KEY);
			/*
			 * XXX validate KEY here
			 */
		}
		l = CDR(l);
		if (!lispNullp(CAR(l))) {
			List	inheritList;

			AssertArg(lispIntegerp(CAR(CAR(l))));
			AssertArg(CInteger(CAR(CAR(l))) == INHERITS);

			inheritList = CDR(CAR(l));
			/* duplicates? */
		}
		l = CDR(l);
		if (!lispNullp(CAR(l))) {
			AssertArg(lispIntegerp(CAR(CAR(l))));
			AssertArg(CInteger(CAR(CAR(l))) == INDEXABLE);
			/*
			 * XXX validate INDEXABLE here
			 */
		}
		l = CDR(l);
		if (!lispNullp(CAR(l))) {
			AssertArg(lispIntegerp(CAR(CAR(l))));
			AssertArg(CInteger(CAR(CAR(l))) == ARCHIVE);
			/*
			 * XXX validate ARCHIVE here
			 */
		}

		/*
		 * validate schema
		 */
		schema = CDR(CDR(args));
		AssertArg(listp(schema));
		foreach (l, schema) {
			AssertArg(listp(CAR(l)));
			AssertArg(listp(CDR(CAR(l))));
			AssertArg(lispStringp(CAR(CAR(l))));
			AssertArg(lispStringp(CADR(CAR(l))));
		}
	}
		break;

	case DESTROY:
		AssertArg(consp(args));
	{
		LispValue relationName;
		foreach (relationName, args) {
			AssertArg(lispStringp(CAR(relationName)));
		}
	}
		break;

	case PURGE:
	{
		List	tags;

		AssertArg(listp(args));
		AssertArg(length(args) == 2);

		tags = CADR(args);
		switch (length(tags)) {
		case 0:
			break;
		case 1:
			AssertArg(lispIntegerp(CAR(CAR(tags))));
			AssertArg(lispStringp(CADR(CAR(tags))));

			AssertArg(CInteger(CAR(CAR(tags))) == BEFORE || CInteger(CAR(CAR(tags))) == AFTER);
			break;
		case 2:
			AssertArg(lispIntegerp(CAR(CAR(tags))));
			AssertArg(CInteger(CAR(CAR(tags))) == BEFORE);
			AssertArg(lispStringp(CADR(CAR(tags))));
			AssertArg(lispIntegerp(CAR(CADR(tags))));
			AssertArg(CInteger(CAR(CADR(tags))) == AFTER);
			AssertArg(lispStringp(CADR(CADR(tags))));
			break;
		default:
			AssertArg(false);
		}
	}
		break;

	case COPY:
	{
		AssertArg(length(args) >= 3);
		AssertArg(length(CAR(args)) == 3);
		AssertArg(lispStringp(CAAR(args)));

		if (!null(CADR(CAR(args)))) {
			AssertArg(lispIntegerp(CADR(CAR(args))));
			AssertArg(CInteger(CADR(CAR(args))) == BINARY);
		}
		if (!null(CADDR(CAR(args)))) {
			AssertArg(lispIntegerp(CADDR(CAR(args))));
			AssertArg(CInteger(CADDR(CAR(args))) == NONULLS);
		}
		/*
		 * discard '("relname" [BINARY] [NONULLS])
		 */
		args = CDR(args);

		AssertArg(length(CAR(args)) == 2);
		AssertArg(lispAtomp(CAAR(args)));
		AssertArg(lispStringp(CADR(CAR(args))));
		AssertArg(CAtom(CAAR(args)) == FROM || CAtom(CAAR(args)) == TO);
		/*
		 * discard '(FROM/TO "filename")
		 */
		args = CDR(args);

		AssertArg(length(CAR(args)) >= 1);
		AssertArg(lispAtomp(CAAR(args)));
		AssertArg(CAtom(CAAR(args)) == USING);

		if (!null(CDR(CAR(args)))) {
			AssertArg(length(CAR(args)) == 2);
			AssertArg(lispStringp(CADR(CAR(args))));
		}
		/*
		 * discard '(USING ["mapName"])
		 */
		args = CDR(args);

		/*
		 * XXX Check the format of args (the domain list) here
		 */
	}
		break;

	case ADD_ATTR:
	{
		List	element;

		AssertArg(length(args) >= 2);
		AssertArg(lispStringp(CAR(args)));

		foreach (element, CDR(args)) {
			AssertArg(consp(CAR(element)));
			AssertArg(lispStringp(CAR(CAR(element))));
			AssertArg(lispStringp(CADR(CAR(element))));
			AssertArg(null(CDR(CDR(CAR(element)))));
		}
	}
		break;

		/*
		 * schema
		 */
	case RENAME:
	{
		int	len;

		len = length(args);

		AssertArg(len == 3 || len == 4);
		AssertArg(lispStringp(CAR(args)));
		AssertArg(lispStringp(CADR(args)));
		AssertArg(lispStringp(CADDR(args)));
		if (len == 3) {
			NameIsEqual((Name)CString(CAR(args)), (Name)"RELATION");
		} else {
			NameIsEqual((Name)CString(CAR(args)),(Name)"ATTRIBUTE");
			AssertArg(lispStringp(CADDR(CDR(args))));
		}
	}
		break;

		/*
		 * object creation
		 */
	case DEFINE:
		/*
		 * Index is an integer; Type, etc. are atoms?!?
		 */
		AssertArg(atom(CAR(args)) || integerp(CAR(args)));

		switch(LISPVALUE_INTEGER(CAR(args))) {
		case INDEX:	/* XXX no support for ARCHIVE indices, yet */
			args = CDR(args);	/* skip "INDEX" token */

			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
			AssertArg(lispStringp(CADR(args)));
			AssertArg(listp(CDR(CDR(args))));
			AssertArg(lispStringp(CADDR(args)));
			AssertArg(listp(CDR(CDR(CDR(args)))));
			AssertArg(listp(CDR(CDR(CDR(CDR(args))))));
			break;
		case OPERATOR:
			args = CDR(args);	/* skip "OPERATOR" token */

			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
			break;
		case FUNCTION:
			args = CDR(args);	/* skip "FUNCTION" token */

			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
			break;
		case AGGREGATE:
			args = CDR(args);

			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
			break;
		case RULE:
			AssertArg(listp(args));
			AssertArg(lispStringp(CADR(args)));
			AssertArg(listp(CADDR(args)));
			break;

		case P_TYPE:
			args = CDR(args);	/* skip "TYPE" token */

			AssertArg(listp(args));
			AssertArg(lispStringp(CAR(args)));
			AssertArg(listp(CDR(args)));
			break;
		default:
			AssertArg(false);
			break;
		}
		break;

		/*
		 * object destruction
		 */
	case REMOVE:
		AssertArg(consp(args));

		switch(CInteger(CAR(args))) {
		case FUNCTION:
		case AGGREGATE:
		case INDEX:
		case RULE:
		case P_TYPE:
			AssertArg(length(args) == 2);
			AssertArg(lispStringp(CADR(args)));
			break;
		case OPERATOR:
		{
			int	argCount;

			AssertArg(consp(CADR(args)));
			AssertArg(null(CDR(CDR(args))));

			args = CADR(args);
			argCount = length(args);

			AssertArg(lispStringp(CAR(args)));
			AssertArg(2 <= argCount && argCount <= 3);
			AssertArg(lispStringp(CADR(args)));
			if (argCount == 3) {
				AssertArg(lispStringp(CADR(CDR(args))));
			}
		}
			break;
		default:
			AssertArg(false);
		}
		break;
			
		/* default */
	default:
		break;
	}
}

/* ----------------
 *	ValidateParse
 * ----------------
 */
void
ValidateParse(parse)
	List	parse;
{
	AssertArg(consp(parse));

	if (!lispIntegerp(CAR(parse))) {
		ValidateParsedQuery(parse);
	} else {
		ValidateUtility(CInteger(CAR(parse)), CDR(parse));
	}
}
