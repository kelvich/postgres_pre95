/* ----------------------------------------------------------------
 * $RCSfile$
 * $Revision$
 * $State$
 * ----------------------------------------------------------------
 */

/****************************************************/

#include "tmp/postgres.h"
#include "utils/log.h"
#include "nodes/pg_lisp.h"		/* for LispValue and lisp support */
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"		/* for accessors to varnodes etc */

#include "rules/prs2.h"			/* XXX - temporarily */
#include "rules/prs2locks.h"		/* XXX - temporarily */

#include "parser/parse.h"		/* for RETRIEVE,REPLACE,APPEND ... */
#include "parser/parsetree.h"		/* for parsetree manipulation */

#include "./RewriteSupport.h"	
#include "./RewriteDebug.h"
#include "./RewriteManip.h"
#include "./RewriteHandler.h"

/*****************************************************/

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
	if (temp ) {
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
	    if (  temp->type == classTag(LispList))
	      ChangeTheseVars ( varno, varattno, (List)temp, replacement );
	}
    }
}

/*
 * used only by procedures, 
 */

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

/*
 * FixResdom
 * - walk the targetlist, fixing the resnodes resnos 
 * as well as removing "extra" resdoms caused by the replacement 
 * of varnodes by entire "targetlists"
 *
 * XXX - should only be called on "retrieve" targetlists
 *	 otherwise badness happens
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
MatchRetrieveLocks ( rulelocks , varno , parse_subtree  )
     RuleLock rulelocks;
     int varno;
     List parse_subtree;
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
	if ( oneLock->lockType == LockTypeRetrieveAction ||
	     oneLock->lockType == LockTypeRetrieveWrite ||
	     oneLock->lockType == LockTypeRetrieveRelation ) {
	    if ( ThisLockWasTriggered ( varno,
				       oneLock->attributeNumber,
				       parse_subtree ))
	      real_locks = lispCons ( oneLock , real_locks );
	}
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
    List user_qual		= parse_qualification(user_parsetree);

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

	    /****************************************

	      XXX - old version

	    OffsetAllVarNodes ( current_varno, 
			       ruleparse, 
			       user_rt_length -2 );
			       
	     *****************************************/
	    HandleVarNodes ( ruleparse,
			     user_parsetree,
			     current_varno,
			     user_rt_length - 2 );
			     
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
		    ChangeTheseVars ( current_varno, 
				     attno,
				     user_qual,
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

	    AddQualifications ( user_parsetree , 
			        rule_qual ,
			        0 );

	    /* add the additional rt_entries */
	    CDR(last(user_rt)) = 
	      CDR(CDR(rule_rangetable));

	    /* XXX - clean up redundant rt_entries ??? */

	    
	    printf ("\n");
	    printf ("*************************\n");
	    printf (" Modified User Parsetree\n");
	    printf ("*************************\n");
	    Print_parse ( user_parsetree );

	} /* foreach of the ruletrees */
    } /* foreach of the locks */

    return ( additional_queries );
}

/*
 * MatchLocks
 * - match the list of locks, 
 */
List
MatchLocks ( locktype, rulelocks , current_varno , user_parsetree )
     Prs2LockType locktype;
     RuleLock rulelocks;
     int current_varno;
     List user_parsetree;
{
    List real_locks 		= NULL;
    Prs2OneLock oneLock		= NULL;
    int nlocks			= 0;
    int i			= 0;
    List actual_result_reln	= NULL;

    Assert ( rulelocks != NULL ); /* we get called iff there is some lock */
    Assert ( user_parsetree != NULL );

    actual_result_reln = 
      root_result_relation ( parse_root ( user_parsetree ) );

    if ( CInteger ( actual_result_reln ) != current_varno ) {
	return ( NULL );
    }

    nlocks = prs2GetNumberOfLocks ( rulelocks );
    Assert (nlocks <= 16 );

    for ( i = 0 ; i < nlocks ; i++ ) {
	oneLock = prs2GetOneLockFromLocks ( rulelocks , i );
	if ( oneLock->lockType == locktype )  {
	    real_locks = lispCons ( oneLock , real_locks );
	} /* if lock is suitable */
    } /* for all locks */
    
    return ( real_locks );
}


List
MatchReplaceLocks ( rulelocks , current_varno, user_parsetree )
     RuleLock rulelocks;
     int current_varno;
     List user_parsetree;
{
    /*
     * currently, don't support LockTypeReplaceWrite
     * which has some kind of on delete do replace current 
     */

    return ( MatchLocks ( LockTypeReplaceAction , 
			  rulelocks, current_varno , user_parsetree ));
}

List
MatchAppendLocks ( rulelocks , current_varno, user_parsetree )
     RuleLock rulelocks;
     int current_varno;
     List user_parsetree;
{
    /*
     * currently, don't support LockTypeAppendWrite
     * which has some kind of on delete do replace current 
     */
    return ( MatchLocks ( LockTypeAppendAction , 
			 rulelocks, current_varno , user_parsetree ));
}

List
MatchDeleteLocks ( rulelocks , current_varno, user_parsetree )
     RuleLock rulelocks;
     int current_varno;
     List user_parsetree;
{
    /*
     * currently, don't support LockTypeDeleteWrite
     * which has some kind of on delete do replace current 
     */

    return ( MatchLocks ( LockTypeDeleteAction , 
			 rulelocks, current_varno , user_parsetree ));
}


/*
 * ModifyUpdateNodes
 * RETURNS:	list of additional parsetrees
 * MODIFIES:	original parsetree, drop_user_query
 * STRATEGY:
 *	foreach rule-action {
 *		action = copy(rule-action);
 *		foreach ( resdom in user_tlist ) {
 *			
 */
List
ModifyUpdateNodes( update_locks , user_parsetree, 
		  drop_user_query ,current_varno )
     List update_locks;
     List user_parsetree;
     bool *drop_user_query;
     int current_varno;
{
    List user_rt	= root_rangetable(parse_root(user_parsetree));
    List i		= NULL;
    List j		= NULL;
    List new_queries	= NULL;
    List ruletrees	= NULL;
    int offset_trigger  = 0;

    Assert ( update_locks != NULL ); /* otherwise we won't have been called */

    /* XXX - for now, instead is always true */
    *drop_user_query = true;

    foreach ( i , update_locks ) {
	Prs2OneLock this_lock 	= (Prs2OneLock)CAR(i);

	printf ("\nNow processing :");
	PrintRuleLock ( this_lock );

	ruletrees = RuleIdGetRuleParsetrees ( this_lock->ruleId );
	foreach ( j , ruletrees ) {
	    List rule_action = CAR(j);
	    List rule_tlist = parse_targetlist(rule_action);
	    List rule_qual = parse_qualification(rule_action);
	    List rule_root = parse_root(rule_action);
	    List rule_rt = root_rangetable(rule_root);
	    int rule_command = root_command_type(rule_root);
	    
	    offset_trigger = length(rule_rt) - 2 + current_varno;
	
	    switch(rule_command) {
		case REPLACE:
		case APPEND:
		    HandleVarNodes ( rule_tlist , user_parsetree,
				     offset_trigger, -2 );
		    root_result_relation(rule_root) =
			lispInteger ( CInteger(root_result_relation(rule_root))
				      - 2 );
		    HandleVarNodes ( rule_qual, user_parsetree, 
				     offset_trigger,
				     -2 );
		    break;
		case RETRIEVE:
		    HandleVarNodes ( rule_tlist , user_parsetree,
				     offset_trigger, -2 );
		    /* 
		     * reset the result relation
		     * to point to the right one,
		     * if there was a "result relation" in the
		     * first place (ie, this was a retrieve into !)
		     */
		    if ( root_result_relation(rule_root)) {
			root_result_relation(rule_root) =
			  lispInteger(CInteger(root_result_relation(rule_root))
				      - 2 );
		    }
		    HandleVarNodes ( rule_qual, user_parsetree, 
				     offset_trigger,
				     -2 );
		    break;
		case DELETE:
		    HandleVarNodes ( rule_tlist , user_parsetree,
				     offset_trigger, -2 );
		    root_result_relation(rule_root) =
			lispInteger ( CInteger(root_result_relation(rule_root))
				      - 2 );
		    HandleVarNodes ( rule_qual, user_parsetree, 
				     offset_trigger,
				     -2 );
		    break;
		default:
		    elog(NOTICE,"unsupported action");
	    }

	    FixRangeTable ( rule_root, user_rt );

	    AddQualifications(rule_action, 
			     parse_qualification(user_parsetree),
			     length(rule_rt) );

	    new_queries = nappend1 ( new_queries, rule_action );
	}
	
    }

    return(new_queries);
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
    List replace_locks		= NULL;
    List append_locks		= NULL;
    List delete_locks		= NULL;
    List additional_queries 	= NULL;
#ifdef UNUSED
    List i 			= NULL;
#endif
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

    retrieve_locks = MatchRetrieveLocks ( rlocks , current_varno , 
					  user_parsetree );
    if ( retrieve_locks ) {
	printf ( "\nThese retrieve rules were triggered: \n");
	PrintRuleLockList ( retrieve_locks );
	additional_queries = ModifyVarNodes ( retrieve_locks,
					     length ( user_rangetable ),
					     current_varno,
					     reldesc,
					     user_tlist,
					     user_rangetable,
					     user_parsetree
					     );
    }

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
#ifdef BOGUS
	retrieve_locks = MatchRetrieveLocks ( rlocks , current_varno,
					      user_parsetree );
	if ( retrieve_locks ) {
	    List new_queries = NULL;
	    printf ( "\nRetrieve triggered the following locks:\n");
	    PrintRuleLockList ( retrieve_locks );
	    
	    new_queries = 
	      ModifyUpdateNodes( retrieve_locks,
				user_parsetree,
				drop_user_query,
				current_varno);

	    additional_queries = append (additional_queries, new_queries );
	}
#endif
	break;
      case DELETE:
	/* no targetlist, so handled differently */
	delete_locks = MatchDeleteLocks ( rlocks , current_varno,
					 user_parsetree );
	if ( delete_locks ) {
	    List new_queries = NULL;
	    extern List ExpandAll();
	    List fake_tlist = NULL;
	    int fake_resno = 1;
	    
	    int result_relation_index = 
	      CInteger(root_result_relation(parse_root(user_parsetree)));
	    Name result_relation_name = 
	      (Name)CString(rt_relname ( nth(result_relation_index- 1 ,
			       root_rangetable(parse_root(user_parsetree)))));

	    printf ( "\nDelete triggered the following locks:\n");
	    PrintRuleLockList ( delete_locks );

	    fake_tlist = ExpandAll(result_relation_name, &fake_resno);
	    parse_targetlist ( user_parsetree ) = fake_tlist;

	    new_queries = 
	      ModifyUpdateNodes( delete_locks,
				user_parsetree,
				drop_user_query,
				current_varno);

	    parse_targetlist ( user_parsetree ) = LispNil;

	    additional_queries = append (additional_queries, new_queries );
	}
	break;
      case APPEND:
	append_locks = MatchAppendLocks ( rlocks , current_varno,
					 user_parsetree );
	if ( append_locks ) {
	    List new_queries = NULL;
	    printf ( "\nAppend triggered the following locks:\n");
	    PrintRuleLockList ( append_locks );
	    
	    new_queries = 
	      ModifyUpdateNodes( append_locks,
				user_parsetree,
				drop_user_query,
				current_varno);

	    additional_queries = append (additional_queries, new_queries );
	}
	break;
      case REPLACE:
	replace_locks = MatchReplaceLocks ( rlocks , current_varno,
					    user_parsetree );
	if ( replace_locks ) {
	    List new_queries = NULL;
	    printf ( "\nReplace triggered the following locks:\n");
	    PrintRuleLockList ( replace_locks );
	    
	    new_queries = 
	      ModifyUpdateNodes( replace_locks,
				user_parsetree,
				drop_user_query,
				current_varno);

	    additional_queries = append (additional_queries, new_queries );
	}
	break;
      default:
	  elog(FATAL,"ProcessOneLock called on non query");
	  /* NOTREACHED */
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
    List new_parsetree 		= NULL;
    List new_rewritten 		= NULL;
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
	    if ( drop_user_query == false && additional_queries != NULL ) {
		/* start the output parse list with the
		 * user query since no qualifying
		 * "instead" clauses were found
		 */
		output_parselist = lispCons ( parsetree, LispNil );
	    }
	    if ( additional_queries != NULL ) {
		List i = NULL;
		foreach ( i , additional_queries ) {
		    new_parsetree = CAR(i);
		    new_rewritten = NULL;
		    
		    if ( new_parsetree != NULL ) 
		      new_rewritten = QueryRewrite ( new_parsetree );
		    if ( new_rewritten == NULL ) {
			/* if nothing in fact got changed */
			output_parselist = 
			  nappend1 ( output_parselist , new_parsetree );
		    } else {
		        output_parselist = append ( output_parselist,
						    new_rewritten );
		    }
		} /* rewrite any new queries */
	    } /* if there are new queries */
	} 
	
    }
    
    return ( output_parselist ); 

}

	
    
#ifdef OLDCODE

/*****************************************************
  
  the following routines still need to be written

 *****************************************************/

List
ModifyDeleteQueries( drop_user_query )
     bool *drop_user_query;
{
    return(NULL);
}

#endif
