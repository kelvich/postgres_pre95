
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

extern int PG_INTERACTIVE;

#include "parse.h"		/* y.tab.h, created by yacc'ing gram.y */
#include "log.h"		/* error logging */

/*   
 *    Note:
 *   	XXX most of the (pgerror *WARN* ...) calls below indicate an
 *   	non-fatal internal error.  The rest of the system should work fine.
 *   
 */

/*    
 *     general utility execution completion code
 *    
 */
LispValue
utility_end (name)
LispValue name ;
{
#if 0
	declare (special (PG_INTERACTIVE));
	/* XXX unless interactive?  why not always? */
	if (!PG_INTERACTIVE) {
		endportal(name);
	}
#endif
}

/*    
 *     general utility function invoker
 *    
 */
LispValue
utility_invoke (command, args)
	int		command;	/* "tag" */
	LispValue	args;
{
	switch (command) {
#if 0
		/* transactions */
	case BEGIN_TRANS:
		if ( null (args) ) {
			start_transaction_block ();
		} else {
			cons (command,args);
		}
		break;
	case END_TRANS:
		if ( null (args) ) {
			commit_transaction_block ();
		} else {
			cons (command,args);
		};
		break;
	case ABORT_TRANS:
		if ( null (args) ) {
			abort_transaction_block ();
		} else {
			cons (command,args);
		}
		break;
#endif
#if 0
		/* portal manipulation */
	case CLOSE:
	{
		LispValue portal_name;
		foreach (portal_name, args) {
			if (! lispStringp(portal_name))
				elog(WARN,"portal list not strings");
			else
				portal_close (CString(portal_name));
		}
	}
		break;
	case FETCH:

		portal_fetch(CString(CAR(args)),     	/* portal name */
			((CInteger(CADR(args)) ==  FORWARD) ?
				true : false ),		/* forward ? */
			CInteger(CADDR (args)));	/* ntups to scan 
							   -1 means "ALL" */
		break;

	case MOVE:
		portal_move (CAR (args),
			((CInteger(CADR(args)) ==  FORWARD) ?
				true : false ),		/* forward ? */
			CInteger(CADDR (args)));	/* ntups to scan 
							   -1 means "ALL" */
		break;
#endif

		/*
		 * relation and attribute manipulation
		 */
	case CREATE:
		DefineRelation(CAR(args),	/*  relation name */
			CADR(args),		/*  parameters */
			CDR(CDR(args)));	/*  schema */
		break;

	case DESTROY:
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

#if 0	  
	case PURGE:
		relation_purge (CAR(args),		/* relation name */
			CADR(args));
		break;

		/* tags/date alist */
	case COPY:
		relation_transform (CAR(CARargs),
			CADR(CAR(args)),
			nth (2,nth (0,args))
			,nth (0,nth (1,args))
			,nth (1,nth (1,args))
			,nth (1,nth (2,args))
			,nthcdr (3,args));
		break;

		/* domain list */

	case ADD_ATTR:
		relation_add_attribute (CAR (args),	/* relation name */
			CDR (args));
		break;

		/* schema */
	case RENAME:
		rename_utility_invoke (CAR (args),cdr (args));
		break;
#endif
		/* object creation */
	case DEFINE:
#ifndef	PERFECTPARSER
		AssertArg(listp(args));
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
			elog(DEBUG,
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
			elog(DEBUG,
				"InvokeUtility: REMOVE RULE now unsupported");
#if 0
			rule_remove(CDR(args));
#endif
		case P_TYPE:
#ifndef	PERFECTPARSER
			AssertArg(lispStringp(CADR(args)));
#endif
			RemoveType(CString(CADR(args)));
			break;
		default:
			elog(WARN, "unknown REMOVE parse type %d",
				CInteger(CAR(args)));
		}
		break;
#if 0
		/* portal retrieve */
		/* XXX Can this be generalized to more than a single retrieve? */
		/* Should it? */
	case PORTAL:
		/*    check validity */
		portal_retrieve (CAR (args),		/* portal name */
			CADR (args),			/* command */
			CADDR (args));
		break;

#endif
		/* default */
	default:
		cons (command,args);
		break;
	}
}


/*    
 *     rename utility function invoker
 *    
 */
#if 0
int
rename_utility_invoke (object_type_name,args)
     LispValue object_type_name,args ;
{
     switch (object_type_name) {
	  
	case 0/*RELATION*/:
	  relation_rename (CAR (args)
			   /*  old relation name */
			   ,CADR (args));
	  break;
     
	  /*  new relation name */
	  
	case 1/*ATTRIBUTE*/:
	  attribute_rename (CAR (args)
				 /*  relation name */
				 ,CADR (args)
				 /*  old attribute name */
				 ,CADDR (args));
	  break;
	  
	  /*  new attribute name */
	default:
	  elog(WARN,"unknown RENAME request");
	  break;
     }
}
#endif	
/*
 *      POSTGRES utility commands follow
 *     
 */

/*    
 *     relation and attribute manipulation functions
 *    
 */
#if 0
relation_purge (relation_name,date_tags)
     LispValue relation_name,date_tags ;
{
     /*RelationPurge (relation_name,cdr (assoc (BEFORE,date_tags)) || 0,
		    CDR (assoc (AFTER,date_tags)) || 0)*/
     utility_end ("PURGE");
}
#endif
#if 0
LispValue
relation_transform (relation_name,format,nonulls,
		    direction,file_name,map_relation,domain_list)
     LispValue relation_name,format,nonulls,direction,
     file_name,map_relation,domain_list ;
{
     char *error_string = "relation-transform: ERROR: ";
     int natts = length (domain_list);
     bool isbinary = (null(format) ? false : true );
     bool isnonulls = (equal (nonulls,NONULLS) ? true : false );
     bool isfrom = (equal(direction,TO) ? false : true );
     bool map_relation_name = (map_relation ? true : false);
/*
     LispValue fixed_domain_list = 
       relation_transform_fix_domains (domain_list);
     LispValue domain_number = vectori_long (0);
*/

}
#endif
