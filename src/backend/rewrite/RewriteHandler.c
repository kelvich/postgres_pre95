/* ----------------------------------------------------------------
 * $Header$
 * ----------------------------------------------------------------
 */

/****************************************************/

#include "tmp/postgres.h"
#include "utils/log.h"
#include "nodes/pg_lisp.h"		/* for LispValue and lisp support */
#include "nodes/primnodes.h"
#include "nodes/primnodes.a.h"		/* for accessors to varnodes etc */

#include "rules/prs2.h"			/* XXX - temporarily */
/* #include "rules/prs2locks.h"		/* XXX - temporarily */

#include "parser/parse.h"		/* for RETRIEVE,REPLACE,APPEND ... */
#include "parser/parsetree.h"		/* for parsetree manipulation */

#include "./RewriteSupport.h"	
#include "./RewriteDebug.h"
#include "./RewriteManip.h"
#include "./RewriteHandler.h"
#include "./locks.h"

extern List lispCopy();

/*****************************************************/

/*
 * ChangeTheseVars
 *
 * 	varno,varattno - varno and varattno of Var node must match this
 *			 to qualify for change
 *	parser_subtree - tree to iterate over
 *	lock	       - the lock which caused this rewrite
 */

ChangeTheseVars ( varno, varattno, parser_subtree, replacement , modified )
     int varno;
     AttributeNumber varattno;
     List parser_subtree;
     List replacement;
     bool *modified;
{
    List i = NULL;

    foreach ( i , parser_subtree ) {
	Node temp = (Node)CAR(i);
	if (temp ) {
	    if (IsA(temp,Var) &&
		((get_varattno((Var)temp) == varattno )||
		 (varattno == -1)) &&
		(get_varno((Var)temp) == varno ) ) {

		*modified = true;
		CAR(i) = replacement;

	    }
	    if (  temp->type == classTag(LispList))
	      ChangeTheseVars ( varno, varattno, (List)temp,
			        replacement , modified );
	}
    }
}

/*
 * used only by procedures,
 */

ReplaceVarWithMulti ( varno, attno, parser_subtree, replacement , modified )
     int varno;
     AttributeNumber attno;
     List parser_subtree;
     List replacement;
     bool *modified;
{
    List i = NULL;
    List vardotfields = NULL;
    List saved = NULL;

    foreach ( i , parser_subtree ) {
	Node temp = (Node)CAR(i);
	
	if (IsA(temp,Var) &&
	    get_varattno((Var)temp) == attno &&
	    get_varno((Var)temp) == varno ) {
	    vardotfields = get_vardotfields ( temp );

 	    if ( vardotfields != NULL ) {
		/* a real postquel procedure invocation */
		List j = NULL;

		*modified = true;
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

	    } /* vardotfields != NULL */

	}

	
	if ( temp->type == PGLISP_DTPR )
	  ReplaceVarWithMulti ( varno, attno, (List)temp,
			        (List)replacement ,
			        modified );
	
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
    List action_info		= NULL;
    List user_qual		= parse_qualification(user_parsetree);
    bool modified		= false;

    foreach ( i , retrieve_locks ) {
	Prs2OneLock this_lock = (Prs2OneLock)CAR(i);
	List rule_tlist = NULL;
	List rule_qual = NULL;
	List rule_rangetable = NULL;
	int result_rtindex = 0 ;
	char *result_relname =  NULL;
	List result_rte = NULL;
	List saved_parsetree = NULL;
	List saved_tl, saved_qual;
	List ev_qual = NULL;

	action_info = RuleIdGetActionInfo ( this_lock->ruleId );

	/* is_instead is always false for replace current
	 * and retrieve-retrieve , so don't need to check
	 */

	ev_qual = CAR(action_info);
	ruletrees = CDR(action_info);

	{
	    List ruleparse = ruletrees;

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
	     XXX - modified always = true here since rule
	     was triggered ? maybe not if retrieving
	     procedure text is accidentally
	     triggered by actual procedure invocation

	     *****************************************/
	    HandleVarNodes ( ruleparse,
			     user_parsetree,
			     current_varno,
			     user_rt_length - 2 );

	    switch ( Action(this_lock)) {
	      case DoReplaceCurrentOrNew:
		/* ON RETRIEVE DO REPLACE CURRENT */
		
		elog ( NOTICE, "replace current action");
		Print_parse ( ruleparse );
		
		result_rtindex =
		  CInteger(root_result_relation(parse_root(ruleparse )));
		result_rte = nth ( result_rtindex -1 , rule_rangetable );
		result_relname = CString ( CAR ( result_rte ));
		if ( strcmp ( result_relname,"*CURRENT*"))
		  elog(WARN,"a on-retr-do-repl rulelock with bogus result");
		saved_parsetree = lispCopy ( user_parsetree );
		saved_qual = parse_qualification(saved_parsetree);
		saved_tl = parse_targetlist(saved_parsetree);
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
				     saved_tl,
				     tlist_expr,
				     &modified );
		    ChangeTheseVars ( current_varno,
				     attno,
				     saved_qual,
				     tlist_expr,
				     &modified );

		} /* foreach */
		break;
	      case DoExecuteProc:
		/* now stuff the entire targetlist
		 * into this one varnode, filtering
		 *  out according to whatever is
		 *  in vardotfields
		 * NOTE: used only by procedures ???
		 */
		elog(NOTICE,"Possibly retrieving procedure result fields");
		ReplaceVarWithMulti ( current_varno,
				     this_lock->attributeNumber,
				     user_tl,
				     rule_tlist ,
				     &modified );
		if ( modified ) {
		    printf ("after replacing tlist entry :\n");
		    lispDisplay ( user_tl, 0 );
		    fflush(stdout);
		    result_relname = "";
		    FixResdom ( user_tl );
		} else {
		    elog(NOTICE,"no actual modification");
		}
		break;
	      case DoOther:
		elog(NOTICE,"retrieve triggering other actions");
		elog(NOTICE,"does not modify existing parsetree");
		break;
	      default:
		elog(WARN,"on retrieve do <action>, action unknown");
	    }

	    if ( modified ) {
		AddQualifications ( saved_parsetree ,
				   rule_qual,
				   0 );
		CDR(last(user_rt)) =
		  CDR(CDR(rule_rangetable));

		/* LP says : no need to clean up redundant rt_entries
		 * because planner will ignore them
		 */
	
		if ( ev_qual ) {
		    /* XXX - offset varnodes so that current and new
		     * point correctly here
		     */
		    AddEventQualifications ( saved_parsetree,
					     ev_qual);
		    AddNotEventQualifications ( user_parsetree,
					     ev_qual);
					
		    Print_parse ( saved_parsetree );
		    additional_queries = lispCons ( saved_parsetree,
						   additional_queries );

		}
		printf ("\n");
		printf ("*************************\n");
		printf (" Modified User Parsetree\n");
		printf ("*************************\n");
		Print_parse ( user_parsetree );
	    } else {
		elog(NOTICE,"parsetree not modified");
	    }
	} /* foreach of the ruletrees */
    } /* foreach of the locks */

    return ( additional_queries );
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
    List action_info	= NULL;
    int offset_trigger  = 0;

    Assert ( update_locks != NULL ); /* otherwise we won't have been called */

    /* drop_user_query must have been false, otherwise, we wouldn't
     * have entered this routine
     */

    Assert ( *drop_user_query == false );

    foreach ( i , update_locks ) {
	Prs2OneLock this_lock 	= (Prs2OneLock)CAR(i);
	
	printf ("\nNow processing :");
	PrintRuleLock ( this_lock );
	printf ("\n");

	action_info = RuleIdGetActionInfo ( this_lock->ruleId );
	ruletrees = CDR(action_info);

	/*
	 * if drop_user_query has been set, we have hit an "instead" rule
	 * which deactivates any rules that may have been activated
	 */

	if (*drop_user_query) {
	    elog ( NOTICE,
		  "drop_user_query is set ; punting on rule with id %d",
		  this_lock->ruleId );
	  return ( new_queries );
	}

	if ( IsInstead(this_lock) ) {
	    *drop_user_query = true;
	}
	
	{
	    List rule_action = ruletrees;
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

	    AddQualifications(rule_action,
			     parse_qualification(user_parsetree),
			     length(rule_rt) -2 );

	    FixRangeTable ( rule_root, user_rt );

	    new_queries = nappend1 ( new_queries, rule_action );
	}

    printf("\n Done processing :\n");
    PrintRuleLock ( this_lock );
    printf("\n");
	
    } /* foreach update_lock */

    return(new_queries);
}

/*
 * ProcessEntry
 * - process a single rangetable entry
 *
 * RETURNS: a list of additional queries that need to be executed
 * MODIFIES: current qual or tlist
 *
 */
List
ProcessEntry ( user_parsetree , reldesc , user_rangetable ,
		 current_varno , command , drop_user_query , rlocks )
     List user_parsetree;
     Relation reldesc;
     List user_rangetable;
     int current_varno;
     int command;
     bool *drop_user_query;
     RuleLock rlocks;
{
    List retrieve_locks 	= NULL;
    List replace_locks		= NULL;
    List append_locks		= NULL;
    List delete_locks		= NULL;
    List additional_queries 	= NULL;
    List user_tlist 		= parse_targetlist(user_parsetree);

    Assert ( rlocks != NULL );	
    Assert ( *drop_user_query == false );

    /*
     * if we got here, there better be some locks on the
     * current range table entry, and drop_user_query cannot
     * possibly have been set
     */

    switch (command) {
      case RETRIEVE:
	/* do nothing since it is handled below */
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

	    /*
	     * reset a "delete" targetlist to nil, otherwise
	     * executor will die a horrible death because it is not
	     * expecting any value there.
	     * why is this ? (who knows)
	     */
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
	  elog(FATAL,"ProcessEntry called on non query");
	  /* NOTREACHED */
    }
    /*
     * MATCH RETRIEVE RULES HERE (regardless of current command type)
     * a varnode that matches the current varno and the varattno matches
     * "rlocks" attnum ( where match means = if attnum is > 0 or always true
     * if attnum = -1 )
     */

    if ( *drop_user_query == false ) {
	retrieve_locks = MatchRetrieveLocks ( rlocks , current_varno ,
					     user_parsetree );
	if ( retrieve_locks ) {
	    printf ( "\nThese retrieve rules were triggered: \n");
	    PrintRuleLockList ( retrieve_locks );
	
	    /*
	     * testing for event qualifications go here
	     * n event quals generate sq(n) "user_parsetrees" ?
	     * or n+1 parsetrees ?
	     */
	
	    additional_queries = ModifyVarNodes ( retrieve_locks,
						 length ( user_rangetable ),
						 current_varno,
						 reldesc,
						 user_tlist,
						 user_rangetable,
						 user_parsetree
						 );
	}
	
    } /* if user_query is still active */

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
    extern List QRS();

    return ( QRS ( parsetree , NULL ) );
}

List
QRS ( parsetree , already_handled )
     List parsetree;
     List already_handled;
{
    List user_root;
    List user_rangetable;
    List command_type;
    List user_qual;
    int  user_command;
    List new_parsetree 		= NULL;
    List new_rewritten 		= NULL;
    List output_parselist	= NULL;
    List user_rtentry_ptr	= NULL;
    List user_tl		= NULL;
    int  user_rt_length		= 0;
    int this_relation_varno	= 1;		/* varnos start with 1 */
    bool drop_user_query	= false;	/* unless ProcessEntry
						 * modifies this, we
						 * add on the user_query
						 * in front of all queries
						 */
    Assert ( parsetree != NULL );
    Assert ( (user_root = parse_root(parsetree)) != NULL );
    command_type = root_command_type_atom(user_root);

    user_rangetable 		= root_rangetable(user_root);
    user_rt_length 		= length ( user_rangetable );

    /* if this is a recursive query, command_type node is funny */
    if ((consp(command_type)) && (CInteger(CAR(command_type)) == '*'))
	user_command = (int) CAtom(CDR(command_type));
    else
        user_command = (int) CAtom(command_type);

#ifdef VERBOSE

    printf("\nQueryRewrite being called with :\n");
    Print_parse ( parsetree );

#endif

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
	RuleLock this_entry_locks = NULL;
	ObjectId this_relid = NULL;

	this_rtname = (Name)CString( CADR ( user_rtentry ));
	
        this_relation = amopenr ( this_rtname );
	Assert ( this_relation != NULL );

	this_entry_locks = RelationGetRelationLocks ( this_relation );
	this_relid = RelationGetRelationId ( this_relation );

	if ( member ( lispInteger( this_relid ), already_handled ) )
	  continue;
	
	already_handled = lispCons ( lispInteger( this_relid ),
				     NULL );

	if ( this_entry_locks != NULL ) {
	    /*
	     * this relation has a lock,
	     */

	    List additional_queries 	= NULL;
	    List additional_query	= NULL;

	    /*
	     * ProcessEntry _may_ modify the contents
	     * of "user_qual" or "user_tl" and "user_rangetable" if
	     * the rule referenced by the lock is of type "XXXWrite"
	     * clause
	     */

	    additional_queries = ProcessEntry ( parsetree,
						 this_relation,
						 user_rangetable,
						 this_relation_varno++ ,
						 user_command ,
						 &drop_user_query ,
						 this_entry_locks );

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
		      new_rewritten = QRS ( new_parsetree , already_handled );
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
