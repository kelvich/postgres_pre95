/* $Header$ */

#include "tmp/postgres.h"

#include "access/ftup.h";
#include "access/heapam.h"		/* access methods like amopenr */
#include "access/htup.h"
#include "access/itup.h"		/* for T_LOCK */
#include "catalog/catname.h"
#include "catalog/syscache.h"		/* for SearchSysCache ... */
#include "catalog_utils.h"
#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"
#include "parser/atoms.h"
#include "parser/parse.h"
#include "parser/parsetree.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"		/* for Relation stuff */

ObjectId LastOidProcessed = InvalidObjectId;

typedef EventType Event;

#define ShowParseTL(ptree)	lispDisplay(parse_targetlist(ptree),0)
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree),0)
#define ShowParseCmd(ptree)	lispDisplay(root_command_type \
					    parse_root(ptree),0))
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree),0)

/*
 *	InsertRule
 *	DESC :  takes the arguments and inserts them
 *		as attributes into the system relation "pg_prs2rule"
 *
 *	MODS :	changes the value of LastOidProcessed as a side
 *		effect of inserting the rule tuple
 *
 *	ARGS :	rulname		-	name of the rule
 *		evtype		-	one of RETRIEVE,REPLACE,DELETE,APPEND
 *		evobj		-	name of relation
 *		evslot		-	comma delimited list of slots
 *					if null => multi-attr rule
 *		evinstead	-	is an instead rule
 *		actiontree	-	parsetree(s) of rule action
 */


OID
InsertRule ( rulname , evtype , evobj , evslot , evqual, evinstead ,
	     actiontree , necessary, sufficient )
     Name 	rulname;
     int 	evtype;
     Name 	evobj;
     Name 	evslot;
     char	*evqual;
     bool	evinstead;
     char	*actiontree;
     double	necessary, sufficient;
{
    static char	rulebuf[4096];
    ObjectId rule_oid = InvalidObjectId;
    ObjectId eventrel_oid = InvalidObjectId;
    AttributeNumber evslot_index = InvalidAttributeNumber;
    Relation eventrel = NULL;

    eventrel = amopenr( evobj );
    if ( eventrel == NULL ) {
	elog ( WARN, "rules cannot be defined on relations not in schema");
    }
    eventrel_oid = RelationGetRelationId(eventrel);

    /*
     * if the slotname is null, we know that this is a multi-attr
     * rule
     */
    if ( evslot == NULL )
      evslot_index = -1;
    else
      evslot_index = varattno ( eventrel,evslot );

    sprintf(rulebuf,
"	    append pg_prs2rule (prs2name=\"%s\",prs2eventtype=\"%d2\"::char,\
	    prs2eventrel=%d::oid,prs2eventattr= %d::int2,\
	    prs2text= `%s`::text , \
            necessary = %f , sufficient = %f )",
	    rulname,
	    AtomValueGetString(evtype),
	    eventrel_oid,
	    evslot_index,
	    actiontree,
	    necessary,
	    sufficient);

    elog(NOTICE,"rule is \n%s\n", rulebuf );

    pg_eval(rulebuf);

    elog(NOTICE,"RuleOID is : %d\n", LastOidProcessed );

    return ( LastOidProcessed );
}

/*
 *	for now, event_object must be a single attribute
 */

DefineQueryRewrite ( args ) 
     List args;
{
     char *rulename 		= CString ( nth ( 0,args )) ;
     LispValue event_type	= nth ( 1 , args );
     List event_obj		= nth ( 2 , args );
     List event_qual	        = nth ( 3 , args );
     bool is_instead	        = (bool)CInteger ( nth ( 4 , args ));
     List action		= nth ( 5 , args );
     Relation event_relation 	= NULL ;
     ObjectId ruleId 		= 0;
     char *eobj_string		= NULL;
     char *eslot_string		= NULL;
     EventType 			this_event;
     List support 		= nth (6 , args );
     int event_attno 		= 0;

     double necessary 		= CDouble(CAR(support));
     double sufficient 		= CDouble(CDR(support));

     extern	char		*PlanToString();

     eobj_string = CString ( CAR ( event_obj));
     
     if ( CDR (event_obj) != LispNil )
	 eslot_string = CString ( CADR ( event_obj));
     else
       eslot_string = NULL;

     event_relation = amopenr ( eobj_string );
     if ( event_relation == NULL ) {
	 elog(WARN, "virtual relations not supported yet");
     }

     if ( NodeHasTag ( event_type, classTag(LispSymbol) ) ) {

	 int action_type = 0;

	 ActionType  prs2actiontype;

	 switch (  CAtom ( event_type ) ) {
	   case APPEND:
	     this_event = EventTypeAppend;
	     break;
	   case DELETE:
	     this_event = EventTypeDelete;
	     break;
	   case RETRIEVE:
	     this_event = EventTypeRetrieve;
	     break;
	   case REPLACE:
	     this_event = EventTypeReplace;
	     break;
	   default:
	    elog(WARN,"unknown event type given to rule definition");

	 } /* end switch */

	 action_type = root_command_type(parse_root(CAR(action)));

	 switch (  action_type  ) {
	   case REPLACE:
	     prs2actiontype = ActionTypeReplaceCurrent;
	     break;
	   case RETRIEVE:
	     prs2actiontype = ActionTypeRetrieveValue;
	     break;
	   default:
	     prs2actiontype = ActionTypeOther;
	     break;
	 }
	 ruleId = InsertRule ( rulename, 
			      CAtom(event_type),
			      eobj_string,
			      eslot_string,
			      PlanToString(event_qual),
			      (int)is_instead,
			      PlanToString(action),
				  necessary,
			      sufficient );

	 if ( eslot_string == NULL ) 
	   event_attno = -1;
	 else
	   event_attno = varattno ( event_relation, eslot_string );

	 prs2PutLocks ( ruleId, RelationGetRelationId (event_relation),
		       event_attno,
		       event_attno,
		       this_event,
		       prs2actiontype  );
    }
}

ShowRuleAction(ruleaction)
     LispValue ruleaction;
{
    if ( ! lispNullp (ruleaction) ) {
	if ( atom(CAR(ruleaction)) ) {
	    printf("Utility Actions are not supported yet");
	} else {
	    switch ( root_command_type (parse_root ( ruleaction ) )) {
	      case RETRIEVE:
		printf("\nAction Type: retrieve\n");
		printf("Available attributes :\n");
		break;
	      case REPLACE: 
		printf("\nAction Type: retrieve\n");
		printf("\nAction Targetlist :\n");
	      case DELETE: 
		printf("\nAction Type: delete\n");
		printf("\nAction Targetlist :\n");
	      case APPEND: 
		printf("\nAction Type: append\n");
		printf("\nAction Targetlist :\n");
	      case EXECUTE:
		printf("\nAction Type: execute\n");
		printf("\nAction Targetlist :\n");
	      default:
		printf("unknown action type\n");
	    }
	    ShowParseTL(ruleaction);
	    printf("\nAdditional Qualifications :\n");
	    ShowParseQual(ruleaction);
	}
    } else {
	printf (" NULL command ");
    }
}


