/* 
 * $Header$
 */

#include "tmp/postgres.h"		/* for oid defs */
#include "utils/log.h"			/* for elog */
#include "parse.h"			/* atom defs */
#include "nodes/pg_lisp.h"		/* lisp support package */
#include "rules/prs2locks.h"		/* prs2 lock definitions */
#include "rules/prs2.h"			/* prs2 routine headers */
#include "nodes/primnodes.h"		/* Var node def */
#include "parser/parsetree.h"		/* parsetree manip routines */
#include "catalog/syscache.h"		/* for SearchSysCache */
#include "./locks.h"			/* for rewrite specific lock defns */
char
PutRelationLocks ( rule_oid, ev_oid, ev_attno,
		   ev_type, is_instead )
     oid rule_oid;
     oid ev_oid;
     int ev_attno;
     int ev_type;
     bool is_instead;
{
    char locktype = (LockIsRewrite | LockIsActive) ;

    switch ( ev_type ) {
      case RETRIEVE:
	locktype |= EventIsRetrieve;
	break;
      case REPLACE:
	locktype |= EventIsReplace;
	break;
      case APPEND:
	locktype |= EventIsAppend;
	break;
      case DELETE:
	locktype |= EventIsDelete;
	break;
      default:
	elog(WARN,"unknown event type %d",ev_type );
    }
    
    if ( is_instead ) {
	locktype |=  DoInstead;
    }
#ifdef DEPRECATED
    switch ( ac_type ) {
      case      0:		/* temp support for instead nothing */
      case APPEND:
      case DELETE:
	locktype |= DoOther;
	break;
      case RETRIEVE:
	if ( ev_type == RETRIEVE ) {
	    /* retrieve-retrieve is really retrieve-relation 
	     * unless it is over a ".all" targetlist, in which
	     * case, it has been transformed by calling routine
	     * into a REPLACE-CURRENT action
	     */
	    locktype |= DoExecuteProc;
	} else {
	    locktype |= DoOther;
	}
	break;
      case REPLACE:
	if (ac_result == 1 || ac_result == 2 ) {
	    locktype |= DoReplaceCurrentOrNew;
	} else {
	    locktype |= DoOther;
	}
	break;
      default:
	elog(WARN,"don't know action type");
	/* NOTREACHED */
	break;
    }
#endif DEPRECATED
    return ( locktype );
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

	if ( !null(temp) ) {
	    if ( IsA(temp,Var) && 
		 ( varno == get_varno((Var)temp)) && 
		 (( get_varattno((Var)temp) == attnum ) || attnum == -1 ) ) {
		     return ( true );
	    } else if ( IsA(temp,LispList) &&
			ThisLockWasTriggered ( varno, attnum, (List) temp )) {
			    return (true);
	    }
	}
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

    for ( i = 0 ; i < nlocks ; i++ ) {
	oneLock = prs2GetOneLockFromLocks ( rulelocks , i );
	if ( IsActive(oneLock) && IsRewrite(oneLock) &&
		LockEventIsRetrieve(oneLock)) {
	    if ( ThisLockWasTriggered ( varno,
				       oneLock->attributeNumber,
				       parse_subtree )) 
	      real_locks = nappend1 ( real_locks, (LispValue)oneLock );
	}
    }

    return ( real_locks );

}


/*
 * MatchLocks
 * - match the list of locks, 
 */
List
MatchLocks ( locktype, rulelocks , varno , user_parsetree )
     Prs2LockType locktype;
     RuleLock rulelocks;
     int varno;
     List user_parsetree;
{
    List real_locks 		= NULL;
    List root;
    Prs2OneLock oneLock		= NULL;
    int nlocks			= 0;
    int i			= 0;
    List actual_result_reln	= NULL;
    
    Assert ( rulelocks != NULL ); /* we get called iff there is some lock */
    Assert ( user_parsetree != NULL );

    root = parse_root ( user_parsetree );

    actual_result_reln =  root_result_relation (root);

    if (root_command_type(root) != RETRIEVE) 
	if (CInteger ( actual_result_reln ) != varno ) {
	    return ( NULL );
	}

    nlocks = prs2GetNumberOfLocks ( rulelocks );

    for ( i = 0 ; i < nlocks ; i++ ) {
	oneLock = prs2GetOneLockFromLocks ( rulelocks , i );
	if ( IsActive(oneLock) && IsRewrite(oneLock) &&
	    Event(oneLock) == locktype )  {
	    if (root_command_type(root) == RETRIEVE) {
		if ( ThisLockWasTriggered ( varno,
					   oneLock->attributeNumber,
					   user_parsetree ))
		    real_locks = nappend1 ( real_locks, (LispValue)oneLock );
	    }
	    else real_locks = nappend1 ( real_locks, (LispValue)oneLock );
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

    return ( MatchLocks ( EventIsReplace , 
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
    return ( MatchLocks ( EventIsAppend , 
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

    return ( MatchLocks ( EventIsDelete , 
			 rulelocks, current_varno , user_parsetree ));
}

