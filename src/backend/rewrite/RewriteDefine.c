/* $Header$ */

#include "tmp/c.h"
#include "parser/parse.h"
#include "parser/atoms.h"
#include "nodes/pg_lisp.h"
#include "utils/log.h"
#include "parser/parsetree.h"
#include "catalog/catname.h"
#include "nodes/relation.h"
#include "access/heapam.h"		/* access methods like amopenr */
#include "access/htup.h"
#include "access/ftup.h";
#include "utils/fmgr.h"
#include "tmp/datum.h"
#include "catalog_utils.h"
#include "utils/rel.h"		/* for Relation stuff */
#include "catalog/syscache.h"		/* for SearchSysCache ... */
#include "access/itup.h"		/* for T_LOCK */
#include "nodes/primnodes.a.h"

ObjectId LastOidProcessed = InvalidObjectId;

typedef EventType Event;

#define ShowParseTL(ptree)	lispDisplay(parse_targetlist(ptree),0)
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree),0)
#define ShowParseCmd(ptree)	lispDisplay(root_command_type \
					    parse_root(ptree),0))
#define ShowParseQual(ptree)	lispDisplay(parse_qualification(ptree),0)

OID  InsertRule ();

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
#include "oid.h"
#include "pmod.h"

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
    static char	rulebuf[1024];
    ObjectId rule_oid = InvalidObjectId;
    ObjectId eventrel_oid = InvalidObjectId;
    AttributeNumber evslot_index = InvalidAttributeNumber;
    Relation eventrel = NULL;

    /* XXX - executor cannot
       handle appends with oids yet, so this
       won't work. temporarily, hack executor/result.c
       to set LastOidProcesed.

       rule_oid = newoid(); /* lib/catalog/catalog.c */

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

    printf("rule is \n%s\n", rulebuf );
    pg_eval(rulebuf);

    printf("RuleOID is : %d\n", LastOidProcessed );

    return ( LastOidProcessed );
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

    sprintf(querybuf,"create %s(",relname );
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
    pg_eval(querybuf);
    fflush(stdout);
}    

Name
attname ( relname , attnum )
     Name relname;
     int attnum;
{
    Relation relptr;
    HeapTuple atp;
    ObjectId reloid;
    Name varname;

    relptr = amopenr (relname );
    reloid = RelationGetRelationId ( relptr );
   
    atp = SearchSysCacheTuple ( ATTNUM, reloid, attnum , NULL, NULL );
    if (!HeapTupleIsValid(atp)) {
	elog(WARN, "getattnvals: no attribute tuple %d %d",
	     reloid, attnum);
	return(0);
    }
    varname = (Name)&( ((AttributeTupleForm) GETSTRUCT(atp))->attname );

    return ( varname );
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
    List tlist = parse_targetlist(parsetree);
    LispValue element;
    extern LispValue p_rtable;

    int i;

    if ( getreldesc (viewname) == NULL ) 
      DefineVirtualRelation ( viewname , tlist );

    if ( ! null ( tlist )) {
	i = 0;
	foreach ( element, tlist ) {
	    TLE 	entry = get_entry((TL)element);
	    Resdom 	res   = get_resdom(entry);
	    Var		var   = get_expr(entry);
	    int		varno = get_varno(var);
	    Name	resname = get_resname(res);
	    Name	varname;
	    Name	varattname;
	    LispValue	temp;

	    temp = (LispValue)root_rangetable(parse_root(parsetree));
	    for ( i = 1 ; i < varno ; i ++ )
	      temp = CDR(temp);

	    varname = (Name)CString(CAR(CAR(temp)));

	    varattname = attname ( varname, get_varattno(var) );
	    DefineViewRule ( viewname, resname, varname, varattname, i++ );

	}
    }	

}

DefineViewRule(viewname,viewattr,basename,baseattr,attrno)
     String viewname;
     String viewattr;
     String basename;
     String baseattr;
     AttributeNumber attrno;
{
    static char rulebuffer[1024];
    int i = 0;

    for ( i = 0 ; i < 1024 ; i++ ) {
	rulebuffer[i] = NULL;
    }

    sprintf(rulebuffer,
    "define rule _r%s%d on retrieve to %s.%s do instead retrieve %s.%s",
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    sprintf(rulebuffer,
	    "define rule _a%s%d on append to %s.%s do instead",
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    sprintf(rulebuffer,
	    "define rule _d%s%d on delete to %s do instead",
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    sprintf(rulebuffer,
	    "define rule _i%s%d on replace to %s do instead", 
	    viewname,attrno,viewname,viewattr,basename,baseattr);
    
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


