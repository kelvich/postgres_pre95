/* $Header$ */
#include "c.h"
#include "parse.h"
#include "atoms.h"
#include "pg_lisp.h"
#include "log.h"
#include "parsetree.h"
#include "catname.h"
#include "relation.h"
#include "heapam.h"		/* access methods like amopenr */
#include "htup.h"
#include "ftup.h";
#include "fmgr.h"
#include "datum.h"

#define ShowParseTL(ptree)	lispDisplay(parse_targetlist(ptree),0)
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree),0)
#define ShowParseCmd(ptree)	lispDisplay(root_command_type \
					    parse_root(ptree),0))
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree),0)

typedef struct _Rule {
    char	*TriggerRel;
    List	TriggerQual; 
    List	NewTree;
    bool	Instead;
    bool	Valid;
} *Rule;


List retrieve_rules = (List)NULL;
List replace_rules = (List)NULL;

OID  InsertRule ();
char *RuleGetTriggerRel (ruleptr) Rule ruleptr; { return(ruleptr->TriggerRel);}
List RuleGetTriggerQual (ruleptr) Rule ruleptr;{ return(ruleptr->TriggerQual);}
List RuleGetNewTree (ruleptr) Rule ruleptr; { return(ruleptr->NewTree); }
bool RuleGetInstead (ruleptr) Rule ruleptr; { return(ruleptr->Instead); }
bool RuleGetValid (ruleptr) Rule ruleptr; { return(ruleptr->Valid); }

HandleReplaceRuleDef()
{
}
HandleRetrieveRuleDef()
{
}
QueryRewrite()
{
}

#ifdef nowrk

void RuleSetTriggerRel (ruleptr,trigger) 
     Rule ruleptr;
     char *trigger;
{
    ruleptr->TriggerRel = trigger;
}

void RuleSetTriggerQual (ruleptr,qual) 
     Rule ruleptr;
     List qual;
{
    ruleptr->TriggerQual = qual;
}
void RuleSetNewTree (ruleptr,newtree) 
     Rule ruleptr;
     List newtree;
{
    ruleptr->NewTree = newtree;
}
void RuleSetInstead (ruleptr,instead) 
     Rule ruleptr;
     bool instead;
{
    ruleptr->Instead = instead;
}
void RuleSetValid (ruleptr,valid) 
     Rule ruleptr;
     bool valid;
{
    ruleptr->Valid = valid;
}
Rule 
TriggerFindRule (trigger) 
     char *trigger;
{
}


Rule
MakeRule (trigger,qual,is_instead,newtree,is_valid)
     char	*trigger;
     List 	qual;
     bool	is_instead;
     LispValue	newtree;
     bool	is_valid;
{
    Rule newrule = ALLOCATE(Rule);
    
    RuleSetTriggerRel(newrule,trigger);
    RuleSetTriggerQual(newrule,trigger);
    RuleSetNewTree(newrule,newtree);
    RuleSetInstead(newrule,is_instead);
    RuleSetValid(newrule,true);

    return(newrule);
}     

/**********************************************************************
  
  NB: providing the syntax is correct, for now we will assume
      that retrieve rules are always definable.  Later, we need
      to check for rule consistency in the ruleset

 **********************************************************************/

LispValue
HandleRetrieveRuleDef(trigger,qual,is_instead,newtree)
     char	*trigger;
     List 	qual;
     bool	is_instead;
     LispValue	newtree;
{
    retrieve_rules = nappend1(retrieve_rules,
			      MakeRule(trigger,qual,is_instead,
					   newtree,true));
} /* HandleRetrieveRuleDef */

/**********************************************************************

  ALGORITHM :

  1)	first, find corresponding retrieve rule, if it doesn't
	exist, then punt, placing marker to say rule_is_invalid,
	leaving it to invocation time to call this routine again.

 *********************************************************************/

LispValue
HandleReplaceRuleDef(trigger,qual,is_instead,newtree)
     char	*trigger;
     List 	qual;
     bool	is_instead;
     LispValue	newtree;
{
    Rule  	retrieve_rule;
    Rule 	replace_rule;

    retrieve_rule = TriggerFindRule(trigger,retrieve_rules);
    replace_rule = MakeRule(trigger,qual,is_instead,newtree,true);

    if (!retrieve_rule) {
	RuleSetValid(replace_rule,false);
	return;
    }
}

ShowReplaceRule(rule)
     Rule rule;
{
    char *trigger = RuleGetTriggerRel(rule);
    List qual = RuleGetTriggerQual(rule);
    List newtree = RuleGetNewTree(rule);
    bool is_instead = RuleGetInstead(rule);
    bool is_valid = RuleGetValid(rule);

    if(!is_valid)
      printf("*** WARN, rule is currently invalid\n");
    
    printf("on replace to %s\n",trigger);

    if(is_instead) 
      printf("do instead\n");
    else
      printf("do\n");

    lispDisplay(newtree);

}

ShowRetrieveRule(rule)
     Rule rule;
{
    char *trigger = RuleGetTriggerRel(rule);
    List qual = RuleGetTriggerQual(rule);
    List newtree = RuleGetNewTree(rule);
    bool is_instead = RuleGetInstead(rule);
    bool is_valid = RuleGetValid(rule);

    if(!is_valid)
      printf("*** WARN, rule is currently invalid\n");
    
    printf("on retrieve to %s\n",trigger);

    if(is_instead) 
      printf("do instead\n");
    else
      printf("do\n");

    /* lispDisplay(newtree); */
    ShowRuleAction(newtree);
}

List
HandleRuleDef(event_type,event_relname,event_qual,instead,action)
     int 	event_type;
     char 	*event_relname;
     List 	event_qual;
     LispValue 	instead;
     LispValue  action;
{
    LispValue retval = LispNil;
    bool is_instead = true;
    
    if ( null (instead) ) {
	bool is_instead = false;
    }

    switch(event_type) {
      case RETRIEVE:
	/* retval = HandleRetrieveRuleDef(event_relname,event_qual,
				       is_instead,action); */
	break;
      case REPLACE:
	retval = HandleReplaceRuleDef(event_relname,event_qual,
				      is_instead,action);
	break;
      default:
	printf("*** WARN unknown event-type in rule-definition");
	/* ABORTS-TRANSACTION, returns to main loop */
    }

    if (retval == LispNil)
      elog(WARN,"rule definition failed");
    else
      return(retval);
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

     extern	char		*PlanToString();

     eobj_string = CString ( CAR ( event_obj));
     eslot_string = CString ( CADR ( event_obj));

     event_relation = amopenr ( eobj_string );
     if ( event_relation == NULL ) {
	 elog(WARN, "virtual relations not supported yet");
     }

     if ( NodeHasTag ( event_type, classTag(LispSymbol) ) ) {

	 switch (  CAtom ( event_type ) ) {
	   case RETRIEVE:
	     ruleId = InsertRule ( rulename, 
				  RETRIEVE,
				  eobj_string,
				  eslot_string,
				  PlanToString(event_qual),
				  (int)is_instead,
				  PlanToString(action) );
			 
	     createAndPutLock ( ruleId, 0, 
			       RelationGetRelationId ( event_relation ),
			       varattno ( event_relation, eslot_string ));

	     /* HandleRetrieveRuleDef ( event_obj, event_qual, false ,
				    action ); */
	     break;
	   case REPLACE:
	     HandleReplaceRuleDef ( event_obj, event_qual, false ,
				   action );
	   default:
	    elog(WARN,"unknown event type given to rule definition");
	 }
    }
}

ShowAllRules()
{
    LispValue i;

    foreach (i,retrieve_rules) {
	ShowRetrieveRule(CAR(i));
    }
    foreach (i,replace_rules) {
	ShowReplaceRule(CAR(i));
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


/*
 *	InsertRule
 *	ARGS :	rulname		-	name of the rule
 *		evtype		-	one of RETRIEVE,REPLACE,DELETE,APPEND
 *		evobj		-	name of relation
 *		evslot		-	comma delimited list of slots
 *		evinstead	-	is an instead rule
 *		actiontree	-	parsetree(s) of rule action
 */

#include "anum.h"		/* why are nattrs hardwired ??? */

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
    Relation 	rule_relation = NULL;
    HeapTuple 	new_rule_tuple = NULL;
    char	*attr_values[RuleRelationNumberOfAttributes];
    char	attr_is_null[RuleRelationNumberOfAttributes];
    int		i = 0 ;

    static char	rulebuf[1024];

    /* sprintf(rulebuf,"append pg_rule (rulname=%s,evtype=%d,evobj=%s,\
       evslot=%s,evqual=%s,evinstead=%d,action=%s",
	    evtype,evobj,evslot,evqual,evinstead,actiontree);
	    */

    rule_relation = RelationNameOpenHeapRelation ( RuleRelationName );

    attr_values[i]  = (char *)rulname ;
    for ( i = 1 ; i < 9 ; i ++ ) {
	attr_values[i] = (char *)(NULL);
    }
    attr_values[i++] = (char *)evtype;
    attr_values[i++] = (char *)evinstead;
    attr_values[i++] = (char *)evobj; 
    attr_values[i++] = fmgr(F_TEXTIN,evslot);		/* slot list */
    attr_values[i++] = fmgr(F_TEXTIN,evqual);
    attr_values[i++] = fmgr(F_TEXTIN,actiontree);
 
    for ( i = 0 ; i < 15 ; i ++ ) {
	attr_is_null[i] = ( (attr_values[i] == NULL) ? 
			   'n' : ' ' );
    }
    if ( !RelationIsValid( rule_relation )) {
	elog(WARN, "RuleInsertCatalog: could not open relation %s",
	     RuleRelationName);
	return((ObjectId) 0);
    }

    new_rule_tuple = FormHeapTuple ( RuleRelationNumberOfAttributes , 
				     &rule_relation->rd_att , attr_values ,
				     attr_is_null );
    (void) RelationInsertHeapTuple ( rule_relation , new_rule_tuple, 
				     (double *)NULL );

    RelationCloseHeapRelation ( rule_relation );
    return ( new_rule_tuple->t_oid );
}
	
/*	DefineVirtualRelation
 *	- takes a relation name and a targetlist
 *	and generates a string "create relation ( ... )"
 *	by taking the attributes and types from the tlist
 *	and reconstructing the attr-list for create.
 *	then calls "pg-eval" to evaluate the creation,
 */
  
#include "relation.h"
#include "relation.a.h"
#include "catalog_utils.h"
#include "primnodes.h"
#include "primnodes.a.h"

DefineVirtualRelation ( relname , tlist )
     char *relname;
     List tlist;
{
    LispValue element;
    static char querybuf[1024];
    char *index = querybuf;
    int i;

    for ( i = 0 ; i < 1024 ; i++ ) {
	querybuf[i] = NULL;
    }

    sprintf(querybuf,"create relation %s(",relname );
    index += strlen(querybuf);
    
    if ( ! null ( tlist )) {
	foreach ( element, tlist ) {
	    TLE 	entry = get_entry((TL)element);
	    Resdom 	res   = get_resdom(entry);
	    Name	resname = get_resname(res);
	    ObjectId	restype = get_restype(res);
	    Name	restypename = tname(get_id_type(restype));

	    sprintf(index,"%s = %s,",resname,restypename);
	    index += strlen(index);
	}
	*(index-1) = ')';
	
    } else
	elog ( WARN, "attempted to define virtual relation with no attrs");
    printf("\n%s\n\n",querybuf);
    fflush(stdout);
}    

/*	HandleView
 *	- takes a "viewname", "parsetree" pair and then
 *	1)	construct the "virtual" relation 
 *	2)	commit the command, so that the relation exists
 *		before the rules are defined.
 *	2)	define the "n" rules specified in the PRS2 paper
 *		over the "virtual" relation
 */

DefineView ( viewname, parsetree )
     String viewname;
     List parsetree;
{
    static char rulebuffer[1024];
    int i = 0;

    for ( i = 0 ; i < 1024 ; i++ ) {
	rulebuffer[i] = NULL;
    }
    if ( getreldesc (viewname) == NULL )
      DefineVirtualRelation ( viewname , parse_targetlist(parsetree));

    CommitTransactionCommand();	
    StartTransactionCommand();
/*    
    sprintf(rulebuffer,
	    "define rule _r%s on retrieve to %s do instead retrieve");
    strlen(rulebuffer)
    sprintf(rulebuffer,
      "define rule _a%s on append to %s do instead";
    sprintf(rulebuffer,
      "define rule _d%s on delete to %s do instead";
    sprintf(rulebuffer,
      "define rule _i%s on replace to %s do instead";
*/

}

#include "rel.h"		/* for Relation stuff */
#include "syscache.h"		/* for SearchSysCache ... */
#include "itup.h"		/* for T_LOCK */

RuleLock
RelationGetRelationLocks ( relation )
     Relation relation;
{
    HeapTuple relationTuple;
    RuleLock relationLock;
    Datum datum;
    Boolean isnull;

    relationTuple = SearchSysCacheTuple(RELNAME,
		       &(RelationGetRelationTupleForm(relation)->relname),
		       (char *) NULL,
		       (char *) NULL,
		       (char *) NULL);

    datum = HeapTupleGetAttributeValue(
		relationTuple,
		InvalidBuffer,
		(AttributeNumber) -2,
		(TupleDescriptor) NULL,
		&isnull);

    relationLock = (RuleLock)DatumGetPointer(datum);
    return(relationLock);
}

/*
 *	RelationHasLocks
 *	- returns true iff a relation has some locks on it
 */

bool
RelationHasLocks ( relation )
     Relation relation;
{
    RuleLock relationLock = RelationGetRelationLocks (relation);

    if ( relationLock == NULL )
      return (false );
    else
      return ( true );
    
}



/*
 *	HandleRewrite
 *	- initial implementation, only do instead
 *
 *	if there exists > 1 do_instead rule pending on a relation.attr 
 *		error
 *	[ there exists at most 1 do instead rule ]
 *	for each pending rule, generate a string corresponding to the action
 *
 *	
 *	if there exists a do instead rule, don't start parselist with
 *	original query.
 */

List
HandleRewrite ( parsetree , trigger ,varno ,varname , commandtype)
     List 	parsetree;
     Relation 	trigger;
     int	varno;
     Name	varname;
     int	commandtype;
{
    RuleLock relationLock = RelationGetRelationLocks(trigger);
    List newparse_list = LispNil;
    RuleLockIntermediate 	*lock  = NULL;
    bool			no_insteads = false;
    RuleLockIntermediate 	*temp = NULL;
    Relation 			ruleRelation = NULL;
    TupleDescriptor		ruleTupdesc = NULL;

    ruleRelation = amopenr ("pg_rule");
    ruleTupdesc = RelationGetTupleDescriptor(ruleRelation);

    lock = RuleLockInternalToIntermediate(relationLock);

    RuleLockIntermediatePrint( stdout , lock );

    if ( no_insteads ) {
	newparse_list = lispCons ( parsetree, LispNil );
    }

    for (temp = lock ; temp != NULL ; temp = temp->next_rulepack ) {
	AttributeNumber oldattno = 0;
	AttributeNumber newattno = 0;
	int   		oldvarno = 0;
	int    		newvarno = 0;
	Name		oldvarname = NULL;
	Name		newvarname = NULL;
	List		actquals = NULL;
	HeapTuple	ruletuple;
	List 		newparse = (List)lispCopy ( parsetree );
	List		ruleparse = NULL;
	char 		*ruleaction = NULL;
	bool		action_is_null = false;

	ruletuple = SearchSysCacheTuple ( RULOID,  temp->ruleid );

	ruleaction = amgetattr ( ruletuple, InvalidBuffer , 15 , 
				ruleTupdesc , &action_is_null ) ;
	ruleaction = fmgr (F_TEXTOUT,ruleaction );

	if ( action_is_null ) {
	    printf ("action is empty !!!\n");
	} else {
	    printf ( "======>  action is \n%s\n", ruleaction );
	}
	fflush(stdout);

	ruleparse = (List)StringToPlan(ruleaction);
	
	newvarname = (Name) CString( CAR (CAR( root_rangetable(
				 parse_root(ruleparse)))));
	oldvarname = varname;

	RewriteModifyRangeTable ( root_rangetable(parse_root(newparse)),
				  oldvarname,
				  newvarname );

	/* RewriteModifyVarnodes ( newparse ); */
	
	lispDisplay(newparse,0);
	
	actquals = parse_qualification ( ruleparse );
	RewriteAddQuals ( newparse ,actquals ,oldvarno );

	lispDisplay(newparse,0);
	
	newparse_list = nappend1 ( newparse_list, newparse );
    }
    
    return(newparse_list );
}

RewriteModifyRangeTable ( rangetable , oldrelname, newrelname )
     List rangetable;
     Name oldrelname;
     Name newrelname;
{
    List i = NULL;

    foreach ( i , rangetable ) {
	List rte = CAR(i);
	Name rname = (Name)CString(CAR(rte));

	if ( !strcmp ( rname , oldrelname )) {
	    CAR(i) = (LispValue)
	      CDR ((LispValue)MakeRangeTableEntry ( newrelname , 
						   LispNil , newrelname ));
	}
    }
}

RewriteAddQuals ( parsetree, rulequals ,varno )
     List parsetree;
     List rulequals;
     int varno;
{
    List userquals = parse_qualification ( parsetree );

    if ( userquals != NULL ) {
	if ( rulequals != NULL ) {
	    parse_qualification ( parsetree ) = 
	      lispCons (lispInteger (AND), 
			lispCons (userquals, 
				  lispCons (rulequals,LispNil)));
	}
    } else {
	if (rulequals != NULL ) {
	    /* (userqual is null, so only rulequals )*/
	    parse_qualification ( parsetree ) = rulequals;
	}
    }

    /* RewriteModifyVarnodes ( rulequals, 1, varno ); */
}

/*
 *	QueryRewrite
 *	- takes a parsetree, if there are no locks at all, return NULL
 *	  if there are locks, evaluate until no locks exist, then
 *	  return list of action-parsetrees, 
 */

List 
QueryRewrite ( parsetree )
     List parsetree;
{
    List root		= NULL;
    List rangetable     = NULL;
    int  command	= 0;
    int	 varno		= 1;
    List parselist	= NULL;
    List i		= NULL;

    if ((root = parse_root(parsetree)) != NULL ) {
	rangetable = root_rangetable(root);
	command = root_command_type ( root );
    }

    foreach ( i , rangetable ) {
	List entry = CAR(i);
	Name varname = (Name)CString(CAR(entry));
	Relation to_be_rewritten = amopenr ( varname );

	if ( to_be_rewritten == NULL ) {
	    elog (WARN,"strangeness ... rangevar %s can't be found",varname);
	}
	printf("checking for locks on %s\n",varname);
	fflush(stdout);
	
	if ( RelationHasLocks( to_be_rewritten )) {
	    parselist = nconc(parselist,
			      HandleRewrite ( parsetree , 
					     to_be_rewritten,
					     varno,
					     varname,
					     command ));
	}
	varno += 1;
    }
    return (parselist);
}


#endif
