/* 
 * $Header$
 */

#include <stdio.h>
#include "tmp/oid.h"

/* #include "rules/prs2locks.h"		/* temporarily */
#include "./locks.h"			
#include "utils/rel.h"			/* for Relation stuff */
#include "access/heapam.h"		/* access methods like amopenr */
#include "utils/log.h"			/* for elog */
#include "nodes/pg_lisp.h"		/* for Lisp support */
#include "parser/parse.h"		/* lisp atom database */
#include "parser/parsetree.h"		/* for parsetree manip defines */

ObjectId LastOidProcessed = InvalidObjectId;


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
    ObjectId eventrel_oid = InvalidObjectId;
    AttributeNumber evslot_index = InvalidAttributeNumber;
    Relation eventrel = NULL;
    char *is_instead = "f";
    extern void eval_as_new_xact();

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
    if ( evinstead )
	is_instead = "t";
    if ( evqual == NULL )
      evqual = "nil";

    sprintf(rulebuf,
	    "append pg_rewrite (rulename=\"%s\",ev_type=\"%d2\"::char,\
	    ev_class=%d::oid,ev_attr= %d::int2,\
	    action= `%s`::text , ev_qual= `%s`::text, \
            fril_lb= %f , fril_ub= %f , is_instead=\"%s\"::bool )",
	    rulname,
	    AtomValueGetString(evtype),
	    eventrel_oid,
	    evslot_index,
	    actiontree,
	    evqual,
	    necessary,
	    sufficient,
	    is_instead );

    /* fprintf(stdout,"rule is \n%s\n", rulebuf ); */

    pg_eval(rulebuf);

    /* elog(NOTICE,"RuleOID is : %d\n", LastOidProcessed ); */

    return ( LastOidProcessed );
}

void
ModifyActionToReplaceCurrent ( retrieve_parsetree )
	List retrieve_parsetree;
{
	List root = parse_root ( retrieve_parsetree );

	CADR(root) = lispAtom("replace");
	CADDR(root) = lispInteger(1); /* CURRENT */
}	
/*
 *	for now, event_object must be a single attribute
 */

DefineQueryRewrite ( args ) 
     List args;
{
    List i			= NULL;
    Name rulename 		= (Name)CString ( nth ( 0,args )) ;
    LispValue event_type	= nth ( 1 , args );
    List event_obj		= nth ( 2 , args );
    List event_qual	        = nth ( 3 , args );
    bool is_instead	        = (bool)CInteger ( nth ( 4 , args ));
    List action		= nth ( 5 , args );
    Relation event_relation 	= NULL ;
    ObjectId ruleId[16];
    ObjectId ev_relid		= 0;
    char locktype[16];
    char *eobj_string		= NULL;
    char *eslot_string		= NULL;
    int event_attno 		= 0;
    int j			= 0;		/* save index */
    int k			= 0;		/* actual lock placement */
    ObjectId event_attype	= 0;
    
    extern ObjectId att_typeid();

#ifdef FUZZY
    List support 		= nth (6 , args );
    double necessary 		= CDouble(CAR(support));
    double sufficient 		= CDouble(CDR(support));
#else
    double necessary 		= 1.0;
    double sufficient 		= 0.0;
#endif
    
    extern	char		*PlanToString();
    extern	char		PutRelationLocks();

    eobj_string = CString ( CAR ( event_obj));
    
    if ( CDR (event_obj) != LispNil )
      eslot_string = CString ( CADR ( event_obj));
    else
      eslot_string = NULL;
    
    event_relation = amopenr ( eobj_string );
    if ( event_relation == NULL ) {
	elog(WARN, "virtual relations not supported yet");
    }
    ev_relid = RelationGetRelationId (event_relation);
    
    if ( eslot_string == NULL ) {
      event_attno = -1;
      event_attype = -1; /* XXX - don't care */
    } else {
	event_attno = varattno ( event_relation, eslot_string );
	event_attype = att_typeid(event_relation,event_attno);
    }
    foreach ( i , action ) {
	List 	this_action 		= CAR(i);
	int 	this_action_is_instead 	= 0;
	int 	action_type 		= 0;	 
	List	action_result		= NULL;
	int	action_result_index	= 0;

	if (CDR(i) == LispNil && is_instead ) {
	    this_action_is_instead = 1;
	 }
	
	action_type = root_command_type(parse_root(this_action));
	action_result = root_result_relation(parse_root(this_action));
	if ( action_type == REPLACE ) {
	    action_result_index = CInteger(action_result);
	}
	if ( action_type == RETRIEVE && event_attype != 32 ) {
	    /* transform to replace current */
	    ModifyActionToReplaceCurrent ( this_action );
	    action_type = REPLACE;
	    action_result_index = 1;
	}
	ruleId[j] = InsertRule ( rulename, 
				  CAtom(event_type),
				  (Name)eobj_string,
				  (Name)eslot_string,
				  PlanToString(event_qual),
				  this_action_is_instead,
				  PlanToString(this_action),
				  necessary,
				  sufficient );
	
	locktype[j] = PutRelationLocks ( ruleId[j], 
					ev_relid,
					event_attno,
					CAtom(event_type),
					action_type,
					action_result_index,
					this_action_is_instead );
	j += 1;
	if ( j > 15 )
	  elog(WARN,"max # of actions exceeded");
	
    } /* foreach action */

    for ( k = 0 ; k < j ; k++ ) {
	if ( k != 0 ) {
	    CommitTransactionCommand();
	    StartTransactionCommand();
	}
	prs2PutRelationLevelLocks(ruleId[k],locktype[k],
			       ev_relid,event_attno);
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


