
/*     
 *      FILE
 *     	utility.l
 *     
 *      DESCRIPTION
 *     	Contains functions which control the execution of the
 *     	POSTGRES utility commands.  Acts as an interface between
 *     	the Lisp and C systems.
 *     
 */
/*
require ("nodeDefs");
require ("parsetree");
require ("pgmacs");
*/
#include "pg_lisp.h"

/* temporary stuff */

#define declare(x) /* x */
#define foreach(x,y) for(x=y;x!=LispNil;x=CDR(x))
extern int PG_INTERACTIVE;

#include "parse.h"		/* y.tab.h, created by yacc'ing gram.y */
#include "pg_lisp.h"		/* lisp-compat package */
#include "c.h"			/* generic defns, needed for bools here */
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
	declare (special (PG_INTERACTIVE));
	/* XXX unless interactive?  why not always? */
#if 0
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
	  /*    transactions */
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
	  /*    portal manipulation */
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

	  portal_fetch (CString(CAR (args)),     	/*  portal name */
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
			CDR(CDR(args)));
		break;
	  
	  /*  schema */
#if 0	  
	case DESTROY:
	  {
	       LispValue relation_name;
	       foreach (relation_name, args) {
		    relation_remove (relation_name);
	       }
	       break;
	  }
	case PURGE:
	  relation_purge (CAR(args),		/*  relation name */
			  CADR(args));
	  break;

	  /*  tags/date alist */
	  
	case COPY:
	  /*
	  relation_transform (CAR(CARargs),
			      CADR(CAR(args)),
			      nth (2,nth (0,args))
			      ,nth (0,nth (1,args))
			      ,nth (1,nth (1,args))
			      ,nth (1,nth (2,args))
			      ,nthcdr (3,args));
			      */
	  break;

	  /*  domain list */

	case ADD_ATTR:
	  relation_add_attribute (CAR (args),	       /*  relation name */
				  CDR (args));
	  break;

	  /*  schema */
	  
	case RENAME:
	  rename_utility_invoke (CAR (args),cdr (args));
	  break;
#endif
#if 0
	  /*    object creation */

	case DEFINE:
		switch(CInteger(CAR(args))) {
		case INDEX:
			index_define(CString(CAR(args)), /* relation name */
				CADR(args),		/* index name */
				CADDR (args),		/* am name */
				cadddr (args),		/* parameters */
				caddddr (args));
			break;
		case OPERATOR:
			operator_define (CAR (args)
				/*  operator name */
				,cdr (args));
			/*  remaining arguments */
			break;
		case FUNCTION:
			function_define (CAR (args)
				/*  function name */
				,cdr (args));
			/*  remaining arguments */
			break;
		case RULE:
			rule_define (CAR(args),		    /*  rule name */
				CADR(args));
			break;
			/*  rule command parses */
		case P_TYPE:
			type_define (CAR (args),CDR (args));
			break;
		}
	  break;
#endif
#if 0
	  /*    object destruction */

	case REMOVE:
	  switch(CInteger(CAR(args))) {
	     case FUNCTION:
	       function_remove(CDR(args));
	       break;
	     case INDEX:
	       index_remove(CDR(args));
	     case OPERATOR:
	       operator_remove(CDR(args));
	     case RULE:
	       rule_remove(CDR(args));
	     case P_TYPE:
	       type_remove(CDR(args));
	     default:
	       elog(WARN,"parser generates unknown remove request");
	  }
	  break;
#endif
#if 0
	  /*    portal retrieve */
	  /*    XXX Can this be generalized to more than a single retrieve? */
	  /*        Should it? */
	case PORTAL:
	  /*    check validity */
	  portal_retrieve (CAR (args),	       		/*  portal name */
			   CADR (args),		/*  command */
			   CADDR (args));
	  break;
     
	  /*  default */
#endif
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
