#include "prs2locks.h"
#include "prs2.h"
#include "pg_lisp.h"
#include "c.h"
#include "parsetree.h"
#include "atoms.h"
#include "catname.h"
#include "relation.h"
#include "heapam.h"		/* access methods like amopenr */
#include "log.h"
#include "parse.h"
#include "htup.h"
#include "ftup.h";
#include "fmgr.h"
#include "datum.h"
#include "catalog_utils.h"
#include "rel.h"		/* for Relation stuff */
#include "syscache.h"		/* for SearchSysCache ... */
#include "itup.h"		/* for T_LOCK */
#include "primnodes.a.h"

/* $Header$ */

extern LispValue TheseLocksWereTriggered ( /* RuleLock, LispValue */ );
extern RuleLock RelationGetRelationLocks ();
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
    Datum datum;
    Boolean isnull;

    relationTuple = SearchSysCacheTuple(RELNAME,
		       &(RelationGetRelationTupleForm(relation)->relname),
		       (char *) NULL,
		       (char *) NULL,
		       (char *) NULL);

    /* datum = HeapTupleGetAttributeValue(
		relationTuple,
		InvalidBuffer,
		(AttributeNumber) -2,
		(TupleDescriptor) NULL,
		&isnull);

    relationLocks = (RuleLock)DatumGetPointer(datum); */

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

#ifdef FUZZY
/*
 * DoParallelProofPaths
 *
 * finds all tlist entries with resnames of "u"
 * and combines their values in the way of FRIL
 *
 */

List
DoParallelProofPaths ( targetlist )
     List targetlist;
{
    List i = targetlist;
    List upper = NULL;
    List lower = NULL;
    int  dempster_length = 0;
    int  index = 0;

    while ( i != LispNil &&
	    CAR(i) != LispNil &&
	    CAR(CAR(i)) != LispNil &&
	    IsA(CAR(CAR(i)),Resdom) &&
	    strcmp ( get_resname (CAR(CAR(i))), "l" ))
      i = CDR(i);

    /* i should now point to the first "l" node */

    dempster_length = length(i);

    if ((dempster_length % 2) != 0 ) 
      elog (WARN, "uneven length of support pairs");
    
    dempster_length = dempster_length / 2;
    dempster_length = dempster_length - 1; 

    /* if only one, don't need to combine */
    if (dempster_length <= 0 )
      return targetlist;

    lispDisplay ( i , 0 );
    printf ("\n%d\n",dempster_length );

    for ( index = 0 ; index < dempster_length ; index ++ ) {
	Func udempster 	= NULL;
	Func ldempster 	= NULL;
	List l1		= CAR(i);
	List u1		= CADR(i);
	List l1_rhs 	= CADR(l1);
	List u1_rhs 	= CADR(u1);
	List l2_rhs 	= CADR(CADR(CDR(i)));
	List u2_rhs 	= CADR(CADR(CDR(CDR(i))));
	List l1_resdom 	= CAR(l1);
	List u1_resdom 	= CAR(u1);
	List u_result 	= NULL;
	List l_result 	= NULL;

	/* remove the second u,l pair from the targetlist 
	   since we already have a handle on it, this should be OK */

	CDR(CDR(i)) = CDR(CDR(CDR(CDR(i))));

	/* XXX - hacks ... should use #defines instead */

	udempster = MakeFunc ( F_FUZ_U_DEMPSTER, 701, LispNil );
	ldempster = MakeFunc ( F_FUZ_L_DEMPSTER, 701, LispNil );

	l_result = lispCons ( ldempster,
		    lispCons (  l1_rhs, 
		     lispCons ( u1_rhs,
		      lispCons ( l2_rhs,
		       lispCons ( u2_rhs,LispNil )))));

	u_result = lispCons ( udempster,
		    lispCons (  l1_rhs, 
		     lispCons ( u1_rhs,
		      lispCons ( l2_rhs,
		       lispCons ( u2_rhs,LispNil )))));
	
	CDR(l1) = lispCons(l_result,LispNil);
	CDR(u1) = lispCons(u_result,LispNil);
    }
    
    printf("\n=======> After Dempsterizing, the new targetlist is :\n");
    lispDisplay ( targetlist, 0 );
}

#endif

/* 
 * RuleIdGetRuleParse
 *
 * given a rule oid, look it up and return the prs2text
 *
 */
List
RuleIdGetRuleParse ( ruleoid )
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
    return (CAR(ruleparse));
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
		elog(WARN,"NEW not properly taken care of");
	    } else {
		set_varno ( thisnode, get_varno(thisnode) + offset );

		CAR( get_varid ( thisnode ) ) =
		  lispInteger (get_varno(thisnode) );
	    }
	}
	if ( thisnode && IsA(thisnode,Param) ) {
	    /* currently only *CURRENT* */
#ifdef BOGUS
	    Var varnode = MakeVar ( trigger_varno, 
				    get_paramid ( thisnode ),
				    get_paramtype ( thisnode ),
				    LispNil,
				    LispNil,
				    lispCons ( lispInteger(trigger_varno),
				      lispCons (
				        lispInteger (get_paramid (thisnode)),
					   LispNil )) );
				   
	    CAR(i) = (List)varnode;
#endif
	}
	if ( thisnode && thisnode->type == PGLISP_DTPR )
	  OffsetAllVarNodes(trigger_varno,thisnode,offset);
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
	  ChangeTheseVars ( varno, varattno, temp, replacement );
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
		    char *var_dotfields_firstname = 
		      CString ( CAR ( vardotfields ) );
		    List var_dotfields_rest = CDR( vardotfields );
		    
		    if ( !strcmp ( rule_resdom_name, 
				  var_dotfields_firstname ) ) {
		      if ( IsA(CAR(saved),Resdom) &&
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
	      }
	  }	
	
	if ( temp->type == PGLISP_DTPR )
	  ReplaceVarWithMulti ( varno, attno, temp, replacement );
	
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
    List parselist	= NULL;
    List i		= NULL;
    List user_tl	= NULL;
    List user_qual	= NULL;
    int  rt_length	= 0;
    int  varno		= 1;

    user_tl = parse_targetlist(parsetree);
    user_qual = parse_qualification(parsetree);

    if ((root = parse_root(parsetree)) != NULL ) {
	rangetable = root_rangetable(root);
	command = root_command_type ( root );
	rt_length = length ( rangetable );
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
	    RuleLock rlocks;
	    LispValue qual_locks = NULL;
	    LispValue tlist_locks = NULL;

	    rlocks = RelationGetRelationLocks ( to_be_rewritten );

	    /*
	     * only for retrieve rules do we have to
	     * worry about the qualifications
	     */

	    if ( command == RETRIEVE )
	      qual_locks = TheseLocksWereTriggered ( rlocks, user_qual, 
						     RETRIEVE );

	    tlist_locks = TheseLocksWereTriggered ( rlocks, user_tl ,
						    command );
	    

	    if ( tlist_locks ) {
		/* 
		 * user query actually triggered a lock 
		 */
		LispValue j;

		printf("Targetlist triggered these locks : \n");
		foreach ( j , tlist_locks ) {
		    Prs2OneLock temp = (Prs2OneLock)CAR(j);
		    List ruleparse = NULL;
		    List rule_tlist = NULL;
		    List rule_qual = NULL;
		    List rule_rangetable = NULL;
		    int action_command = 0;

		    /*
		     * for now, since the 289 project
		     * only requires a single "action"
		     * this returns a single parsetree.
		     * XXX - this needs to change to multi-parsetrees
		     */

		    ruleparse = RuleIdGetRuleParse ( temp->ruleId );
		    
		    Assert (ruleparse != NULL );
		    rule_tlist = parse_targetlist(ruleparse);
		    rule_qual = parse_qualification(ruleparse);
		    rule_rangetable = root_rangetable(parse_root
						      (ruleparse));
		    action_command = CInteger ( CADR ( CAR ( ruleparse) ));

		    printf("\t{ rulid = %ld ",temp->ruleId);
		    printf("locktype = %c ", temp->lockType );
		    printf("attnum = %d }\n", temp->attributeNumber );

		    if ( temp->attributeNumber == -1 )
		      elog(NOTICE, "firing a multi-attribute rule");

		    switch ( temp->lockType ) {
		      case LockTypeRetrieveWrite:
			OffsetAllVarNodes ( varno, 
					   ruleparse, rt_length -2 );
			Print_parse ( ruleparse );
			{
			    int result_rtindex = 0 ;
			    List result_rte = NULL;
			    char *result_relname =  NULL;

			    if ( action_command == REPLACE ) {
				result_rtindex = 
				  CInteger (root_result_relation 
					    ( parse_root ( ruleparse )) );
				result_rte = 
				  nth ( result_rtindex -1 , rule_rangetable );
				
				result_relname = 
				  CString ( CAR ( result_rte ));
			    } else if (action_command == RETRIEVE ) {
				/* now stuff the entire targetlist
				   into this one varnode, filtering
				   out according to whatever is
				   in vardotfields
				 */
				ReplaceVarWithMulti ( varno, 
						     temp->attributeNumber, 
						     user_tl, 
						     rule_tlist );
				result_relname = "";
				CDR(last(rangetable)) = 
				  CDR(CDR(rule_rangetable));
				/* 
				CAR(i) = CAR(CDR(i));
				CDR(i) = CDR(CDR(i));
				*/
				if ( parse_qualification(parsetree) == NULL )
				  parse_qualification(parsetree) = rule_qual;
				
				else
				  parse_qualification ( parsetree ) =
				    lispCons ( lispInteger(AND),  
				     lispCons ( parse_qualification(parsetree),
				      lispCons ( rule_qual, LispNil )));    
			    }

			    if ( !strcmp ( result_relname, "*CURRENT*" )) {
				List i = LispNil;
				
				elog ( NOTICE, "replace current action");
				foreach ( i , rule_tlist ) {
				    List tlist_entry = CAR(i);
				    Resdom tlist_resdom = (Resdom)
				      CAR(tlist_entry);
				    List tlist_expr = CADR(tlist_entry);
				    char *attname = (char *)
				      get_resname ( tlist_resdom );
				    int attno;
				    
				    elog ( NOTICE, "replacing %s", attname );
				    attno = 
				      varattno ( to_be_rewritten, attname );
				    
				    ChangeTheseVars ( varno, 
						     attno,
						     user_tl,
						     tlist_expr );
				} /* foreach */
				CDR(last(rangetable)) = 
				  CDR(CDR(rule_rangetable));
				/* 
				CAR(i) = CAR(CDR(i));
				CDR(i) = CDR(CDR(i));
				*/
				if ( parse_qualification(parsetree) == NULL )
				  parse_qualification(parsetree) = rule_qual;
				
				else
				  parse_qualification ( parsetree ) =
				    lispCons ( lispInteger(AND),  
				     lispCons ( parse_qualification(parsetree),
				      lispCons ( rule_qual, LispNil )));    
				

			    } else {
				elog ( NOTICE, "normal replace-action");
			    }
		        } /* case replace action */
			break;
		      default:
			elog(WARN,"unsupported rule type");
			break;
			
		    } /* end switch */
		    rt_length = length ( rangetable );
		    
		    printf ("\nAfter one lock\n");
		    Print_parse ( parsetree );
		    
		    FixResdom ( user_tl );
#ifdef FUZZY
		    DoParallelProofPaths(user_tl);
#endif
		    
		}
	    }
	    if ( qual_locks ) {
		LispValue j;
		
		printf("qualifications triggered these locks : \n" );
		foreach ( j , qual_locks ) {
		    Prs2OneLock temp = (Prs2OneLock)CAR(j);
		    printf("\t{ rulid = %ld ",temp->ruleId);
		    printf("locktype = %c ", temp->lockType );
		    printf("attnum = %d }\n", temp->attributeNumber );
		}
	    } /* if qual_locks */

	} /* if RelationHasLocks ... */ 
	
	varno = varno + 1; /* check the next rangetable entry */
#ifdef BOGUS
	parselist = cons ( parsetree , parselist );
#endif
    } /* foreach entry of rangetable */

    return (parselist);
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
ThisLockWasTriggered ( attnum, parse_subtree )
     AttributeNumber attnum;
     LispValue parse_subtree;
{
    LispValue i;

    foreach ( i , parse_subtree ) {

	Node temp = (Node)CAR(i);

	if ( IsA(temp,Var) && 
	     (( get_varattno((Var)temp) == attnum ) ||
	        attnum == -1 ) )
	  return ( true );
	if ( temp->type == PGLISP_DTPR &&
	     ThisLockWasTriggered ( attnum, temp ) )
	  return ( true );

    }

    return ( false );
}

LispValue
TheseLocksWereTriggered ( rulelocks , parse_subtree, event_type )
     RuleLock rulelocks;
     LispValue parse_subtree;
     int event_type;
{
    int nlocks		= 0;
    int i 		= 0;
    Prs2OneLock oneLock	= NULL;
    int j 		= 0;
    List real_locks 	= NULL;
    Assert ( rulelocks != NULL );
    nlocks = prs2GetNumberOfLocks ( rulelocks );
    Assert (nlocks <= 16 );

    for ( i = 0 ; i < nlocks ; i++ ) {

	oneLock = prs2GetOneLockFromLocks ( rulelocks, i );

	switch ( oneLock->lockType ) {
	  case LockTypeRetrieveAction:
	  case LockTypeRetrieveWrite:
	    if ( event_type == RETRIEVE &&
		ThisLockWasTriggered ( oneLock->attributeNumber , 
				      parse_subtree ) )
	      real_locks = lispCons ( oneLock, real_locks );
	    else
	      continue;
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

