/* ----------------------------------------------------------------
 *   FILE
 *	RuleHandler.c
 *	
 *   NOTES
 *
 *   IDENTIFICATION
 * $Header$
 * ----------------------------------------------------------------
 */

#include "tmp/postgres.h"

/*************************************************************

  LockTypeRetrieveAction = "on retrieve do x" where x = {append,delete,replace}
  LockTypeRetrieveWrite = "on retrieve do x" where x = "replace current"
  LockTypeRetrieveRelation = "on retrieve do instead retrieve"

#define LockTypeRetrieveAction		((Prs2LockType) 'r')
#define LockTypeAppendAction		((Prs2LockType) 'a')
#define LockTypeDeleteAction		((Prs2LockType) 'd')
#define LockTypeReplaceAction		((Prs2LockType) 'u')
#define LockTypeRetrieveWrite		((Prs2LockType) 'R')
#define LockTypeAppendWrite		((Prs2LockType) 'A')
#define LockTypeDeleteWrite		((Prs2LockType) 'D')
#define LockTypeReplaceWrite		((Prs2LockType) 'U')
#define LockTypeRetrieveRelation	((Prs2LockType) 'V')

**************************************************************/

#include "access/ftup.h";
#include "access/heapam.h"		/* access methods like amopenr */
#include "access/htup.h"
#include "access/itup.h"		/* for T_LOCK */
#include "parser/atoms.h"
#include "parser/parse.h"
#include "parser/parsetree.h"
#include "rules/prs2.h"
#include "rules/prs2locks.h"
#include "utils/fmgr.h"
#include "utils/log.h"
#include "utils/rel.h"		/* for Relation stuff */

#include "nodes/pg_lisp.h"
#include "nodes/relation.h"
#include "nodes/primnodes.a.h"
    
#include "catalog/catname.h"
#include "catalog/syscache.h"		/* for SearchSysCache ... */
    
#include "catalog_utils.h"

extern LispValue TheseLocksWereTriggered ( /* RuleLock, LispValue */ );
/*
 *	RelationHasLocks
 *	- returns true iff a relation has some locks on it
 */
extern RuleLock prs2GetLocksFromTuple();

RuleLock
RelationGetRelationLocks ( relation )
     Relation relation;
{
    HeapTuple relationTuple;
    RuleLock relationLocks;

    relationTuple = SearchSysCacheTuple(RELNAME,
		       &(RelationGetRelationTupleForm(relation)->relname),
		       (char *) NULL,
		       (char *) NULL,
		       (char *) NULL);

    relationLocks = prs2GetLocksFromTuple( relationTuple,
					   InvalidBuffer,
					   (TupleDescriptor) NULL );
    return(relationLocks);
}

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
 * RuleIdGetRuleParsetrees
 *
 * given a rule oid, look it up and return the prs2text
 *
 */

List
RuleIdGetRuleParsetrees ( ruleoid )
     OID ruleoid;
{
    HeapTuple 		ruletuple;
    char 		*ruleaction = NULL;
    bool		action_is_null = false;
    Relation 		ruleRelation = NULL;
    TupleDescriptor	ruleTupdesc = NULL;
    List    		ruleparse = NULL;
    List		i = NULL;

    ruleRelation = amopenr ("pg_prs2rule");
    ruleTupdesc = RelationGetTupleDescriptor(ruleRelation);
    ruletuple = SearchSysCacheTuple ( RULOID,  ruleoid );
    ruleaction = amgetattr ( ruletuple, InvalidBuffer , 7 , 
			    ruleTupdesc , &action_is_null ) ;
    
    ruleaction = fmgr (F_TEXTOUT,ruleaction );
    ruleparse = (List)StringToPlan(ruleaction);
    
    if ( action_is_null ) {
	printf ("action is empty !!!\n");
	return ( LispNil );
    } else {
	foreach ( i , ruleparse ) {
	    Print_parse ( CAR(i) );
	}
    }
    fflush(stdout);

    amclose ( ruleRelation );
    return (ruleparse);
}

List
RuleIdGetRuleParse (ruleoid)
     OID ruleoid;
{
    /* XXX - assume only one parsetree */
    elog(WARN,"calling unsupported routine RuleIdGetRuleParse");
    return (CAR(RuleIdGetRuleParsetrees(ruleoid)));
}

/*
 * offset all the varnos in the rule parsetree by length
 * of the user parsetree - 2 ( -2 is because of current and new )
 *
 */
OffsetAllVarNodes ( trigger_varno, parsetree, offset )
     int trigger_varno;
     List parsetree;
     int offset;
{
    List i;

    foreach ( i , parsetree ) {
	Node thisnode = (Node)CAR(i);
	if ( thisnode && IsA(thisnode,Var)) {
	    if (get_varno (thisnode) == 1) {
		/* *CURRENT* */
		printf("taking care of a varnode pointing to *CURRENT*");
		set_varno ( thisnode, trigger_varno );
		CAR ( get_varid (thisnode) ) =
		  lispInteger ( trigger_varno );
		/* XXX - should fix the varattno too !!! */
	    } else if (get_varno(thisnode) == 2 ) {
		/* *NEW* 
		 *
		 * should take the right side out of the corresponding
		 * targetlist entry from the user query
		 * and stick it here
		 */
		elog(NOTICE,"NEW not properly taken care of");
	    } else {
		set_varno ( thisnode, get_varno(thisnode) + offset );

		CAR( get_varid ( thisnode ) ) =
		  lispInteger (get_varno(thisnode) );
	    }
	}
	if ( thisnode && IsA(thisnode,Param) ) {
	    /* currently only *CURRENT* */
	}
	if ( thisnode && thisnode->type == PGLISP_DTPR )
	  OffsetAllVarNodes(trigger_varno,(List)thisnode,offset);
    }
}

/*
 * ChangeTheseVars
 * 
 * 	varno,varattno - varno and varattno of Var node must match this
 *			 to qualify for change
 *	parser_subtree - tree to iterate over
 *	lock	       - the lock which caused this rewrite
 */

ChangeTheseVars ( varno, varattno, parser_subtree, replacement )
     int varno;
     AttributeNumber varattno;
     List parser_subtree;
     List replacement;
{
    List i = NULL;

    foreach ( i , parser_subtree ) {
	Node temp = (Node)CAR(i);
	
	if (IsA(temp,Var) &&
	    ((get_varattno((Var)temp) == varattno )||
	     (varattno == -1 )) && 
	    (get_varno((Var)temp) == varno ) ) {

	    CAR(i) = replacement;
	    /*
	    CDR(last(replacement)) = CDR(i);
	    CDR(i) = CDR(replacement);
	    */
	}
	if ( temp->type == PGLISP_DTPR )
	  ChangeTheseVars ( varno, varattno, (List)temp, replacement );
    }
}

ReplaceVarWithMulti ( varno, attno, parser_subtree, replacement )
     int varno;
     AttributeNumber attno;
     List parser_subtree;
     List replacement;
{
    List i = NULL;
    List vardotfields = NULL;
    List saved = NULL;

    foreach ( i , parser_subtree ) {
	Node temp = (Node)CAR(i);
	
	if (IsA(temp,Var) &&
	    get_varattno((Var)temp) == attno &&
	    get_varno((Var)temp) == varno ) {
	    elog (NOTICE, "now replacing ( %d %d )",varno,attno);
	    vardotfields = get_vardotfields ( temp );

 	    if ( vardotfields != NULL ) { 
		/* a real postquel procedure invocation */
		List j = NULL;
		
		foreach ( j , replacement ) {
		    List  rule_tlentry = CAR(j);
		    Resdom rule_resdom = (Resdom)CAR(rule_tlentry);
		    List rule_tlexpr = CDR(rule_tlentry);
		    Name rule_resdom_name = get_resname ( rule_resdom );
		    char *var_dotfields_firstname = NULL;

		    var_dotfields_firstname = 
		      CString ( CAR ( vardotfields ) );
		    
		    if ( !strcmp ( rule_resdom_name, 
				  var_dotfields_firstname ) ) {
		      if ( saved && saved->type == PGLISP_DTPR  &&
			   IsA(CAR(saved),Resdom) &&
			   get_restype (CAR(saved)) !=
			   get_restype (rule_resdom) ) {
			set_restype (CAR(saved), get_restype(rule_resdom));
			set_reslen  (CAR(saved), get_reslen(rule_resdom));
		      }
		      CAR(i) = CAR(rule_tlexpr);
		      CDR(i) = CDR(rule_tlexpr);
		  }
		    
		}

	    } else {
		/* no dotfields, so retrieve text ??? */
		List j = NULL;
		foreach ( j , replacement ) {
		    List  rule_tlentry = CAR(j);
		    /* Resdom rule_resdom = (Resdom)CAR(rule_tlentry); */
		    List rule_tlexpr = CDR(rule_tlentry);
		    /* Name rule_resdom_name = get_resname ( rule_resdom );*/
		    
		    CAR(i) = CAR(rule_tlexpr);
		    CDR(i) = CDR(rule_tlexpr);
		    
		}
		
	    } /* vardotfields != NULL */

	}

	
	if ( temp->type == PGLISP_DTPR )
	  ReplaceVarWithMulti ( varno, attno, (List)temp, (List)replacement );
	
	saved = i;
    }

}
StickOnUpperAndLower ( user, rule )
     List user;
     List rule;
{
    List i = NULL;

    foreach ( i , rule ) {
	List entry = CAR(i);
	if ((! strcmp ( get_resname (CAR(entry)), "u") ) ||
	    (! strcmp ( get_resname (CAR(entry)), "l") )      )
	  CDR(last(user)) = lispCons ( entry, LispNil );
    }
    
}
/*
 * walk the targetlist,
 * fixing the resnodes resnos 
 * as well as removing "extra" resdoms
 * caused by the replacement of varnodes by
 * entire "targetlists"
 */

FixResdom ( targetlist )
     List targetlist;
{
    List i;
    int res_index = 1;

    foreach ( i , targetlist ) {
	List tle = CAR(i);
	if (CADR(tle)->type == PGLISP_DTPR &&
	    IsA (CAR(CADR(tle)),Resdom)) {
	    CAR(i) = CADR(tle);
	    CDR(last(CDR(tle))) = CDR(i);
	    CDR(i) = CDR(CDR(tle));
	}
    }

    foreach ( i , targetlist ) {
	List tle = CAR(i);
	Resdom res = (Resdom)CAR(tle);

	set_resno(res,res_index++);
    }
}

/*
 * ThisLockWasTriggered
 *
 * walk the tree, if there we find a varnode,
 * we check the varattno against the attnum
 * if we find at least one such match, we return true
 * otherwise, we return false
 */


bool
ThisLockWasTriggered ( varno, attnum, parse_subtree )
     int varno;
     AttributeNumber attnum;
     List parse_subtree;
{
    List i;

    foreach ( i , parse_subtree ) {

	Node temp = (Node)CAR(i);

	if ( !null(temp) &&
	     IsA(temp,Var) && 
	     ( varno == get_varno((Var)temp)) && 
	     (( get_varattno((Var)temp) == attnum ) ||
	        attnum == -1 ) )
	  return ( true );
	if ( temp && temp->type == PGLISP_DTPR &&
	     ThisLockWasTriggered ( varno, attnum, (List) temp ) )
	  return ( true );

    }

    return ( false );
}

/*
 * MatchRetrieveLocks
 * - looks for varnodes that match the rulelock
 * (where match(foo) => varno = foo.varno AND 
 *			        ( (oneLock->attNum == -1) OR
 *				  (oneLock->attNum = foo.varattno ))
 *
 * RETURNS: list of rulelocks
 * XXX can be improved by searching for all locks
 * at once by NOT calling ThisLockWasTriggered
 */
List
MatchRetrieveLocks ( rulelocks , parse_subtree , varno )
     RuleLock rulelocks;
     List parse_subtree;
     int varno;
{
    int nlocks		= 0;
    int i 		= 0;
    Prs2OneLock oneLock	= NULL;
    List real_locks 	= NULL;

    Assert ( rulelocks != NULL );

    nlocks = prs2GetNumberOfLocks ( rulelocks );
    Assert (nlocks <= 16 );

    for ( i = 0 ; i < nlocks ; i++ ) {
	oneLock = prs2GetOneLockFromLocks ( rulelocks , i );
	if ( ThisLockWasTriggered ( varno,
				   oneLock->attributeNumber,
				    parse_subtree ))
	  real_locks = lispCons ( oneLock , real_locks );
    }

    return ( real_locks );

}

LispValue
TheseLocksWereTriggered ( rulelocks , parse_subtree, event_type , varno )
     RuleLock rulelocks;
     List parse_subtree;
     int event_type;
     int varno;
{
    int nlocks		= 0;
    int i 		= 0;
    Prs2OneLock oneLock	= NULL;
    /* int j 		= 0;*/
    List real_locks 	= NULL;
    Assert ( rulelocks != NULL );
    nlocks = prs2GetNumberOfLocks ( rulelocks );
    Assert (nlocks <= 16 );

    for ( i = 0 ; i < nlocks ; i++ ) {

	oneLock = prs2GetOneLockFromLocks ( rulelocks, i );

	switch ( oneLock->lockType ) {
	  case LockTypeRetrieveAction:
	  case LockTypeRetrieveWrite:
	    if (( event_type == RETRIEVE ) &&
		( ThisLockWasTriggered ( varno,
					oneLock->attributeNumber , 
					parse_subtree )) ) {
		real_locks = lispCons ( oneLock, real_locks );
	    } else {
		continue;
	    }
	  case LockTypeInvalid:
	  case LockTypeAppendAction:	
	  case LockTypeDeleteAction:	
	  case LockTypeReplaceAction:	
	  case LockTypeAppendWrite:	
	  case LockTypeDeleteWrite:	
	  case LockTypeReplaceWrite:
	    continue; /* skip the rest of the stuff in this
			 iteration */
	} /* switch */

    } /* for */
    return ( real_locks );
}

Print_quals ( quals )
     List quals;
{
    printf("\nQualifications = \n");
    lispDisplay(quals,0);
    fflush(stdout);
}
Print_targetlist ( tlist )
     List tlist;
{
    printf("\nTargetlist = \n");
    lispDisplay(tlist,0);
    fflush(stdout);
}
Print_rangetable ( rtable )
     List rtable;
{
    printf("\nRangetable = \n");
    lispDisplay(rtable,0);
    fflush(stdout);
}

Print_parse ( parsetree )
     List parsetree;
{
    List quals = parse_qualification(parsetree);
    List tlist = parse_targetlist(parsetree);
    List rtable = root_rangetable (parse_root (parsetree));
    
    Print_rangetable(rtable);
    Print_targetlist(tlist);
    Print_quals(quals);
    fflush(stdout);
}

void
PrintRuleLock ( rlock )
     Prs2OneLock rlock;
{
    printf("#S(RuleLock ");
    printf(":rulid %ld ",rlock->ruleId);
    printf(":locktype %c ", rlock->lockType );
    printf(":attnum %d )", rlock->attributeNumber );
    fflush(stdout);
}

void
PrintRuleLockList ( rlist )
     List rlist;
{
    Prs2OneLock temp = NULL;
    List j = NULL;
    foreach ( j , rlist ) {
	temp = (Prs2OneLock)CAR(j);
	PrintRuleLock ( temp );
    }
    printf("\n");
}

void
AddRuleQualifications ( user_parsetree , rule_qual )
     List user_parsetree;
     List rule_qual;
{
    if ( parse_qualification(user_parsetree) == NULL )
      parse_qualification(user_parsetree) = rule_qual;
    else
      parse_qualification ( user_parsetree ) =
	lispCons ( lispInteger(AND),  
		  lispCons ( parse_qualification(user_parsetree),
			    lispCons ( rule_qual, LispNil )));    
}

List
ModifyVarNodes( retrieve_locks , user_rt_length , current_varno , 
	        to_be_rewritten , user_tl , user_rt ,user_parsetree )
     List retrieve_locks;
     int user_rt_length;
     int current_varno;
     Relation to_be_rewritten;
     List user_tl;
     List user_rt;
     List user_parsetree;
{
    List i			= NULL;
    List j			= NULL;
    List k			= NULL;
    List ruletrees		= NULL;
    List additional_queries	= NULL; /* set by locktyperetrieveaction */

    foreach ( i , retrieve_locks ) {
	Prs2OneLock this_lock = (Prs2OneLock)CAR(i);
	List rule_tlist = NULL;
	List rule_qual = NULL;
	List rule_rangetable = NULL;
	int result_rtindex = 0 ;
	char *result_relname =  NULL;
	List result_rte = NULL;
	
	ruletrees = RuleIdGetRuleParsetrees ( this_lock->ruleId );
	foreach ( j , ruletrees ) {
	    List ruleparse = CAR(j);

	    Assert (ruleparse != NULL );
	    rule_tlist = parse_targetlist(ruleparse);
	    rule_qual = parse_qualification(ruleparse);
	    rule_rangetable = root_rangetable(parse_root
					      (ruleparse));
	    
	    if ( this_lock->attributeNumber == -1 )
	      elog(NOTICE, "firing a multi-attribute rule");

	    OffsetAllVarNodes ( current_varno, 
			        ruleparse, 
			        user_rt_length -2 );
	    
	    switch ( this_lock->lockType ) {
	      case LockTypeRetrieveWrite: 
		/* ON RETRIEVE DO REPLACE CURRENT */
		
		elog ( NOTICE, "replace current action");
		Print_parse ( ruleparse );
		
		result_rtindex = 
		  CInteger(root_result_relation(parse_root(ruleparse )));
		result_rte = nth ( result_rtindex -1 , rule_rangetable );
		result_relname = CString ( CAR ( result_rte ));
		if ( strcmp ( result_relname,"*CURRENT*")) 
		  elog(WARN,"a on-retr-do-repl rulelock with bogus result");
		foreach ( k , rule_tlist ) {
		    List tlist_entry = CAR(k);
		    Resdom tlist_resdom = (Resdom)CAR(tlist_entry);
		    List tlist_expr = CADR(tlist_entry);
		    char *attname = (char *)
		      get_resname ( tlist_resdom );
		    int attno;
		    
		    elog ( NOTICE, "replacing %s", attname );
		    attno = 
		      varattno ( to_be_rewritten, attname );
		    
		    ChangeTheseVars ( current_varno, 
				     attno,
				     user_tl,
				     tlist_expr );
		} /* foreach */
		break;
	      case LockTypeRetrieveRelation:
		/* now stuff the entire targetlist
		 * into this one varnode, filtering
		 *  out according to whatever is
		 *  in vardotfields
		 * NOTE: used only by procedures ??? 
		 */
		elog(NOTICE,"retrieving procedure result fields");
		ReplaceVarWithMulti ( current_varno, 
				     this_lock->attributeNumber, 
				     user_tl, 
				     rule_tlist );
		
		printf ("after replacing tlist entry :\n");
		lispDisplay ( user_tl, 0 );
		fflush(stdout);
		
		result_relname = "";
		FixResdom ( user_tl );
		break;
	      case LockTypeRetrieveAction:
		elog(NOTICE,"retrieve triggering other actions");
		elog(NOTICE,"does not modify existing parsetree");
		break;
	      default:
		elog(WARN,"on retrieve do <action>, action unknown");
	    }
	    
	    /* add the additional rt_entries */
	    CDR(last(user_rt)) = 
	      CDR(CDR(rule_rangetable));

	    /* XXX - clean up redundant rt_entries ??? */

	    AddRuleQualifications ( user_parsetree , rule_qual );
	    
	    printf ("\n");
	    printf ("*************************\n");
	    printf (" Modified User Parsetree\n");
	    printf ("*************************\n");
	    Print_parse ( user_parsetree );

	} /* foreach of the ruletrees */
    } /* foreach of the locks */

    return ( additional_queries );
}
List
ModifyDeleteQueries( drop_user_query )
     bool *drop_user_query;
{
    return(NULL);
}

List
MatchDeleteLocks ( )
{

}
/*
 * MatchUpdateLocks
 * - match the list of locks, 
 */
List
MatchUpdateLocks ( command , rulelocks )
     int command;
     RuleLock rulelocks;
{
    List real_locks 		= NULL;
    Prs2OneLock oneLock		= NULL;
    int nlocks			= 0;
    int i			= 0;

    Assert ( rulelocks != NULL ); /* we get called iff there is some lock */

    if ( command != REPLACE && command != APPEND ) 
      return ( NULL );

    /*
     * if we reach the following statement, the
     * user_command must be a replace or append
     * XXX - (does it matter which ???)
     */
    nlocks = prs2GetNumberOfLocks ( rulelocks );
    Assert (nlocks <= 16 );

    for ( i = 0 ; i < nlocks ; i++ ) {
	oneLock = prs2GetOneLockFromLocks ( rulelocks , i );

	PrintRuleLock ( oneLock );
#ifdef NOTYET
	  real_locks = lispCons ( oneLock , real_locks );
#endif
    }

    return ( real_locks );


}

List
ModifyUpdateNodes( drop_user_query )
     bool *drop_user_query;
{
    return(NULL);
}
/*
 * ProcessOneLock
 * - process a single rangetable entry
 *
 * RETURNS: a list of additional queries that need to be executed
 * MODIFIES: current qual or tlist
 *
 */
List 
ProcessOneLock ( user_parsetree , reldesc , user_rangetable , 
		 current_varno , command , drop_user_query )
     List user_parsetree;
     Relation reldesc;
     List user_rangetable;
     int current_varno;
     int command;
     bool *drop_user_query;
{
    RuleLock rlocks 		= NULL;
    List retrieve_locks 	= NULL;
    List update_locks		= NULL;
    List delete_locks		= NULL;
    List additional_queries = NULL;
    List user_tlist = parse_targetlist(user_parsetree);

#ifdef NOTYET
    List user_qual = parse_qualification(user_parsetree);
#endif

    rlocks = RelationGetRelationLocks ( reldesc );

    if ( rlocks == NULL ) {

	/* this relation (rangevar) has no locks on it, so
	 * return without either modifying the user query 
	 * or generating a new (extra) query
	 */

	return ( NULL );
    }

    /*
     * MATCH RETRIEVE RULES HERE (regardless of current command type)
     * a varnode that matches the current varno and the varattno matches
     * "rlocks" attnum ( where match means = if attnum is > 0 or always true
     * if attnum = -1 )
     */

    retrieve_locks = MatchRetrieveLocks ( rlocks , user_parsetree , 
					  current_varno );
    if ( retrieve_locks ) {
	printf ( "\nThese retrieve rules were triggered: \n");
	PrintRuleLockList ( retrieve_locks );
    }

    additional_queries = ModifyVarNodes ( retrieve_locks,
					  length ( user_rangetable ),
					  current_varno,
					  reldesc,
					  user_tlist,
					  user_rangetable,
					  user_parsetree
					 );
    /*
     * drop_user_query IS NOT SET in this routine
     * because "retrieve" rules when operating on qualifications
     * modify the user_parsetree, therefore, the "instead"ness 
     * of the rule is satisfied. 
     * 
     * XXX - right now, retrieve rules cannot be of the "also"
     * type because Postgres does not support union'ed qualifiers
     * so, for example the rule :
     * 		on retrieve to toy
     *		do retrieve ( emp.all ) where emp.dept = "toy"
     * which theoretically should produce the partially materialized
     * view "toy", of which some tuples are really in "toy" and
     * others in "emp", does not really work.
     */

    switch (command) {
      case RETRIEVE:
	/* do nothing since it is handled above */
	break;
      case DELETE:
	/* no targetlist, so handled differently */
	delete_locks = MatchDeleteLocks ();

	additional_queries = append ( additional_queries, 
				     ModifyDeleteQueries( drop_user_query ) );
	break;
      case APPEND:
      case REPLACE:
	update_locks = MatchUpdateLocks ( command , rlocks );
        additional_queries = append ( additional_queries,
				     ModifyUpdateNodes( drop_user_query ) );
	break;
    }

    /* when we get here, additional queries is either null 
     * (no additional queries) OR additional queries is a list
     * of rule generated queries (in addition to the user query)
     * NOTE: at this point, the original user query
     * may have mutated beyond recognition
     * ALSO: what do we do if the retrieve rule says "instead"
     * but another rule ( that is also active on this query ) doesn't
     */
    
    return ( additional_queries );
}

List 
QueryRewrite ( parsetree )
     List parsetree;
{
    List user_root		= NULL;
    List user_rangetable    	= NULL;
    int  user_command		;
    List output_parselist	= NULL;
    List user_rtentry_ptr	= NULL;
    List user_tl		= NULL;
    List user_qual		= NULL;
    int  user_rt_length		= 0;
    int this_relation_varno	= 1;		/* varnos start with 1 */
    bool drop_user_query	= false;	/* unless ProcessOneLock
						 * modifies this, we
						 * add on the user_query
						 * in front of all queries
						 */

    Assert ( parsetree != NULL );
    Assert ( (user_root = parse_root(parsetree)) != NULL );

    user_rangetable 		= root_rangetable(user_root);
    user_command 		= root_command_type ( user_root );
    user_rt_length 		= length ( user_rangetable );
    
    /*
     * only for a delete may the targetlist be NULL
     */
    if ( user_command != DELETE ) 
      Assert ( (user_tl = parse_targetlist(parsetree)) != NULL );
    
    user_qual = parse_qualification(parsetree);		/* may be NULL */

    foreach ( user_rtentry_ptr , user_rangetable ) {
	List user_rtentry = CAR(user_rtentry_ptr);
	Name this_rtname = NULL;
	Relation this_relation = NULL;

	this_rtname = (Name)CString( CADR ( user_rtentry ));
	    
        this_relation = amopenr ( this_rtname );
	Assert ( this_relation != NULL );

	elog(NOTICE,"checking for locks on %s", this_rtname );

	if ( RelationHasLocks ( this_relation )) {
	    List additional_queries 	= NULL;
	    List additional_query	= NULL;

	    /*
	     * ProcessOneLock _may_ modify the contents
	     * of "user_qual" or "user_tl" and "user_rangetable" if
	     * the rule referenced by the lock has a "do instead"
	     * clause
	     */
	    
	    additional_queries = ProcessOneLock ( parsetree,
						 this_relation, 
						 user_rangetable,
						 this_relation_varno++ ,
						 user_command ,
						 &drop_user_query );
	    if ( drop_user_query == false ) {
		/* start the output parse list with the
		 * user query since no qualifying
		 * "instead" clauses were found
		 */
		output_parselist = parsetree;
	    }
	    if ( additional_queries != NULL )
	      foreach ( additional_query, additional_queries ) {
		  List additional_query_parsetree = CAR(additional_query);
		  nappend1 ( output_parselist , additional_query_parsetree );
	      }
	} 
	
    }

    return ( output_parselist ); 

}

	
    
