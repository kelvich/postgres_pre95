/*
 * pquery.c --
 *	POSTGRES process query command code.
 */

#include "c.h"

RcsId("$Header$");

#include "executor.h"		/* XXX a catch-all include */
#include "globals.h"		/* for IsUnderPostmaster */
#include "heapam.h"
#include "htup.h"
#include "log.h"
#include "oid.h"
#include "parse.h"
#include "pg_lisp.h"
#include "portal.h"
#include "rel.h"
#include "sdir.h"
#include "tim.h"

#include "command.h"

/* static ? */
List
MakeQueryDesc(command, parsetree, plantree, state, feature)
	List  command;
	List  parsetree;
	List  plantree;
	List  state;
	List  feature;
{
	return ((List)lispCons(command,
		lispCons(parsetree,
			lispCons(plantree,
				lispCons(state,
					lispCons(feature, LispNil))))));
}

void
ProcessQuery(parser_output, plan)
	List parser_output;
	Plan plan;
{
	bool		isPortal = false;
	bool		isInto = false;
	Relation	intoRelation;
	List		parseRoot;
	List		resultDesc;

	List 		queryDesc;
	EState 		state;
	List 		feature;
	List 		attinfo;		/* returned by ExecMain */

	int		commandType;
	String		tag;
	ObjectId	intoRelationId;
	String		intoName;
	int		numatts;
	AttributePtr	tupleDesc;

	ScanDirection   direction;
	abstime         time;
	ObjectId        owner;
	List            locks;
	List            subPlanInfo;
	Name            errorMessage;
	List            rangeTable;
	HeapTuple       qualTuple;
	ItemPointer     qualTupleID;
	Relation        relationRelationDesc;
	Index        	resultRelationIndex;
	Relation        resultRelationDesc;

	/* ----------------
	 *	resolve the status of our query... are we retrieving
	 *  into a portal, into a relation, etc.
	 * ----------------
	 */
	parseRoot = parse_root(parser_output);

	commandType = root_command_type(parseRoot);
	switch (commandType) {
	case RETRIEVE:
		tag = "RETRIEVE";
		break;
	case APPEND:
		tag = "APPEND";
		break;
	case DELETE:
		tag = "DELETE";
		break;
	case EXECUTE:
		tag = "EXECUTE";
		break;
	case REPLACE:
		tag = "REPLACE";
		break;
	default:
		elog(WARN, "ProcessQuery: unknown command type %d",
			commandType);
	}

	if (root_command_type(parseRoot) == RETRIEVE) {
		resultDesc = root_result_relation(parseRoot);
		if (!null(resultDesc)) {
			int	destination;
			destination = CAtom(CAR(resultDesc));

			switch (destination) {
			case INTO:
				isInto = true;
				break;
			case PORTAL:
				isPortal = true;
				break;
			default:
				elog(WARN, "ProcessQuery: bad result %d",
					destination);
			}
			intoName = CString(CADR(resultDesc));
		}
	}

	/* ----------------
	 *	create the Executor State structure
	 * ----------------
	 */

	direction = 	EXEC_FRWD;
	time = 		0;
	owner = 	0;
	locks = 	LispNil;
	qualTuple =	NULL;
	qualTupleID =	0;

	/* ----------------
	 *   currently these next are initialized in InitPlan.  For now
	 *   we pass dummy variables.. Eventually this should be cleaned up.
	 *   -cim 8/5/89
	 * ----------------
	 */
	rangeTable = 	  	LispNil;
	subPlanInfo = 	  	LispNil;
	errorMessage = 	  	NULL;
	relationRelationDesc = 	NULL;
	resultRelationIndex =	0;
	resultRelationDesc =   	NULL;

	state = MakeEState(direction,
		time,
		owner,
		locks,
		subPlanInfo,
		errorMessage,
		rangeTable,
		qualTuple,
		qualTupleID,
		relationRelationDesc,
		resultRelationIndex,
		resultRelationDesc);

	/* ----------------
	 *	now, prepare the plan for execution by calling ExecMain()
	 *	feature = '(start)
	 * ----------------
	 */

	feature = lispCons(lispInteger(EXEC_START), LispNil);
	queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))),	parser_output,
		plan, state, feature);

	attinfo = ExecMain(queryDesc);

	/* ----------------
	 *   extract result type information from attinfo
	 *	 returned by ExecMain()
	 * ----------------
	 */
	numatts = CInteger(CAR(attinfo));
	tupleDesc = (AttributePtr) CADR(attinfo);

	/* ----------------
	 *   display the result of the first call to ExecMain()
	 * ----------------
	 */

	switch(commandType) {
	  case RETRIEVE:
	    BeginCommand("blank",attinfo);
	    break;
	  default:
	    putnchar("P",1);
	    putint(0,4);
	    putstr("blank");
	}

	/* ----------------
	 *   now how in the hell is this ever supposed to work?
	 *   we have only initialized the plan..  Why do we then
	 *   return here if isPortal?  This *CANNOT* be correct.
	 *   but portals aren't *my* problem (yet..) -cim 8/29/89
	 *
	 * Response:  This is correct; ExecMain has been called to
	 * initialize the query execution.  Named portals do not
	 * do a "fetch all" initially.  The blank portal may
	 * behave in the same way in the future (as an option).
	 *	-hirohama
	 * ----------------
	 */
	if (isPortal) {
		Portal	portal;

		portal = BlankPortalAssignName(intoName);
		PortalSetQuery(portal, parser_output, plan, state);

		/*
		 * Return blank portal for now.
		 * Otherwise, this named portal will be cleaned.
		 * Note: portals will only be supported within a BEGIN...END
		 * block in the near future.  Later, someone will fix it to
		 * do what is possible across transaction boundries.
		 */
		(void)GetPortalByName(NULL);

		return;			/* XXX see previous comment */
	}

	/* ----------------
	 *	if we are retrieveing into a result relation, then
	 *  open it..  This should probably be done in InitPlan
	 *  so I am going to move it there soon. -cim 8/29/89
	 * ----------------
	 */
	if (isInto) {
		char		archiveMode;
		/*
		 * Archive mode must be set at create time.  Unless this
		 * mode information is made specifiable in POSTQUEL, users
		 * will have to COPY, rename, etc. to change archive mode.
		 */
		archiveMode = 'n';

		intoRelationId = RelationNameCreateHeapRelation(intoName,
			archiveMode, numatts, tupleDesc);

		setheapoverride(true);	/* XXX change "amopen" args instead */

		intoRelation = ObjectIdOpenHeapRelation(intoRelationId);

		setheapoverride(false);	/* XXX change "amopen" args instead */
	}

	/* ----------------
	 *   Now we get to the important call to ExecMain() where we
	 *   actually run the plan..
	 * ----------------
	 */

	if (isInto) {
		/* ----------------
		 *  XXX hack - we are passing the relation
		 *  descriptor as an integer.. we should either
		 *  think of a better way to do this or come up
		 *  with a node type suited to handle it..
		 * ----------------
		 */
		feature = lispCons(lispInteger(EXEC_RESULT),
			lispCons(lispInteger(intoRelation), LispNil));

	} else if (IsUnderPostmaster) {
		/*
		 * I suggest that the IsUnderPostmaster test be
		 * performed at a lower level in the code--at
		 * least inside the executor to reduce the number
		 * of externally visible requests. -hirohama
		 */
		feature = lispCons(lispInteger(EXEC_DUMP), LispNil);
	} else {
		feature = lispCons(lispInteger(EXEC_DEBUG), LispNil);
	}

	queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))), parser_output,
		plan, state, feature);

	(void)ExecMain(queryDesc);

	/* ----------------
	 *   final call to ExecMain.. we close down all the scans
	 *   and free allocated resources...
	 * ----------------
	 */

	feature = lispCons(lispInteger(EXEC_END), LispNil);
	queryDesc = MakeQueryDesc(CAR(CDR(CAR(parser_output))), parser_output,
		plan, state, feature);

	(void)ExecMain(queryDesc);

	/* ----------------
	 *   close result relation
	 *   XXX this will be moved to moved to EndPlan soon
	 *		-cim 8/29/89
	 * ----------------
	 */
	if (isInto) {
		RelationCloseHeapRelation(intoRelation);
	}

	/* ----------------
	 *   not certain what this does.. -cim 8/29/89
	 * A: Notify the frontend of end of processing.
	 * Perhaps it would be cleaner for ProcessQuery
	 * and ProcessCommand to return the tag, and for
	 * the "traffic cop" to call EndCommand on it.
	 *	-hirohama
	 * ----------------
	 */
	EndCommand(tag);
}
