/* 
 * $Header$
 */

#include <stdio.h>
#include "tmp/postgres.h"

/* #include "rules/prs2locks.h"		/* temporarily */
#include "utils/rel.h"			/* for Relation stuff */
#include "access/heapam.h"		/* access methods like amopenr */
#include "utils/log.h"			/* for elog */
#include "nodes/pg_lisp.h"		/* for Lisp support */
#include "parser/parse.h"		/* lisp atom database */
#include "parser/parsetree.h"		/* for parsetree manip defines */
#include "./locks.h"			
ObjectId LastOidProcessed = InvalidObjectId;
bool prs2AttributeIsOfBasicType();

/*
 * This is too small for many rule plans, but it'll have to do for now.
 * Rule plans, etc will eventually have to be large objects.
 * 
 * should this be smaller?
 */

#define RULE_PLAN_SIZE 8192 

#define ShowParseTL(ptree)	lispDisplay(parse_targetlist(ptree))
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree))
#define ShowParseCmd(ptree)	lispDisplay(root_command_type \
					    parse_root(ptree))
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree))

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

static char *foo = "reality is for dead birds" ;

void strcpyq(dest, source)
	char *dest, *source;
{
	char *current=source,*destp= dest;	

	for(current=source; *current; current++) {
		if (*current == '\"')  {
			*destp = '\\';
			destp++;
		}
		*destp = *current;
		destp++;
	}
	*destp = '\0';
}

OID
InsertRule ( rulname , evtype , evobj , evslot , evqual, evinstead ,
	     actiontree )
     Name 	rulname;
     int 	evtype;
     Name 	evobj;
     Name 	evslot;
     char	*evqual;
     bool	evinstead;
     char	*actiontree;

{
    static char	rulebuf[RULE_PLAN_SIZE];
    static char actionbuf[RULE_PLAN_SIZE];
    static char qualbuf[RULE_PLAN_SIZE];
    ObjectId eventrel_oid = InvalidObjectId;
    AttributeNumber evslot_index = InvalidAttributeNumber;
    Relation eventrel = NULL;
    char *template = 
"append pg_rewrite (rulename=\"%s\",ev_type=\"%d2\"::char,ev_class=%d::oid,\
ev_attr= %d::int2,action= \"%s\"::text,ev_qual= \"%s\"::text,\
is_instead=\"%s\"::bool)";
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
    amclose(eventrel);
    if (IsDefinedRewriteRule(rulname)) 
	elog(WARN, "Attempt to insert rule '%s' failed: already exists",
	     rulname);
	strcpyq(actionbuf,actiontree);	
	strcpyq(qualbuf, evqual);
    sprintf(rulebuf, template, rulname, AtomValueGetString(evtype),
	    eventrel_oid, evslot_index, actionbuf, qualbuf, is_instead );

    /* fprintf(stdout,"rule is \n%s\n", rulebuf ); */

    pg_eval(rulebuf, (char *) NULL, (ObjectId *) NULL, 0);

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

void ValidateRule(event_type, eobj_string, eslot_string, event_qual,
		  action, is_instead,event_attype)
     char *eobj_string, *eslot_string;
     int is_instead,event_type;
     List *action,event_qual;
    ObjectId event_attype;
{
    int count;
    char *template = "(((0 retrieve nil nil 0 nil nil nil )((#S(resdom \
:resno 1 :restype %d :reslen %d :resname \"%s\" :reskey 0 :reskeyop 0 \
:resjunk 0)#S(const :consttype %d :constlen 0 :constisnull true \
:constvalue NIL :constbyval nil))) nil))";
    count = length(*action);
    if (((event_type == APPEND) || (event_type == DELETE)) && eslot_string)
	elog(WARN, "'to class.attribute' rules not allowed for this event");
    if (event_qual && !*action && is_instead)
	elog(WARN,
	     "event_quals on 'instead nothing' rules not currently supported");
/*    if (event_type == RETRIEVE && is_instead && count > 1)
	elog(WARN,
	     "multiple rule actions not supported on 'retrieve instead' rules");*/
    /* on retrieve to class.attribute do instead nothing is converted
     * to 'on retrieve to class.attribute do instead
     *        retrieve (attribute = NULL)'
     * --- this is also a terrible hack that works well -- glass*/
    if (is_instead && !*action && eslot_string && event_type == RETRIEVE) {
	char *temp_buffer = (char *) palloc(strlen(template)+80);
	sprintf(temp_buffer, template, event_attype,
		get_typlen(event_attype), eslot_string,
		event_attype);
	*action = (List) StringToPlan(temp_buffer);
	pfree(temp_buffer);
    }
}
     

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
    ObjectId ruleId;
    ObjectId ev_relid		= 0;
    char locktype;
    char *eobj_string		= NULL;
    char *eslot_string		= NULL;
    int event_attno 		= 0;
    int j			= 0;		/* save index */
    int k			= 0;		/* actual lock placement */
    ObjectId event_attype	= 0;
	char *actionP, *event_qualP;
    
    extern ObjectId att_typeid();

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
    ev_relid = RelationGetRelationId (event_relation);
    
    if ( eslot_string == NULL ) {
      event_attno = -1;
      event_attype = -1; /* XXX - don't care */
    } else {
	event_attno = varattno ( event_relation, eslot_string );
	event_attype = att_typeid(event_relation,event_attno);
    }
    amclose(event_relation);
    /* fix bug about instead nothing */
    ValidateRule(CAtom(event_type), eobj_string,
		 eslot_string, event_qual, &action,
		 is_instead,event_attype);
    if (action == LispNil) {
	if (!is_instead) return;	/* doesn't do anything */

	actionP = PlanToString(event_qual);

	if (strlen(actionP) > RULE_PLAN_SIZE)
		elog(WARN, "DefineQueryRewrite: rule action too long.");

	ruleId = InsertRule ( rulename, 
				CAtom(event_type),
				(Name)eobj_string,
				(Name)eslot_string,
				actionP,
				1,
				"nil ");
	locktype = PutRelationLocks ( ruleId,
					ev_relid,
					event_attno,
					CAtom(event_type),
					1);
	prs2AddRelationLevelLock(ruleId,locktype,
				 ev_relid,event_attno);
    } else {

	/*
	 * I don't use the some of the more interesting LockTypes...so
	 * PutRelationLocks() has suddenly got much dumber -- glass
	 */

	event_qualP = PlanToString(event_qual);
	actionP = PlanToString(action);

	if (strlen(event_qualP) > RULE_PLAN_SIZE)
		elog(WARN, "DefineQueryRewrite: event qual plan string too big.");

	if (strlen(actionP) > RULE_PLAN_SIZE)
		elog(WARN, "DefineQueryRewrite: action plan string too big.");

	ruleId = InsertRule ( rulename, 
				CAtom(event_type),
				(Name)eobj_string,
				(Name)eslot_string,
				event_qualP,
				is_instead,
				actionP);

	locktype = PutRelationLocks ( ruleId,
					ev_relid,
					event_attno,
					CAtom(event_type),
					is_instead);
	/* what is the max size of type text? XXX -- glass */
	j = length(action);
	if ( j > 15 )
	    elog(WARN,"max # of actions exceeded"); 
	prs2AddRelationLevelLock(ruleId,locktype,
				 ev_relid,event_attno);
    }
}

ShowRuleAction(ruleaction)
     LispValue ruleaction;
{
    if ( ! lispNullp (ruleaction) ) {
	if ( atom(CAR(ruleaction)) ) {
	    elog(WARN, "Utility Actions are not supported yet");
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





