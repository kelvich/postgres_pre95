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
#include "parser/parsetree.h"
#include "utils/log.h"

#include "nodes/pg_lisp.h"
#include "tcop/dest.h"

#ifndef NO_SECURITY
#include "tmp/miscadmin.h"
#include "tmp/acl.h"
#include "catalog/syscache.h"
#endif

#include "rules/prs2.h" /* for GetRuleEventTargetFromParse() */

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
ProcessUtility(command, args, commandString, dest)
    int		command;	/* "tag" */
    LispValue	args;
    char 	*commandString;
    CommandDest dest;
{
    String	commandTag = NULL;
    String	relname;
    Name	relationName;
    NameData	user;
    extern void GetUserName();
    
    GetUserName(&user);

    switch (command) {
	/* ********************************
	 *	transactions
	 * ********************************
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
	UserAbortTransactionBlock();
	break;
	
	/* ********************************
	 *	portal manipulation
	 * ********************************
	 */
    case CLOSE:
	commandTag = "CLOSE";
	CHECK_IF_ABORTED();
	
	PerformPortalClose((null(CAR(args))) ? NULL : CString(CAR(args)),
			   dest);
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
	    PerformPortalFetch(portalName, forward, count, commandTag, dest);
	}
	break;
	
    case MOVE:
	commandTag = "MOVE";
	CHECK_IF_ABORTED();
	
	elog(NOTICE, "MOVE: currently unimplemented");
	break;
	
	/* ********************************
	 *	relation and attribute manipulation
	 * ********************************
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
		LispValue arg;
		
		foreach (arg, args) {
			relname = CString(CAR(arg));
			if (NameIsSystemRelationName(relname))
				elog(WARN, "class \"%-*s\" is a system catalog",
				     sizeof(NameData), relname);
#ifndef NO_SECURITY
			if (!pg_ownercheck(user.data, relname, RELNAME))
				elog(WARN, "you do not own class \"%-*s\"",
				     sizeof(NameData), relname);
#endif
		}
		foreach (arg, args) {
			relname = CString(CAR(arg));
			RemoveRelation(relname);
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
	    
	    tags = CAR(CADR(args));
	    switch (length(tags)) {
	    case 1:
		break;
	    case 2:
		if (CInteger(CAR(tags)) == BEFORE) {
		    beforeString = CString(CADR(tags));
		} else {
		    afterString = CString(CADR(tags));
		}
		break;
	    case 3:
		beforeString = CString(CADR(tags));
		afterString = CString(CADR(tags));
		break;
	    }
	    RelationPurge(CString(CAR(args)), beforeString, afterString);
	}
	break;
	
    case COPY:
	commandTag = "COPY";
	CHECK_IF_ABORTED();
	
	{
	    String	fileName;
	    String	mapName = NULL;
	    bool	isBinary;
	    bool	noNulls;
	    bool	isFrom;
            bool        pipe;

	    relname = CString(CAAR(args));
	    isBinary = (bool)!null(CADR(CAR(args)));
	    noNulls = (bool)!null(CADDR(CAR(args)));
	    /*
	     * discard '("relname" [BINARY] [NONULLS])
	     */
	    args = CDR(args);
	    
	    isFrom = (bool)(CAtom(CAAR(args)) == FROM);
	    fileName = CString(CADR(CAR(args)));

#ifndef NO_SECURITY
	    if (isFrom) {
		    if (!pg_aclcheck(relname, user.data, ACL_RD))
			    elog(WARN, "read on \"%-*s\": permission denied",
				 sizeof(NameData), relname);
	    } else {
		    if (!pg_aclcheck(relname, user.data, ACL_WR))
			    elog(WARN, "write on \"-*%s\": permission denied",
				 sizeof(NameData), relname);
	    }
#endif
	    
	    /* Free up file descriptors - going to do a read... */
	    
	    closeOneVfd();

            if (isFrom && !strcmp(fileName, "stdin"))
            {
                pipe = true;
            }
            else if (!isFrom && !strcmp(fileName, "stdout"))
            {
                pipe = true;
            }

            /*
	     * Insist on a full pathname
	     */

            else if (*fileName == '/')
            {
                pipe = false;
            }
	    else
	    {
		    elog(WARN, "COPY: full pathname required for filename");
	    }
	    
	    /*
	     * discard '(FROM/TO "filename")
	     */
	    args = CDR(args);

		if (pipe && IsUnderPostmaster) dest = CopyEnd;

		DoCopy(relname, isBinary, isFrom, pipe, fileName);
	}
	break;
	
    case ADD_ATTR:
	commandTag = "ADD";
	CHECK_IF_ABORTED();

	relname = CString(CAR(args));
	if (NameIsSystemRelationName(relname))
		elog(WARN, "class \"%-*s\" is a system catalog",
		     sizeof(NameData), relname);
#ifndef NO_SECURITY
	if (!pg_ownercheck(user.data, relname, RELNAME))
		elog(WARN, "you do not own class \"%-*s\"",
		     sizeof(NameData), relname);
#endif
	PerformAddAttribute(relname, CDR(args));
	break;
	
	/*
	 * schema
	 */
    case RENAME:
	commandTag = "RENAME";
	CHECK_IF_ABORTED();
	
	{
	    int	len = length(args);
	    
	    /*
	     * skip unused "RELATION" or "ATTRIBUTE" tag
	     */
	    args = CDR(args);

	    relname = CString(CAR(args));
	    if (NameIsSystemRelationName(relname))
		    elog(WARN, "class \"-*%s\" is a system catalog",
			 sizeof(NameData), relname);
#ifndef NO_SECURITY
	    if (!pg_ownercheck(user.data, relname, RELNAME))
		    elog(WARN, "you do not own class \"%-*s\"",
			 sizeof(NameData), relname);
#endif

	    /* ----------------
	     *	XXX using len == 3 to tell the difference
	     *	    between "rename rel to newrel" and
	     *	    "rename att in rel to newatt" will not
	     *	    work soon because "rename type/operator/rule"
	     *	    stuff is being added. - cim 10/24/90
	     * ----------------
	     */
	    if (len == 3) {
		/* ----------------
		 *	rename relation
		 *
		 *	Note: we also rename the "type" tuple
		 *	corresponding to the relation.
		 * ----------------
		 */
		renamerel(CString(CAR(args)), 	 /* old name */
			  CString(CADR(args)));	 /* new name */
		TypeRename(CString(CAR(args)),	 /* old name */
			   CString(CADR(args))); /* new name */
	    } else {
		/* ----------------
		 *	rename attribute
		 * ----------------
		 */
		renameatt(CString(CAR(args)), 	 /* relname */
			  CString(CADR(args)), 	 /* old att name */
			  CString(CADDR(args))); /* new att name */
	    }
	}
	break;
    case CHANGE:
	commandTag = "CHANGE";
	CHECK_IF_ABORTED();

	{
		LispValue i;
		AclItem *aip;
		unsigned modechg;

		args = CDR(args);
		aip = (AclItem *) LISPVALUE_BYTEVECTOR(CAR(args));
		args = CDR(args);
		modechg = CInteger(CAR(args));
		args = CDR(args);
#ifndef NO_SECURITY
		foreach (i, args) {
			relname = CString(CAR(i));
			if (!pg_ownercheck(user.data, relname, RELNAME))
				elog(WARN, "you do not own class \"%-*s\"",
				     sizeof(NameData), relname);
		}
#endif
		foreach (i, args) {
			relname = CString(CAR(i));
			ChangeAcl(relname, aip, modechg);
		}
	}
	break;

	/* ********************************
	 *	object creation / destruction
	 * ********************************
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
			CADDR(CDR(CDR(args))),	/* with */
			CADDR(CDR(CDR(CDR(args)))));	/* where */
	    break;
	case OPERATOR:
	    DefineOperator(CString(CADR(args)),	/* operator name */
			   CDR(CDR(args)));	/* rest */
	    break;
	case AGGREGATE:
	    DefineAggregate(CString(CADR(args)),/*aggregate name */
			    CDR(CDR(args)));   /* rest */
	    break;

        case FUNCTION:
	    DefineFunction(CDR(args), dest);   /* everything */
	    break;

        case RULE:
	    elog(WARN, "Sorry, the old rule system is not supported any more");
	    break;
	case REWRITE:
	    args = CDR(CDR(args));
#ifndef NO_SECURITY
	    relname = CString(CAR(nth(2, args)));
	    if (!pg_aclcheck(relname, user.data, ACL_RU))
		    elog(WARN, "define rule on \"%-*s\": permission denied",
			 sizeof(NameData), relname);
#endif
	    DefineQueryRewrite(args); 
	    break;
	case P_TUPLE:
#ifndef NO_SECURITY
	    relname = CString(CAR(GetRuleEventTargetFromParse(args)));
	    if (!pg_aclcheck(relname, user.data, ACL_RU))
		    elog(WARN, "define rule on \"%-*s\": permission denied",
			 sizeof(NameData), relname);
#endif
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
	
    case REMOVE:
	commandTag = "REMOVE";
	CHECK_IF_ABORTED();
	
	switch(CInteger(CAR(args))) {
	case FUNCTION:
#ifndef NO_SECURITY
	    /* XXX moved to remove.c */
#endif
	    RemoveFunction(CString(CADR(args)));
	    break;
	case AGGREGATE:
#ifndef NO_SECURITY
	    /* XXX moved to remove.c */
#endif
	    RemoveAggregate(CString(CADR(args)));
	    break;
	case INDEX:
	    relname = CString(CADR(args));
	    if (NameIsSystemRelationName(relname))
		    elog(WARN, "class \"%-*s\" is a system catalog index",
			 sizeof(NameData), relname);
#ifndef NO_SECURITY
	    if (!pg_ownercheck(user.data, relname, RELNAME))
		    elog(WARN, "you do not own class \"%-*s\"",
			 sizeof(NameData), relname);
#endif
	    RemoveIndex(relname);
	    break;
	case OPERATOR:
	    {
		String	type2 = NULL;
		
#ifndef NO_SECURITY
		/* XXX moved to remove.c */
#endif
		args = CADR(args);
		if (length(args) == 3) {
		    type2 = CString(CADR(CDR(args)));
		}
		RemoveOperator(CString(CAR(args)), CString(CADR(args)),
			       type2);
	    }
	    break;
	case P_TUPLE:
	    {
		    String rulename = CString(CADR(args));
#ifndef NO_SECURITY
		    extern Name prs2GetRuleEventRel();

		    relationName = prs2GetRuleEventRel(rulename);
		    if (!pg_aclcheck(relationName, user.data, ACL_RU))
			    elog(WARN, "remove rule on \"%-*s\": permission denied",
				 sizeof(NameData), relationName);
#endif
		    prs2RemoveTupleRule(rulename);
	    }
	    break;
	case REWRITE:
	    {
		    String rulename = CString(CADR(args));
#ifndef NO_SECURITY
		    extern Name RewriteGetRuleEventRel();

		    relationName = RewriteGetRuleEventRel(rulename);
		    if (!pg_aclcheck(relationName, user.data, ACL_RU))
			    elog(WARN, "remove rule on \"%-*s\": permission denied",
				 sizeof(NameData), relationName);
#endif
		    RemoveRewriteRule(rulename);
	    }
	    break;
	case P_TYPE:
#ifndef NO_SECURITY
	    /* XXX moved to remove.c */
#endif
	    RemoveType(CString(CADR(args)));
	    break;
	case VIEW:
#ifndef NO_SECURITY
	    {
		    String viewName = CString(CADR(args));
		    NameData ruleName;
		    extern Name RewriteGetRuleEventRel();

		    makeRetrieveViewRuleName(&ruleName, viewName);
		    relationName = RewriteGetRuleEventRel(&ruleName);
		    if (!pg_ownercheck(user.data, relationName, RELNAME))
			    elog(WARN, "remove view \"%-*s\": permission denied",
				 sizeof(NameData), relationName);
	    }
#endif
	    RemoveView(CString(CADR(args)));
	    break;
	}
	break;
	
    case FORWARD:
	commandTag = "VERSION";
	CHECK_IF_ABORTED();
	
	CreateVersion(CString(CAR(args)), CADR(args));
	break;
	
    case BACKWARD:
	commandTag = "VERSION";
	CHECK_IF_ABORTED();
#ifdef NOTYET
	CreateBackwardDelta( CString(CAR(args)),
			    CString(CADR(args)));
	break;
#endif
	
    case CREATEDB:
	commandTag = "CREATEDB";
	CHECK_IF_ABORTED();
	{
	    char *dbname;
	    dbname = CString(CAR(args));
	    createdb(dbname);
	}
	break;
    case DESTROYDB:
	commandTag = "DESTROYDB";
	CHECK_IF_ABORTED();
	{
	    char *dbname;
	    dbname = CString(CAR(args));
	    destroydb(dbname);
	}
	break;

		/* Query-level asynchronous notification */
    case NOTIFY:
	commandTag = "NOTIFY";
	CHECK_IF_ABORTED();
	{
	    char *relname;
	    relname = CString(rt_relname(nth(CInteger(CAR(args))-1,
					 CAR(CDR(args)))));
	    Async_Notify(relname);
	}
        break;

    case LISTEN:
	commandTag = "LISTEN";
	CHECK_IF_ABORTED();
	{
	    extern int MasterPid;
	    char *relname;
	    relname = CString(CAR(args));
	    Async_Listen(relname,MasterPid);
	}
	break;

	/* ********************************
	 *	dynamic loader
	 * ********************************
	 */
    case LOAD:
	commandTag = "LOAD";
	CHECK_IF_ABORTED();
	{
	    FILE *fp, *fopen();
	    
	    char *filename;
	    filename = CString(CAR(args));
	    if (*filename != '/') {
		elog(WARN, "Use full pathname for LOAD command.");
	    }
		
		closeAllVfds();
		if ((fp = fopen(filename, "r")) == NULL) {
		elog(WARN, "LOAD: could not open file %s", filename);
	    }
	    
	    fclose(fp);
	    load_file(filename);
	}
	break;

    case VACUUM:
	commandTag = "VACUUM";
	CHECK_IF_ABORTED();
	vacuum();
	break;

	/* ********************************
	 *	default
	 * ********************************
	 */
    default:
	elog(WARN, "ProcessUtility: command #%d unsupported", command);
	break;
    }
    
    /* ----------------
     *	tell fe/be or whatever that we're done.
     * ----------------
     */
    EndCommand(commandTag, dest);
}

